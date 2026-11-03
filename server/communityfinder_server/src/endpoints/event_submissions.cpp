#include "event_submissions.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "business_logic/auth/session.h"
#include "business_logic/events/submission_helper.h"
#include "endpoints/endpoint_auth_helper.h"
#include "util/error_codes.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

// Reads the form into a SubmissionRequest. Checks types only — whether a value
// is missing or acceptable is SubmissionHelper's call (and its error code).
// Absent and null both mean "not given".
class SubmissionReader {
public:
    explicit SubmissionReader(const Json::Value& body) : body_(body) {}

    // "" when the body is well formed; otherwise the problem, and Field() says
    // which field it is in.
    std::string Read(Events::SubmissionRequest& request) {
        if (!body_.HasChildren()) {
            problem_ = "The request body must be a JSON object";
            return problem_;
        }
        ReadString("title", request.title);
        ReadDates(request.dates);
        ReadOptionalInt("venue_id", request.venueId);
        ReadString("location_text", request.locationText);
        ReadString("description", request.description);
        ReadString("url", request.url);
        ReadString("cost_text", request.costText);
        ReadStrings("category_slugs", request.categorySlugs);
        ReadStrings("scene_tag_slugs", request.sceneTagSlugs);
        ReadBool("regular_thing", request.regularThing);
        ReadString("relationship", request.relationship);
        ReadString("submitter_note", request.submitterNote);
        ReadBool("agreed", request.agreed);
        ReadString("website", request.website);
        return problem_;
    }

    const std::string& Field() const { return field_; }

private:
    const Json::Value* Child(const Json::Value& parent, std::string_view name) {
        const Json::Value* child = nullptr;
        if (!problem_.empty() || !parent.HasChild(name, &child) || child->IsNull()) {
            return nullptr;
        }
        return child;
    }

    void Fail(std::string_view field, std::string_view what) {
        field_ = std::string(field);
        problem_ = std::string(field) + " must be " + std::string(what);
    }

    void ReadString(std::string_view name, std::string& out) {
        if (const Json::Value* field = Child(body_, name)) {
            if (const std::string* text = field->TryGet<std::string>()) {
                out = *text;
            } else {
                Fail(name, "a string");
            }
        }
    }

    void ReadBool(std::string_view name, bool& out) {
        if (const Json::Value* field = Child(body_, name)) {
            if (const bool* flag = field->TryGet<bool>()) {
                out = *flag;
            } else {
                Fail(name, "true or false");
            }
        }
    }

    void ReadOptionalInt(std::string_view name, std::optional<int64_t>& out) {
        if (const Json::Value* field = Child(body_, name)) {
            if (const int64_t* number = field->TryGet<int64_t>()) {
                out = *number;
            } else {
                Fail(name, "an integer");
            }
        }
    }

    void ReadStrings(std::string_view name, std::vector<std::string>& out) {
        const Json::Value* field = Child(body_, name);
        if (field == nullptr) {
            return;
        }
        const Json::JsonArray* array = field->TryGetArray();
        if (array == nullptr) {
            Fail(name, "an array of strings");
            return;
        }
        for (const Json::Value& element : *array) {
            const std::string* text = element.TryGet<std::string>();
            if (text == nullptr) {
                Fail(name, "an array of strings");
                return;
            }
            out.push_back(*text);
        }
    }

    void ReadDates(std::vector<Events::SubmissionDate>& out) {
        constexpr std::string_view kDates = "dates";
        const Json::Value* field = Child(body_, kDates);
        if (field == nullptr) {
            return;
        }
        const Json::JsonArray* array = field->TryGetArray();
        if (array == nullptr) {
            Fail(kDates, "an array of {starts_at_us, ends_at_us}");
            return;
        }
        for (const Json::Value& element : *array) {
            Events::SubmissionDate date;
            const Json::Value* starts = Child(element, "starts_at_us");
            const int64_t* startsAtUs = starts ? starts->TryGet<int64_t>() : nullptr;
            if (startsAtUs == nullptr) {
                Fail(kDates, "an array of {starts_at_us, ends_at_us}, each with an integer starts_at_us");
                return;
            }
            date.startsAtUs = *startsAtUs;
            if (const Json::Value* ends = Child(element, "ends_at_us")) {
                const int64_t* endsAtUs = ends->TryGet<int64_t>();
                if (endsAtUs == nullptr) {
                    Fail(kDates, "an array of {starts_at_us, ends_at_us}, with an integer ends_at_us when given");
                    return;
                }
                date.endsAtUs = *endsAtUs;
            }
            out.push_back(date);
        }
    }

    const Json::Value& body_;
    std::string problem_;
    std::string field_;
};

