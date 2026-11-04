#include "business_logic/jobs/scheduled_jobs.h"

#include <string>

#include "business_logic/events/submission_helper.h"
#include "business_logic/jobs/event_lifecycle_jobs.h"

namespace Jobs {

namespace {

int64_t NowUs(Transaction& transaction) {
    return std::stoll(transaction.RunSqlStatementReturningOneValue("SELECT now_us()"));
}

Json::Value ArchivePastEvents(Transaction& transaction, DatabaseHelper databaseHelper) {
    ArchiveResult archive = EventLifecycleJobs(databaseHelper)
        .ArchivePastEvents(transaction, NowUs(transaction), kEventSweepBatchLimit);
    return Json::Value(Json::JsonObject{
        { "archived", Json::Value(archive.archived) },
    });
}

Json::Value ExpireUnreviewedEvents(Transaction& transaction, DatabaseHelper databaseHelper,
                                   Secrets::SecretsHelper& secrets,
                                   const ::Mail::MailHelperPtr& mailHelper) {
    ExpireResult expire = EventLifecycleJobs(databaseHelper)
        .ExpireUnreviewedEvents(transaction, NowUs(transaction), kEventSweepBatchLimit);
    // A send that fails records an admin alert (which the digest mails) and
    // never fails the job.
    Events::SubmissionHelper submissionHelper(databaseHelper);
    int64_t emailsSent = 0;
    for (const Events::DecisionNotice& notice : expire.submissionNotices) {
        if (submissionHelper.SendDecisionNotice(transaction, secrets, mailHelper, notice)) {
            ++emailsSent;
        }
    }
    return Json::Value(Json::JsonObject{
        { "expired", Json::Value(expire.expired) },
        { "decision_emails_sent", Json::Value(emailsSent) },
    });
}

}  // namespace

const std::vector<std::string_view>& ScheduledJobNames() {
    static const std::vector<std::string_view> names = {
        kJobArchivePastEvents,
        kJobExpireUnreviewedEvents,
        kJobCleanupExpiredTokens,
        kJobCleanupLoginAttempts,
        kJobCleanupScaledPhotos,
        kJobCleanupIdempotencyKeys,
        kJobNotifyAdminAlerts,
    };
    return names;
}

std::optional<Json::Value> RunScheduledJob(
    Transaction& transaction,
    DatabaseHelper databaseHelper,
    Secrets::SecretsHelper& secrets,
    const ::Mail::MailHelperPtr& mailHelper,
    std::string_view jobName) {
    if (jobName == kJobArchivePastEvents) {
        return ArchivePastEvents(transaction, databaseHelper);
    }
    if (jobName == kJobExpireUnreviewedEvents) {
        return ExpireUnreviewedEvents(transaction, databaseHelper, secrets, mailHelper);
    }
    // The rest are honuware's; it answers nullopt for a name it doesn't know.
    return PlatformJobs::RunPlatformJob(transaction, databaseHelper, secrets, mailHelper, jobName);
}

}  // namespace Jobs
