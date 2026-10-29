import {
  Category,
  EventSeries,
  EventStatus,
  Neighborhood,
  NeighborhoodRegion,
  PublicEvent,
  SceneCoverage,
  SceneTag,
  US_PER_DAY,
} from '../types/events.types';
import {
  pacificDateTime,
  pacificDayOfWeek,
  pacificWallClockToUs,
  tonightRangeUs,
  weekendRangeUs,
} from '../utils/events-date.utils';

// The offline events dataset for the `local` (mock) build: everything
// `ng serve` shows on the events pages with no server behind it, and the demo
// and screenshot data. CommunityMockAccess serves it with the server's rules
// (approved only, the same window and filters).
//
// The vocabularies are GD1's, copied verbatim from the server seed
// (business_logic/create_app_tables.cpp), so a filter that works here works
// against the real API. The venues, series and events are made up, and the
// events are generated relative to "now" so the demo never goes stale: a
// couple tonight, a weekend cluster, a weekly series, and a spread over the
// next weeks. One past event and one pending event are there to prove they
// never show.

const HOUR_US = 3_600 * 1_000_000;
const QUARTER_HOUR_US = 15 * 60 * 1_000_000;

// ---- the vocabularies (GD1 §1–§3, in display order) ----

const CATEGORY_SEEDS: Array<[string, string]> = [
  ['bar', 'Bar & club nights'],
  ['drag', 'Drag'],
  ['nightlife', 'Nightlife'],
  ['music', 'Music & concerts'],
  ['theatre', 'Theatre & film'],
  ['arts', 'Arts & culture'],
  ['pride', 'Pride & festivals'],
  ['sports', 'Sports & recreation'],
  ['outdoors', 'Outdoors'],
  ['volunteer', 'Volunteering'],
  ['protest', 'Protest & civic'],
  ['community', 'Community & social'],
];

const SCENE_TAG_SEEDS: Array<[string, string, SceneCoverage]> = [
  ['bear', 'Bears', 'full'],
  ['leather', 'Leather & kink', 'full'],
  ['jock', 'Jocks & sports', 'full'],
  ['sober', 'Sober', 'full'],
  ['elders', 'Elders', 'full'],
  ['geek', 'Geeks & gaymers', 'full'],
  ['furry', 'Furries', 'full'],
  ['bipoc', 'BIPOC', 'full'],
  ['sapphic', 'Sapphic', 'link_out'],
  ['trans', 'Trans', 'link_out'],
  ['qtpoc', 'QTPOC', 'link_out'],
];

const NEIGHBORHOOD_SEEDS: Array<[string, string, NeighborhoodRegion]> = [
  ['capitol-hill', 'Capitol Hill', 'seattle-core'],
  ['first-hill', 'First Hill', 'seattle-core'],
  ['central-district', 'Central District', 'seattle-core'],
  ['downtown', 'Downtown', 'seattle-core'],
  ['sodo', 'SODO', 'seattle-core'],
  ['queen-anne', 'Queen Anne', 'seattle-core'],
  ['u-district', 'U District', 'seattle-core'],
  ['wallingford', 'Wallingford', 'seattle-core'],
  ['ballard', 'Ballard', 'seattle-core'],
  ['fremont', 'Fremont', 'seattle-core'],
  ['beacon-hill', 'Beacon Hill', 'seattle-south'],
  ['columbia-city', 'Columbia City', 'seattle-south'],
  ['georgetown', 'Georgetown', 'seattle-south'],
  ['west-seattle', 'West Seattle', 'seattle-south'],
  ['white-center', 'White Center', 'seattle-south'],
  ['shoreline', 'Shoreline', 'seattle-north'],
  ['bellevue', 'Bellevue', 'eastside'],
  ['kirkland', 'Kirkland', 'eastside'],
  ['redmond', 'Redmond', 'eastside'],
  ['bothell', 'Bothell', 'eastside'],
  ['issaquah', 'Issaquah', 'eastside'],
  ['sammamish', 'Sammamish', 'eastside'],
  ['burien', 'Burien', 'south-king'],
  ['renton', 'Renton', 'south-king'],
  ['tukwila', 'Tukwila', 'south-king'],
];

// The seed steps sort_order by 10, so curators can slot new entries in.
const SORT_ORDER_STEP = 10;

export const MOCK_CATEGORIES: Category[] = CATEGORY_SEEDS.map(([slug, display_name], index) => ({
  id: index + 1,
  slug,
  display_name,
  description: '',
  sort_order: (index + 1) * SORT_ORDER_STEP,
}));

