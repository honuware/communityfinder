#pragma once

#include <cstddef>
#include <string>
#include <string_view>

namespace DbSchema {

	// CM3 — the one embedded video an event or a series may carry (`video_url`
	// on both tables). The column holds a canonical link, never pasted HTML:
	// business_logic's ParseVideoLink turns a pasted link into one of these two
	// shapes, and the CHECK below is the backstop for the generic CRUD editor,
	// which no app code sees. '' = no video.
	inline constexpr std::string_view kVideoUrlYoutubePrefix = "https://www.youtube.com/watch?v=";
	inline constexpr std::string_view kVideoUrlVimeoPrefix = "https://vimeo.com/";

	// A YouTube id is exactly 11 of [A-Za-z0-9_-]; a Vimeo id is a number.
	inline constexpr std::size_t kYoutubeVideoIdLength = 11;
	inline constexpr std::size_t kVimeoVideoIdMaxDigits = 12;

	// The CHECK expression for a `video_url` column: '' or a canonical link.
	inline std::string VideoUrlCheckExpression(std::string_view column) {
		std::string name(column);
		return name + " = '' OR " +
			name + R"( ~ '^https://www\.youtube\.com/watch\?v=[A-Za-z0-9_-]{11}$' OR )" +
			name + R"( ~ '^https://vimeo\.com/[0-9]{1,12}$')";
	}

}  // namespace DbSchema
