#pragma once

#include <string_view>

// App-side ownership of the internal service-account identity (Phase 3.2
// bootstrap-seam audit). The framework auth code (business_logic/auth/
// service_account) does not hard-code a brand domain: `IsServiceAccountEmail`
// takes the domain as a parameter and `EnsureSchedulerServiceAccount` takes the
// account email, so this header is the single seam where the identity lives.
//
// It is a DEPLOY-FIXED identity, not tunable config: the seeded `people` row's
// email is built from these values and the exclusion filter + the scheduler's
// login must match it forever, so a runtime-mutable config secret would be a
// footgun (changing it silently breaks the filter and the scheduler login). It
// therefore parallels business_logic/app_database_config.h (App::kDatabaseName)
// rather than the mail-identity secrets. The multi-tenant work later sources the
// domain per tenant instead of from these process-wide constants.
//
// INVARIANT: kSchedulerServiceAccountEmail MUST end with kServiceAccountEmailDomain.
namespace App {

inline constexpr std::string_view kServiceAccountEmailDomain = "@communityfinder.local";

inline constexpr std::string_view kSchedulerServiceAccountEmail =
    "scheduler@communityfinder.local";

}  // namespace App
