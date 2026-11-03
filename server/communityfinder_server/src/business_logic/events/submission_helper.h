#pragma once

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "business_logic/events/event_helper.h"
#include "business_logic/events/submission_email.h"
#include "business_logic/events/submission_settings.h"
#include "sql_util/database_access/database_helper.h"
#include "sql_util/table_helpers/categories.h"
#include "sql_util/table_helpers/event_category_assignments.h"
#include "sql_util/table_helpers/event_scene_tag_assignments.h"
#include "sql_util/table_helpers/event_submissions.h"
#include "sql_util/table_helpers/events.h"
#include "sql_util/table_helpers/people.h"
#include "sql_util/table_helpers/scene_tags.h"
#include "sql_util/table_helpers/venues.h"
#include "util/mail/mail_helper.h"
#include "util/secrets/secrets_helper.h"

namespace Events {

// EV3 §3.1 — the public submission pipeline: the submit form's door, "My
// submissions", withdrawal, and the curator's side of a submitted date
// (review, the reason and message, and the one decision email).
//
// Like EventHelper, every error path returns a descriptive errorCode string
// instead of throwing (knottyyoga's ClassScheduleHelper convention).

// Submit error codes. Two are shared with ingest: END_BEFORE_START
// (kEventErrorEndBeforeStart) and VENUE_NOT_FOUND (kEventErrorVenueNotFound).
inline constexpr std::string_view kSubmissionErrorEmailNotVerified = "EMAIL_NOT_VERIFIED";
// The curators have paused submissions (§3.4's switch on the settings page).
inline constexpr std::string_view kSubmissionErrorPaused = "SUBMISSIONS_PAUSED";
inline constexpr std::string_view kSubmissionErrorNotAgreed = "NOT_AGREED";
inline constexpr std::string_view kSubmissionErrorTitleLength = "TITLE_LENGTH";
inline constexpr std::string_view kSubmissionErrorNoDates = "NO_DATES";
inline constexpr std::string_view kSubmissionErrorTooManyDates = "TOO_MANY_DATES";
inline constexpr std::string_view kSubmissionErrorDuplicateDate = "DUPLICATE_DATE";
inline constexpr std::string_view kSubmissionErrorStartTooSoon = "START_TOO_SOON";
inline constexpr std::string_view kSubmissionErrorStartTooFar = "START_TOO_FAR";
// An end more than kSubmissionMaxDurationUs after its start.
inline constexpr std::string_view kSubmissionErrorTooLong = "TOO_LONG";
inline constexpr std::string_view kSubmissionErrorBadUrl = "BAD_URL";
inline constexpr std::string_view kSubmissionErrorFieldTooLong = "FIELD_TOO_LONG";
inline constexpr std::string_view kSubmissionErrorBadRelationship = "BAD_RELATIONSHIP";
inline constexpr std::string_view kSubmissionErrorTooManyTags = "TOO_MANY_TAGS";
inline constexpr std::string_view kSubmissionErrorRateLimited = "RATE_LIMITED";

// Withdraw and the curator's side.
// A submission that isn't the caller's — or doesn't exist, so another person's
// submission ids reveal nothing.
inline constexpr std::string_view kSubmissionErrorNotYours = "NOT_YOURS";
inline constexpr std::string_view kSubmissionErrorNothingPending = "NOTHING_PENDING";
inline constexpr std::string_view kSubmissionErrorNotFound = "SUBMISSION_NOT_FOUND";
// A reason outside the six a curator picks from, or a reason on an approval.
inline constexpr std::string_view kSubmissionErrorBadReason = "BAD_REASON";

// The limits the form is held to (EV3 "Validation"). Lengths count characters
// (UTF-8 code points), after trimming the single-line fields.
inline constexpr size_t kSubmissionTitleMinLength = 3;
inline constexpr size_t kSubmissionTitleMaxLength = 120;
inline constexpr size_t kSubmissionMaxDates = 12;
inline constexpr size_t kSubmissionDescriptionMaxLength = 4000;
inline constexpr size_t kSubmissionLocationMaxLength = 300;
inline constexpr size_t kSubmissionCostMaxLength = 100;
inline constexpr size_t kSubmissionUrlMaxLength = 500;
inline constexpr size_t kSubmissionNoteMaxLength = 1000;
inline constexpr size_t kSubmissionMessageMaxLength = 2000;
inline constexpr size_t kSubmissionMaxTagsPerKind = 3;
inline constexpr int64_t kSubmissionMinLeadUs = 3'600LL * 1'000'000LL;  // 1 hour
inline constexpr int64_t kSubmissionMaxLeadUs = 365 * kUsPerDay;
inline constexpr int64_t kSubmissionMaxDurationUs = 4 * kUsPerDay;
inline constexpr int64_t kSubmissionRateWindowUs = kUsPerDay;
// A date "might be" an existing event that starts within this much of it.
inline constexpr int64_t kDuplicateHintWindowUs = 3LL * 3'600LL * 1'000'000LL;

// The review note a withdrawn date carries; it is what tells "My submissions"
// a withdrawn date from one a curator turned down.
inline constexpr std::string_view kReviewNoteWithdrawnBySubmitter = "withdrawn by the submitter";

// A date's state as "My submissions" shows it.
inline constexpr std::string_view kSubmissionDateWaiting = "waiting";      // pending
inline constexpr std::string_view kSubmissionDateListed = "listed";        // approved
inline constexpr std::string_view kSubmissionDateNotListed = "not_listed"; // rejected by a curator
inline constexpr std::string_view kSubmissionDateWithdrawn = "withdrawn";  // rejected by Withdraw
inline constexpr std::string_view kSubmissionDatePast = "past";            // archived (EV4)

struct SubmissionDate {
    int64_t startsAtUs = 0;
    std::optional<int64_t> endsAtUs;
};

// The form, as the endpoint read it. Empty strings mean "not given".
struct SubmissionRequest {
    std::string title;
    std::vector<SubmissionDate> dates;
    std::optional<int64_t> venueId;
    std::string locationText;
    std::string description;
    std::string url;
    std::string costText;
    std::vector<std::string> categorySlugs;
    std::vector<std::string> sceneTagSlugs;
    bool regularThing = false;
    std::string relationship;  // empty = other
    std::string submitterNote;
    bool agreed = false;
    // The honeypot: the form hides it, so only a bot fills it in.
    std::string website;
};

// An existing event one of the new dates might be.
struct DuplicateCandidate {
    int64_t eventId = 0;
    std::string title;
    int64_t startsAtUs = 0;
    std::optional<int64_t> venueId;
};

struct DuplicateHint {
    size_t dateIndex = 0;  // which of the request's dates
    int64_t eventId = 0;
    std::string title;
    int64_t startsAtUs = 0;
};

struct SubmitResult {
    bool ok = false;
    std::string errorCode;
    // The form field the error belongs to, by its JSON name ("title",
    // "dates", "url", …); for RATE_LIMITED, which limit (kSubmissionLimit*).
    std::string errorField;
    // For a date's error, which date; -1 otherwise.
    int64_t errorDateIndex = -1;
    // Plain words for the submitter. For RATE_LIMITED it states the limit.
    std::string errorMessage;
    // 0 when the honeypot was filled in: the caller answers as if it worked.
    int64_t submissionId = 0;
    std::vector<int64_t> eventIds;  // parallel to the request's dates
    std::vector<std::string> ignoredCategorySlugs;
    std::vector<std::string> ignoredSceneTagSlugs;
    // Approved events only: a submitter never sees another person's pending one.
    std::vector<DuplicateHint> possibleDuplicates;
};

struct SubmissionDateView {
    int64_t eventId = 0;
    std::string title;
    int64_t startsAtUs = 0;
    std::optional<int64_t> endsAtUs;
    std::string state;  // kSubmissionDate*
};

// One submission as its submitter sees it. Deliberately has no curator note
// (review_notes): that stays with the curators.
struct SubmissionView {
    int64_t id = 0;
    int64_t createdUs = 0;
    std::string relationship;
    bool regularThing = false;
    std::string submitterNote;
    std::string rejectReason;
    std::string messageToSubmitter;
    std::vector<SubmissionDateView> dates;  // in start order
};

struct WithdrawResult {
    bool ok = false;
    std::string errorCode;
    std::vector<int64_t> withdrawnEventIds;
};

// What a curator adds when deciding a submitted date or a whole submission.
struct SubmissionDecision {
    // One of the six curator reasons, only with a rejection; empty = leave the
    // submission's reason as it is.
    std::string reason;
    // Absent = leave the submission's message as it is.
    std::optional<std::string> messageToSubmitter;
};

struct DateReviewResult {
    ReviewResult review;
    // When this decision finished the submission: the email to send.
    std::optional<DecisionNotice> notice;
};

struct SubmissionReviewResult {
    bool ok = false;
    std::string errorCode;
    std::vector<int64_t> decidedEventIds;
    std::optional<DecisionNotice> notice;
};

// Lower-cased ASCII letters and digits, with every other ASCII character
// dropped (punctuation and spaces alike, so "Drag-Brunch!" matches "drag
// brunch"); bytes of other scripts are kept as they are.
std::string NormalizeTitleForMatching(std::string_view title);

// The candidates that start within ±kDuplicateHintWindowUs of `startsAtUs`
// (inclusive) and share its venue or its normalized title (a title that
// normalizes to nothing matches nothing). In the candidates' order.
std::vector<DuplicateCandidate> FindDuplicateHints(
    std::string_view title,
    int64_t startsAtUs,
    std::optional<int64_t> venueId,
    const std::vector<DuplicateCandidate>& candidates);

class SubmissionHelper {
public:
    explicit SubmissionHelper(DatabaseHelper databaseHelper);
    SubmissionHelper(const SubmissionHelper&) = default;
    SubmissionHelper& operator=(const SubmissionHelper&) = default;
    ~SubmissionHelper() = default;

