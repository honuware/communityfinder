#pragma once

#include <optional>
#include <string>
#include <string_view>

namespace Events {

// CM3 §3.1 — the one parser for an embedded video's link. An event or series
// stores the canonical link (db_schema/video_links.h), and payloads carry only
// {provider, id}: the client builds the player's address from the id alone, so
// no pasted HTML or arbitrary address ever reaches a page (OQ3: YouTube and
// Vimeo only).

inline constexpr std::string_view kVideoProviderYoutube = "youtube";
inline constexpr std::string_view kVideoProviderVimeo = "vimeo";

// Not a link to one video we can read: blank, not a web address, a playlist,
// a channel or album page, an unlisted Vimeo link (its private hash can't be
// embedded by id alone), or an id of the wrong shape.
inline constexpr std::string_view kVideoErrorBadUrl = "BAD_VIDEO_URL";
// A web address, but not on YouTube or Vimeo.
inline constexpr std::string_view kVideoErrorHostNotSupported = "VIDEO_HOST_NOT_SUPPORTED";

struct VideoLink {
    std::string provider;      // kVideoProvider*
    std::string id;            // 11 characters on YouTube; digits on Vimeo
    std::string canonicalUrl;  // what the video_url column stores
};

struct VideoLinkResult {
    bool ok = false;
    std::string errorCode;  // kVideoError* when !ok
    VideoLink link;
};

// A pasted link → its canonical form. Accepts https, http or no scheme, and
// YouTube's variants (youtu.be/…, /shorts/…, /live/…, /embed/…, m.youtube.com,
// extra parameters such as &t=), and Vimeo's vimeo.com/<number>,
// vimeo.com/channels/<name>/<number> and player.vimeo.com/video/<number>.
// Surrounding whitespace is ignored.
VideoLinkResult ParseVideoLink(std::string_view text);

// A stored video_url → its video; nullopt for '' (no video) or anything that
// doesn't parse (which the column's CHECK rules out).
std::optional<VideoLink> StoredVideoLink(std::string_view videoUrl);

}  // namespace Events
