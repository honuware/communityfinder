#include "communityfinder_job_catalog.h"

#include <cstdlib>
#include <iterator>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

// EV4 §6.1 — the job catalog, its validation and the password fallback.
namespace CommunityFinder {
namespace {

const Scheduler::ScheduledJob* FindByName(const std::vector<Scheduler::ScheduledJob>& jobs,
                                          std::string_view name) {
    for (const Scheduler::ScheduledJob& job : jobs) {
        if (job.name == name) return &job;
    }
    return nullptr;
}

JobTarget ValidTarget() {
    JobTarget target;
    target.serviceAccountPassword = "sample-password";
    return target;
}

// Every field 0 (aggregate order), so a test can switch on just the ones it
// wants.
JobIntervals AllOff() {
    return { 0, 0, 0, 0, 0, 0, 0, 0, 0 };
}

TEST(CommunityFinderJobCatalogTest, TheDefaultsBuildEveryJobButTheReservedTwo) {
    struct Expected {
        std::string_view name;
        int64_t intervalSeconds;
    };
    const Expected expected[] = {
        { "archive_past_events", 3600 },
        { "expire_unreviewed_events", 3600 },
        { "cleanup_expired_tokens", 86400 },
        { "cleanup_login_attempts", 86400 },
        { "cleanup_scaled_photos", 86400 },
        { "cleanup_idempotency_keys", 86400 },
        { "notify_admin_alerts", 3600 },
    };
    const std::vector<Scheduler::ScheduledJob> jobs = BuildCommunityFinderJobs(ValidTarget(), JobIntervals{});
    EXPECT_EQ(jobs.size(), std::size(expected));
    for (const Expected& want : expected) {
        const Scheduler::ScheduledJob* job = FindByName(jobs, want.name);
        ASSERT_NE(job, nullptr) << want.name;
        EXPECT_EQ(job->method, "POST") << want.name;
        EXPECT_EQ(job->path, "/api/admin/" + std::string(want.name));
        EXPECT_EQ(job->intervalSeconds, want.intervalSeconds) << want.name;
    }
    // Their endpoints arrive with EV6 and EV5.
    EXPECT_EQ(FindByName(jobs, "import_event_feeds"), nullptr);
    EXPECT_EQ(FindByName(jobs, "run_event_scan"), nullptr);
}

TEST(CommunityFinderJobCatalogTest, TheReservedJobsAppearOnceGivenAnInterval) {
    JobIntervals intervals = AllOff();
    intervals.importEventFeedsSeconds = 3600;
    intervals.runEventScanSeconds = 86400;
    const std::vector<Scheduler::ScheduledJob> jobs = BuildCommunityFinderJobs(ValidTarget(), intervals);
    ASSERT_EQ(jobs.size(), 2u);
    const Scheduler::ScheduledJob* imports = FindByName(jobs, "import_event_feeds");
    ASSERT_NE(imports, nullptr);
    EXPECT_EQ(imports->path, "/api/admin/import_event_feeds");
    const Scheduler::ScheduledJob* scan = FindByName(jobs, "run_event_scan");
    ASSERT_NE(scan, nullptr);
    EXPECT_EQ(scan->intervalSeconds, 86400);
}

// Each job carries the whole target, so the engine never needs the catalog.
TEST(CommunityFinderJobCatalogTest, EveryJobCarriesTheTarget) {
    JobTarget target;
    target.serverUrl = "https://origin.example.com";
    target.serviceAccountEmail = "scheduler@example.com";
    target.serviceAccountPassword = "sample-password";
    target.headers = { { "X-Origin-Secret", "sample-secret" } };
    const std::vector<Scheduler::ScheduledJob> jobs = BuildCommunityFinderJobs(target, JobIntervals{});
    ASSERT_FALSE(jobs.empty());
    for (const Scheduler::ScheduledJob& job : jobs) {
        EXPECT_EQ(job.serverUrl, "https://origin.example.com") << job.name;
        EXPECT_EQ(job.serviceAccountEmail, "scheduler@example.com") << job.name;
        EXPECT_EQ(job.serviceAccountPassword, "sample-password") << job.name;
        ASSERT_EQ(job.headers.size(), 1u) << job.name;
        EXPECT_EQ(job.headers[0].first, "X-Origin-Secret");
        EXPECT_EQ(job.headers[0].second, "sample-secret");
    }
}

TEST(CommunityFinderJobCatalogTest, TheDefaultTargetIsTheLocalServerAndTheSeededAccount) {
    JobTarget target;
    EXPECT_EQ(target.serverUrl, "http://localhost:18081");
    EXPECT_EQ(target.serviceAccountEmail, "scheduler@communityfinder.local");
    EXPECT_TRUE(target.serviceAccountPassword.empty());
}

TEST(CommunityFinderJobCatalogTest, ZeroOrNegativeIntervalsDropAJob) {
    JobIntervals intervals;
    intervals.archivePastEventsSeconds = 0;
    intervals.photoCleanupSeconds = -1;
    const std::vector<Scheduler::ScheduledJob> jobs = BuildCommunityFinderJobs(ValidTarget(), intervals);
    EXPECT_EQ(jobs.size(), 5u);
    EXPECT_EQ(FindByName(jobs, "archive_past_events"), nullptr);
    EXPECT_EQ(FindByName(jobs, "cleanup_scaled_photos"), nullptr);
    EXPECT_NE(FindByName(jobs, "expire_unreviewed_events"), nullptr);

    EXPECT_TRUE(BuildCommunityFinderJobs(ValidTarget(), AllOff()).empty());
}

TEST(CommunityFinderJobCatalogTest, IntervalsArePassedThrough) {
    JobIntervals intervals;
    intervals.archivePastEventsSeconds = 60;
    intervals.adminAlertsNotifySeconds = 4242;
    const std::vector<Scheduler::ScheduledJob> jobs = BuildCommunityFinderJobs(ValidTarget(), intervals);
    EXPECT_EQ(FindByName(jobs, "archive_past_events")->intervalSeconds, 60);
    EXPECT_EQ(FindByName(jobs, "notify_admin_alerts")->intervalSeconds, 4242);
}

// --- ValidateCommunityFinderConfig ---

TEST(CommunityFinderConfigTest, TheDefaultsWithAPasswordAreValid) {
    EXPECT_TRUE(ValidateCommunityFinderConfig(ValidTarget(), JobIntervals{}).empty());
    // Everything off is a valid choice too.
    EXPECT_TRUE(ValidateCommunityFinderConfig(ValidTarget(), AllOff()).empty());
}

TEST(CommunityFinderConfigTest, EachMissingTargetFieldIsNamed) {
    struct Case {
        std::string_view flag;
        void (*clear)(JobTarget&);
    };
    const Case cases[] = {
        { "server_url", [](JobTarget& target) { target.serverUrl.clear(); } },
        { "service_account_email", [](JobTarget& target) { target.serviceAccountEmail.clear(); } },
        { "service_account_password", [](JobTarget& target) { target.serviceAccountPassword.clear(); } },
    };
    for (const Case& testCase : cases) {
        JobTarget target = ValidTarget();
        testCase.clear(target);
        const std::vector<std::string> errors = ValidateCommunityFinderConfig(target, JobIntervals{});
        ASSERT_EQ(errors.size(), 1u) << testCase.flag;
        EXPECT_NE(errors[0].find(testCase.flag), std::string::npos) << errors[0];
    }
}

TEST(CommunityFinderConfigTest, NegativeIntervalsAreNamedAndEveryProblemIsReported) {
    JobTarget target;  // no password
    JobIntervals intervals;
    intervals.expireUnreviewedEventsSeconds = -1;
    intervals.runEventScanSeconds = -5;
    const std::vector<std::string> errors = ValidateCommunityFinderConfig(target, intervals);
    ASSERT_EQ(errors.size(), 3u);
    auto mentions = [&](std::string_view text) {
        for (const std::string& error : errors) {
            if (error.find(text) != std::string::npos) return true;
        }
        return false;
    };
    EXPECT_TRUE(mentions("service_account_password"));
    EXPECT_TRUE(mentions("expire_unreviewed_events_interval"));
    EXPECT_TRUE(mentions("run_event_scan_interval"));
}

// --- ResolveServiceAccountPassword ---

void SetEnv(const char* name, const char* value) {
#ifdef _WIN32
    _putenv_s(name, value);
#else
    setenv(name, value, /*overwrite=*/1);
#endif
}

void UnsetEnv(const char* name) {
#ifdef _WIN32
    _putenv_s(name, "");
#else
    unsetenv(name);
#endif
}

// Restores a variable on scope exit.
class EnvScope {
public:
    explicit EnvScope(const char* name) : name_(name) {
        if (const char* value = std::getenv(name_)) {
            had_ = true;
            prior_ = value;
        }
        UnsetEnv(name_);
    }
    ~EnvScope() {
        if (had_) {
            SetEnv(name_, prior_.c_str());
        } else {
            UnsetEnv(name_);
        }
    }

private:
    const char* name_;
    bool had_ = false;
    std::string prior_;
};

constexpr const char* kTestVariable = "COMMUNITYFINDER_SCHEDULER_TEST_PASSWORD";

TEST(CommunityFinderConfigTest, ThePasswordFlagWinsOverTheEnvironment) {
    EnvScope scope(kTestVariable);
    SetEnv(kTestVariable, "from-env");
    EXPECT_EQ(ResolveServiceAccountPassword("from-flag", kTestVariable), "from-flag");
}

TEST(CommunityFinderConfigTest, ThePasswordFallsBackToTheEnvironment) {
    EnvScope scope(kTestVariable);
    SetEnv(kTestVariable, "from-env");
    EXPECT_EQ(ResolveServiceAccountPassword("", kTestVariable), "from-env");
}

TEST(CommunityFinderConfigTest, NoPasswordAnywhereIsEmpty) {
    EnvScope scope(kTestVariable);
    EXPECT_EQ(ResolveServiceAccountPassword("", kTestVariable), "");
}

}  // namespace
}  // namespace CommunityFinder
