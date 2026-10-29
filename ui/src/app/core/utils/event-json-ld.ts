import { PublicEvent } from '../types/events.types';
import { formatPacificIso } from './events-date.utils';

// schema.org Event structured data for an event's page (EV2's JSON-LD pin):
// what Google reads to show an event in search results. Built here, injected
// by PageMeta. Only what the data actually says goes in: a cost is
// structured only when it is exactly "Free", never guessed from free text.

export type JsonLd = Record<string, unknown>;

function isFree(costText: string): boolean {
  return costText.trim().toLowerCase() === 'free';
}

// The venue as a Place; failing that, the free-text location. Google wants an
// address on every Place, so a bare location doubles as its own address.
function buildLocation(event: PublicEvent): JsonLd | null {
  if (event.venue_name) {
    return {
      '@type': 'Place',
      name: event.venue_name,
      ...(event.venue_address ? { address: event.venue_address } : {}),
    };
  }
  if (event.location_text) {
    return { '@type': 'Place', name: event.location_text, address: event.location_text };
  }
  return null;
}

export function buildEventJsonLd(
  event: PublicEvent,
  canonicalUrl: string,
  imageUrl?: string,
): JsonLd {
  const location = buildLocation(event);
  return {
    '@context': 'https://schema.org',
    '@type': 'Event',
    name: event.title,
    startDate: formatPacificIso(event.starts_at_us),
    ...(event.ends_at_us !== null ? { endDate: formatPacificIso(event.ends_at_us) } : {}),
    eventStatus: 'https://schema.org/EventScheduled',
    eventAttendanceMode: 'https://schema.org/OfflineEventAttendanceMode',
    ...(location ? { location } : {}),
    ...(event.description ? { description: event.description } : {}),
    ...(imageUrl ? { image: [imageUrl] } : {}),
    url: canonicalUrl,
    ...(isFree(event.cost_text)
      ? { offers: { '@type': 'Offer', price: 0, priceCurrency: 'USD', url: canonicalUrl } }
      : {}),
  };
}
