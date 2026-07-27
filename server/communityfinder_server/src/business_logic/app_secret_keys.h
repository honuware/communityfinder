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
// NOTE: There are no app-specific secret keys yet — CommunityFinder is a
// skeleton at this point. When a domain feature needs a tunable secret, add its
// KEY here AND its DEFAULT VALUE in app_secret_values.cpp (keep the two in
// sync). Framework keys (mail server, auth, image, website routing, site logo,
// etc.) already live in util/secrets/secret_keys.h and are not repeated here.
namespace Secrets {

// (No app-specific secret keys yet.)

}  // namespace Secrets
