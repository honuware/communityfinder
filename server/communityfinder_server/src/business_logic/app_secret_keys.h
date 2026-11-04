#pragma once

#include <string_view>

// App-owned secret KEY names (Phase 1.3 componentization seam). This header is
// the app-side home for secret keys that belong to CommunityFinder's own domain
// features, split out of the framework `util/secrets/secret_keys.h` so that
// header stays brand-/domain-free for the honuware extraction.
//
// App keys stay in `namespace Secrets` (not `App`) on purpose: consumers refer
// to them as `Secrets::kSomeAppKey`, so adding one here is a pure include change
// with no symbol churn. Their DEFAULT VALUES live app-side too — see
// business_logic/app_secret_values.{h,cpp}.
//
// When a domain feature needs a tunable secret, add its KEY here AND its DEFAULT
// VALUE in app_secret_values.cpp (keep the two in sync). Framework keys (mail
// server, auth, image, website routing, site logo, etc.) already live in
// util/secrets/secret_keys.h and are not repeated here.
namespace Secrets {

// EV3 §3.3 / §3.4 — the event submission settings (the two rate limits and the
// pause switch), which curators change on the Submission limits page
// (/api/admin/event_submission_settings). Read them only through
// Events::LoadSubmissionSettings: a missing, blank or bad value reads as the
// default there, so an older database without the rows still works.
inline constexpr std::string_view kEventSubmissionMaxPendingDates =
    "event_submission_max_pending_dates";
inline constexpr std::string_view kEventSubmissionMaxPerDay = "event_submission_max_per_day";
// "true" closes the submit form for everyone; anything else leaves it open.
inline constexpr std::string_view kEventSubmissionsPaused = "event_submissions_paused";

// EV4 §10 — the past-events settings, changed on the same admin page
// (/api/admin/past_event_settings). Read them only through
// Events::LoadPastEventSettings, which reads a missing or bad value as the
// default. How many days an event stays public after it ends (OQ8):
inline constexpr std::string_view kArchivedEventsVisibleDays = "archived_events_visible_days";
// "true" starts the calendar's "Show past events" switch on for a visitor who
// hasn't chosen (OQ9); anything else starts it off.
inline constexpr std::string_view kEventsShowPastByDefault = "events_show_past_by_default";

}  // namespace Secrets
