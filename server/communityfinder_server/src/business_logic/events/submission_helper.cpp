#include "business_logic/events/submission_helper.h"

#include <algorithm>
#include <cctype>
#include <map>
#include <set>
#include <stdexcept>

#include "db_schema/categories.h"
#include "db_schema/event_submissions.h"
#include "db_schema/events.h"
#include "db_schema/people.h"
#include "db_schema/scene_tags.h"
#include "sql_util/table_helpers/admin_alerts.h"
#include "util/logging.h"
#include "util/mail/tenant_branding.h"
#include "util/types.h"

namespace Events {

namespace {

constexpr std::string_view kFieldAgreed = "agreed";
constexpr std::string_view kFieldTitle = "title";
constexpr std::string_view kFieldDates = "dates";
constexpr std::string_view kFieldVenueId = "venue_id";
constexpr std::string_view kFieldLocationText = "location_text";
constexpr std::string_view kFieldDescription = "description";
constexpr std::string_view kFieldUrl = "url";
constexpr std::string_view kFieldCostText = "cost_text";
constexpr std::string_view kFieldSubmitterNote = "submitter_note";
constexpr std::string_view kFieldRelationship = "relationship";
constexpr std::string_view kFieldCategorySlugs = "category_slugs";
constexpr std::string_view kFieldSceneTagSlugs = "scene_tag_slugs";

constexpr std::string_view kPendingLimitMessage =
    "You can have up to {limit} waiting for review at a time. You have {pending} waiting, "
    "and this would add {adding}. We'll get to them soon.";
constexpr std::string_view kDailyLimitMessage =
    "You can send up to {limit} in 24 hours. Please try again later.";

constexpr std::string_view kMailFailedAlert =
    "Event submission #{submission_id}: the decision email to its submitter could not be "
    "sent ({error}). The next decision on it will try again.";

std::string Column(const KeyValueTable& row, std::string_view column) {
    auto it = row.find(std::string(column));
    return it == row.end() ? std::string() : it->second;
}

// NULL reads back as "", which is "no value" here.
std::optional<int64_t> OptionalIdColumn(const KeyValueTable& row, std::string_view column) {
    std::string value = Column(row, column);
    if (value.empty()) {
        return std::nullopt;
    }
    return std::stoll(value);
}

// "1 date" / "3 dates".
std::string Count(int64_t count, std::string_view singular, std::string_view plural) {
    return StringFromInt(count) + " " + std::string(count == 1 ? singular : plural);
}

size_t CharacterCount(std::string_view text) {
    return static_cast<size_t>(std::count_if(text.begin(), text.end(), [](char c) {
        return (static_cast<unsigned char>(c) & 0xC0) != 0x80;
    }));
}

bool IsAsciiSpace(char c) {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v';
}

// A one-line field: control characters become spaces (a title ends up in an
// email subject), then the ends are trimmed.
std::string SingleLine(std::string_view text) {
    std::string line;
    line.reserve(text.size());
    for (char c : text) {
        line += (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) ? ' ' : c;
    }
    size_t first = 0;
    while (first < line.size() && IsAsciiSpace(line[first])) ++first;
    size_t last = line.size();
    while (last > first && IsAsciiSpace(line[last - 1])) --last;
    return line.substr(first, last - first);
}

bool StartsWithNoCase(std::string_view text, std::string_view prefix) {
    if (text.size() < prefix.size()) {
        return false;
    }
    for (size_t i = 0; i < prefix.size(); ++i) {
        if (std::tolower(static_cast<unsigned char>(text[i])) != prefix[i]) {
            return false;
        }
    }
    return true;
}

// http or https, something after the scheme, and no spaces.
bool IsWebUrl(std::string_view url) {
    size_t schemeLength = StartsWithNoCase(url, "https://") ? 8
        : StartsWithNoCase(url, "http://") ? 7 : 0;
    if (schemeLength == 0 || url.size() == schemeLength) {
        return false;
    }
    return std::none_of(url.begin(), url.end(), [](char c) { return IsAsciiSpace(c); });
}

bool IsRelationship(std::string_view value) {
    for (std::string_view known : { DbSchema::kEventSubmissionsRelationshipOrganizer,
                                    DbSchema::kEventSubmissionsRelationshipVenue,
                                    DbSchema::kEventSubmissionsRelationshipPerformer,
                                    DbSchema::kEventSubmissionsRelationshipAttendee,
                                    DbSchema::kEventSubmissionsRelationshipOther }) {
        if (value == known) return true;
    }
    return false;
}

bool IsCuratorReason(std::string_view value) {
    for (std::string_view known : { DbSchema::kEventSubmissionsRejectReasonDuplicate,
                                    DbSchema::kEventSubmissionsRejectReasonNotAFit,
                                    DbSchema::kEventSubmissionsRejectReasonMissingDetails,
                                    DbSchema::kEventSubmissionsRejectReasonPast,
                                    DbSchema::kEventSubmissionsRejectReasonSpam,
                                    DbSchema::kEventSubmissionsRejectReasonOther }) {
        if (value == known) return true;
    }
    return false;
}

// slug → id over a vocabulary's GetAll, which is enabled rows only — so a
// disabled slug resolves to nothing, exactly like an unknown one.
std::map<std::string, int64_t> SlugIds(const KeyValueTableArray& rows, std::string_view idColumn,
                                       std::string_view slugColumn) {
    std::map<std::string, int64_t> ids;
    for (const KeyValueTable& row : rows) {
        ids[Column(row, slugColumn)] = std::stoll(Column(row, idColumn));
    }
    return ids;
}

// The request's slugs for one kind of tag, resolved: known ones in the order
// given (repeats once), unknown or disabled ones reported.
struct ResolvedTags {
    std::vector<int64_t> ids;
    std::vector<std::string> ignoredSlugs;
};

ResolvedTags ResolveTags(const std::vector<std::string>& slugs,
                         const std::map<std::string, int64_t>& known) {
    ResolvedTags resolved;
    std::set<int64_t> seen;
    for (const std::string& slug : slugs) {
        auto it = known.find(slug);
        if (it == known.end()) {
            resolved.ignoredSlugs.push_back(slug);
        } else if (seen.insert(it->second).second) {
            resolved.ids.push_back(it->second);
        }
    }
    return resolved;
}

std::string DateState(const KeyValueTable& event) {
    const std::string status = Column(event, DbSchema::kEventsStatus);
    if (status == DbSchema::kEventsStatusPending) return std::string(kSubmissionDateWaiting);
    if (status == DbSchema::kEventsStatusApproved) return std::string(kSubmissionDateListed);
    if (status == DbSchema::kEventsStatusArchived) return std::string(kSubmissionDatePast);
    if (Column(event, DbSchema::kEventsReviewNotes) == kReviewNoteWithdrawnBySubmitter) {
        return std::string(kSubmissionDateWithdrawn);
    }
    return std::string(kSubmissionDateNotListed);
}

std::vector<KeyValueTable> PendingOf(const KeyValueTableArray& events) {
    std::vector<KeyValueTable> pending;
    for (const KeyValueTable& event : events) {
        if (Column(event, DbSchema::kEventsStatus) == DbSchema::kEventsStatusPending) {
            pending.push_back(event);
        }
    }
    return pending;
}

}  // namespace

std::string NormalizeTitleForMatching(std::string_view title) {
    std::string normalized;
    normalized.reserve(title.size());
    for (char c : title) {
        unsigned char byte = static_cast<unsigned char>(c);
        if (byte >= 0x80) {
            normalized += c;
        } else if (std::isalnum(byte)) {
            normalized += static_cast<char>(std::tolower(byte));
        }
    }
    return normalized;
}

std::vector<DuplicateCandidate> FindDuplicateHints(
    std::string_view title,
    int64_t startsAtUs,
    std::optional<int64_t> venueId,
    const std::vector<DuplicateCandidate>& candidates) {
    const std::string normalizedTitle = NormalizeTitleForMatching(title);
    std::vector<DuplicateCandidate> matches;
    for (const DuplicateCandidate& candidate : candidates) {
        const int64_t apartUs = candidate.startsAtUs > startsAtUs
            ? candidate.startsAtUs - startsAtUs : startsAtUs - candidate.startsAtUs;
        if (apartUs > kDuplicateHintWindowUs) {
            continue;
        }
        const bool sameVenue = venueId && candidate.venueId == venueId;
        const bool sameTitle = !normalizedTitle.empty()
            && NormalizeTitleForMatching(candidate.title) == normalizedTitle;
        if (sameVenue || sameTitle) {
            matches.push_back(candidate);
        }
    }
    return matches;
}

std::string ValidateSubmissionDecision(ReviewAction action, const SubmissionDecision& decision) {
    if (!decision.reason.empty()
        && (action != ReviewAction::Reject || !IsCuratorReason(decision.reason))) {
        return std::string(kSubmissionErrorBadReason);
    }
    if (decision.messageToSubmitter
        && CharacterCount(*decision.messageToSubmitter) > kSubmissionMessageMaxLength) {
        return std::string(kSubmissionErrorFieldTooLong);
    }
    return {};
}

SubmissionHelper::SubmissionHelper(DatabaseHelper databaseHelper)
    : databaseHelper_(databaseHelper),
      eventHelper_(databaseHelper),
      categories_(databaseHelper),
      sceneTags_(databaseHelper),
      venues_(databaseHelper),
      events_(databaseHelper),
      submissions_(databaseHelper),
      eventCategories_(databaseHelper),
      eventSceneTags_(databaseHelper),
      people_(databaseHelper) {
}

int64_t SubmissionHelper::NowUs(Transaction& transaction) const {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

SubmitResult SubmissionHelper::Submit(
    Transaction& transaction,
    Secrets::SecretsHelper& secrets,
    const SubmissionRequest& request,
    int64_t personId) {
    SubmitResult result;
    auto fail = [&](std::string_view errorCode, std::string_view field, std::string message,
                    int64_t dateIndex = -1) {
        result.errorCode = std::string(errorCode);
        result.errorField = std::string(field);
        result.errorMessage = std::move(message);
        result.errorDateIndex = dateIndex;
        return result;
    };

    // Pass 1 — check everything before writing anything.
    KeyValueTable person = people_.GetPersonById(transaction, personId);
    if (person.empty() || Column(person, DbSchema::kPeopleEmailVerifiedAt).empty()) {
        return fail(kSubmissionErrorEmailNotVerified, "",
                    "Confirm your email address first.");
    }
    const SubmissionSettings settings = LoadSubmissionSettings(secrets, transaction);
    if (settings.submissionsPaused) {
        return fail(kSubmissionErrorPaused, "",
                    "We're not taking event submissions right now. Please check back soon.");
    }
    if (!request.website.empty()) {
        // Only a bot fills in the hidden field. It gets an answer that looks
        // like success, and nothing is written.
        result.ok = true;
        return result;
    }
    if (!request.agreed) {
        return fail(kSubmissionErrorNotAgreed, kFieldAgreed,
                    "Please confirm the details are accurate and can be listed publicly.");
    }

    const std::string title = SingleLine(request.title);
    const size_t titleLength = CharacterCount(title);
    if (titleLength < kSubmissionTitleMinLength || titleLength > kSubmissionTitleMaxLength) {
        return fail(kSubmissionErrorTitleLength, kFieldTitle,
                    "The title must be 3 to 120 characters.");
    }

    if (request.dates.empty()) {
        return fail(kSubmissionErrorNoDates, kFieldDates, "Add at least one date.");
    }
    if (request.dates.size() > kSubmissionMaxDates) {
        return fail(kSubmissionErrorTooManyDates, kFieldDates, "A submission can have up to 12 dates.");
    }
    const int64_t nowUs = NowUs(transaction);
    std::set<int64_t> starts;
    for (size_t index = 0; index < request.dates.size(); ++index) {
        const SubmissionDate& date = request.dates[index];
        const int64_t dateIndex = static_cast<int64_t>(index);
        if (date.startsAtUs < nowUs + kSubmissionMinLeadUs) {
            return fail(kSubmissionErrorStartTooSoon, kFieldDates,
                        "Each date must start at least an hour from now.", dateIndex);
        }
        if (date.startsAtUs > nowUs + kSubmissionMaxLeadUs) {
            return fail(kSubmissionErrorStartTooFar, kFieldDates,
                        "Each date must be within the next year.", dateIndex);
        }
        if (date.endsAtUs && *date.endsAtUs <= date.startsAtUs) {
            return fail(kEventErrorEndBeforeStart, kFieldDates,
                        "The end must come after the start.", dateIndex);
        }
        if (date.endsAtUs && *date.endsAtUs - date.startsAtUs > kSubmissionMaxDurationUs) {
            return fail(kSubmissionErrorTooLong, kFieldDates,
                        "A date can last at most 4 days.", dateIndex);
        }
        if (!starts.insert(date.startsAtUs).second) {
            return fail(kSubmissionErrorDuplicateDate, kFieldDates,
                        "This date is listed twice.", dateIndex);
        }
    }

    if (request.venueId && (*request.venueId <= 0
                            || venues_.GetVenue(transaction, *request.venueId).empty())) {
        return fail(kEventErrorVenueNotFound, kFieldVenueId, "That venue isn't in our list.");
    }

    const std::string locationText = SingleLine(request.locationText);
    const std::string costText = SingleLine(request.costText);
    const std::string url = SingleLine(request.url);
    const struct {
        std::string_view field;
        const std::string& value;
        size_t maxLength;
    } lengths[] = {
        { kFieldLocationText, locationText, kSubmissionLocationMaxLength },
        { kFieldDescription, request.description, kSubmissionDescriptionMaxLength },
        { kFieldCostText, costText, kSubmissionCostMaxLength },
        { kFieldUrl, url, kSubmissionUrlMaxLength },
        { kFieldSubmitterNote, request.submitterNote, kSubmissionNoteMaxLength },
    };
    for (const auto& length : lengths) {
        if (CharacterCount(length.value) > length.maxLength) {
            return fail(kSubmissionErrorFieldTooLong, length.field,
                        "This can be at most " + StringFromInt(static_cast<int64_t>(length.maxLength))
                            + " characters.");
        }
    }
    if (!url.empty() && !IsWebUrl(url)) {
        return fail(kSubmissionErrorBadUrl, kFieldUrl, "Links must start with http:// or https://.");
    }

    const std::string relationship = request.relationship.empty()
        ? std::string(DbSchema::kEventSubmissionsRelationshipOther) : request.relationship;
    if (!IsRelationship(relationship)) {
        return fail(kSubmissionErrorBadRelationship, kFieldRelationship,
                    "Pick how you know about the event.");
    }

    const ResolvedTags categories = ResolveTags(
        request.categorySlugs,
        SlugIds(categories_.GetAll(transaction), DbSchema::kCategoriesId, DbSchema::kCategoriesSlug));
    const ResolvedTags sceneTags = ResolveTags(
        request.sceneTagSlugs,
        SlugIds(sceneTags_.GetAll(transaction), DbSchema::kSceneTagsId, DbSchema::kSceneTagsSlug));
    if (categories.ids.size() > kSubmissionMaxTagsPerKind) {
        return fail(kSubmissionErrorTooManyTags, kFieldCategorySlugs, "Pick up to 3 categories.");
    }
    if (sceneTags.ids.size() > kSubmissionMaxTagsPerKind) {
        return fail(kSubmissionErrorTooManyTags, kFieldSceneTagSlugs, "Pick up to 3 scenes.");
    }

    // The rate limits last, so a form with a mistake hears about the mistake.
    if (submissions_.CountSubmissionsSince(transaction, personId, nowUs - kSubmissionRateWindowUs)
        >= settings.maxSubmissionsPerDay) {
        return fail(kSubmissionErrorRateLimited, kSubmissionLimitMaxSubmissionsPerDay,
                    FormatString(kDailyLimitMessage, {
                        { "limit", Count(settings.maxSubmissionsPerDay, "submission", "submissions") },
                    }));
    }
    const int64_t pending = events_.CountPendingSubmittedBy(transaction, personId);
    const int64_t adding = static_cast<int64_t>(request.dates.size());
    if (pending + adding > settings.maxPendingDates) {
        return fail(kSubmissionErrorRateLimited, kSubmissionLimitMaxPendingDates,
                    FormatString(kPendingLimitMessage, {
                        { "limit", Count(settings.maxPendingDates, "date", "dates") },
                        { "pending", StringFromInt(pending) },
                        { "adding", StringFromInt(adding) },
                    }));
    }

    // Approved events only: these go back to the submitter, and another
    // person's pending submission is not theirs to see.
    for (size_t index = 0; index < request.dates.size(); ++index) {
        const int64_t startsAtUs = request.dates[index].startsAtUs;
        std::vector<DuplicateCandidate> candidates;
        for (const KeyValueTable& row : events_.GetEventsStartingBetween(
                 transaction, startsAtUs - kDuplicateHintWindowUs,
                 startsAtUs + kDuplicateHintWindowUs + 1, { DbSchema::kEventsStatusApproved })) {
            candidates.push_back({ std::stoll(Column(row, DbSchema::kEventsId)),
                                   Column(row, DbSchema::kEventsTitle),
                                   std::stoll(Column(row, DbSchema::kEventsStartsAtUs)),
                                   OptionalIdColumn(row, DbSchema::kEventsVenueId) });
        }
        for (const DuplicateCandidate& match :
             FindDuplicateHints(title, startsAtUs, request.venueId, candidates)) {
            result.possibleDuplicates.push_back({ index, match.eventId, match.title, match.startsAtUs });
        }
    }

    // Pass 2 — write.
    KeyValueTable submission = {
        { std::string(DbSchema::kEventSubmissionsSubmittedByPersonId), StringFromInt(personId) },
        { std::string(DbSchema::kEventSubmissionsRelationship), relationship },
        { std::string(DbSchema::kEventSubmissionsRegularThing), request.regularThing ? "true" : "false" },
    };
    if (!request.submitterNote.empty()) {
        submission[std::string(DbSchema::kEventSubmissionsSubmitterNote)] = request.submitterNote;
    }
    result.submissionId = submissions_.AddSubmission(transaction, submission);

    for (const SubmissionDate& date : request.dates) {
        KeyValueTable values = {
            { std::string(DbSchema::kEventsTitle), title },
            { std::string(DbSchema::kEventsStartsAtUs), StringFromInt(date.startsAtUs) },
            { std::string(DbSchema::kEventsOrigin), std::string(DbSchema::kEventsOriginUserSubmitted) },
            { std::string(DbSchema::kEventsStatus), std::string(DbSchema::kEventsStatusPending) },
            { std::string(DbSchema::kEventsCreatedByPersonId), StringFromInt(personId) },
            { std::string(DbSchema::kEventsSubmissionId), StringFromInt(result.submissionId) },
        };
        if (date.endsAtUs) values[std::string(DbSchema::kEventsEndsAtUs)] = StringFromInt(*date.endsAtUs);
        if (request.venueId) values[std::string(DbSchema::kEventsVenueId)] = StringFromInt(*request.venueId);
        if (!locationText.empty()) values[std::string(DbSchema::kEventsLocationText)] = locationText;
        if (!request.description.empty()) {
            values[std::string(DbSchema::kEventsDescription)] = request.description;
        }
        if (!url.empty()) values[std::string(DbSchema::kEventsUrl)] = url;
        if (!costText.empty()) values[std::string(DbSchema::kEventsCostText)] = costText;

        const int64_t eventId = events_.AddEvent(transaction, values);
        for (int64_t categoryId : categories.ids) {
            eventCategories_.Assign(transaction, eventId, categoryId);
        }
        for (int64_t sceneTagId : sceneTags.ids) {
            eventSceneTags_.Assign(transaction, eventId, sceneTagId);
        }
        result.eventIds.push_back(eventId);
    }
    result.ignoredCategorySlugs = categories.ignoredSlugs;
    result.ignoredSceneTagSlugs = sceneTags.ignoredSlugs;
    result.ok = true;
    return result;
}

std::vector<SubmissionView> SubmissionHelper::GetMySubmissions(
    Transaction& transaction, int64_t personId) {
    const KeyValueTableArray rows = submissions_.GetSubmissionsForPerson(transaction, personId);
    std::vector<int64_t> ids;
    for (const KeyValueTable& row : rows) {
        ids.push_back(std::stoll(Column(row, DbSchema::kEventSubmissionsId)));
    }
    // One batch read; it comes back ordered by submission, then start.
    std::map<int64_t, std::vector<SubmissionDateView>> datesBySubmission;
    for (const KeyValueTable& event : events_.GetEventsBySubmissions(transaction, ids)) {
        SubmissionDateView date;
        date.eventId = std::stoll(Column(event, DbSchema::kEventsId));
        date.title = Column(event, DbSchema::kEventsTitle);
        date.startsAtUs = std::stoll(Column(event, DbSchema::kEventsStartsAtUs));
        date.endsAtUs = OptionalIdColumn(event, DbSchema::kEventsEndsAtUs);
        date.state = DateState(event);
        datesBySubmission[std::stoll(Column(event, DbSchema::kEventsSubmissionId))].push_back(
            std::move(date));
    }

    std::vector<SubmissionView> views;
    views.reserve(rows.size());
    for (const KeyValueTable& row : rows) {
        SubmissionView view;
        view.id = std::stoll(Column(row, DbSchema::kEventSubmissionsId));
        view.createdUs = std::stoll(Column(row, DbSchema::kEventSubmissionsCreatedUs));
        view.relationship = Column(row, DbSchema::kEventSubmissionsRelationship);
        view.regularThing = Column(row, DbSchema::kEventSubmissionsRegularThing) == "t";
        view.submitterNote = Column(row, DbSchema::kEventSubmissionsSubmitterNote);
        view.rejectReason = Column(row, DbSchema::kEventSubmissionsRejectReason);
        view.messageToSubmitter = Column(row, DbSchema::kEventSubmissionsMessageToSubmitter);
        auto datesIt = datesBySubmission.find(view.id);
        if (datesIt != datesBySubmission.end()) view.dates = std::move(datesIt->second);
        views.push_back(std::move(view));
    }
    return views;
}

WithdrawResult SubmissionHelper::Withdraw(
    Transaction& transaction, int64_t submissionId, int64_t personId) {
    WithdrawResult result;
    // Locked first, as every path that decides a submission's dates does.
    KeyValueTable submission = submissions_.GetSubmissionForUpdate(transaction, submissionId);
    if (submission.empty()
        || OptionalIdColumn(submission, DbSchema::kEventSubmissionsSubmittedByPersonId) != personId) {
        result.errorCode = std::string(kSubmissionErrorNotYours);
        return result;
    }
    const std::vector<KeyValueTable> pending =
        PendingOf(events_.GetEventsBySubmission(transaction, submissionId));
    if (pending.empty()) {
        result.errorCode = std::string(kSubmissionErrorNothingPending);
        return result;
    }

    for (const KeyValueTable& event : pending) {
        const int64_t eventId = std::stoll(Column(event, DbSchema::kEventsId));
        ReviewResult review = eventHelper_.ReviewEvent(
            transaction, eventId, ReviewAction::Reject, personId, kReviewNoteWithdrawnBySubmitter);
        if (!review.ok) {
            // pending → rejected is always allowed, so this is a bug, not a user error.
            throw std::runtime_error("SubmissionHelper::Withdraw - " + review.errorCode);
        }
        result.withdrawnEventIds.push_back(eventId);
    }
    if (Column(submission, DbSchema::kEventSubmissionsRejectReason).empty()) {
        submissions_.UpdateSubmission(transaction, submissionId, {
            { std::string(DbSchema::kEventSubmissionsRejectReason),
              std::string(DbSchema::kEventSubmissionsRejectReasonWithdrawn) },
        });
    }
    // No email for a withdrawal, now or from a later path.
    submissions_.MarkDecisionNotified(transaction, submissionId, NowUs(transaction));
    result.ok = true;
    return result;
}

DateReviewResult SubmissionHelper::ReviewDate(
    Transaction& transaction,
    int64_t eventId,
    ReviewAction action,
    int64_t reviewerPersonId,
    std::string_view notes,
    const SubmissionDecision& decision) {
    DateReviewResult result;
    std::string problem = ValidateSubmissionDecision(action, decision);
    if (!problem.empty()) {
        result.review.errorCode = std::move(problem);
        return result;
    }

    // The submission's lock before the event's (see GetSubmissionForUpdate).
    const std::optional<int64_t> submissionId = OptionalIdColumn(
        events_.GetEvent(transaction, eventId), DbSchema::kEventsSubmissionId);
    if (submissionId) {
        submissions_.GetSubmissionForUpdate(transaction, *submissionId);
    }
    result.review = eventHelper_.ReviewEvent(transaction, eventId, action, reviewerPersonId, notes);
    if (!result.review.ok || !submissionId) {
        return result;
    }
    RecordDecision(transaction, *submissionId, decision);
    result.notice = ClaimDecisionNotice(transaction, *submissionId);
    return result;
}

SubmissionReviewResult SubmissionHelper::ReviewSubmission(
    Transaction& transaction,
    int64_t submissionId,
    ReviewAction action,
    int64_t reviewerPersonId,
    std::string_view notes,
    const SubmissionDecision& decision) {
    SubmissionReviewResult result;
    result.errorCode = ValidateSubmissionDecision(action, decision);
    if (!result.errorCode.empty()) {
        return result;
    }
    if (submissions_.GetSubmissionForUpdate(transaction, submissionId).empty()) {
        result.errorCode = std::string(kSubmissionErrorNotFound);
        return result;
    }
    const std::vector<KeyValueTable> pending =
        PendingOf(events_.GetEventsBySubmission(transaction, submissionId));
    if (pending.empty()) {
        result.errorCode = std::string(kSubmissionErrorNothingPending);
        return result;
    }

    for (const KeyValueTable& event : pending) {
        const int64_t eventId = std::stoll(Column(event, DbSchema::kEventsId));
        ReviewResult review = eventHelper_.ReviewEvent(
            transaction, eventId, action, reviewerPersonId, notes);
        if (!review.ok) {
            throw std::runtime_error("SubmissionHelper::ReviewSubmission - " + review.errorCode);
        }
        result.decidedEventIds.push_back(eventId);
    }
    RecordDecision(transaction, submissionId, decision);
    result.notice = ClaimDecisionNotice(transaction, submissionId);
    result.ok = true;
    return result;
}

std::optional<DecisionNotice> SubmissionHelper::OnDateReviewed(
    Transaction& transaction, int64_t eventId) {
    const std::optional<int64_t> submissionId = OptionalIdColumn(
        events_.GetEvent(transaction, eventId), DbSchema::kEventsSubmissionId);
    if (!submissionId) {
        return std::nullopt;
    }
    submissions_.GetSubmissionForUpdate(transaction, *submissionId);
    return ClaimDecisionNotice(transaction, *submissionId);
}

std::optional<DecisionNotice> SubmissionHelper::ClaimDecisionNotice(
    Transaction& transaction, int64_t submissionId) {
    const KeyValueTable submission = submissions_.GetSubmission(transaction, submissionId);
    if (submission.empty()
        || !Column(submission, DbSchema::kEventSubmissionsDecisionNotifiedUs).empty()) {
        return std::nullopt;
    }
    const KeyValueTableArray events = events_.GetEventsBySubmission(transaction, submissionId);
    if (events.empty() || !PendingOf(events).empty()) {
        return std::nullopt;
    }
    if (!submissions_.MarkDecisionNotified(transaction, submissionId, NowUs(transaction))) {
        return std::nullopt;
    }
    const KeyValueTable person = people_.GetPersonById(
        transaction, std::stoll(Column(submission, DbSchema::kEventSubmissionsSubmittedByPersonId)));
    if (person.empty()) {
        // The account is gone, so there is no one to tell. The claim stays.
        return std::nullopt;
    }

    DecisionNotice notice;
    notice.submissionId = submissionId;
    notice.recipientFirstName = Column(person, DbSchema::kPeopleFirstName);
    notice.recipientLastName = Column(person, DbSchema::kPeopleLastName);
    notice.recipientEmail = Column(person, DbSchema::kPeopleEmail);
    notice.rejectReason = Column(submission, DbSchema::kEventSubmissionsRejectReason);
    notice.messageToSubmitter = Column(submission, DbSchema::kEventSubmissionsMessageToSubmitter);
    for (const KeyValueTable& event : events) {
        notice.dates.push_back({ std::stoll(Column(event, DbSchema::kEventsId)),
                                 Column(event, DbSchema::kEventsTitle),
                                 std::stoll(Column(event, DbSchema::kEventsStartsAtUs)),
                                 Column(event, DbSchema::kEventsStatus) });
    }
    return notice;
}

void SubmissionHelper::RecordDecision(
    Transaction& transaction, int64_t submissionId, const SubmissionDecision& decision) {
    KeyValueTable updates;
    if (!decision.reason.empty()) {
        updates[std::string(DbSchema::kEventSubmissionsRejectReason)] = decision.reason;
    }
    if (decision.messageToSubmitter) {
        updates[std::string(DbSchema::kEventSubmissionsMessageToSubmitter)] =
            *decision.messageToSubmitter;
    }
    if (!updates.empty()) {
        submissions_.UpdateSubmission(transaction, submissionId, updates);
    }
}

bool SubmissionHelper::SendDecisionNotice(
    Transaction& transaction,
    Secrets::SecretsHelper& secrets,
    const ::Mail::MailHelperPtr& mailHelper,
    const DecisionNotice& notice) {
    std::string error;
    try {
        if (!mailHelper) {
            throw std::runtime_error("no mail helper");
        }
        const ::Mail::TenantBranding branding = ::Mail::LoadTenantBranding(secrets, transaction);
        const DecisionEmail email =
            RenderDecisionEmail(notice, branding.studioName, branding.websiteUrl);
        std::string recipientName = notice.recipientFirstName;
        if (!notice.recipientLastName.empty()) {
            recipientName += " " + notice.recipientLastName;
        }
        ::Mail::MailMessage message(::Mail::LoadSenderAddress(secrets, transaction),
                                    ::Mail::MailAddress{ recipientName, notice.recipientEmail });
        message.SetSubject(email.subject);
        message.SetBodyHtml(email.bodyHtml);
        mailHelper->SendMail(message);
        return true;
    }
    catch (const std::exception& e) {
        error = e.what();
    }
    catch (...) {
        error = "unknown error";
    }

    // The review stands; the email waits for the next decision or EV4's digest.
    LogError() << "[event_submissions] event=decision_mail_failed submission_id="
               << notice.submissionId << " error=\"" << error << "\"\n";
    TableHelpers::AdminAlerts(databaseHelper_).AddAdminAlert(transaction, FormatString(
        kMailFailedAlert, {
            { "submission_id", StringFromInt(notice.submissionId) },
            { "error", error },
        }));
    submissions_.UpdateSubmission(transaction, notice.submissionId, {
        { std::string(DbSchema::kEventSubmissionsDecisionNotifiedUs), "NULL" },
    });
    return false;
}

}  // namespace Events
