#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace Events {

// EV3 §3.2 — the one email a submitter gets when their submission is fully
// decided. SubmissionHelper builds the notice (and claims the right to send
// it); this file only turns it into words, so it is tested without a database.

// One date of the submission and how it was decided.
struct DecisionDate {
    int64_t eventId = 0;
    std::string title;
    int64_t startsAtUs = 0;
    std::string status;  // the event's: approved, rejected (or archived, for EV4)
};

struct DecisionNotice {
    int64_t submissionId = 0;
    std::string recipientFirstName;
    std::string recipientLastName;
    std::string recipientEmail;
    std::vector<DecisionDate> dates;  // in start order
    // The submission's reason (DbSchema::kEventSubmissionsRejectReason*), when
    // a date was not listed; empty otherwise.
    std::string rejectReason;
    std::string messageToSubmitter;
};

struct DecisionEmail {
    std::string subject;
    std::string bodyHtml;  // already NormalizeCrLf'd
};

// `siteUrl` is the SPA's base ("https://example.org/"; a missing trailing
// slash is added). Everything the submitter or a curator typed is
// HTML-escaped, and the subject is kept to one line.
DecisionEmail RenderDecisionEmail(
    const DecisionNotice& notice, std::string_view siteName, std::string_view siteUrl);

// The plain-words reason shown to the submitter; empty for `other` (the
// curator's message says it) and for unknown values.
std::string DescribeRejectReason(std::string_view reason);

// <, >, &, " and ' as entities, for text placed in HTML.
std::string EscapeHtml(std::string_view text);

}  // namespace Events
