// The guide domain as the server sends it (GD2 §4's /api/guide/* endpoints).
// Field names mirror the wire, so a payload and its type read the same. Times
// are `_us`: microseconds since the epoch.
//
// Stream B's own types. The guide reads two things from Stream A's public
// events API (a venue's and a scene's upcoming events), but it reads them into
// its own GuideEvent shape here rather than importing Stream A's client code
// (GD3's cross-stream pin). That is also why the small coercion helpers below
// repeat events.types' rather than share them.
//
// The server's JSON has two quirks the normalize* functions smooth over: a
// date that was never set arrives as "" rather than null, and anything that
// might be absent is defaulted (`?? []`, `?? ''`), so pages never have to
// guard against a field that isn't there.

// GD1 §5's freshness statuses. closed and moved listings are "retired": out of
// the directories, but their own pages still load (a closed one is its
// graveyard entry).
export type GuideFreshnessStatus = 'open' | 'seasonal' | 'unverified' | 'closed' | 'moved';
export type GuideSceneCoverage = 'full' | 'link_out';
export type GuideNeighborhoodRegion =
  'seattle-core' | 'seattle-north' | 'seattle-south' | 'eastside' | 'south-king';
export type GuideOrganizationKind =
  | 'sports-league'
  | 'chorus-performance'
  | 'club-hobby'
  | 'recovery'
  | 'faith'
  | 'professional'
  | 'community-support'
  | 'social-meetup';
export type GuidePlaceKind = 'park-beach' | 'landmark' | 'historical';
export type GuideServiceKind =
  | 'sti-testing'
  | 'prep'
  | 'hiv-care'
  | 'primary-care'
  | 'mental-health'
  | 'gender-affirming'
  | 'dental'
  | 'everyday-service';
export type GuidePageSection =
  'scenes' | 'health' | 'services' | 'new-to-seattle' | 'freeze' | 'new-to' | 'adult';
export type GuideLinkPlatform =
  'discord' | 'bluesky' | 'instagram' | 'facebook-group' | 'meetup' | 'website' | 'other';

// What every listing carries to the public (GD2's Interface Contract 3): the
// "Verified {Month Year}" stamp and the closed marker render from these.
// null = never verified.
export interface GuideFreshness {
  status: GuideFreshnessStatus;
  last_verified_at_us: number | null;
}

// "Where this scene talks": an outbound link.
export interface GuideLink {
  id: number;
  label: string;
  url: string;
  platform: GuideLinkPlatform;
}

// A venue as the guide shows it: Stream A's venue joined with its guide
// profile. The URL identity is the profile's slug; venue_id is what an event
// page knows, which /guide/venues/by-id/:venueId resolves from the directory.
export interface GuideVenue extends GuideFreshness {
  venue_id: number;
  profile_id: number;
  slug: string;
  name: string;
  description: string;
  // One line, e.g. "1400 E Pine St, Seattle, WA 98122"; '' when unknown.
  address: string;
  url: string;
  neighborhood_slug: string;
  crowd_vibe: string;
  // '$' – '$$$'; '' = not stated.
  price_band: string;
  adult: boolean;
  closure_note: string;
  scene_tag_slugs: string[];
  // last_verified_at_us is the OLDER of these two (never verified if either
  // is): the page shows both kinds of fact, and the stamp mustn't vouch for
  // more than was checked (GD2 OQ11 (a)).
  venue_last_verified_at_us: number | null;
  profile_last_verified_at_us: number | null;
}

export interface GuideOrganization extends GuideFreshness {
  id: number;
  slug: string;
  name: string;
  org_kind: GuideOrganizationKind;
  description: string;
  url: string;
  meeting_cadence_text: string;
  // '' = countywide, or it moves around.
  neighborhood_slug: string;
  adult: boolean;
  closure_note: string;
  scene_tag_slugs: string[];
}

export interface GuidePlace extends GuideFreshness {
  id: number;
  slug: string;
  name: string;
  place_kind: GuidePlaceKind;
  description: string;
  address_text: string;
  neighborhood_slug: string;
  adult: boolean;
  scene_tag_slugs: string[];
}

// Services have no scene tags, links or detail page at MVP (GD3 §3.6).
export interface GuideService extends GuideFreshness {
  id: number;
  slug: string;
  name: string;
  service_kind: GuideServiceKind;
  description: string;
  url: string;
  phone: string;
  address_text: string;
  neighborhood_slug: string;
  // The MHMDA rendering flag: no person is ever shown recommending a health
  // listing.
  is_health: boolean;
}