Json::Value StringArray(const std::vector<std::string>& strings) {
    Json::JsonArray array;
    for (const std::string& value : strings) {
        array.push_back(Json::Value(value));
    }
    return Json::Value(array);
}

Json::Value IdArray(const std::vector<int64_t>& ids) {
    Json::JsonArray array;
    for (int64_t id : ids) {
        array.push_back(Json::Value(id));
    }
    return Json::Value(array);
}

// The helper's refusal as a response: 403 for an unconfirmed email, 503 while
// submissions are paused, 429 for a rate limit (its detail states the limit),
// 400 for the rest.
crow::response SubmitError(const Events::SubmitResult& result) {
    std::string_view type = ErrorCodes::kValidationError;
    if (result.errorCode == Events::kSubmissionErrorEmailNotVerified) {
        type = ErrorCodes::kNotAuthorized;
    } else if (result.errorCode == Events::kSubmissionErrorPaused) {
        type = ErrorCodes::kServiceUnavailable;
    } else if (result.errorCode == Events::kSubmissionErrorRateLimited) {
        type = ErrorCodes::kTooManyAttempts;
    }
    Json::JsonObject extensions{ { "error_code", Json::Value(result.errorCode) } };
    if (!result.errorField.empty()) {
        extensions["field"] = Json::Value(result.errorField);
    }
    if (result.errorDateIndex >= 0) {
        extensions["date_index"] = Json::Value(result.errorDateIndex);
    }
    return ErrorResponse::Create(type, ErrorCodes::GetTitleForType(type),
                                 ErrorCodes::GetStatusForType(type), result.errorMessage,
                                 extensions);
}

crow::response ErrorWithCode(std::string_view type, std::string_view detail,
                             std::string_view errorCode) {
    return ErrorResponse::Create(type, ErrorCodes::GetTitleForType(type),
                                 ErrorCodes::GetStatusForType(type), detail,
                                 { { "error_code", Json::Value(std::string(errorCode)) } });
}

template <class Handler>
void Handle(WebApp* webApp, const crow::request& req, crow::response& resp, Handler handler) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = handler(endpointAuthHelper);
        if (resp.code == 200) {
            resp.set_header("Content-Type", "application/json");
            resp.write(result.ToString());
        }
    }
    catch (std::exception& e) {
        resp = ErrorResponse::InternalError(e.what());
    }
    resp.end();
}

class SetupRouting : public RoutingBase {
public:
    void AddRoute(WebApp* webApp) override {
        CROW_ROUTE(webApp->GetApp(), "/api/event_submissions")
            .methods(crow::HTTPMethod::Post)(
                [=](const crow::request& req, crow::response& resp) {
                    Handle(webApp, req, resp, [&](EndpointAuthHelper& helper) {
                        return PostEventSubmission(helper, req, resp);
                    });
                });
        CROW_ROUTE(webApp->GetApp(), "/api/event_submissions/mine")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    Handle(webApp, req, resp, [&](EndpointAuthHelper& helper) {
                        return GetMyEventSubmissions(helper, req, resp);
                    });
                });
        CROW_ROUTE(webApp->GetApp(), "/api/event_submissions/status")
            .methods(crow::HTTPMethod::Get)(
                [=](const crow::request& req, crow::response& resp) {
                    Handle(webApp, req, resp, [&](EndpointAuthHelper& helper) {
                        return GetEventSubmissionStatus(helper, req, resp);
                    });
                });
        CROW_ROUTE(webApp->GetApp(), "/api/event_submissions/<int>/withdraw")
            .methods(crow::HTTPMethod::Post)(
                [=](const crow::request& req, crow::response& resp, int64_t submissionId) {
                    Handle(webApp, req, resp, [&](EndpointAuthHelper& helper) {
                        return PostWithdrawEventSubmission(helper, req, resp, submissionId);
                    });
                });
    }
} g_setupRouting;

}  // namespace

