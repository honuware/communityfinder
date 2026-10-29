import { normalizeEvent } from '../types/events.types';
import { buildEventJsonLd } from './event-json-ld';
import { pacificWallClockToUs } from './events-date.utils';

const CANONICAL = 'https://communityfinder.example/events/42';

describe('buildEventJsonLd', () => {
  it('describes a full event', () => {
    const event = normalizeEvent({
      id: 42,
      title: 'Queer Craft Fair',
      description: 'Thirty local makers.',
      starts_at_us: pacificWallClockToUs(2026, 8, 15, 14),
      ends_at_us: pacificWallClockToUs(2026, 8, 15, 18),
      venue_name: 'Columbia City Social Club',
      venue_address: '4800 Rainier Ave S, Seattle, WA 98118',
      cost_text: 'Free',
    });
    expect(buildEventJsonLd(event, CANONICAL, 'https://communityfinder.example/photo.jpg')).toEqual(
      {
        '@context': 'https://schema.org',
        '@type': 'Event',
        name: 'Queer Craft Fair',
        startDate: '2026-08-15T14:00:00-07:00',
        endDate: '2026-08-15T18:00:00-07:00',
        eventStatus: 'https://schema.org/EventScheduled',
        eventAttendanceMode: 'https://schema.org/OfflineEventAttendanceMode',
        location: {
          '@type': 'Place',
          name: 'Columbia City Social Club',
          address: '4800 Rainier Ave S, Seattle, WA 98118',
        },
        description: 'Thirty local makers.',
        image: ['https://communityfinder.example/photo.jpg'],
        url: CANONICAL,
        offers: { '@type': 'Offer', price: 0, priceCurrency: 'USD', url: CANONICAL },
      },
    );
  });

  it('describes a minimal event: no venue, no end, a cost that is not "Free"', () => {
    const event = normalizeEvent({
      id: 43,
      title: 'Sober Saturday Hike',
      starts_at_us: pacificWallClockToUs(2026, 12, 5, 10),
      location_text: 'Cougar Mountain, Red Town trailhead',
      cost_text: '$5 suggested',
    });
    expect(buildEventJsonLd(event, CANONICAL)).toEqual({
      '@context': 'https://schema.org',
      '@type': 'Event',
      name: 'Sober Saturday Hike',
      startDate: '2026-12-05T10:00:00-08:00',
      eventStatus: 'https://schema.org/EventScheduled',
      eventAttendanceMode: 'https://schema.org/OfflineEventAttendanceMode',
      location: {
        '@type': 'Place',
        name: 'Cougar Mountain, Red Town trailhead',
        address: 'Cougar Mountain, Red Town trailhead',
      },
      url: CANONICAL,
    });
  });

  it('structures only an exact "Free", never a price read from free text', () => {
    const at = (cost_text: string) =>
      buildEventJsonLd(
        normalizeEvent({ id: 1, title: 'T', starts_at_us: 1, cost_text }),
        CANONICAL,
      );
    expect(at(' free ')['offers']).toBeDefined();
    expect(at('Free with RSVP')['offers']).toBeUndefined();
    expect(at('$10')['offers']).toBeUndefined();
    expect(at('')['offers']).toBeUndefined();
  });

  it('leaves out the location when there is none', () => {
    const jsonLd = buildEventJsonLd(
      normalizeEvent({ id: 1, title: 'T', starts_at_us: 1 }),
      CANONICAL,
    );
    expect('location' in jsonLd).toBe(false);
  });
});