// A guide page with its body. A public read only ever returns published
// pages; a manage_guide curator also gets unpublished ones (published: false)
// to preview.
export interface GuidePage extends GuideFreshness {
  id: number;
  slug: string;
  title: string;
  section: GuidePageSection;
  body_markdown: string;
  adult: boolean;
  draft: boolean;
  publish_at_us: number | null;
  published: boolean;
  scene_tag_slugs: string[];
}

// A section index entry: no body.
export interface GuidePageSummary extends GuideFreshness {
  id: number;
  slug: string;
  title: string;
  section: GuidePageSection;
  adult: boolean;
  publish_at_us: number | null;
}

// A scene for the scene grid. The counts are what its landing page lists.
export interface GuideScene {
  slug: string;
  display_name: string;
  description: string;
  coverage: GuideSceneCoverage;
  venue_count: number;
  organization_count: number;
  place_count: number;
}

// A scene landing. intro_page is the published `scenes` page whose slug is
// the scene's; null until one is written. A link_out scene gets the same
// payload, and the page decides what to render.
export interface GuideSceneDetail {
  scene: GuideScene;
  venues: GuideVenue[];
  organizations: GuideOrganization[];
  places: GuidePlace[];
  links: GuideLink[];
  intro_page: GuidePage | null;
}

export interface GuideVenueDetail {
  venue: GuideVenue;
  links: GuideLink[];
}

export interface GuideOrganizationDetail {
  organization: GuideOrganization;
  links: GuideLink[];
}

export interface GuidePlaceDetail {
  place: GuidePlace;
  links: GuideLink[];
}

export interface GuideNeighborhood {
  slug: string;
  display_name: string;
  region: GuideNeighborhoodRegion;
  venue_count: number;
  organization_count: number;
  place_count: number;
}

// A neighborhood page. rundown_page is the published `new-to-seattle` page
// whose slug is the neighborhood's; null until one is written.
export interface GuideNeighborhoodDetail {
  neighborhood: GuideNeighborhood;
  venues: GuideVenue[];
  organizations: GuideOrganization[];
  places: GuidePlace[];
  rundown_page: GuidePage | null;
}

// The closure graveyard: closed (not moved) venues and organizations.
export interface GuideGraveyard {
  venues: GuideVenue[];
  organizations: GuideOrganization[];
}

// An upcoming event, read from Stream A's public /api/events/upcoming for the
// "coming up" lists on venue and scene pages. Only the fields those lists
// show; the event's own page is Stream A's /events/:id.
export interface GuideEvent {
  id: number;
  title: string;
  starts_at_us: number;
  ends_at_us: number | null;
  venue_id: number | null;
  venue_name: string;
  // Where it is when there's no venue ("Cougar Mountain trailhead").
  location_text: string;
  cost_text: string;
  scene_tag_slugs: string[];
}

// Empty / unset = not filtered.
export interface GuideVenueFilters {
  scene?: string;
  neighborhood?: string;
  // Also closed and moved venues. The by-id resolver asks for them, so an old
  // event at a venue that has since closed still lands on its graveyard page.
  include_closed?: boolean;
}