Json::Value PostEventSubmission(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& req,
    crow::response& resp) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        Auth::Session& session = endpointAuthHelper.GetSession();
        if (!session.IsLoggedIn()) {
            resp = ErrorResponse::NotAuthenticated("Sign in to submit an event");
            return;
        }

        Json::Value body;
        try {
            body = Json::Value::FromText(req.body);
        }
        catch (const std::exception&) {
            resp = ErrorResponse::BadRequest("Request body must be JSON");
            return;
        }
        Events::SubmissionRequest request;
        SubmissionReader reader(body);
        std::string problem = reader.Read(request);
        if (!problem.empty()) {
            resp = ErrorResponse::ValidationError(problem, reader.Field());
            return;
        }

        Events::SubmissionHelper helper(endpointAuthHelper.GetDatabaseHelper());
        Events::SubmitResult submitted = helper.Submit(
            transaction, *endpointAuthHelper.GetSecretsHelper(), request, session.GetPersonId());
        if (!submitted.ok) {
            resp = SubmitError(submitted);
            return;
        }

        Json::JsonArray duplicates;
        for (const Events::DuplicateHint& hint : submitted.possibleDuplicates) {
            duplicates.push_back(Json::Value(Json::JsonObject{
                { "date_index", Json::Value(static_cast<int64_t>(hint.dateIndex)) },
                { "event_id", Json::Value(hint.eventId) },
                { "title", Json::Value(hint.title) },
                { "starts_at_us", Json::Value(hint.startsAtUs) },
            }));
        }
        result = Json::Value(Json::JsonObject{
            { "submission_id", Json::Value(submitted.submissionId) },
            { "event_ids", IdArray(submitted.eventIds) },
            { "ignored_category_slugs", StringArray(submitted.ignoredCategorySlugs) },
            { "ignored_scene_tag_slugs", StringArray(submitted.ignoredSceneTagSlugs) },
            { "possible_duplicates", Json::Value(duplicates) },
        });
        resp.code = 200;
    });

    return result;
}

Json::Value GetMyEventSubmissions(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& /*req*/,
    crow::response& resp) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        Auth::Session& session = endpointAuthHelper.GetSession();
        if (!session.IsLoggedIn()) {
            resp = ErrorResponse::NotAuthenticated("Sign in to see your submissions");
            return;
        }

        Events::SubmissionHelper helper(endpointAuthHelper.GetDatabaseHelper());
        Json::JsonArray items;
        for (const Events::SubmissionView& view :
             helper.GetMySubmissions(transaction, session.GetPersonId())) {
            Json::JsonArray dates;
            for (const Events::SubmissionDateView& date : view.dates) {
                dates.push_back(Json::Value(Json::JsonObject{
                    { "event_id", Json::Value(date.eventId) },
                    { "title", Json::Value(date.title) },
                    { "starts_at_us", Json::Value(date.startsAtUs) },
                    { "ends_at_us", date.endsAtUs ? Json::Value(*date.endsAtUs) : Json::Value(nullptr) },
                    { "state", Json::Value(date.state) },
                }));
            }
            items.push_back(Json::Value(Json::JsonObject{
                { "id", Json::Value(view.id) },
                { "created_us", Json::Value(view.createdUs) },
                { "relationship", Json::Value(view.relationship) },
                { "regular_thing", Json::Value(view.regularThing) },
                { "submitter_note", Json::Value(view.submitterNote) },
                { "reject_reason", Json::Value(view.rejectReason) },
                { "message_to_submitter", Json::Value(view.messageToSubmitter) },
                { "dates", Json::Value(dates) },
            }));
        }
        result = Json::Value(Json::JsonObject{ { "items", Json::Value(items) } });
        resp.code = 200;
    });

    return result;
}

Json::Value GetEventSubmissionStatus(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& /*req*/,
    crow::response& resp) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        const Events::SubmissionSettings settings = Events::LoadSubmissionSettings(
            *endpointAuthHelper.GetSecretsHelper(), transaction);
        result = Json::Value(Json::JsonObject{
            { "accepting_submissions", Json::Value(!settings.submissionsPaused) },
        });
        resp.code = 200;
    });

    return result;
}

Json::Value PostWithdrawEventSubmission(
    EndpointAuthHelper& endpointAuthHelper,
    const crow::request& /*req*/,
    crow::response& resp,
    int64_t submissionId) {
    auto tp = endpointAuthHelper.GetTransactionProvider();
    if (!tp) {
        resp = ErrorResponse::InternalError("Database unavailable");
        return {};
    }

    Json::Value result;
    tp->RunInTransaction([&](Transaction& transaction) {
        Auth::Session& session = endpointAuthHelper.GetSession();
        if (!session.IsLoggedIn()) {
            resp = ErrorResponse::NotAuthenticated("Sign in to withdraw a submission");
            return;
        }

        Events::SubmissionHelper helper(endpointAuthHelper.GetDatabaseHelper());
        Events::WithdrawResult withdrawn =
            helper.Withdraw(transaction, submissionId, session.GetPersonId());
        if (!withdrawn.ok) {
            if (withdrawn.errorCode == Events::kSubmissionErrorNotYours) {
                resp = ErrorResponse::NotFound("Submission not found");
            } else {
                resp = ErrorWithCode(ErrorCodes::kValidationError,
                                     "Nothing in this submission is waiting for review",
                                     withdrawn.errorCode);
            }
            return;
        }

        result = Json::Value(Json::JsonObject{
            { "submission_id", Json::Value(submissionId) },
            { "withdrawn_event_ids", IdArray(withdrawn.withdrawnEventIds) },
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
