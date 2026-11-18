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

// CM3 OQ10: CommunityFinder's own values for FRAMEWORK keys that honuware
// already defaults — today, the public photo cap (1600×1200, not honuware's
// 800×800). honuware seeds these rows itself, so they can't be app defaults:
// config_secrets.name is UNIQUE, and FillInAppSecretDefaults must not overlap
// the framework's. The seed overwrites honuware's rows with them instead
// (ApplyFrameworkSecretOverrides, create_app_tables.h), and the test main
// layers them over the test secrets double so tests see what a live server does.
void FillInFrameworkSecretOverrides(
    std::function<void(std::string_view, std::string_view)> overrideSecret);

// std::string variant, for the test secrets double.
void FillInFrameworkSecretOverridesString(
    std::function<void(const std::string&, const std::string&)> overrideSecret);

}  // namespace App