export interface GuideOrganizationFilters {
  kind?: GuideOrganizationKind;
  scene?: string;
  neighborhood?: string;
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

function toBoolean(value: unknown): boolean {
  return value === true || value === 'true';
}

function toArray<T>(value: unknown, normalize: (item: unknown) => T): T[] {
  return Array.isArray(value) ? value.map(normalize) : [];
}

// A list endpoint's { items: [...] }.
function itemsOf<T>(value: unknown, normalize: (item: unknown) => T): T[] {
  return toArray(asRaw(value)['items'], normalize);
}

function freshnessOf(raw: Raw): GuideFreshness {
  return {
    status: toText(raw['status']) as GuideFreshnessStatus,
    last_verified_at_us: toOptionalNumber(raw['last_verified_at_us']),
  };
}

export function normalizeGuideLink(value: unknown): GuideLink {
  const raw = asRaw(value);
  return {
    id: toNumber(raw['id']),
    label: toText(raw['label']),
    url: toText(raw['url']),
    platform: toText(raw['platform']) as GuideLinkPlatform,
  };
}

export function normalizeGuideVenue(value: unknown): GuideVenue {
  const raw = asRaw(value);
  return {
    venue_id: toNumber(raw['venue_id']),
    profile_id: toNumber(raw['profile_id']),
    slug: toText(raw['slug']),
    name: toText(raw['name']),
    description: toText(raw['description']),
    address: toText(raw['address']),
    url: toText(raw['url']),
    neighborhood_slug: toText(raw['neighborhood_slug']),
    crowd_vibe: toText(raw['crowd_vibe']),
    price_band: toText(raw['price_band']),
    adult: toBoolean(raw['adult']),
    closure_note: toText(raw['closure_note']),
    scene_tag_slugs: toSlugs(raw['scene_tag_slugs']),
    ...freshnessOf(raw),
    venue_last_verified_at_us: toOptionalNumber(raw['venue_last_verified_at_us']),
    profile_last_verified_at_us: toOptionalNumber(raw['profile_last_verified_at_us']),
  };
}

export function normalizeGuideOrganization(value: unknown): GuideOrganization {
  const raw = asRaw(value);
  return {
    id: toNumber(raw['id']),
    slug: toText(raw['slug']),
    name: toText(raw['name']),
    org_kind: toText(raw['org_kind']) as GuideOrganizationKind,
    description: toText(raw['description']),
    url: toText(raw['url']),
    meeting_cadence_text: toText(raw['meeting_cadence_text']),
    neighborhood_slug: toText(raw['neighborhood_slug']),
    adult: toBoolean(raw['adult']),
    closure_note: toText(raw['closure_note']),
    scene_tag_slugs: toSlugs(raw['scene_tag_slugs']),
    ...freshnessOf(raw),
  };
}

export function normalizeGuidePlace(value: unknown): GuidePlace {
  const raw = asRaw(value);
  return {
    id: toNumber(raw['id']),
    slug: toText(raw['slug']),
    name: toText(raw['name']),
    place_kind: toText(raw['place_kind']) as GuidePlaceKind,
    description: toText(raw['description']),
    address_text: toText(raw['address_text']),
    neighborhood_slug: toText(raw['neighborhood_slug']),
    adult: toBoolean(raw['adult']),
    scene_tag_slugs: toSlugs(raw['scene_tag_slugs']),
    ...freshnessOf(raw),
  };
}

export function normalizeGuideService(value: unknown): GuideService {
  const raw = asRaw(value);
  return {
    id: toNumber(raw['id']),
    slug: toText(raw['slug']),
    name: toText(raw['name']),
    service_kind: toText(raw['service_kind']) as GuideServiceKind,
    description: toText(raw['description']),
    url: toText(raw['url']),
    phone: toText(raw['phone']),
    address_text: toText(raw['address_text']),
    neighborhood_slug: toText(raw['neighborhood_slug']),
    is_health: toBoolean(raw['is_health']),
    ...freshnessOf(raw),
  };
}

export function normalizeGuidePage(value: unknown): GuidePage {
  const raw = asRaw(value);
  return {
    id: toNumber(raw['id']),
    slug: toText(raw['slug']),
    title: toText(raw['title']),
    section: toText(raw['section']) as GuidePageSection,
    body_markdown: toText(raw['body_markdown']),
    adult: toBoolean(raw['adult']),
    draft: toBoolean(raw['draft']),
    publish_at_us: toOptionalNumber(raw['publish_at_us']),
    published: toBoolean(raw['published']),
    scene_tag_slugs: toSlugs(raw['scene_tag_slugs']),
    ...freshnessOf(raw),
  };
}

// null when the server sends null (no intro / rundown written yet).
function toOptionalPage(value: unknown): GuidePage | null {
  return value === null || value === undefined ? null : normalizeGuidePage(value);
}

export function normalizeGuidePageSummary(value: unknown): GuidePageSummary {
  const raw = asRaw(value);
  return {
    id: toNumber(raw['id']),
    slug: toText(raw['slug']),
    title: toText(raw['title']),
    section: toText(raw['section']) as GuidePageSection,
    adult: toBoolean(raw['adult']),
    publish_at_us: toOptionalNumber(raw['publish_at_us']),
    ...freshnessOf(raw),
  };
}

export function normalizeGuideScene(value: unknown): GuideScene {
  const raw = asRaw(value);
  return {
    slug: toText(raw['slug']),
    display_name: toText(raw['display_name']),
    description: toText(raw['description']),
    coverage: toText(raw['coverage']) as GuideSceneCoverage,
    venue_count: toNumber(raw['venue_count']),
    organization_count: toNumber(raw['organization_count']),
    place_count: toNumber(raw['place_count']),
  };
}

export function normalizeGuideNeighborhood(value: unknown): GuideNeighborhood {
  const raw = asRaw(value);
  return {
    slug: toText(raw['slug']),
    display_name: toText(raw['display_name']),
    region: toText(raw['region']) as GuideNeighborhoodRegion,
    venue_count: toNumber(raw['venue_count']),
    organization_count: toNumber(raw['organization_count']),
    place_count: toNumber(raw['place_count']),
  };
}

export function normalizeGuideEvent(value: unknown): GuideEvent {
  const raw = asRaw(value);
  return {
    id: toNumber(raw['id']),
    title: toText(raw['title']),
    starts_at_us: toNumber(raw['starts_at_us']),
    ends_at_us: toOptionalNumber(raw['ends_at_us']),
    venue_id: toOptionalNumber(raw['venue_id']),
    venue_name: toText(raw['venue_name']),
    location_text: toText(raw['location_text']),
    cost_text: toText(raw['cost_text']),
    scene_tag_slugs: toSlugs(raw['scene_tag_slugs']),
  };
}

// ---- whole replies ----

export function normalizeGuideScenes(value: unknown): GuideScene[] {
  return itemsOf(value, normalizeGuideScene);
}

export function normalizeGuideSceneDetail(value: unknown): GuideSceneDetail {
  const raw = asRaw(value);
  return {
    scene: normalizeGuideScene(raw['scene']),
    venues: toArray(raw['venues'], normalizeGuideVenue),
    organizations: toArray(raw['organizations'], normalizeGuideOrganization),
    places: toArray(raw['places'], normalizeGuidePlace),
    links: toArray(raw['links'], normalizeGuideLink),
    intro_page: toOptionalPage(raw['intro_page']),
  };
}

export function normalizeGuideVenues(value: unknown): GuideVenue[] {
  return itemsOf(value, normalizeGuideVenue);
}

export function normalizeGuideVenueDetail(value: unknown): GuideVenueDetail {
  const raw = asRaw(value);
  return {
    venue: normalizeGuideVenue(raw['venue']),
    links: toArray(raw['links'], normalizeGuideLink),
  };
}

export function normalizeGuideOrganizations(value: unknown): GuideOrganization[] {
  return itemsOf(value, normalizeGuideOrganization);
}

export function normalizeGuideOrganizationDetail(value: unknown): GuideOrganizationDetail {
  const raw = asRaw(value);
  return {
    organization: normalizeGuideOrganization(raw['organization']),
    links: toArray(raw['links'], normalizeGuideLink),
  };
}

export function normalizeGuidePlaces(value: unknown): GuidePlace[] {
  return itemsOf(value, normalizeGuidePlace);
}

export function normalizeGuidePlaceDetail(value: unknown): GuidePlaceDetail {
  const raw = asRaw(value);
  return {
    place: normalizeGuidePlace(raw['place']),
    links: toArray(raw['links'], normalizeGuideLink),
  };
}

export function normalizeGuideServices(value: unknown): GuideService[] {
  return itemsOf(value, normalizeGuideService);
}

export function normalizeGuideNeighborhoods(value: unknown): GuideNeighborhood[] {
  return itemsOf(value, normalizeGuideNeighborhood);
}

export function normalizeGuideNeighborhoodDetail(value: unknown): GuideNeighborhoodDetail {
  const raw = asRaw(value);
  return {
    neighborhood: normalizeGuideNeighborhood(raw['neighborhood']),
    venues: toArray(raw['venues'], normalizeGuideVenue),
    organizations: toArray(raw['organizations'], normalizeGuideOrganization),
    places: toArray(raw['places'], normalizeGuidePlace),
    rundown_page: toOptionalPage(raw['rundown_page']),
  };
}

export function normalizeGuideGraveyard(value: unknown): GuideGraveyard {
  const raw = asRaw(value);
  return {
    venues: toArray(raw['venues'], normalizeGuideVenue),
    organizations: toArray(raw['organizations'], normalizeGuideOrganization),
  };
}

export function normalizeGuidePageSummaries(value: unknown): GuidePageSummary[] {
  return itemsOf(value, normalizeGuidePageSummary);
}

// GET /api/events/upcoming → { items, from_us, to_us }; the guide keeps the items.
export function normalizeGuideEvents(value: unknown): GuideEvent[] {
  return itemsOf(value, normalizeGuideEvent);
}

// ---- query strings ----

type QueryValue = string | number | boolean | undefined;

// "" or "?scene=bear&…", in the order given, leaving out unset values. A yes/no
// is sent only when yes, as "true" (the server reads "true" or "1").
export function buildGuideQueryString(pairs: Array<[string, QueryValue]>): string {
  const parts: string[] = [];
  for (const [key, value] of pairs) {
    if (value === undefined || value === null || value === '' || value === false) {
      continue;
    }
    parts.push(`${key}=${encodeURIComponent(String(value))}`);
  }
  return parts.length > 0 ? `?${parts.join('&')}` : '';
}
