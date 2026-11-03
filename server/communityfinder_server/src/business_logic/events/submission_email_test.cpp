#include "business_logic/events/submission_email.h"

#include <string>

#include <gtest/gtest.h>

#include "db_schema/event_submissions.h"
#include "db_schema/events.h"

// EV3 §3.2 — the decision email, rendered without a database.
namespace Events {
namespace {

// 2026-11-14 03:00 UTC = Friday, November 13, 2026, 7:00 PM in Seattle (PST).
constexpr int64_t kFridayEveningUs = 1'794'625'200LL * 1'000'000LL;
constexpr int64_t kUsPerWeek = 7LL * 86'400LL * 1'000'000LL;

DecisionNotice MakeNotice(std::initializer_list<std::string_view> statuses) {
    DecisionNotice notice;
    notice.submissionId = 12;
    notice.recipientFirstName = "Sam";
    notice.recipientLastName = "Sample";
    notice.recipientEmail = "submitter@example.com";
    int64_t startsAtUs = kFridayEveningUs;
    int64_t eventId = 100;
    for (std::string_view status : statuses) {
        notice.dates.push_back({ eventId++, "Sample Dance Night", startsAtUs, std::string(status) });
        startsAtUs += kUsPerWeek;
    }
    return notice;
}

bool Contains(const std::string& text, std::string_view part) {
    return text.find(part) != std::string::npos;
}

TEST(SubmissionEmailTest, AllListed) {
    DecisionEmail email = RenderDecisionEmail(
        MakeNotice({ DbSchema::kEventsStatusApproved, DbSchema::kEventsStatusApproved }),
        "Sample Site", "https://example.org/");
    EXPECT_EQ(email.subject, "Your event is listed: Sample Dance Night");
    EXPECT_TRUE(Contains(email.bodyHtml, "Hi Sam,"));
    EXPECT_TRUE(Contains(email.bodyHtml, "It's on the calendar now."));
    EXPECT_TRUE(Contains(email.bodyHtml,
        "<li>November 13, 2026, 7:00 PM: Listed (<a href=\"https://example.org/events/100\">see it</a>)</li>"));
    EXPECT_TRUE(Contains(email.bodyHtml, "https://example.org/events/101"));
    EXPECT_FALSE(Contains(email.bodyHtml, "Not listed"));
    EXPECT_FALSE(Contains(email.bodyHtml, "Why:"));
    EXPECT_TRUE(Contains(email.bodyHtml, "https://example.org/account/submissions"));
    EXPECT_TRUE(Contains(email.bodyHtml, "<p>Sample Site</p>"));
}

TEST(SubmissionEmailTest, NoneListedSaysWhy) {
    DecisionNotice notice = MakeNotice({ DbSchema::kEventsStatusRejected });
    notice.rejectReason = std::string(DbSchema::kEventSubmissionsRejectReasonMissingDetails);
    DecisionEmail email = RenderDecisionEmail(notice, "Sample Site", "https://example.org");
    EXPECT_EQ(email.subject, "Your event wasn't listed: Sample Dance Night");
    EXPECT_TRUE(Contains(email.bodyHtml, "We weren't able to list it this time."));
    EXPECT_TRUE(Contains(email.bodyHtml, "<li>November 13, 2026, 7:00 PM: Not listed</li>"));
    EXPECT_TRUE(Contains(email.bodyHtml, "<p>Why: We needed more details to list it.</p>"));
    EXPECT_FALSE(Contains(email.bodyHtml, "see it"));
    // A base URL without its trailing slash still makes good links.
    EXPECT_TRUE(Contains(email.bodyHtml, "https://example.org/account/submissions"));
}

TEST(SubmissionEmailTest, AMixListsEachDate) {
    DecisionNotice notice = MakeNotice({ DbSchema::kEventsStatusApproved,
                                         DbSchema::kEventsStatusRejected,
                                         DbSchema::kEventsStatusApproved });
    notice.rejectReason = std::string(DbSchema::kEventSubmissionsRejectReasonPast);
    DecisionEmail email = RenderDecisionEmail(notice, "Sample Site", "https://example.org/");
    EXPECT_EQ(email.subject, "Your event has been reviewed: Sample Dance Night");
    EXPECT_TRUE(Contains(email.bodyHtml, "Some of its dates are on the calendar now."));
    EXPECT_TRUE(Contains(email.bodyHtml, "events/100"));
    EXPECT_TRUE(Contains(email.bodyHtml, "November 20, 2026, 7:00 PM: Not listed"));
    EXPECT_FALSE(Contains(email.bodyHtml, "events/101"));
    EXPECT_TRUE(Contains(email.bodyHtml, "events/102"));
    EXPECT_TRUE(Contains(email.bodyHtml, "Why: The date had already passed."));
}

TEST(SubmissionEmailTest, TheMessageIsIncludedWithItsLineBreaks) {
    DecisionNotice notice = MakeNotice({ DbSchema::kEventsStatusRejected });
    notice.rejectReason = std::string(DbSchema::kEventSubmissionsRejectReasonOther);
    notice.messageToSubmitter = "Thanks for this!\nCould you send it again with the address?";
    DecisionEmail email = RenderDecisionEmail(notice, "Sample Site", "https://example.org/");
    EXPECT_TRUE(Contains(email.bodyHtml, "A note from the curators:"));
    EXPECT_TRUE(Contains(email.bodyHtml,
        "<blockquote>Thanks for this!<br>\r\nCould you send it again with the address?</blockquote>"));
    // `other` has no canned reason: the message says it.
    EXPECT_FALSE(Contains(email.bodyHtml, "Why:"));
}

TEST(SubmissionEmailTest, NoMessageNoNote) {
    DecisionEmail email = RenderDecisionEmail(
        MakeNotice({ DbSchema::kEventsStatusApproved }), "Sample Site", "https://example.org/");
    EXPECT_FALSE(Contains(email.bodyHtml, "A note from the curators"));
    EXPECT_FALSE(Contains(email.bodyHtml, "blockquote"));
}

// Whatever a submitter or curator typed is escaped, a typed "{placeholder}"
// stays text, and the subject stays on one line.
TEST(SubmissionEmailTest, TypedTextIsEscaped) {
    DecisionNotice notice = MakeNotice({ DbSchema::kEventsStatusRejected });
    notice.dates.front().title = "<b>Bold</b> & {reason}\r\nBcc: someone@example.com";
    notice.recipientFirstName = "<script>";
    notice.messageToSubmitter = "See {my_submissions_url} \"soon\"";
    DecisionEmail email = RenderDecisionEmail(notice, "Sample Site", "https://example.org/");

    EXPECT_EQ(email.subject.find('\n'), std::string::npos);
    EXPECT_EQ(email.subject.find('\r'), std::string::npos);
    EXPECT_FALSE(Contains(email.bodyHtml, "<b>Bold</b>"));
    EXPECT_TRUE(Contains(email.bodyHtml, "&lt;b&gt;Bold&lt;/b&gt; &amp; &#123;reason&#125;"));
    EXPECT_TRUE(Contains(email.bodyHtml, "Hi &lt;script&gt;,"));
    EXPECT_TRUE(Contains(email.bodyHtml, "See &#123;my_submissions_url&#125; &quot;soon&quot;"));
}

TEST(SubmissionEmailTest, TheBodyUsesCrLf) {
    DecisionEmail email = RenderDecisionEmail(
        MakeNotice({ DbSchema::kEventsStatusApproved }), "Sample Site", "https://example.org/");
    for (size_t i = 0; i < email.bodyHtml.size(); ++i) {
        if (email.bodyHtml[i] == '\n') {
            ASSERT_GT(i, 0u);
            EXPECT_EQ(email.bodyHtml[i - 1], '\r') << "bare \\n at " << i;
        }
    }
}

TEST(SubmissionEmailTest, EveryCuratorReasonButOtherHasWords) {
    for (std::string_view reason : { DbSchema::kEventSubmissionsRejectReasonDuplicate,
                                     DbSchema::kEventSubmissionsRejectReasonNotAFit,
                                     DbSchema::kEventSubmissionsRejectReasonMissingDetails,
                                     DbSchema::kEventSubmissionsRejectReasonPast,
                                     DbSchema::kEventSubmissionsRejectReasonSpam,
                                     DbSchema::kEventSubmissionsRejectReasonExpired }) {
        EXPECT_FALSE(DescribeRejectReason(reason).empty()) << reason;
    }
    EXPECT_TRUE(DescribeRejectReason(DbSchema::kEventSubmissionsRejectReasonOther).empty());
    EXPECT_TRUE(DescribeRejectReason("unknown").empty());
}

}  // namespace
}  // namespace Events