export const MOCK_SCENE_TAGS: SceneTag[] = SCENE_TAG_SEEDS.map(
  ([slug, display_name, coverage], index) => ({
    id: index + 1,
    slug,
    display_name,
    description: '',
    coverage,
    sort_order: (index + 1) * SORT_ORDER_STEP,
  }),
);

export const MOCK_NEIGHBORHOODS: Neighborhood[] = NEIGHBORHOOD_SEEDS.map(
  ([slug, display_name, region], index) => ({
    id: index + 1,
    slug,
    display_name,
    region,
    sort_order: (index + 1) * SORT_ORDER_STEP,
  }),
);

// ---- venues and series (made up) ----

export interface MockVenue {
  id: number;
  name: string;
  address: string;
  neighborhoodSlug: string;
}

const LANTERN_ROOM: MockVenue = {
  id: 1,
  name: 'The Lantern Room',
  address: '1400 E Pine St, Seattle, WA 98122',
  neighborhoodSlug: 'capitol-hill',
};
const DRIFTWOOD_HALL: MockVenue = {
  id: 2,
  name: 'Driftwood Hall',
  address: '5300 Ballard Ave NW, Seattle, WA 98107',
  neighborhoodSlug: 'ballard',
};
const COLUMBIA_CITY_SOCIAL: MockVenue = {
  id: 3,
  name: 'Columbia City Social Club',
  address: '4800 Rainier Ave S, Seattle, WA 98118',
  neighborhoodSlug: 'columbia-city',
};
const EASTSIDE_COMMONS: MockVenue = {
  id: 4,
  name: 'Eastside Commons',
  address: '200 Bellevue Way NE, Bellevue, WA 98004',
  neighborhoodSlug: 'bellevue',
};

function makeSeries(
  fields: Omit<EventSeries, 'venue_id' | 'venue_name' | 'neighborhood_slug'>,
  venue?: MockVenue,
): EventSeries {
  return {
    ...fields,
    venue_id: venue?.id ?? null,
    venue_name: venue?.name ?? '',
    neighborhood_slug: venue?.neighborhoodSlug ?? '',
  };
}

// ---- events ----

// An event plus its review status. The public never sees `status`; the mock
// uses it to serve approved events only, as the server does.
export interface MockEventRecord {
  status: EventStatus;
  event: PublicEvent;
}

interface EventSpec {
  id: number;
  title: string;
  startsAtUs: number;
  // No duration = no end time (the pages and the ICS feed handle that).
  durationHours?: number;
  venue?: MockVenue;
  locationText?: string;
  description?: string;
  url?: string;
  costText?: string;
  // The event's own slugs. Leave them out to inherit the series' (EV1's rule).
  categorySlugs?: string[];
  sceneTagSlugs?: string[];
  series?: EventSeries;
  status?: EventStatus;
}

function makeEvent(spec: EventSpec): MockEventRecord {
  const { venue, series } = spec;
  return {
    status: spec.status ?? 'approved',
    event: {
      id: spec.id,
      title: spec.title,
      description: spec.description ?? '',
      starts_at_us: spec.startsAtUs,
      ends_at_us: spec.durationHours ? spec.startsAtUs + spec.durationHours * HOUR_US : null,
      venue_id: venue?.id ?? null,
      venue_name: venue?.name ?? '',
      venue_address: venue?.address ?? '',
      neighborhood_slug: venue?.neighborhoodSlug ?? '',
      location_text: spec.locationText ?? '',
      url: spec.url ?? '',
      cost_text: spec.costText ?? '',
      category_slugs: spec.categorySlugs?.length
        ? spec.categorySlugs
        : [...(series?.category_slugs ?? [])],
      scene_tag_slugs: spec.sceneTagSlugs?.length
        ? spec.sceneTagSlugs
        : [...(series?.scene_tag_slugs ?? [])],
      series_id: series?.id ?? null,
      series_slug: series?.slug ?? '',
      series_title: series?.title ?? '',
      origin: 'manual',
    },
  };
}

export interface EventsMockData {
  categories: Category[];
  sceneTags: SceneTag[];
  neighborhoods: Neighborhood[];
  venues: MockVenue[];
  series: EventSeries[];
  events: MockEventRecord[];
}

// Up to the next quarter hour, so generated times read like real ones.
function ceilToQuarterHour(us: number): number {
  return Math.ceil(us / QUARTER_HOUR_US) * QUARTER_HOUR_US;
}

