import { buildEventsQueryString, PublicEvent } from '../types/events.types';
import { EVENTS_TIME_ZONE, usToDate } from './events-date.utils';

// The addresses an event page links to: its own page, the one-event calendar
// download, the subscribe feed, and Google Calendar's quick-add. The ICS
// endpoints are EV2 §1's; the Google link is a plain templated URL (OQ1), no
// API involved.

// The subscribe feed's filters (/api/events/feed.ics): the list's, minus the
// window and the venue (EV2 OQ6 (d)).
export interface EventsFeedFilters {
  category?: string;
  scene?: string;
  neighborhood?: string;
  series_id?: number;
}

// An event with no end runs two hours, as in the ICS download
// (EV2 §1.1's kICalDefaultDurationUs), so both calendar paths agree.
export const CALENDAR_DEFAULT_DURATION_US = 2 * 3_600 * 1_000_000;

// Google puts the whole event in the URL; a long description is cut short.
const GOOGLE_DETAILS_MAX_LENGTH = 1_000;

export function eventPagePath(eventId: number): string {
  return `/events/${eventId}`;
}

export function seriesPagePath(slug: string): string {
  return `/events/series/${encodeURIComponent(slug)}`;
}

// GET /api/events/<id>/ical — approved events only.
export function eventIcsUrl(apiBase: string, eventId: number): string {
  return `${apiBase}/events/${eventId}/ical`;
}

// GET /api/events/feed.ics with the set filters, in the list's order.
export function eventsFeedPath(apiBase: string, filters: EventsFeedFilters): string {
  const query = buildEventsQueryString({
    category: filters.category,
    scene: filters.scene,
    neighborhood: filters.neighborhood,
    series_id: filters.series_id,
  });
  return `${apiBase}/events/feed.ics${query}`;
}

// A root-relative path ("/events/42") against the site's origin.
export function absoluteUrl(origin: string, path: string): string {
  return new URL(path, origin).toString();
}

// The same address as webcal://, which calendar apps open as a subscription
// (Apple Calendar, Outlook) instead of a one-time download.
export function webcalUrl(httpUrl: string): string {
  return httpUrl.replace(/^https?:/, 'webcal:');
}

// Where it is, as one line: "venue, address", the venue alone, or the
// free-text location. The same rule as the ICS LOCATION (EV2 §1.1).
export function eventLocationLine(event: PublicEvent): string {
  if (event.venue_name) {
    return event.venue_address ? `${event.venue_name}, ${event.venue_address}` : event.venue_name;
  }
  return event.location_text;
}

// 20260815T050000Z — Google's UTC form.
function googleUtc(us: number): string {
  return usToDate(us)
    .toISOString()
    .replace(/[-:]/g, '')
    .replace(/\.\d{3}Z$/, 'Z');
}

function trimmed(text: string, maxLength: number): string {
  return text.length <= maxLength ? text : `${text.slice(0, maxLength - 1).trimEnd()}…`;
}

// Google Calendar's "add this event" page, filled in. The details carry the
// description, the event's own link and its page here, like the ICS entry.
export function googleCalendarUrl(event: PublicEvent, pageUrl: string): string {
  const endsAtUs = event.ends_at_us ?? event.starts_at_us + CALENDAR_DEFAULT_DURATION_US;
  const details = [
    trimmed(event.description, GOOGLE_DETAILS_MAX_LENGTH),
    [event.url, pageUrl].filter(Boolean).join('\n'),
  ]
    .filter(Boolean)
    .join('\n\n');
  const params = new URLSearchParams({
    action: 'TEMPLATE',
    text: event.title,
    dates: `${googleUtc(event.starts_at_us)}/${googleUtc(endsAtUs)}`,
    ctz: EVENTS_TIME_ZONE,
  });
  if (details) {
    params.set('details', details);
  }
  const location = eventLocationLine(event);
  if (location) {
    params.set('location', location);
  }
  return `https://calendar.google.com/calendar/render?${params.toString()}`;
}
