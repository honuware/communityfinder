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

TEST(AppSecretValuesTest, RegistersTheSubmissionSettingDefaults) {
    auto d = CollectAppDefaults();
    EXPECT_EQ(d[std::string(Secrets::kEventSubmissionMaxPendingDates)], "10");
    EXPECT_EQ(d[std::string(Secrets::kEventSubmissionMaxPerDay)], "5");
    EXPECT_EQ(d[std::string(Secrets::kEventSubmissionsPaused)], "false");
}

TEST(AppSecretValuesTest, RegistersThePastEventSettingDefaults) {
    auto d = CollectAppDefaults();
    EXPECT_EQ(d[std::string(Secrets::kArchivedEventsVisibleDays)], "365");
    EXPECT_EQ(d[std::string(Secrets::kEventsShowPastByDefault)], "false");
}

TEST(AppSecretValuesTest, RegistersTheGalleryCapDefault) {
    auto d = CollectAppDefaults();
    EXPECT_EQ(d[std::string(Secrets::kEventGalleryMaxPhotos)], "12");
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

// --- CM3 OQ10: CommunityFinder's values for framework-defaulted keys ---

std::map<std::string, std::string> CollectFrameworkOverrides() {
    std::map<std::string, std::string> out;
    FillInFrameworkSecretOverrides([&](std::string_view k, std::string_view v) {
        out[std::string(k)] = std::string(v);
    });
    return out;
}

TEST(AppSecretValuesTest, RaisesThePublicPhotoCapTo1600By1200) {
    auto overrides = CollectFrameworkOverrides();
    EXPECT_EQ(overrides.size(), 2u);
    EXPECT_EQ(overrides[std::string(Secrets::kImageMaxPublicScaledWidth)], "1600");
    EXPECT_EQ(overrides[std::string(Secrets::kImageMaxPublicScaledHeight)], "1200");
}

// The inverse of the no-overlap rule: an override replaces a row honuware's seed
// writes, so each must name a key honuware defaults (a key honuware renamed
// would otherwise overwrite nothing), and none may also be an app default.
TEST(AppSecretValuesTest, OverridesNameOnlyFrameworkDefaultedKeys) {
    auto fw = CollectFrameworkDefaults();
    auto app = CollectAppDefaults();
    for (const auto& [key, value] : CollectFrameworkOverrides()) {
        EXPECT_EQ(fw.count(key), 1u) << "honuware doesn't default " << key;
        EXPECT_EQ(app.count(key), 0u) << "also an app default: " << key;
        EXPECT_NE(fw[key], value) << key << " matches honuware's own default";
    }
}

TEST(AppSecretValuesTest, OverridesStringVariantMatchesStringViewVariant) {
    std::map<std::string, std::string> s;
    FillInFrameworkSecretOverridesString([&](const std::string& k, const std::string& v) {
        s[k] = v;
    });
    EXPECT_EQ(CollectFrameworkOverrides(), s);
}

}  // namespace
}  // namespace App
