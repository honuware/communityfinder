# EV4 §4 — the scheduled jobs' business logic: the two event sweeps (archive
# past events, expire unreviewed ones), and scheduled_jobs, which runs any job by
# name (these two, plus honuware's cleanups and admin-alerts digest) — the one
# code path behind both the job endpoints (endpoints/admin_*) and the
# test_helper's run-job.
target_sources(communityfinder_core PRIVATE
    event_lifecycle_jobs.h
    event_lifecycle_jobs.cpp
    scheduled_jobs.h
    scheduled_jobs.cpp
)

target_sources(communityfinder_test_cases PUBLIC
    event_lifecycle_jobs_test.cpp
    scheduled_jobs_test.cpp
)
