import { VideoProvider, VideoRef } from '../types/events.types';

// CM3's video links on the client: reading a pasted link the way the server
// does (business_logic/events/video_link.cpp, ParseVideoLink), so the event
// editor and the submit form can say what they found before anything is
// sent, and building a player's address from a {provider, id} alone. The
// server parses again and has the last word; the database CHECK backs it up.

export const VIDEO_ERROR_BAD_URL = 'BAD_VIDEO_URL';
export const VIDEO_ERROR_HOST_NOT_SUPPORTED = 'VIDEO_HOST_NOT_SUPPORTED';

export type VideoLinkResult =
  { ok: true; video: VideoRef; canonicalUrl: string } | { ok: false; errorCode: string };

// What the stored column holds (db_schema/video_links.h).
const YOUTUBE_CANONICAL_PREFIX = 'https://www.youtube.com/watch?v=';
const VIMEO_CANONICAL_PREFIX = 'https://vimeo.com/';

const YOUTUBE_HOSTS = [
  'youtube.com',
  'www.youtube.com',
  'm.youtube.com',
  'youtube-nocookie.com',
  'www.youtube-nocookie.com',
];
const YOUTUBE_SHORT_HOST = 'youtu.be';
const VIMEO_HOSTS = ['vimeo.com', 'www.vimeo.com'];
const VIMEO_PLAYER_HOST = 'player.vimeo.com';
// The YouTube paths whose next segment is the id: /shorts/<id> and the like.
const YOUTUBE_ID_PATH_PREFIXES = ['shorts', 'live', 'embed', 'v'];

const YOUTUBE_ID = /^[A-Za-z0-9_-]{11}$/;
const VIMEO_ID = /^[0-9]{1,12}$/;
const HOST = /^[A-Za-z0-9.-]+$/;

export function isYoutubeId(id: string): boolean {
  return YOUTUBE_ID.test(id);
}

export function isVimeoId(id: string): boolean {
  return VIMEO_ID.test(id);
}

function failure(errorCode: string): VideoLinkResult {
  return { ok: false, errorCode };
}

function found(provider: VideoProvider, id: string): VideoLinkResult {
  if (provider === 'youtube' ? !isYoutubeId(id) : !isVimeoId(id)) {
    return failure(VIDEO_ERROR_BAD_URL);
  }
  const prefix = provider === 'youtube' ? YOUTUBE_CANONICAL_PREFIX : VIMEO_CANONICAL_PREFIX;
  return { ok: true, video: { provider, id }, canonicalUrl: prefix + id };
}

// The value of `name` in "a=1&b=2", or '' when it isn't there. Not decoded,
// as on the server: an id never needs it.
function queryValue(query: string, name: string): string {
  for (const pair of query.split('&')) {
    const equals = pair.indexOf('=');
    if (equals !== -1 && pair.slice(0, equals) === name) {
      return pair.slice(equals + 1);
    }
  }
  return '';
}