// The dataset as of `now` (`_us`). Pure: the same `now` builds the same data.
export function buildEventsMockData(now: number): EventsMockData {
  const today = pacificDateTime(now);
  const todayDayOfWeek = pacificDayOfWeek(now);
  // A Pacific wall-clock time `dayOffset` days from today.
  const at = (dayOffset: number, hour: number, minute = 0) =>
    pacificWallClockToUs(today.year, today.month, today.day + dayOffset, hour, minute);
  // Days from today to the next given weekday (0 = Sunday), today included.
  const daysUntil = (dayOfWeek: number) => (dayOfWeek - todayDayOfWeek + 7) % 7;

  const bearHappyHour = makeSeries(
    {
      id: 11,
      title: 'Bear Happy Hour',
      slug: 'bear-happy-hour',
      description: 'The weekly after-work hang for bears, cubs, otters and admirers.',
      cadence_text: 'Thursdays, 6–9 PM',
      url: 'https://example.org/bear-happy-hour',
      status: 'open',
      last_verified_at_us: now - 20 * US_PER_DAY,
      category_slugs: ['bar'],
      scene_tag_slugs: ['bear'],
    },
    LANTERN_ROOM,
  );
  const soberHike = makeSeries({
    id: 12,
    title: 'Sober Saturday Hike',
    slug: 'sober-saturday-hike',
    description: 'An easy group hike, no drinking before or after. Carpools from Capitol Hill.',
    cadence_text: 'One Saturday a month, spring through fall',
    url: 'https://example.org/sober-saturday-hike',
    status: 'seasonal',
    last_verified_at_us: null,
    category_slugs: ['outdoors'],
    scene_tag_slugs: ['sober'],
  });
  // Ended: off the series index, but its page still renders (with an "Ended"
  // marker), and it has no upcoming events.
  const dragBrunchClub = makeSeries(
    {
      id: 13,
      title: 'Drag Brunch Club',
      slug: 'drag-brunch-club',
      description: 'Sunday drag brunch. The last one was in the spring.',
      cadence_text: 'Sundays',
      url: '',
      status: 'closed',
      last_verified_at_us: now - 120 * US_PER_DAY,
      category_slugs: ['drag'],
      scene_tag_slugs: [],
    },
    DRIFTWOOD_HALL,
  );

  const events: MockEventRecord[] = [];

  // Tonight: two events inside tonight's window, whenever "now" is.
  const tonight = tonightRangeUs(now);
  const tonightLength = tonight.toUs - tonight.fromUs;
  events.push(
    makeEvent({
      id: 101,
      title: 'Karaoke Night',
      startsAtUs: ceilToQuarterHour(tonight.fromUs + tonightLength * 0.2),
      durationHours: 3,
      venue: LANTERN_ROOM,
      description: 'Sign up at the bar. Duets encouraged.',
      costText: 'Free',
      categorySlugs: ['bar', 'music'],
    }),
    makeEvent({
      id: 102,
      title: 'Late Show: Drag Revue',
      startsAtUs: ceilToQuarterHour(tonight.fromUs + tonightLength * 0.5),
      durationHours: 2,
      venue: DRIFTWOOD_HALL,
      description: 'Four queens, two kings, one very tired DJ.',
      url: 'https://example.org/drag-revue',
      costText: '$15',
      categorySlugs: ['drag', 'nightlife'],
    }),
  );

  // The weekend cluster, on this weekend's (or the coming weekend's) Friday to
  // Sunday. From inside the weekend, the ones already past drop out on their own.
  const weekendMonday = pacificDateTime(weekendRangeUs(now).toUs);
  const onWeekend = (dayAfterFriday: number, hour: number) =>
    pacificWallClockToUs(
      weekendMonday.year,
      weekendMonday.month,
      weekendMonday.day - 3 + dayAfterFriday,
      hour,
    );
  events.push(
    makeEvent({
      id: 103,
      title: 'Leather Night',
      startsAtUs: onWeekend(0, 21),
      durationHours: 5,
      venue: LANTERN_ROOM,
      description: 'Dress code encouraged, not enforced. Coat check at the door.',
      costText: '$10 cover',
      categorySlugs: ['bar'],
      sceneTagSlugs: ['leather'],
    }),
    makeEvent({
      id: 104,
      title: 'Queer Craft Fair',
      startsAtUs: onWeekend(1, 14),
      durationHours: 4,
      venue: COLUMBIA_CITY_SOCIAL,
      description: 'Thirty local makers, zines, prints and a button station.',
      costText: 'Free',
      categorySlugs: ['arts', 'community'],
      sceneTagSlugs: ['sapphic'],
    }),
    makeEvent({
      id: 105,
      title: 'Disco Inferno Dance Party',
      startsAtUs: onWeekend(1, 22),
      durationHours: 4,
      venue: DRIFTWOOD_HALL,
      costText: '$12',
      categorySlugs: ['nightlife', 'music'],
    }),
    makeEvent({
      id: 106,
      title: 'Drag Brunch',
      startsAtUs: onWeekend(2, 11),
      durationHours: 2,
      venue: EASTSIDE_COMMONS,
      description: 'Bottomless mimosas optional; reservations recommended.',
      url: 'https://example.org/eastside-drag-brunch',
      costText: '$35',
      categorySlugs: ['drag'],
    }),
  );

  // Bear Happy Hour: the next three Thursdays still ahead, inheriting the
  // series' bar / bear tags.
  const thursdays = [0, 1, 2, 3]
    .map((week) => at(daysUntil(4) + week * 7, 18))
    .filter((startsAtUs) => startsAtUs > now)
    .slice(0, 3);
  thursdays.forEach((startsAtUs, index) =>
    events.push(
      makeEvent({
        id: 107 + index,
        title: 'Bear Happy Hour',
        startsAtUs,
        durationHours: 3,
        venue: LANTERN_ROOM,
        costText: 'No cover',
        series: bearHappyHour,
      }),
    ),
  );

  // Sober Saturday Hike: next week's Saturday and four weeks after, with no
  // venue (a trailhead) and no set end.
  const firstHike = daysUntil(6) + 7;
  [firstHike, firstHike + 28].forEach((dayOffset, index) =>
    events.push(
      makeEvent({
        id: 110 + index,
        title: 'Sober Saturday Hike',
        startsAtUs: at(dayOffset, 10),
        locationText: 'Cougar Mountain, Red Town trailhead',
        url: 'https://example.org/sober-saturday-hike',
        costText: 'Free',
        series: soberHike,
      }),
    ),
  );

  // Spread over the coming weeks.
  events.push(
    makeEvent({
      id: 112,
      title: 'Board Game Night',
      startsAtUs: at(9, 19),
      venue: EASTSIDE_COMMONS,
      description: 'Bring a game or learn one. Snacks provided.',
      costText: 'Free',
      categorySlugs: ['community'],
      sceneTagSlugs: ['geek'],
    }),
    makeEvent({
      id: 113,
      title: 'Food Bank Volunteer Shift',
      startsAtUs: at(16, 9),
      durationHours: 3,
      locationText: 'Community food bank, Rainier Valley',
      description: 'Sorting and packing. Closed-toe shoes, please.',
      costText: 'Free',
      categorySlugs: ['volunteer'],
    }),
    makeEvent({
      id: 114,
      title: 'Queer Shorts Film Night',
      startsAtUs: at(25, 19, 30),
      durationHours: 2,
      venue: COLUMBIA_CITY_SOCIAL,
      description: 'Eight short films by Pacific Northwest trans and nonbinary filmmakers.',
      costText: '$8 suggested',
      categorySlugs: ['theatre', 'arts'],
      sceneTagSlugs: ['trans'],
    }),
    // Past the list's default 42 days: on the calendar, not the list.
    makeEvent({
      id: 115,
      title: 'Pride Planning Picnic',
      startsAtUs: at(50, 12),
      durationHours: 3,
      locationText: 'Volunteer Park, near the water tower',
      costText: 'Free',
      categorySlugs: ['pride', 'community'],
    }),
  );

  // Never shown: one already over, one not yet reviewed.
  events.push(
    makeEvent({
      id: 116,
      title: 'Trivia Night',
      startsAtUs: now - 3 * US_PER_DAY,
      durationHours: 2,
      venue: LANTERN_ROOM,
      categorySlugs: ['bar'],
    }),
    makeEvent({
      id: 117,
      title: 'Open Mic (awaiting review)',
      startsAtUs: at(1, 20),
      durationHours: 2,
      venue: DRIFTWOOD_HALL,
      categorySlugs: ['music'],
      status: 'pending',
    }),
  );

  return {
    categories: MOCK_CATEGORIES,
    sceneTags: MOCK_SCENE_TAGS,
    neighborhoods: MOCK_NEIGHBORHOODS,
    venues: [LANTERN_ROOM, DRIFTWOOD_HALL, COLUMBIA_CITY_SOCIAL, EASTSIDE_COMMONS],
    series: [bearHappyHour, soberHike, dragBrunchClub],
    events,
  };
}
