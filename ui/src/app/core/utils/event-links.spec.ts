import { normalizeEvent, PublicEvent } from '../types/events.types';
import {
  absoluteUrl,
  eventIcsUrl,
  eventLocationLine,
  eventPagePath,
  eventsFeedPath,
  googleCalendarUrl,
  seriesPagePath,
  webcalUrl,
} from './event-links';
import { pacificWallClockToUs } from './events-date.utils';

// Fri Aug 14, 2026, 10:00 PM PDT = Sat 05:00 UTC.
const STARTS_AT_US = pacificWallClockToUs(2026, 8, 14, 22);
const HOUR_US = 3_600 * 1_000_000;

function event(fields: Partial<PublicEvent> = {}): PublicEvent {
  return {
    ...normalizeEvent({
      id: 42,
      title: 'Drag & Disco',
      description: 'Four queens, one DJ.',
      starts_at_us: STARTS_AT_US,
      ends_at_us: STARTS_AT_US + 3 * HOUR_US,
      venue_name: 'The Lantern Room',
      venue_address: '1400 E Pine St, Seattle, WA 98122',
      url: 'https://example.org/disco',
    }),
    ...fields,
  };
}

function googleParams(url: string): URLSearchParams {
  expect(url.startsWith('https://calendar.google.com/calendar/render?')).toBe(true);
  return new URL(url).searchParams;
}

describe('event-links', () => {
  it('builds the page, series and download addresses', () => {
    expect(eventPagePath(42)).toBe('/events/42');
    expect(seriesPagePath('bear-happy-hour')).toBe('/events/series/bear-happy-hour');
    expect(eventIcsUrl('/api', 42)).toBe('/api/events/42/ical');
    expect(absoluteUrl('https://communityfinder.example', '/events/42')).toBe(
      'https://communityfinder.example/events/42',
    );
  });

  it('builds the feed address from the set filters, in the list order', () => {
    expect(eventsFeedPath('/api', {})).toBe('/api/events/feed.ics');
    expect(
      eventsFeedPath('/api', { neighborhood: 'capitol-hill', category: 'drag', scene: '' }),
    ).toBe('/api/events/feed.ics?category=drag&neighborhood=capitol-hill');
    expect(eventsFeedPath('/api', { series_id: 11 })).toBe('/api/events/feed.ics?series_id=11');
  });

  it('turns an http(s) address into a webcal:// subscription', () => {
    expect(webcalUrl('https://example.org/api/events/feed.ics?scene=bear')).toBe(
      'webcal://example.org/api/events/feed.ics?scene=bear',
    );
    expect(webcalUrl('http://localhost:4201/api/events/feed.ics')).toBe(
      'webcal://localhost:4201/api/events/feed.ics',
    );
  });

  it('writes the location as the ICS feed does', () => {
    expect(eventLocationLine(event())).toBe('The Lantern Room, 1400 E Pine St, Seattle, WA 98122');
    expect(eventLocationLine(event({ venue_address: '' }))).toBe('The Lantern Room');
    expect(
      eventLocationLine(
        event({ venue_name: '', venue_address: '', location_text: 'Volunteer Park' }),
      ),
    ).toBe('Volunteer Park');
  });

  describe('googleCalendarUrl', () => {
    it('fills in the title, UTC times, zone, details and location, encoded', () => {
      const url = googleCalendarUrl(event(), 'https://example.org/events/42');
      // "&" in the title must not start a new parameter.
      expect(url).toContain('text=Drag+%26+Disco');
      const params = googleParams(url);
      expect(params.get('action')).toBe('TEMPLATE');
      expect(params.get('text')).toBe('Drag & Disco');
      expect(params.get('dates')).toBe('20260815T050000Z/20260815T080000Z');
      expect(params.get('ctz')).toBe('America/Los_Angeles');
      expect(params.get('details')).toBe(
        'Four queens, one DJ.\n\nhttps://example.org/disco\nhttps://example.org/events/42',
      );
      expect(params.get('location')).toBe('The Lantern Room, 1400 E Pine St, Seattle, WA 98122');
    });

    it('gives an event with no end two hours, like the ICS download', () => {
      const params = googleParams(googleCalendarUrl(event({ ends_at_us: null }), ''));
      expect(params.get('dates')).toBe('20260815T050000Z/20260815T070000Z');
    });

    it('leaves out what an event does not have', () => {
      const bare = event({ description: '', url: '', venue_name: '', venue_address: '' });
      const params = googleParams(googleCalendarUrl(bare, ''));
      expect(params.has('details')).toBe(false);
      expect(params.has('location')).toBe(false);
    });

    it('cuts a long description short', () => {
      const params = googleParams(googleCalendarUrl(event({ description: 'x'.repeat(5000) }), ''));
      expect(params.get('details')!.length).toBeLessThan(1100);
    });
  });
});
