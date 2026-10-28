import { DataResults } from '@honuware/ui/access';

// The events domain as the server sends it (EV1 §5's endpoints, plus the
// vocabulary tables read through the framework's generic table read). Field
// names mirror the wire, so a payload and its type read the same. Times are
// `_us`: microseconds since the epoch, real UTC instants (EV1's time pin);
// core/utils/events-date.utils owns turning them into Pacific dates and times.
//
// The server's JSON has two quirks the normalize* functions below smooth over:
// a missing end / venue / series / verification time arrives as "" rather than
// null, and anything that might be absent is defaulted (`?? []`, `?? ''`), so
// pages never have to guard against a field that isn't there.

export type EventStatus = 'pending' | 'approved' | 'rejected' | 'archived';
export type EventOrigin = 'scanned' | 'manual' | 'user_submitted';
// A series' freshness (GD1 §5). closed / moved series are left out of the
// series index but keep their page.
export type FreshnessStatus = 'open' | 'seasonal' | 'unverified' | 'closed' | 'moved';
export type SceneCoverage = 'full' | 'link_out';
export type NeighborhoodRegion =
  'seattle-core' | 'seattle-north' | 'seattle-south' | 'eastside' | 'south-king';

// An approved event (GET /api/events/upcoming, /api/events/<id>,
// /api/event_series/<slug>). The slug lists are the EFFECTIVE ones: the
// event's own, or its series' when it has none.
export interface PublicEvent {
  id: number;
  title: string;
  description: string;
  starts_at_us: number;
  ends_at_us: number | null;
  venue_id: number | null;
  venue_name: string;
  // One line, e.g. "1533 13th Ave, Seattle, WA 98122"; '' without a venue.
  venue_address: string;
  // The venue's neighborhood; '' when there's no venue or it has none.
  neighborhood_slug: string;
  // Where it is when there's no venue ("Volunteer Park amphitheater").
  location_text: string;
  // The event's own page (the organizer's or venue's), not ours.
  url: string;
  cost_text: string;
  category_slugs: string[];
  scene_tag_slugs: string[];
  series_id: number | null;
  series_slug: string;
  series_title: string;
  origin: EventOrigin;
  // Only for a manage_events curator, who can preview an event that isn't
  // approved yet; absent for everyone else.
  status?: EventStatus;
  review_notes?: string;
}

// GET /api/events/upcoming — the events plus the window the server actually
// served after clamping it, so a client paging through months knows where the
// next one starts.
export interface UpcomingEvents {
  items: PublicEvent[];
  from_us: number;
  to_us: number;
}

// The upcoming list's query string. Every field is optional: no window means
// now → +42 days, and a missing filter means "not filtered". The ICS feed
// (/api/events/feed.ics) takes the same filter names, minus venue_id.
export interface UpcomingEventsQuery {
  from_us?: number;
  to_us?: number;
  category?: string;
  scene?: string;
  neighborhood?: string;
  venue_id?: number;
  series_id?: number;
}

// The server's window pins (EV1 behavior pin), for clients that page by month.
export const UPCOMING_DEFAULT_WINDOW_DAYS = 42;
export const UPCOMING_MAX_WINDOW_DAYS = 92;
export const UPCOMING_LOOK_BACK_DAYS = 1;
export const US_PER_DAY = 86_400 * 1_000_000;

// A recurring series (GET /api/event_series, /api/event_series/<slug>). The
// slug lists are the series' own, which its events inherit.
export interface EventSeries {
  id: number;
  title: string;
  slug: string;
  description: string;
  cadence_text: string;
  url: string;
  venue_id: number | null;
  venue_name: string;
  neighborhood_slug: string;
  status: FreshnessStatus;
  last_verified_at_us: number | null;
  category_slugs: string[];
  scene_tag_slugs: string[];
}

// GET /api/event_series/<slug>: the series plus its upcoming approved events
// in the default window.
export interface EventSeriesDetail {
  series: EventSeries;
  upcoming: PublicEvent[];
}

// The shared vocabularies (GD1 §1–§3). Read through the framework's generic
// table read, so they arrive as table rows; CommunityAccess returns only the
// enabled ones, in display order.
export interface Category {
  id: number;
  slug: string;
  display_name: string;
  description: string;
  sort_order: number;
}

export interface SceneTag {
  id: number;
  slug: string;
  display_name: string;
  description: string;
  // full = curated here; link_out = shared anchors here, the rest linked out.
  coverage: SceneCoverage;
  sort_order: number;
}

export interface Neighborhood {
  id: number;
  slug: string;
  display_name: string;
  region: NeighborhoodRegion;
  sort_order: number;
}

// ---- normalizers: wire JSON → the types above ----

type Raw = Record<string, unknown>;

function asRaw(value: unknown): Raw {
  return value !== null && typeof value === 'object' ? (value as Raw) : {};
}

function toNumber(value: unknown): number {
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : 0;
}

// "" is the server's "none", like null and a missing field.
function toOptionalNumber(value: unknown): number | null {
  if (value === null || value === undefined || value === '') {
    return null;
  }
  const parsed = Number(value);
  return Number.isFinite(parsed) ? parsed : null;
}

function toText(value: unknown): string {
  return value === null || value === undefined ? '' : String(value);
}

function toSlugs(value: unknown): string[] {
  return Array.isArray(value) ? value.map(String).filter((slug) => slug !== '') : [];
}

// Table rows carry booleans as Postgres text ("t" / "f").
function toBoolean(value: unknown): boolean {
  return value === true || value === 't' || value === 'true' || value === '1' || value === 1;
}