    // In two passes, like IngestEvents. The first checks the person (a
    // confirmed email), the pause switch, the honeypot, every field, and both
    // rate limits (the switch and limits are read through
    // LoadSubmissionSettings on every call, so a change on the settings page
    // applies to the next submission), and finds the duplicate hints. The
    // second writes the submission, one pending, user_submitted event per date
    // (created by the person, no source) and their tags. A filled-in honeypot
    // returns ok with nothing written.
    SubmitResult Submit(
        Transaction& transaction,
        Secrets::SecretsHelper& secrets,
        const SubmissionRequest& request,
        int64_t personId);

    // The person's submissions, newest first, each with its dates.
    std::vector<SubmissionView> GetMySubmissions(Transaction& transaction, int64_t personId);

    // Every pending date of the submission becomes rejected, with the
    // submitter as reviewer and kReviewNoteWithdrawnBySubmitter. Decided dates
    // stay as they are (an approved one stays listed). The reason becomes
    // `withdrawn` unless a curator already gave one, and the decision email
    // is marked sent: withdrawing sends none, now or later.
    WithdrawResult Withdraw(Transaction& transaction, int64_t submissionId, int64_t personId);

    // EventHelper::ReviewEvent, plus — for a submitted date — the decision's
    // reason and message stored on its submission, and the notice when this
    // was the submission's last pending date. The reason and message are
    // ignored for an event that came from elsewhere.
    DateReviewResult ReviewDate(
        Transaction& transaction,
        int64_t eventId,
        ReviewAction action,
        int64_t reviewerPersonId,
        std::string_view notes,
        const SubmissionDecision& decision);