// A pasted link → its video and canonical form, or why not. Accepts https,
// http or no scheme; YouTube's watch, youtu.be, /shorts/, /live/, /embed/ and
// /v/ links (extra parameters such as &t= are dropped); Vimeo's
// vimeo.com/<number>, vimeo.com/channels/<name>/<number> and
// player.vimeo.com/video/<number>. Refuses playlists, channel, album,
// showcase and user pages, and unlisted Vimeo links.
export function parseVideoLink(text: string): VideoLinkResult {
  let rest = text.trim();
  // Blank, or space or control characters inside: not one link.
  // eslint-disable-next-line no-control-regex
  if (!rest || /[\s\x00-\x1f]/.test(rest)) {
    return failure(VIDEO_ERROR_BAD_URL);
  }
  const lower = rest.toLowerCase();
  if (lower.startsWith('https://')) {
    rest = rest.slice(8);
  } else if (lower.startsWith('http://')) {
    rest = rest.slice(7);
  } else if (rest.includes('://')) {
    // javascript:, ftp://, data: and the like.
    return failure(VIDEO_ERROR_BAD_URL);
  }

  const hostEnd = rest.search(/[/?#]/);
  const host = (hostEnd === -1 ? rest : rest.slice(0, hostEnd)).toLowerCase();
  rest = hostEnd === -1 ? '' : rest.slice(hostEnd);
  if (!host.includes('.') || !HOST.test(host)) {
    // No dot, or a user, a port, or other punctuation.
    return failure(VIDEO_ERROR_BAD_URL);
  }
  const hash = rest.indexOf('#');
  if (hash !== -1) {
    rest = rest.slice(0, hash);
  }
  const queryStart = rest.indexOf('?');
  const path = queryStart === -1 ? rest : rest.slice(0, queryStart);
  const query = queryStart === -1 ? '' : rest.slice(queryStart + 1);
  const segments = path.split('/').filter((segment) => segment !== '');

  if (host === YOUTUBE_SHORT_HOST) {
    return segments.length === 1 ? found('youtube', segments[0]) : failure(VIDEO_ERROR_BAD_URL);
  }
  if (YOUTUBE_HOSTS.includes(host)) {
    if (segments.length === 1 && segments[0] === 'watch') {
      // The v anywhere among the parameters; a playlist link with no v names
      // no single video.
      return found('youtube', queryValue(query, 'v'));
    }
    if (segments.length === 2 && YOUTUBE_ID_PATH_PREFIXES.includes(segments[0])) {
      return found('youtube', segments[1]);
    }
    return failure(VIDEO_ERROR_BAD_URL);
  }
  if (VIMEO_HOSTS.includes(host)) {
    if (segments.length === 1) {
      return found('vimeo', segments[0]);
    }
    if (segments.length === 3 && segments[0] === 'channels') {
      return found('vimeo', segments[2]);
    }
    return failure(VIDEO_ERROR_BAD_URL);
  }
  if (host === VIMEO_PLAYER_HOST) {
    return segments.length === 2 && segments[0] === 'video'
      ? found('vimeo', segments[1])
      : failure(VIDEO_ERROR_BAD_URL);
  }
  return failure(VIDEO_ERROR_HOST_NOT_SUPPORTED);
}

export function videoProviderName(provider: VideoProvider): string {
  return provider === 'youtube' ? 'YouTube' : 'Vimeo';
}

// What a form says under its video field as the link is typed: '' while it's
// blank, "YouTube video ✓" once it reads, or why it doesn't.
export interface VideoFieldFeedback {
  ok: boolean;
  message: string;
}

export function videoFieldFeedback(text: string): VideoFieldFeedback {
  if (!text.trim()) {
    return { ok: true, message: '' };
  }
  const result = parseVideoLink(text);
  if (result.ok) {
    return { ok: true, message: `${videoProviderName(result.video.provider)} video ✓` };
  }
  return { ok: false, message: videoErrorMessage(result.errorCode) };
}

export function videoErrorMessage(errorCode: string): string {
  return errorCode === VIDEO_ERROR_HOST_NOT_SUPPORTED
    ? 'Only YouTube and Vimeo videos can be shown. Put other links in the website field.'
    : "That isn't a link to one YouTube or Vimeo video. Copy the address of the video's own page.";
}

// The player's address, built from the id alone (OQ3): YouTube's no-cookie
// player and Vimeo's do-not-track one. Not autoplaying: the player is on the
// page from the start (CM3 OQ23), so it waits for the visitor's own press.
// '' for an id of the wrong shape, so nothing that isn't a bare id ever
// reaches an address.
export function videoEmbedUrl(video: VideoRef): string {
  if (video.provider === 'youtube') {
    return isYoutubeId(video.id) ? `https://www.youtube-nocookie.com/embed/${video.id}` : '';
  }
  return isVimeoId(video.id) ? `https://player.vimeo.com/video/${video.id}?dnt=1` : '';
}