export function normalizeEvent(value: unknown): PublicEvent {
  const raw = asRaw(value);
  const event: PublicEvent = {
    id: toNumber(raw['id']),
    title: toText(raw['title']),
    description: toText(raw['description']),
    starts_at_us: toNumber(raw['starts_at_us']),
    ends_at_us: toOptionalNumber(raw['ends_at_us']),
    venue_id: toOptionalNumber(raw['venue_id']),
    venue_name: toText(raw['venue_name']),
    venue_address: toText(raw['venue_address']),
    neighborhood_slug: toText(raw['neighborhood_slug']),
    location_text: toText(raw['location_text']),
    url: toText(raw['url']),
    cost_text: toText(raw['cost_text']),
    category_slugs: toSlugs(raw['category_slugs']),
    scene_tag_slugs: toSlugs(raw['scene_tag_slugs']),
    series_id: toOptionalNumber(raw['series_id']),
    series_slug: toText(raw['series_slug']),
    series_title: toText(raw['series_title']),
    origin: toText(raw['origin']) as EventOrigin,
  };
  if (raw['status'] !== undefined) {
    event.status = toText(raw['status']) as EventStatus;
    event.review_notes = toText(raw['review_notes']);
  }
  return event;
}

export function normalizeEvents(value: unknown): PublicEvent[] {
  return Array.isArray(value) ? value.map(normalizeEvent) : [];
}

export function normalizeUpcomingEvents(value: unknown): UpcomingEvents {
  const raw = asRaw(value);
  return {
    items: normalizeEvents(raw['items']),
    from_us: toNumber(raw['from_us']),
    to_us: toNumber(raw['to_us']),
  };
}

export function normalizeEventSeries(value: unknown): EventSeries {
  const raw = asRaw(value);
  return {
    id: toNumber(raw['id']),
    title: toText(raw['title']),
    slug: toText(raw['slug']),
    description: toText(raw['description']),
    cadence_text: toText(raw['cadence_text']),
    url: toText(raw['url']),
    venue_id: toOptionalNumber(raw['venue_id']),
    venue_name: toText(raw['venue_name']),
    neighborhood_slug: toText(raw['neighborhood_slug']),
    status: toText(raw['status']) as FreshnessStatus,
    last_verified_at_us: toOptionalNumber(raw['last_verified_at_us']),
    category_slugs: toSlugs(raw['category_slugs']),
    scene_tag_slugs: toSlugs(raw['scene_tag_slugs']),
  };
}

// GET /api/event_series → { items: [...] }.
export function normalizeEventSeriesList(value: unknown): EventSeries[] {
  const items = asRaw(value)['items'];
  return Array.isArray(items) ? items.map(normalizeEventSeries) : [];
}

export function normalizeEventSeriesDetail(value: unknown): EventSeriesDetail {
  const raw = asRaw(value);
  return {
    series: normalizeEventSeries(raw['series']),
    upcoming: normalizeEvents(raw['upcoming']),
  };
}

// The generic table read's { sortedColumnNames, dataTable } as one record per
// row, keyed by column name.
export function dataResultsToRecords(results: DataResults): Record<string, string>[] {
  const columns = results?.sortedColumnNames ?? [];
  return (results?.dataTable ?? []).map((row) => {
    const record: Record<string, string> = {};
    columns.forEach((column, index) => (record[column] = row[index] ?? ''));
    return record;
  });
}

// A vocabulary's rows → the enabled entries in display order (sort_order,
// then id — the server's own order for its pickers).
function enabledInDisplayOrder<T extends { id: number; sort_order: number }>(
  results: DataResults,
  normalize: (record: Record<string, string>) => T,
): T[] {
  return dataResultsToRecords(results)
    .filter((record) => toBoolean(record['enabled']))
    .map(normalize)
    .sort((a, b) => a.sort_order - b.sort_order || a.id - b.id);
}

export function categoriesFromRows(results: DataResults): Category[] {
  return enabledInDisplayOrder(results, (record) => ({
    id: toNumber(record['id']),
    slug: toText(record['slug']),
    display_name: toText(record['display_name']),
    description: toText(record['description']),
    sort_order: toNumber(record['sort_order']),
  }));
}

export function sceneTagsFromRows(results: DataResults): SceneTag[] {
  return enabledInDisplayOrder(results, (record) => ({
    id: toNumber(record['id']),
    slug: toText(record['slug']),
    display_name: toText(record['display_name']),
    description: toText(record['description']),
    coverage: toText(record['coverage']) as SceneCoverage,
    sort_order: toNumber(record['sort_order']),
  }));
}

export function neighborhoodsFromRows(results: DataResults): Neighborhood[] {
  return enabledInDisplayOrder(results, (record) => ({
    id: toNumber(record['id']),
    slug: toText(record['slug']),
    display_name: toText(record['display_name']),
    region: toText(record['region']) as NeighborhoodRegion,
    sort_order: toNumber(record['sort_order']),
  }));
}

// The upcoming list's query string ("" or "?category=drag&…"), in a fixed
// order, leaving out unset fields. 0 counts as unset, as it does on the server.
export function buildEventsQueryString(query: UpcomingEventsQuery): string {
  const pairs: string[] = [];
  const add = (key: string, value: string | number | undefined) => {
    if (value === undefined || value === null || value === '' || value === 0) {
      return;
    }
    pairs.push(`${key}=${encodeURIComponent(String(value))}`);
  };
  add('from_us', query.from_us);
  add('to_us', query.to_us);
  add('category', query.category);
  add('scene', query.scene);
  add('neighborhood', query.neighborhood);
  add('venue_id', query.venue_id);
  add('series_id', query.series_id);
  return pairs.length > 0 ? `?${pairs.join('&')}` : '';
}
