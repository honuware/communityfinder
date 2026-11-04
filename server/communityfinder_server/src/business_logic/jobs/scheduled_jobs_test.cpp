#include "business_logic/jobs/scheduled_jobs.h"

#include <optional>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include <gtest/gtest.h>

#include "test/src/util/database_test_helper.h"
#include "util/mail/mail_helper_test_util.h"

// EV4 §5/§7 — the jobs by name, as the endpoints and the test_helper's run-job
// call them. Each job's rules are tested in its own file (honuware's five in
// honuware); this pins the names and the count keys.
namespace Jobs {
namespace {

std::set<std::string> KeysOf(const Json::Value& value) {
    std::set<std::string> keys;
    for (const auto& [key, child] : value.GetChildren()) {
        keys.insert(key);
    }
    return keys;
}

TEST(ScheduledJobsTest, EveryJobRunsOnAnEmptyDatabaseAndReportsItsCounts) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("EveryJobRunsOnAnEmptyDatabase", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        ::Mail::Test::TestMailHelperPtr mail = ::Mail::Test::MakeTestMailHelper();
        const std::vector<std::pair<std::string_view, std::set<std::string>>> expected = {
            { kJobArchivePastEvents, { "archived" } },
            { kJobExpireUnreviewedEvents, { "expired", "decision_emails_sent" } },
            { kJobCleanupExpiredTokens, { "device_tokens_deleted", "email_verifications_deleted" } },
            { kJobCleanupLoginAttempts, { "login_attempts_deleted" } },
            { kJobCleanupScaledPhotos, { "scaled_photos_deleted" } },
            { kJobCleanupIdempotencyKeys, { "idempotency_keys_deleted" } },
            { kJobNotifyAdminAlerts, { "alerts_count", "mail_sent", "last_notified_us" } },
        };
        ASSERT_EQ(ScheduledJobNames().size(), expected.size());
        for (size_t index = 0; index < expected.size(); ++index) {
            const auto& [name, keys] = expected[index];
            EXPECT_EQ(ScheduledJobNames()[index], name);
            std::optional<Json::Value> counts = RunScheduledJob(transaction, db, *secrets, mail, name);
            ASSERT_TRUE(counts.has_value()) << name;
            EXPECT_EQ(KeysOf(*counts), keys) << name;
        }
        EXPECT_TRUE(mail->GetMessages().empty());
    });
}

TEST(ScheduledJobsTest, AnUnknownJobIsNothing) {
    TestDatabaseUtil testDb;
    testDb.RunInTransaction("AnUnknownJobIsNothing", [&](Transaction& transaction) {
        DatabaseHelper db = testDb.GetDatabaseHelper();
        Secrets::SecretsHelperPtr secrets = Secrets::MakeSecretsHelper(db);
        EXPECT_FALSE(RunScheduledJob(transaction, db, *secrets, nullptr, "import_event_feeds").has_value());
        EXPECT_FALSE(RunScheduledJob(transaction, db, *secrets, nullptr, "").has_value());
    });
}

}  // namespace
}  // namespace Jobs
