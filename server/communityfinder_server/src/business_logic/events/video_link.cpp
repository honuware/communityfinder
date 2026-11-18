#include "business_logic/events/video_link.h"

#include <algorithm>
#include <cctype>
#include <vector>

#include "db_schema/video_links.h"

namespace Events {

namespace {

constexpr std::string_view kYoutubeHosts[] = {
    "youtube.com", "www.youtube.com", "m.youtube.com",
    "youtube-nocookie.com", "www.youtube-nocookie.com",
};
constexpr std::string_view kYoutubeShortHost = "youtu.be";
constexpr std::string_view kVimeoHosts[] = { "vimeo.com", "www.vimeo.com" };
constexpr std::string_view kVimeoPlayerHost = "player.vimeo.com";

// The YouTube paths whose next segment is the id: /shorts/<id> and the like.
constexpr std::string_view kYoutubeIdPathPrefixes[] = { "shorts", "live", "embed", "v" };

bool IsSpace(char c) {
    return std::isspace(static_cast<unsigned char>(c)) != 0;
}

std::string_view Trim(std::string_view text) {
    while (!text.empty() && IsSpace(text.front())) text.remove_prefix(1);
    while (!text.empty() && IsSpace(text.back())) text.remove_suffix(1);
    return text;
}

std::string Lower(std::string_view text) {
    std::string lower(text);
    std::transform(lower.begin(), lower.end(), lower.begin(),
                   [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return lower;
}

bool StartsWithIgnoringCase(std::string_view text, std::string_view prefix) {
    return text.size() >= prefix.size() && Lower(text.substr(0, prefix.size())) == prefix;
}

template <size_t N>
bool OneOf(std::string_view value, const std::string_view (&options)[N]) {
    return std::find(std::begin(options), std::end(options), value) != std::end(options);
}

// The path's segments, without the empty ones a leading or doubled "/" makes.
std::vector<std::string_view> Segments(std::string_view path) {
    std::vector<std::string_view> segments;
    while (!path.empty()) {
        size_t slash = path.find('/');
        std::string_view segment = path.substr(0, slash);
        if (!segment.empty()) segments.push_back(segment);
        if (slash == std::string_view::npos) break;
        path.remove_prefix(slash + 1);
    }
    return segments;
}

// The value of `name` in "a=1&b=2", or "" when it isn't there.
std::string_view QueryValue(std::string_view query, std::string_view name) {
    while (!query.empty()) {
        size_t amp = query.find('&');
        std::string_view pair = query.substr(0, amp);
        size_t equals = pair.find('=');
        if (equals != std::string_view::npos && pair.substr(0, equals) == name) {
            return pair.substr(equals + 1);
        }
        if (amp == std::string_view::npos) break;
        query.remove_prefix(amp + 1);
    }
    return {};
}

bool IsYoutubeId(std::string_view id) {
    return id.size() == DbSchema::kYoutubeVideoIdLength
        && std::all_of(id.begin(), id.end(), [](unsigned char c) {
               return std::isalnum(c) || c == '_' || c == '-';
           });
}

bool IsVimeoId(std::string_view id) {
    return !id.empty() && id.size() <= DbSchema::kVimeoVideoIdMaxDigits
        && std::all_of(id.begin(), id.end(), [](unsigned char c) { return std::isdigit(c); });
}

VideoLinkResult Failure(std::string_view errorCode) {
    VideoLinkResult result;
    result.errorCode = std::string(errorCode);
    return result;
}

VideoLinkResult Youtube(std::string_view id) {
    if (!IsYoutubeId(id)) return Failure(kVideoErrorBadUrl);
    VideoLinkResult result;
    result.ok = true;
    result.link = { std::string(kVideoProviderYoutube), std::string(id),
                    std::string(DbSchema::kVideoUrlYoutubePrefix) + std::string(id) };
    return result;
}

VideoLinkResult Vimeo(std::string_view id) {
    if (!IsVimeoId(id)) return Failure(kVideoErrorBadUrl);
    VideoLinkResult result;
    result.ok = true;
    result.link = { std::string(kVideoProviderVimeo), std::string(id),
                    std::string(DbSchema::kVideoUrlVimeoPrefix) + std::string(id) };
    return result;
}

}  // namespace

VideoLinkResult ParseVideoLink(std::string_view text) {
    std::string_view rest = Trim(text);
    if (rest.empty() || std::any_of(rest.begin(), rest.end(), [](char c) {
            return IsSpace(c) || static_cast<unsigned char>(c) < 0x20;
        })) {
        return Failure(kVideoErrorBadUrl);
    }

    // The scheme: https, http, or none at all. Anything else (javascript:,
    // ftp://, data:) is not a video link.
    if (StartsWithIgnoringCase(rest, "https://")) {
        rest.remove_prefix(8);
    } else if (StartsWithIgnoringCase(rest, "http://")) {
        rest.remove_prefix(7);
    } else if (rest.find("://") != std::string_view::npos) {
        return Failure(kVideoErrorBadUrl);
    }

    // host, then path, then ?query, then #fragment (ignored).
    size_t hostEnd = rest.find_first_of("/?#");
    std::string host = Lower(rest.substr(0, hostEnd));
    rest = hostEnd == std::string_view::npos ? std::string_view() : rest.substr(hostEnd);
    if (host.empty() || host.find('.') == std::string::npos
        || !std::all_of(host.begin(), host.end(), [](unsigned char c) {
               return std::isalnum(c) || c == '.' || c == '-';
           })) {
        // No dot, or a user, a port, or other punctuation: not a web address
        // we can vouch for.
        return Failure(kVideoErrorBadUrl);
    }
    rest = rest.substr(0, rest.find('#'));
    size_t queryStart = rest.find('?');
    std::string_view path = rest.substr(0, queryStart);
    std::string_view query =
        queryStart == std::string_view::npos ? std::string_view() : rest.substr(queryStart + 1);
    const std::vector<std::string_view> segments = Segments(path);

    if (host == kYoutubeShortHost) {
        // youtu.be/<id>
        return segments.size() == 1 ? Youtube(segments[0]) : Failure(kVideoErrorBadUrl);
    }
    if (OneOf(host, kYoutubeHosts)) {
        if (segments.size() == 1 && segments[0] == "watch") {
            // /watch?v=<id>, the v anywhere among the parameters. A playlist
            // link with no v names no single video.
            return Youtube(QueryValue(query, "v"));
        }
        if (segments.size() == 2 && OneOf(segments[0], kYoutubeIdPathPrefixes)) {
            return Youtube(segments[1]);
        }
        return Failure(kVideoErrorBadUrl);
    }
    if (OneOf(host, kVimeoHosts)) {
        // vimeo.com/<number>, or a video shown in a channel. An unlisted link
        // (/<number>/<hash>), an album, a showcase, or a channel's own page is
        // refused: none of them is one embeddable video.
        if (segments.size() == 1) return Vimeo(segments[0]);
        if (segments.size() == 3 && segments[0] == "channels") return Vimeo(segments[2]);
        return Failure(kVideoErrorBadUrl);
    }
    if (host == kVimeoPlayerHost) {
        // player.vimeo.com/video/<number>, the address an embed code holds.
        return segments.size() == 2 && segments[0] == "video" ? Vimeo(segments[1])
                                                              : Failure(kVideoErrorBadUrl);
    }
    return Failure(kVideoErrorHostNotSupported);
}

std::optional<VideoLink> StoredVideoLink(std::string_view videoUrl) {
    if (Trim(videoUrl).empty()) return std::nullopt;
    VideoLinkResult result = ParseVideoLink(videoUrl);
    if (!result.ok) return std::nullopt;
    return result.link;
}

}  // namespace Events
