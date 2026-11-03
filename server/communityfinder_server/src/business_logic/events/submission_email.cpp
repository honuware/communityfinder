#include "business_logic/events/submission_email.h"

#include "business_logic/events/event_ical.h"
#include "db_schema/event_submissions.h"
#include "db_schema/events.h"
#include "util/date_time_util.h"
#include "util/types.h"

namespace Events {

namespace {

constexpr std::string_view kSubjectAllListed = "Your event is listed: {title}";
constexpr std::string_view kSubjectNoneListed = "Your event wasn't listed: {title}";
constexpr std::string_view kSubjectSomeListed = "Your event has been reviewed: {title}";

constexpr std::string_view kSummaryAllListed = "It's on the calendar now.";
constexpr std::string_view kSummaryNoneListed = "We weren't able to list it this time.";
constexpr std::string_view kSummarySomeListed = "Some of its dates are on the calendar now.";

constexpr std::string_view kBodyTemplate = R"(<p>Hi {first_name},</p>
<p>Thanks for sending us {title}. {summary}</p>
<ul>
{date_rows}</ul>
{reason}{message}<p>You can see everything you've sent us on <a href="{my_submissions_url}">My submissions</a>.</p>
<p>{site_name}</p>
)";

constexpr std::string_view kListedRowTemplate =
    "<li>{when}: Listed (<a href=\"{event_url}\">see it</a>)</li>\n";
constexpr std::string_view kNotListedRowTemplate = "<li>{when}: Not listed</li>\n";
constexpr std::string_view kReasonTemplate = "<p>Why: {reason}</p>\n";
constexpr std::string_view kMessageTemplate =
    "<p>A note from the curators:</p>\n<blockquote>{message}</blockquote>\n";

constexpr std::string_view kMySubmissionsPath = "account/submissions";
constexpr std::string_view kEventPathPrefix = "events/";

// The submitter typed the title, so it can hold anything; a header must be one line.
std::string OneLine(std::string_view text) {
    std::string line;
    line.reserve(text.size());
    for (char c : text) {
        line += (static_cast<unsigned char>(c) < 0x20 || c == 0x7F) ? ' ' : c;
    }
    return line;
}

std::string WithTrailingSlash(std::string_view url) {
    std::string base(url);
    if (base.empty() || base.back() != '/') {
        base += '/';
    }
    return base;
}

// Escaped, with line breaks kept as <br>.
std::string EscapeHtmlParagraph(std::string_view text) {
    std::string html;
    for (char c : EscapeHtml(text)) {
        if (c == '\r') {
            continue;
        }
        if (c == '\n') {
            html += "<br>\n";
        } else {
            html += c;
        }
    }
    return html;
}

std::string When(int64_t startsAtUs) {
    return DateTimeUtil::FormatDateFromMicroseconds(startsAtUs, kEventsTimeZone) + ", "
        + DateTimeUtil::FormatTimeFromMicroseconds(startsAtUs, kEventsTimeZone);
}

}  // namespace

std::string EscapeHtml(std::string_view text) {
    std::string escaped;
    escaped.reserve(text.size());
    for (char c : text) {
        switch (c) {
            case '&': escaped += "&amp;"; break;
            case '<': escaped += "&lt;"; break;
            case '>': escaped += "&gt;"; break;
            case '"': escaped += "&quot;"; break;
            case '\'': escaped += "&#39;"; break;
            // Braces too, so FormatString never reads a typed "{reason}" as one
            // of its placeholders.
            case '{': escaped += "&#123;"; break;
            case '}': escaped += "&#125;"; break;
            default: escaped += c;
        }
    }
    return escaped;
}

std::string DescribeRejectReason(std::string_view reason) {
    if (reason == DbSchema::kEventSubmissionsRejectReasonDuplicate) {
        return "It's already on the calendar.";
    }
    if (reason == DbSchema::kEventSubmissionsRejectReasonNotAFit) {
        return "It isn't a fit for this calendar.";
    }
    if (reason == DbSchema::kEventSubmissionsRejectReasonMissingDetails) {
        return "We needed more details to list it.";
    }
    if (reason == DbSchema::kEventSubmissionsRejectReasonPast) {
        return "The date had already passed.";
    }
    if (reason == DbSchema::kEventSubmissionsRejectReasonSpam) {
        return "It didn't meet our listing guidelines.";
    }
    if (reason == DbSchema::kEventSubmissionsRejectReasonExpired) {
        return "We didn't get to it before the date passed. Sorry about that.";
    }
    return {};
}

DecisionEmail RenderDecisionEmail(
    const DecisionNotice& notice, std::string_view siteName, std::string_view siteUrl) {
    const std::string baseUrl = WithTrailingSlash(siteUrl);
    const std::string title = notice.dates.empty() ? std::string() : notice.dates.front().title;

    size_t listed = 0;
    std::string dateRows;
    for (const DecisionDate& date : notice.dates) {
        if (date.status == DbSchema::kEventsStatusApproved) {
            ++listed;
            dateRows += FormatString(kListedRowTemplate, {
                { "when", When(date.startsAtUs) },
                { "event_url", EscapeHtml(baseUrl + std::string(kEventPathPrefix)
                                          + StringFromInt(date.eventId)) },
            });
        } else {
            dateRows += FormatString(kNotListedRowTemplate, { { "when", When(date.startsAtUs) } });
        }
    }
    const bool allListed = listed == notice.dates.size();
    const bool noneListed = listed == 0;

    std::string reason;
    const std::string reasonText = DescribeRejectReason(notice.rejectReason);
    if (!allListed && !reasonText.empty()) {
        reason = FormatString(kReasonTemplate, { { "reason", EscapeHtml(reasonText) } });
    }
    std::string message;
    if (!notice.messageToSubmitter.empty()) {
        message = FormatString(kMessageTemplate,
                               { { "message", EscapeHtmlParagraph(notice.messageToSubmitter) } });
    }

    DecisionEmail email;
    email.subject = FormatString(
        allListed ? kSubjectAllListed : noneListed ? kSubjectNoneListed : kSubjectSomeListed,
        { { "title", OneLine(title) } });
    email.bodyHtml = NormalizeCrLf(FormatString(kBodyTemplate, {
        { "first_name", EscapeHtml(notice.recipientFirstName) },
        { "title", EscapeHtml(title) },
        { "summary", std::string(allListed ? kSummaryAllListed
                                 : noneListed ? kSummaryNoneListed : kSummarySomeListed) },
        { "date_rows", dateRows },
        { "reason", reason },
        { "message", message },
        { "my_submissions_url", EscapeHtml(baseUrl + std::string(kMySubmissionsPath)) },
        { "site_name", EscapeHtml(siteName) },
    }));
    return email;
}

}  // namespace Events
