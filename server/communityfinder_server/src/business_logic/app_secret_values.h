#pragma once

#include <functional>
#include <string>
#include <string_view>

// Phase 1.3 componentization: app-side default VALUES for secrets.
//
// The framework (util/secrets/secret_values.cpp) registers only non-brand,
// non-domain defaults. This app-side registration supplies:
//   - defaults for any app-owned domain keys (see app_secret_keys.h — none yet);
//   - brand-specific default VALUES for framework keys whose value is
//     CommunityFinder-specific (mail sender identity, email subjects, website
//     addresses).
//
// Both the seed path (create_database) and the test secrets helper call this
// alongside Secrets::Values::FillInSecretsStringView so every default lands.
namespace App {

// StringView variant — matches Secrets::Values::FillInSecretsStringView.
void FillInAppSecretDefaults(
    std::function<void(std::string_view, std::string_view)> addSecret);

// std::string variant — matches Secrets::Values::FillInSecretsString, for
// callers whose sink takes const std::string& (e.g. the test secrets helper).
void FillInAppSecretDefaultsString(
    std::function<void(const std::string&, const std::string&)> addSecret);

}  // namespace App
