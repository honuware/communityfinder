#include "business_logic/app_secret_values.h"

#include <map>
#include <string>
#include <string_view>

#include <gtest/gtest.h>

#include "business_logic/app_secret_keys.h"
#include "util/secrets/secret_keys.h"
#include "util/secrets/secret_values.h"

// Phase 1.3 componentization: verifies the app-side default registration and
// the framework/app split. The framework (Secrets::Values) must not emit any
// brand value; the app (App::FillInAppSecretDefaults) must supply exactly the
// CommunityFinder brand defaults and no framework-owned key.

namespace App {
namespace {

std::map<std::string, std::string> CollectAppDefaults() {
    std::map<std::string, std::string> out;
    FillInAppSecretDefaults([&](std::string_view k, std::string_view v) {
        out[std::string(k)] = std::string(v);
    });
    return out;
}

std::map<std::string, std::string> CollectFrameworkDefaults() {
    std::map<std::string, std::string> out;
    Secrets::Values::FillInSecretsStringView(
        [&](std::string_view k, std::string_view v) {
            out[std::string(k)] = std::string(v);
        });
    return out;
}

TEST(AppSecretValuesTest, RegistersBrandDefaultsForFrameworkKeys) {
    auto d = CollectAppDefaults();
    EXPECT_EQ(d[std::string(Secrets::kMailSenderName)], "CommunityFinder");
    EXPECT_EQ(d[std::string(Secrets::kMailSenderAddress)], "community.finder.seattle@gmail.com");
    EXPECT_EQ(d[std::string(Secrets::kMailActivationEmailSubject)], "CommunityFinder Account Activation");
    EXPECT_EQ(d[std::string(Secrets::kAdminAlertsDigestSubject)], "CommunityFinder Admin Alerts Digest");
    // Website address is build-mode dependent; assert it is set and non-empty.
    EXPECT_FALSE(d[std::string(Secrets::kWebsiteAddress)].empty());
    EXPECT_FALSE(d[std::string(Secrets::kWebsiteAddressLogin)].empty());
}

TEST(AppSecretValuesTest, StringVariantMatchesStringViewVariant) {
    auto sv = CollectAppDefaults();
    std::map<std::string, std::string> s;
    FillInAppSecretDefaultsString([&](const std::string& k, const std::string& v) {
        s[k] = v;
    });
    EXPECT_EQ(sv, s);
}

TEST(AppSecretValuesTest, FrameworkDefaultsExcludeBrandValues) {
    auto fw = CollectFrameworkDefaults();
    // Brand-valued framework keys are NOT defaulted by the framework — the app
    // supplies their values.
    EXPECT_FALSE(fw.count(std::string(Secrets::kMailSenderName)));
    EXPECT_FALSE(fw.count(std::string(Secrets::kMailSenderAddress)));
    EXPECT_FALSE(fw.count(std::string(Secrets::kMailActivationEmailSubject)));
    EXPECT_FALSE(fw.count(std::string(Secrets::kAdminAlertsDigestSubject)));
    EXPECT_FALSE(fw.count(std::string(Secrets::kWebsiteAddress)));
    EXPECT_FALSE(fw.count(std::string(Secrets::kWebsiteAddressLogin)));
    // Genuine framework defaults DO remain.
    EXPECT_TRUE(fw.count(std::string(Secrets::kMailServerName)));
    EXPECT_TRUE(fw.count(std::string(Secrets::kAuthArgon2OpsLimit)));
    EXPECT_TRUE(fw.count(std::string(Secrets::kServerProductionMode)));
    EXPECT_TRUE(fw.count(std::string(Secrets::kImageMaxUploadBytes)));
    // Non-brand website routing bits stay framework.
    EXPECT_TRUE(fw.count(std::string(Secrets::kWebsiteActivationLink)));
    EXPECT_TRUE(fw.count(std::string(Secrets::kWebsiteLoginLink)));
    // site_logo_url is framework-owned (default ""), NOT an app brand value.
    EXPECT_TRUE(fw.count(std::string(Secrets::kSiteLogoUrl)));
}

TEST(AppSecretValuesTest, FrameworkAndAppKeySetsDoNotOverlap) {
    auto fw = CollectFrameworkDefaults();
    auto app = CollectAppDefaults();
    for (const auto& [key, value] : app) {
        EXPECT_EQ(fw.count(key), 0u)
            << "key registered by BOTH framework and app: " << key;
    }
}

}  // namespace
}  // namespace App
