#include "admin_ingest_events.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "business_logic/events/event_helper.h"
#include "db_schema/events.h"
#include "endpoints/endpoint_auth_helper.h"
#include "util/error_codes.h"
#include "util/error_response.h"

namespace Endpoints {
namespace {

// Every outcome, so `counts` always carries all six (zeros included).
constexpr std::string_view kOutcomes[] = {
    Events::kIngestOutcomeCreated,
    Events::kIngestOutcomeUpdated,
    Events::kIngestOutcomeUnchanged,
    Events::kIngestOutcomeRevertedToPending,
    Events::kIngestOutcomeRejectedKept,
    Events::kIngestOutcomeArchivedKept,
};

// A 400 validation error that says which item failed.
crow::response ItemError(std::string_view detail, size_t itemIndex,
                         std::string_view errorCode = "") {
    Json::JsonObject extensions{
        { "item_index", Json::Value(static_cast<int64_t>(itemIndex)) },
    };
    if (!errorCode.empty()) {
        extensions["error_code"] = Json::Value(std::string(errorCode));
    }
    return ErrorResponse::Create(
        ErrorCodes::kValidationError,
        ErrorCodes::GetTitleForType(ErrorCodes::kValidationError),
        ErrorCodes::GetStatusForType(ErrorCodes::kValidationError),
        detail,
        extensions);
}

// Reads one item into `item`. Checks types only — whether a value is missing
// or acceptable is the business layer's call (and its error code). Returns the
// problem, or "" when the item is well formed. Absent and null both mean
// "not stated".
class ItemReader {
public:
    explicit ItemReader(const Json::Value& value) : value_(value) {}

    std::string Read(Events::IngestItem& item) {
        if (!value_.HasChildren()) {
            return "must be an object";
        }
        ReadInt("source_id", item.sourceId);
        ReadString("external_key", item.externalKey);
        ReadString("title", item.title);
        ReadInt("starts_at_us", item.startsAtUs);
        ReadOptionalInt("ends_at_us", item.endsAtUs);
        ReadOptionalString("description", item.description);
        ReadOptionalString("url", item.url);
        ReadOptionalString("cost_text", item.costText);
        ReadOptionalString("location_text", item.locationText);
        ReadOptionalInt("venue_id", item.venueId);
        ReadOptionalString("series_slug", item.seriesSlug);
        ReadOptionalStrings("category_slugs", item.categorySlugs);
        ReadOptionalStrings("scene_tag_slugs", item.sceneTagSlugs);
        return problem_;
    }

private:
    const Json::Value* Field(std::string_view name) {
        const Json::Value* field = nullptr;
        if (!problem_.empty() || !value_.HasChild(name, &field) || field->IsNull()) {
            return nullptr;
        }
        return field;
    }

    void Fail(std::string_view name, std::string_view what) {
        problem_ = std::string(name) + " must be " + std::string(what);
    }

    void ReadInt(std::string_view name, int64_t& out) {
        std::optional<int64_t> value;
        ReadOptionalInt(name, value);
        if (value) out = *value;
    }

    void ReadString(std::string_view name, std::string& out) {
        std::optional<std::string> value;
        ReadOptionalString(name, value);
        if (value) out = *value;
    }

    void ReadOptionalInt(std::string_view name, std::optional<int64_t>& out) {
        if (const Json::Value* field = Field(name)) {
            if (const int64_t* number = field->TryGet<int64_t>()) {
                out = *number;
            } else {
                Fail(name, "an integer");
            }
        }
    }

    void ReadOptionalString(std::string_view name, std::optional<std::string>& out) {
        if (const Json::Value* field = Field(name)) {
            if (const std::string* text = field->TryGet<std::string>()) {
                out = *text;
            } else {
                Fail(name, "a string");
            }
        }
    }

    void ReadOptionalStrings(std::string_view name,
                             std::optional<std::vector<std::string>>& out) {
        if (const Json::Value* field = Field(name)) {
            const Json::JsonArray* array = field->TryGetArray();
            if (array == nullptr) {
                Fail(name, "an array of strings");
                return;
            }
            std::vector<std::string> strings;
            for (const Json::Value& element : *array) {
                const std::string* text = element.TryGet<std::string>();
                if (text == nullptr) {
                    Fail(name, "an array of strings");
                    return;
                }
                strings.push_back(*text);
            }
            out = std::move(strings);
        }
    }

    const Json::Value& value_;
    std::string problem_;
};

Json::Value StringArray(const std::vector<std::string>& strings) {
    Json::JsonArray array;
    for (const std::string& value : strings) {
        array.push_back(Json::Value(value));
    }
    return Json::Value(array);
}

void HandlePost(WebApp* webApp, const crow::request& req, crow::response& resp) {
    try {
        EndpointAuthHelper endpointAuthHelper(*webApp, req, resp);
        endpointAuthHelper.Initialize();
        Json::Value result = PostAdminIngestEvents(endpointAuthHelper, req, resp);
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
        CROW_ROUTE(webApp->GetApp(), "/api/admin/ingest_events")
            .methods(crow::HTTPMethod::Post)(
                [=](const crow::request& req, crow::response& resp) {
                    HandlePost(webApp, req, resp);
                });
    }
} g_setupRouting;

}  // namespace

Json::Value PostAdminIngestEvents(
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
        // Permission first, so a caller without it learns nothing from the
        // body's validation.
        if (!endpointAuthHelper.RequirePermission(
                transaction, DbSchema::kPermissionManageEvents, resp)) {
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
        const Json::Value* itemsValue = nullptr;
        const Json::JsonArray* itemsArray = nullptr;
        if (!body.HasChild("items", &itemsValue)
            || (itemsArray = itemsValue->TryGetArray()) == nullptr) {
            resp = ErrorResponse::ValidationError("items must be an array", "items");
            return;
        }

        std::vector<Events::IngestItem> items(itemsArray->size());
        for (size_t index = 0; index < itemsArray->size(); ++index) {
            std::string problem = ItemReader((*itemsArray)[index]).Read(items[index]);
            if (!problem.empty()) {
                resp = ItemError(problem, index);
                return;
            }
        }

        Events::EventHelper helper(endpointAuthHelper.GetDatabaseHelper());
        Events::IngestResult ingest = helper.IngestEvents(transaction, items);
        if (!ingest.ok) {
            resp = ItemError(ingest.errorCode, static_cast<size_t>(ingest.errorItemIndex),
                             ingest.errorCode);
            return;
        }

        Json::JsonObject counts;
        for (std::string_view outcome : kOutcomes) {
            counts[std::string(outcome)] = Json::Value(static_cast<int64_t>(0));
        }
        Json::JsonArray itemResults;
        itemResults.reserve(ingest.items.size());
        for (const Events::IngestItemResult& item : ingest.items) {
            counts[item.outcome] = Json::Value(counts[item.outcome].Get<int64_t>() + 1);
            itemResults.push_back(Json::Value(Json::JsonObject{
                { "event_id", Json::Value(item.eventId) },
                { "outcome", Json::Value(item.outcome) },
                { "ignored_category_slugs", StringArray(item.ignoredCategorySlugs) },
                { "ignored_scene_tag_slugs", StringArray(item.ignoredSceneTagSlugs) },
                { "ignored_series_slug", Json::Value(item.ignoredSeriesSlug) },
            }));
        }
        result = Json::Value(Json::JsonObject{
            { "items", Json::Value(itemResults) },
            { "counts", Json::Value(counts) },
        });
        resp.code = 200;
    });

    return result;
}

}  // namespace Endpoints
