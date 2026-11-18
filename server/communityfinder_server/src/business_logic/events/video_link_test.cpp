#include "business_logic/events/video_link.h"

#include <string>
#include <string_view>

#include <gtest/gtest.h>

namespace Events {
namespace {

constexpr std::string_view kYoutubeCanonical = "https://www.youtube.com/watch?v=dQw4w9WgXcQ";

void ExpectYoutube(std::string_view pasted) {
    VideoLinkResult result = ParseVideoLink(pasted);
    ASSERT_TRUE(result.ok) << pasted << ": " << result.errorCode;
    EXPECT_EQ(result.link.provider, kVideoProviderYoutube) << pasted;
    EXPECT_EQ(result.link.id, "dQw4w9WgXcQ") << pasted;
    EXPECT_EQ(result.link.canonicalUrl, kYoutubeCanonical) << pasted;
}

void ExpectVimeo(std::string_view pasted) {
    VideoLinkResult result = ParseVideoLink(pasted);
    ASSERT_TRUE(result.ok) << pasted << ": " << result.errorCode;
    EXPECT_EQ(result.link.provider, kVideoProviderVimeo) << pasted;
    EXPECT_EQ(result.link.id, "76979871") << pasted;
    EXPECT_EQ(result.link.canonicalUrl, "https://vimeo.com/76979871") << pasted;
}

void ExpectRefused(std::string_view pasted, std::string_view errorCode) {
    VideoLinkResult result = ParseVideoLink(pasted);
    EXPECT_FALSE(result.ok) << pasted;
    EXPECT_EQ(result.errorCode, errorCode) << pasted;
}

TEST(VideoLinkTest, YoutubeVariantsBecomeTheCanonicalWatchLink) {
    ExpectYoutube("https://www.youtube.com/watch?v=dQw4w9WgXcQ");
    ExpectYoutube("https://youtube.com/watch?v=dQw4w9WgXcQ");
    ExpectYoutube("https://m.youtube.com/watch?v=dQw4w9WgXcQ");
    ExpectYoutube("https://youtu.be/dQw4w9WgXcQ");
    ExpectYoutube("https://www.youtube.com/shorts/dQw4w9WgXcQ");
    ExpectYoutube("https://www.youtube.com/live/dQw4w9WgXcQ");
    ExpectYoutube("https://www.youtube.com/embed/dQw4w9WgXcQ");
    ExpectYoutube("https://www.youtube-nocookie.com/embed/dQw4w9WgXcQ");
}

TEST(VideoLinkTest, YoutubeExtraParametersAreDropped) {
    ExpectYoutube("https://www.youtube.com/watch?v=dQw4w9WgXcQ&t=42s");
    ExpectYoutube("https://www.youtube.com/watch?feature=share&v=dQw4w9WgXcQ&list=PL123");
    ExpectYoutube("https://youtu.be/dQw4w9WgXcQ?t=42");
    ExpectYoutube("https://youtu.be/dQw4w9WgXcQ?si=abc123#comments");
    ExpectYoutube("https://www.youtube.com/shorts/dQw4w9WgXcQ?feature=share");
}

TEST(VideoLinkTest, SchemeCaseAndWhitespaceAreForgiven) {
    ExpectYoutube("http://www.youtube.com/watch?v=dQw4w9WgXcQ");
    ExpectYoutube("www.youtube.com/watch?v=dQw4w9WgXcQ");
    ExpectYoutube("HTTPS://WWW.YouTube.com/watch?v=dQw4w9WgXcQ");
    ExpectYoutube("  https://youtu.be/dQw4w9WgXcQ\n");
    ExpectVimeo("http://vimeo.com/76979871");
}

// A playlist names no single video; a channel page is a list too.
TEST(VideoLinkTest, YoutubeListsAreRefused) {
    ExpectRefused("https://www.youtube.com/playlist?list=PLx0sYbCqOb8TBPRdmBHs5Iftvv9TPboYG",
                  kVideoErrorBadUrl);
    ExpectRefused("https://www.youtube.com/watch?list=PLx0sYbCqOb8TBPRdmBHs5Iftvv9TPboYG",
                  kVideoErrorBadUrl);
    ExpectRefused("https://www.youtube.com/@SomeChannel", kVideoErrorBadUrl);
    ExpectRefused("https://www.youtube.com/", kVideoErrorBadUrl);
}

TEST(VideoLinkTest, YoutubeIdsOfTheWrongShapeAreRefused) {
    ExpectRefused("https://youtu.be/dQw4w9WgXc", kVideoErrorBadUrl);    // 10
    ExpectRefused("https://youtu.be/dQw4w9WgXcQQ", kVideoErrorBadUrl);  // 12
    ExpectRefused("https://www.youtube.com/watch?v=dQw4w9WgX%3F", kVideoErrorBadUrl);
    ExpectRefused("https://youtu.be/dQw4w9WgXcQ/extra", kVideoErrorBadUrl);
}

TEST(VideoLinkTest, VimeoNumbersBecomeTheCanonicalLink) {
    ExpectVimeo("https://vimeo.com/76979871");
    ExpectVimeo("https://www.vimeo.com/76979871/");
    ExpectVimeo("https://vimeo.com/76979871?share=copy");
    ExpectVimeo("https://vimeo.com/channels/staffpicks/76979871");
    ExpectVimeo("https://player.vimeo.com/video/76979871?h=abc&dnt=1");
}

// None of these is one embeddable video.
TEST(VideoLinkTest, VimeoChannelsAlbumsAndUnlistedLinksAreRefused) {
    ExpectRefused("https://vimeo.com/channels/staffpicks", kVideoErrorBadUrl);
    ExpectRefused("https://vimeo.com/album/2222222", kVideoErrorBadUrl);
    ExpectRefused("https://vimeo.com/showcase/2222222", kVideoErrorBadUrl);
    ExpectRefused("https://vimeo.com/someuser", kVideoErrorBadUrl);
    ExpectRefused("https://vimeo.com/76979871/abcdef1234", kVideoErrorBadUrl);
    ExpectRefused("https://vimeo.com/1234567890123", kVideoErrorBadUrl);  // 13 digits
}

TEST(VideoLinkTest, OtherHostsAreNotSupported) {
    ExpectRefused("https://www.instagram.com/reel/abc123/", kVideoErrorHostNotSupported);
    ExpectRefused("https://www.tiktok.com/@someone/video/123", kVideoErrorHostNotSupported);
    ExpectRefused("https://example.com/watch?v=dQw4w9WgXcQ", kVideoErrorHostNotSupported);
    // A look-alike is another host.
    ExpectRefused("https://youtube.com.example.com/watch?v=dQw4w9WgXcQ",
                  kVideoErrorHostNotSupported);
}

TEST(VideoLinkTest, NonLinksAreRefused) {
    ExpectRefused("", kVideoErrorBadUrl);
    ExpectRefused("   ", kVideoErrorBadUrl);
    ExpectRefused("not a link", kVideoErrorBadUrl);
    ExpectRefused("javascript:alert(1)", kVideoErrorBadUrl);
    ExpectRefused("ftp://youtube.com/watch?v=dQw4w9WgXcQ", kVideoErrorBadUrl);
    ExpectRefused("https://user@youtube.com/watch?v=dQw4w9WgXcQ", kVideoErrorBadUrl);
    ExpectRefused("https://youtube.com:8080/watch?v=dQw4w9WgXcQ", kVideoErrorBadUrl);
    ExpectRefused("<iframe src=\"https://www.youtube.com/embed/dQw4w9WgXcQ\"></iframe>",
                  kVideoErrorBadUrl);
}

// The canonical forms are what the column stores; reading one back gives the
// same video, and '' is no video.
TEST(VideoLinkTest, StoredVideoLinkReadsTheColumn) {
    std::optional<VideoLink> youtube = StoredVideoLink(kYoutubeCanonical);
    ASSERT_TRUE(youtube.has_value());
    EXPECT_EQ(youtube->provider, kVideoProviderYoutube);
    EXPECT_EQ(youtube->id, "dQw4w9WgXcQ");
    EXPECT_EQ(youtube->canonicalUrl, kYoutubeCanonical);

    std::optional<VideoLink> vimeo = StoredVideoLink("https://vimeo.com/76979871");
    ASSERT_TRUE(vimeo.has_value());
    EXPECT_EQ(vimeo->provider, kVideoProviderVimeo);

    EXPECT_FALSE(StoredVideoLink("").has_value());
    EXPECT_FALSE(StoredVideoLink("https://example.com/").has_value());
}

}  // namespace
}  // namespace Events
