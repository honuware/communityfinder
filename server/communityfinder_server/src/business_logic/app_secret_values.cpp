#include "business_logic/app_secret_values.h"

#include "business_logic/app_secret_keys.h"
#include "util/secrets/secret_keys.h"

namespace App {
namespace {

// --- Brand default values for FRAMEWORK keys ---
// The keys live in util/secrets/secret_keys.h (framework), but their default
// values are CommunityFinder-specific, so the app supplies them here (Phase
// 1.3). Consumers MUST look them up via SecretsHelper (e.g.
// ::Mail::LoadSenderAddress) — never use the constant as the value.
inline constexpr std::string_view kMailSenderNameValue = "CommunityFinder";
// The dedicated Gmail sender account for CommunityFinder. mailio authenticates to
// Gmail using this address as the SMTP username, so it MUST match the account the
// app password (HONUWARE_MAIL_APP_PASSWORD, seeded in create_database) belongs to
// — a mismatch is the "Mail sender rejection" 400 on /api/register. Not a secret
// (it appears in every outbound From header); the password is the secret and stays
// in the environment. Production moves to a verified-domain sender (SES).
inline constexpr std::string_view kMailSenderAddressValue = "community.finder.seattle@gmail.com";
inline constexpr std::string_view kMailActivationEmailSubjectValue = "CommunityFinder Account Activation";
inline constexpr std::string_view kAdminAlertsDigestSubjectValue = "CommunityFinder Admin Alerts Digest";
// Dev URLs. Production addresses are still TBD; both build modes currently point
// at the local dev server (app server on 18081, `ng serve` on 4201). Introduce
// a real production branch here once the prod hostname is decided.
#ifdef _DEBUG
inline constexpr std::string_view kWebsiteAddressValue = "http://localhost:18081/";
inline constexpr std::string_view kWebsiteAddressLoginValue = "http://localhost:4201/";
#else
inline constexpr std::string_view kWebsiteAddressValue = "http://localhost:18081/";
inline constexpr std::string_view kWebsiteAddressLoginValue = "http://localhost:4201/";
#endif

// --- Default values for APP-owned domain keys ---
// None yet — CommunityFinder has no app-specific secret keys (see
// app_secret_keys.h). Add them here when a domain feature introduces one.

}  // namespace

void FillInAppSecretDefaults(
    std::function<void(std::string_view, std::string_view)> addSecret) {
    // Brand values for framework keys.
    addSecret(Secrets::kMailSenderName, kMailSenderNameValue);
    addSecret(Secrets::kMailSenderAddress, kMailSenderAddressValue);
    addSecret(Secrets::kMailActivationEmailSubject, kMailActivationEmailSubjectValue);
    addSecret(Secrets::kAdminAlertsDigestSubject, kAdminAlertsDigestSubjectValue);
    addSecret(Secrets::kWebsiteAddress, kWebsiteAddressValue);
    addSecret(Secrets::kWebsiteAddressLogin, kWebsiteAddressLoginValue);

    // App-owned domain keys: none yet.
    //
    // NOTE: site_logo_url (Secrets::kSiteLogoUrl) is intentionally NOT registered
    // here — the framework owns it and already defaults it to "" (empty) in
    // util/secrets/secret_values.cpp. Registering it app-side too would violate
    // the framework/app "no overlap" invariant.
}

void FillInAppSecretDefaultsString(
    std::function<void(const std::string&, const std::string&)> addSecret) {
    FillInAppSecretDefaults(
        [&](std::string_view key, std::string_view value) {
            addSecret(std::string(key), std::string(value));
        });
}

}  // namespace App