    // Decides every pending date of a submission at once (Approve all /
    // Reject all); decided dates are left alone. SUBMISSION_NOT_FOUND,
    // NOTHING_PENDING, BAD_REASON, FIELD_TOO_LONG.
    SubmissionReviewResult ReviewSubmission(
        Transaction& transaction,
        int64_t submissionId,
        ReviewAction action,
        int64_t reviewerPersonId,
        std::string_view notes,
        const SubmissionDecision& decision);

    // When the event belongs to a submission that has no pending dates left
    // and no email sent yet, claims the email (MarkDecisionNotified) and
    // returns what it should say; otherwise nullopt. Only one caller ever wins
    // the claim. Also nullopt, but claimed, when the submitter's account is gone.
    std::optional<DecisionNotice> OnDateReviewed(Transaction& transaction, int64_t eventId);

    // Sends the notice's email from the site's sender. If it can't be sent
    // (a send error, or no mail helper), writes an admin_alerts row and clears
    // the claim, so the next decision on the submission (or EV4's digest)
    // tries again. Never throws; says whether it was sent.
    bool SendDecisionNotice(
        Transaction& transaction,
        Secrets::SecretsHelper& secrets,
        const ::Mail::MailHelperPtr& mailHelper,
        const DecisionNotice& notice);

private:
    DatabaseHelper databaseHelper_;
    EventHelper eventHelper_;
    TableHelpers::Categories categories_;
    TableHelpers::SceneTags sceneTags_;
    TableHelpers::Venues venues_;
    TableHelpers::Events events_;
    TableHelpers::EventSubmissions submissions_;
    TableHelpers::EventCategoryAssignments eventCategories_;
    TableHelpers::EventSceneTagAssignments eventSceneTags_;
    TableHelpers::People people_;

    int64_t NowUs(Transaction& transaction) const;

    // OnDateReviewed's work, once the submission is known (and locked).
    std::optional<DecisionNotice> ClaimDecisionNotice(Transaction& transaction, int64_t submissionId);

    // The decision's reason and message onto the submission (only what is stated).
    void RecordDecision(Transaction& transaction, int64_t submissionId,
                        const SubmissionDecision& decision);
};

// BAD_REASON / FIELD_TOO_LONG for a decision, or "" when it is acceptable.
std::string ValidateSubmissionDecision(ReviewAction action, const SubmissionDecision& decision);

}  // namespace Events
