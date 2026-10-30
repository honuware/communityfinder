import {
  GuideEvent,
  GuideFreshnessStatus,
  GuideLink,
  GuideLinkPlatform,
  GuideNeighborhoodRegion,
  GuideOrganization,
  GuidePage,
  GuidePlace,
  GuideSceneCoverage,
  GuideService,
  GuideVenue,
} from '../types/guide.types';

// The offline guide dataset for the `local` (mock) build: everything
// `ng serve` shows on the guide pages with no server behind it. GuideMockAccess
// serves it by GD2's rules (retired listings out of the lists, published pages
// only, counts that match the lists).
//
// The vocabularies are GD1's, copied verbatim from the server seed
// (business_logic/create_app_tables.cpp). Everything else is made up, except
// Denny Blaine, GD1's example of an 18+ place, which the mock names but says
// nothing about. Venues 1–4 are the events mock's venues (same ids and names),
// so offline an event's venue link resolves through /guide/venues/by-id/:id.
// Stream B keeps its own copy rather than importing Stream A's mock.
//
// Times are relative to "now", so the "Verified" stamps and the coming-up
// lists never go stale. One draft page and one page scheduled for next week
// are there to prove they never show.

const US_PER_DAY = 86_400 * 1_000_000;
const US_PER_HOUR = 3_600 * 1_000_000;

// ---- the vocabularies (GD1 §2–§3, in display order) ----

export interface GuideMockSceneTag {
  slug: string;
  display_name: string;
  description: string;
  coverage: GuideSceneCoverage;
}

export interface GuideMockNeighborhood {
  slug: string;
  display_name: string;
  region: GuideNeighborhoodRegion;
}

const SCENE_TAG_SEEDS: Array<[string, string, GuideSceneCoverage]> = [
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

const NEIGHBORHOOD_SEEDS: Array<[string, string, GuideNeighborhoodRegion]> = [
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

// ---- the dataset's shape ----

export interface GuideMockScene {
  scene: GuideMockSceneTag;
  links: GuideLink[];
}

export interface GuideMockVenue {
  venue: GuideVenue;
  links: GuideLink[];
}

export interface GuideMockOrganization {
  organization: GuideOrganization;
  links: GuideLink[];
}

export interface GuideMockPlace {
  place: GuidePlace;
  links: GuideLink[];
}

export interface GuideMockData {
  scenes: GuideMockScene[];
  neighborhoods: GuideMockNeighborhood[];
  venues: GuideMockVenue[];
  organizations: GuideMockOrganization[];
  places: GuideMockPlace[];
  services: GuideService[];
  // Every page, whatever its state; `published` is as of the `now` the data
  // was built for. GuideMockAccess re-applies the rule on every call.
  pages: GuidePage[];
  // Approved upcoming events, as Stream A's /api/events/upcoming would send
  // them; one already over and one too far out to be in the default window.
  events: GuideEvent[];
}

// The publish rule, as GD2's guide_pages SQL spells it: not a draft, a publish
// time, and that time has arrived.
export function isGuidePagePublished(
  page: Pick<GuidePage, 'draft' | 'publish_at_us'>,
  now: number,
): boolean {
  return !page.draft && page.publish_at_us !== null && page.publish_at_us <= now;
}

// The older of two verifications, or null if either never happened (GD2
// OQ11 (a)): a guide venue's stamp.
function olderVerification(first: number | null, second: number | null): number | null {
  return first === null || second === null ? null : Math.min(first, second);
}

// The dataset as of `now` (`_us`). Pure: the same `now` builds the same data.
export function buildGuideMockData(now: number): GuideMockData {
  let nextLinkId = 1;
  const link = (label: string, platform: GuideLinkPlatform, url: string): GuideLink => ({
    id: nextLinkId++,
    label,
    url,
    platform,
  });
  const daysAgo = (days: number) => now - days * US_PER_DAY;
  // 03:00 UTC on the UTC day `days` from now: an evening in Seattle (7 or 8
  // PM the day before), and after "now" for any days >= 1.
  const evening = (days: number) =>
    Math.floor(now / US_PER_DAY) * US_PER_DAY + days * US_PER_DAY + 3 * US_PER_HOUR;

  // ---- scenes: links on the link-out scenes, whose landing is mostly links ----

  const sceneLinks: Record<string, GuideLink[]> = {
    leather: [link('Leather scene chat', 'discord', 'https://example.org/leather-chat')],
    sapphic: [
      link('Sapphic Seattle calendar', 'website', 'https://example.org/sapphic-calendar'),
      link('Sapphic meetups', 'meetup', 'https://example.org/sapphic-meetups'),
    ],
    trans: [
      link('Trans community Discord', 'discord', 'https://example.org/trans-discord'),
      link('Trans resource list', 'website', 'https://example.org/trans-resources'),
    ],
    qtpoc: [link('QTPOC collective', 'instagram', 'https://example.org/qtpoc')],
  };
  const scenes: GuideMockScene[] = SCENE_TAG_SEEDS.map(([slug, display_name, coverage]) => ({
    scene: { slug, display_name, description: '', coverage },
    links: sceneLinks[slug] ?? [],
  }));

  const neighborhoods: GuideMockNeighborhood[] = NEIGHBORHOOD_SEEDS.map(
    ([slug, display_name, region]) => ({ slug, display_name, region }),
  );

  // ---- venues ----

  interface VenueSpec {
    venueId: number;
    slug: string;
    name: string;
    description: string;
    address: string;
    neighborhood: string;
    crowdVibe: string;
    priceBand: string;
    sceneTags: string[];
    venueVerifiedAtUs: number | null;
    profileVerifiedAtUs: number | null;
    status?: GuideFreshnessStatus;
    closureNote?: string;
    adult?: boolean;
    url?: string;
  }
  const venue = (spec: VenueSpec): GuideVenue => ({
    venue_id: spec.venueId,
    profile_id: 200 + spec.venueId,
    slug: spec.slug,
    name: spec.name,
    description: spec.description,
    address: spec.address,
    url: spec.url ?? '',
    neighborhood_slug: spec.neighborhood,
    crowd_vibe: spec.crowdVibe,
    price_band: spec.priceBand,
    adult: spec.adult ?? false,
    closure_note: spec.closureNote ?? '',
    scene_tag_slugs: spec.sceneTags,
    status: spec.status ?? 'open',
    last_verified_at_us: olderVerification(spec.venueVerifiedAtUs, spec.profileVerifiedAtUs),
    venue_last_verified_at_us: spec.venueVerifiedAtUs,
    profile_last_verified_at_us: spec.profileVerifiedAtUs,
  });

  const venues: GuideMockVenue[] = [
    {
      venue: venue({
        venueId: 1,
        slug: 'the-lantern-room',
        name: 'The Lantern Room',
        description: 'A neighborhood bar with a back room for events.',
        address: '1400 E Pine St, Seattle, WA 98122',
        neighborhood: 'capitol-hill',
        crowdVibe: 'Bears and their friends after work; louder and younger after 10.',
        priceBand: '$$',
        sceneTags: ['bear'],
        venueVerifiedAtUs: daysAgo(20),
        profileVerifiedAtUs: daysAgo(35),
        url: 'https://example.org/lantern-room',
      }),
      links: [link('The Lantern Room on Instagram', 'instagram', 'https://example.org/lantern-ig')],
    },
    {
      // Its profile has never been verified, so its stamp says so.
      venue: venue({
        venueId: 2,
        slug: 'driftwood-hall',
        name: 'Driftwood Hall',
        description: 'A dance hall with a stage and a big floor.',
        address: '5300 Ballard Ave NW, Seattle, WA 98107',
        neighborhood: 'ballard',
        crowdVibe: 'Mixed crowd, here to dance. Drag on weekends.',
        priceBand: '$$',
        sceneTags: [],
        venueVerifiedAtUs: daysAgo(45),
        profileVerifiedAtUs: null,
      }),
      links: [],
    },
    {
      venue: venue({
        venueId: 3,
        slug: 'columbia-city-social-club',
        name: 'Columbia City Social Club',
        description: 'A community space with a café counter.',
        address: '4800 Rainier Ave S, Seattle, WA 98118',
        neighborhood: 'columbia-city',
        crowdVibe: 'Daytime makers and readers; craft fairs and film nights.',
        priceBand: '$',
        sceneTags: ['sapphic', 'bipoc'],
        venueVerifiedAtUs: daysAgo(60),
        profileVerifiedAtUs: daysAgo(10),
      }),
      links: [link('Events calendar', 'website', 'https://example.org/ccsc-calendar')],
    },
    {
      venue: venue({
        venueId: 4,
        slug: 'eastside-commons',
        name: 'Eastside Commons',
        description: 'A community room and café.',
        address: '200 Bellevue Way NE, Bellevue, WA 98004',
        neighborhood: 'bellevue',
        crowdVibe: 'Board gamers on weeknights, brunch on Sundays.',
        priceBand: '$',
        sceneTags: ['geek'],
        venueVerifiedAtUs: daysAgo(5),
        profileVerifiedAtUs: daysAgo(5),
      }),
      links: [],
    },
    {
      // In the graveyard: its page still loads, with the closure note.
      venue: venue({
        venueId: 5,
        slug: 'the-wren',
        name: 'The Wren',
        description: 'A small bar with a patio.',
        address: '1100 E Union St, Seattle, WA 98122',
        neighborhood: 'capitol-hill',
        crowdVibe: 'The bear bar before the bear bars.',
        priceBand: '$',
        sceneTags: ['bear'],
        venueVerifiedAtUs: daysAgo(200),
        profileVerifiedAtUs: daysAgo(200),
        status: 'closed',
        closureNote: 'Closed after twelve years when the building was sold.',
      }),
      links: [],
    },
    {
      venue: venue({
        venueId: 6,
        slug: 'iron-and-oak',
        name: 'Iron & Oak',
        description: 'A leather bar with a dress-code night.',
        address: '1500 E Olive Way, Seattle, WA 98122',
        neighborhood: 'capitol-hill',
        crowdVibe: 'Leather and gear crowd; dress code on Fridays.',
        priceBand: '$$',
        sceneTags: ['leather'],
        venueVerifiedAtUs: daysAgo(15),
        profileVerifiedAtUs: daysAgo(15),
        adult: true,
      }),
      links: [link('Iron & Oak events', 'website', 'https://example.org/iron-and-oak')],
    },
  ];

  // ---- organizations ----

  interface OrganizationSpec {
    id: number;
    slug: string;
    name: string;
    kind: GuideOrganization['org_kind'];
    description: string;
    cadence: string;
    neighborhood: string;
    sceneTags: string[];
    verifiedAtUs: number | null;
    status?: GuideFreshnessStatus;
    closureNote?: string;
    adult?: boolean;
  }
  const organization = (spec: OrganizationSpec): GuideOrganization => ({
    id: spec.id,
    slug: spec.slug,
    name: spec.name,
    org_kind: spec.kind,
    description: spec.description,
    url: `https://example.org/${spec.slug}`,
    meeting_cadence_text: spec.cadence,
    neighborhood_slug: spec.neighborhood,
    adult: spec.adult ?? false,
    closure_note: spec.closureNote ?? '',
    scene_tag_slugs: spec.sceneTags,
    status: spec.status ?? 'open',
    last_verified_at_us: spec.verifiedAtUs,
  });

  const organizations: GuideMockOrganization[] = [
    {
      // Seasonal: still listed, with a "Seasonal" marker.
      organization: organization({
        id: 301,
        slug: 'cascade-queer-softball',
        name: 'Cascade Queer Softball League',
        kind: 'sports-league',
        description: 'Recreational and competitive divisions; new players placed on teams.',
        cadence: 'Sundays, April through August',
        neighborhood: '',
        sceneTags: ['jock'],
        verifiedAtUs: daysAgo(30),
        status: 'seasonal',
      }),
      links: [link('League sign-ups', 'website', 'https://example.org/softball-signups')],
    },
    {
      organization: organization({
        id: 302,
        slug: 'harbor-voices-chorus',
        name: 'Harbor Voices Chorus',
        kind: 'chorus-performance',
        description: 'A community chorus. No audition for the fall session.',
        cadence: 'Rehearsals Monday evenings',
        neighborhood: 'capitol-hill',
        sceneTags: [],
        verifiedAtUs: daysAgo(90),
      }),
      links: [],
    },
    {
      organization: organization({
        id: 303,
        slug: 'clear-path-recovery-circle',
        name: 'Clear Path Recovery Circle',
        kind: 'recovery',
        description: 'Peer recovery meetings. Newcomers welcome.',
        cadence: 'Tuesdays and Fridays, 7 PM',
        neighborhood: 'capitol-hill',
        sceneTags: ['sober'],
        verifiedAtUs: daysAgo(14),
      }),
      links: [],
    },
    {
      // Never verified: its stamp says so.
      organization: organization({
        id: 304,
        slug: 'open-table-fellowship',
        name: 'Open Table Fellowship',
        kind: 'faith',
        description: 'An affirming congregation with a Sunday potluck.',
        cadence: 'Sundays at 11 AM',
        neighborhood: 'first-hill',
        sceneTags: [],
        verifiedAtUs: null,
      }),
      links: [],
    },
    {
      organization: organization({
        id: 305,
        slug: 'lakeside-queer-professionals',
        name: 'Lakeside Queer Professionals',
        kind: 'professional',
        description: 'Networking and mentoring across Eastside employers.',
        cadence: 'Second Wednesdays',
        neighborhood: 'bellevue',
        sceneTags: [],
        verifiedAtUs: daysAgo(40),
      }),
      links: [link('Mailing list', 'other', 'https://example.org/lqp-list')],
    },
    {
      organization: organization({
        id: 306,
        slug: 'rolling-dice-gamers',
        name: 'Rolling Dice Gamers',
        kind: 'club-hobby',
        description: 'Board and tabletop games; teaching tables every week.',
        cadence: 'Thursdays',
        neighborhood: 'bellevue',
        sceneTags: ['geek'],
        verifiedAtUs: daysAgo(7),
      }),
      links: [link('Game night Discord', 'discord', 'https://example.org/rolling-dice')],
    },
    {
      organization: organization({
        id: 307,
        slug: 'pacific-leather-collective',
        name: 'Pacific Leather Collective',
        kind: 'social-meetup',
        description: 'Socials and workshops for the leather community.',
        cadence: 'First Fridays',
        neighborhood: 'capitol-hill',
        sceneTags: ['leather'],
        verifiedAtUs: daysAgo(20),
        adult: true,
      }),
      links: [],
    },
    {
      // In the graveyard.
      organization: organization({
        id: 308,
        slug: 'night-owls-book-club',
        name: 'Night Owls Book Club',
        kind: 'club-hobby',
        description: 'A monthly book club for older readers.',
        cadence: 'Last Sundays',
        neighborhood: 'wallingford',
        sceneTags: ['elders'],
        verifiedAtUs: daysAgo(150),
        status: 'closed',
        closureNote: 'Wound down after its last meeting; members moved to a library group.',
      }),
      links: [],
    },
    {
      // Moved: out of the directory AND the graveyard (it points to its new
      // home), but its page still loads.
      organization: organization({
        id: 309,
        slug: 'eastlake-run-club',
        name: 'Eastlake Run Club',
        kind: 'sports-league',
        description: 'Now meets as part of a citywide running group.',
        cadence: 'Saturdays, 9 AM',
        neighborhood: '',
        sceneTags: ['jock'],
        verifiedAtUs: daysAgo(60),
        status: 'moved',
      }),
      links: [],
    },
  ];

  // ---- places ----

  const places: GuideMockPlace[] = [
    {
      place: {
        id: 401,
        slug: 'denny-blaine-park',
        name: 'Denny Blaine Park',
        place_kind: 'park-beach',
        description: "Mock listing. The real entry and its current-rules page are GD10's.",
        address_text: '',
        neighborhood_slug: '',
        adult: true,
        scene_tag_slugs: [],
        status: 'open',
        last_verified_at_us: daysAgo(10),
      },
      links: [],
    },
    {
      place: {
        id: 402,
        slug: 'rainbow-mural-wall',
        name: 'Rainbow Mural Wall',
        place_kind: 'landmark',
        description: 'A community mural, repainted every few summers.',
        address_text: 'E Pike St and 11th Ave',
        neighborhood_slug: 'capitol-hill',
        adult: false,
        scene_tag_slugs: [],
        status: 'open',
        last_verified_at_us: daysAgo(100),
      },
      links: [link('Mural project', 'website', 'https://example.org/rainbow-mural')],
    },
  ];

  // ---- services: health care and everyday, all with made-up 555 numbers ----

  interface ServiceSpec {
    id: number;
    slug: string;
    name: string;
    kind: GuideService['service_kind'];
    neighborhood: string;
    description: string;
    verifiedAtUs: number | null;
  }
  const service = (spec: ServiceSpec): GuideService => ({
    id: spec.id,
    slug: spec.slug,
    name: spec.name,
    service_kind: spec.kind,
    description: spec.description,
    url: `https://example.org/${spec.slug}`,
    // An all-digit text field: it must stay a string.
    phone: `206555${String(spec.id).padStart(4, '0')}`,
    address_text: '',
    neighborhood_slug: spec.neighborhood,
    is_health: spec.kind !== 'everyday-service',
    status: 'open',
    last_verified_at_us: spec.verifiedAtUs,
  });

  const services: GuideService[] = [
    service({
      id: 601,
      slug: 'neighborhood-health-clinic',
      name: 'Neighborhood Health Clinic',
      kind: 'sti-testing',
      neighborhood: 'capitol-hill',
      description: 'Walk-in testing on weekday evenings.',
      verifiedAtUs: daysAgo(25),
    }),
    service({
      id: 602,
      slug: 'prep-navigation-program',
      name: 'PrEP Navigation Program',
      kind: 'prep',
      neighborhood: '',
      description: 'Help starting PrEP, including with insurance.',
      verifiedAtUs: daysAgo(50),
    }),
    service({
      id: 603,
      slug: 'eastside-gender-care',
      name: 'Eastside Gender Care',
      kind: 'gender-affirming',
      neighborhood: 'bellevue',
      description: 'Gender-affirming primary care.',
      verifiedAtUs: daysAgo(70),
    }),
    service({
      id: 604,
      slug: 'affirming-counseling-collective',
      name: 'Affirming Counseling Collective',
      kind: 'mental-health',
      neighborhood: 'fremont',
      description: 'Sliding-scale counseling.',
      verifiedAtUs: null,
    }),
    service({
      id: 605,
      slug: 'friendly-dental-group',
      name: 'Friendly Dental Group',
      kind: 'dental',
      neighborhood: 'columbia-city',
      description: 'General dentistry.',
      verifiedAtUs: daysAgo(120),
    }),
    service({
      id: 606,
      slug: 'northside-barbers',
      name: 'Northside Barbers',
      kind: 'everyday-service',
      neighborhood: 'fremont',
      description: 'Haircuts for every gender, priced by length.',
      verifiedAtUs: daysAgo(30),
    }),
  ];

  // ---- pages ----

  interface PageSpec {
    id: number;
    slug: string;
    title: string;
    section: GuidePage['section'];
    body: string;
    publishAtUs: number | null;
    draft?: boolean;
    adult?: boolean;
    sceneTags?: string[];
    verifiedAtUs?: number | null;
  }
  const page = (spec: PageSpec): GuidePage => {
    const draft = spec.draft ?? false;
    return {
      id: spec.id,
      slug: spec.slug,
      title: spec.title,
      section: spec.section,
      body_markdown: spec.body,
      adult: spec.adult ?? false,
      draft,
      publish_at_us: spec.publishAtUs,
      published: isGuidePagePublished({ draft, publish_at_us: spec.publishAtUs }, now),
      scene_tag_slugs: spec.sceneTags ?? [],
      status: 'open',
      last_verified_at_us: spec.verifiedAtUs ?? spec.publishAtUs,
    };
  };

  const pages: GuidePage[] = [
    // A full scene's intro: the `scenes` page whose slug is the scene's.
    page({
      id: 701,
      slug: 'leather',
      title: 'Leather & kink',
      section: 'scenes',
      body: [
        'A mock scene intro. Start at [Iron & Oak](/guide/venues/iron-and-oak) on a Friday,',
        'or meet people at a [Pacific Leather Collective](/guide/orgs/pacific-leather-collective)',
        'social.',
      ].join('\n'),
      publishAtUs: daysAgo(30),
      sceneTags: ['leather'],
    }),
    // A link-out scene's intro: the landing is this plus its links.
    page({
      id: 702,
      slug: 'trans',
      title: 'Trans Seattle',
      section: 'scenes',
      body: [
        'A mock scene intro. This scene is curated by its own people, so we point you to them,',
        'with credit, rather than listing it ourselves.',
      ].join('\n'),
      publishAtUs: daysAgo(15),
      sceneTags: ['trans'],
    }),
    // A neighborhood rundown: the `new-to-seattle` page whose slug is the
    // neighborhood's.
    page({
      id: 703,
      slug: 'capitol-hill',
      title: 'Capitol Hill',
      section: 'new-to-seattle',
      body: [
        'A mock neighborhood rundown.',
        '',
        '- Bars: [The Lantern Room](/guide/venues/the-lantern-room)',
        '- Getting tested: see [Health](/guide/sections/health)',
      ].join('\n'),
      publishAtUs: daysAgo(20),
    }),
    page({
      id: 704,
      slug: 'getting-tested',
      title: 'Getting tested',
      section: 'health',
      body: 'A mock health page. Clinics are listed under [Services](/guide/services).',
      publishAtUs: daysAgo(5),
      verifiedAtUs: daysAgo(5),
    }),
    page({
      id: 705,
      slug: 'etiquette-basics',
      title: 'Etiquette basics',
      section: 'adult',
      body: 'A mock 18+ page, behind the gate.',
      publishAtUs: daysAgo(10),
      adult: true,
    }),
    // Never shown: a draft, and a page scheduled for next week.
    page({
      id: 706,
      slug: 'insurance-and-costs',
      title: 'Insurance and costs (draft)',
      section: 'health',
      body: 'Not written yet.',
      publishAtUs: null,
      draft: true,
      verifiedAtUs: null,
    }),
    page({
      id: 707,
      slug: 'beat-the-freeze',
      title: 'Beat the Freeze (scheduled)',
      section: 'freeze',
      body: 'Goes up next week.',
      publishAtUs: now + 7 * US_PER_DAY,
      verifiedAtUs: null,
    }),
  ];

  // ---- upcoming events at the venues and in the scenes ----

  const event = (
    id: number,
    title: string,
    startsAtUs: number,
    venueId: number | null,
    sceneTags: string[],
    extra: Partial<GuideEvent> = {},
  ): GuideEvent => {
    const atVenue = venues.find((candidate) => candidate.venue.venue_id === venueId)?.venue;
    return {
      id,
      title,
      starts_at_us: startsAtUs,
      ends_at_us: startsAtUs + 3 * US_PER_HOUR,
      venue_id: venueId,
      venue_name: atVenue?.name ?? '',
      location_text: '',
      cost_text: '',
      scene_tag_slugs: sceneTags,
      ...extra,
    };
  };

  const events: GuideEvent[] = [
    event(501, 'Karaoke Night', evening(1), 1, [], { cost_text: 'Free' }),
    event(502, 'Disco Night', evening(2), 2, [], { cost_text: '$12' }),
    event(503, 'Bear Happy Hour', evening(3), 1, ['bear'], { cost_text: 'No cover' }),
    event(504, 'Leather Night', evening(5), 6, ['leather'], { cost_text: '$10 cover' }),
    event(505, 'Queer Craft Fair', evening(6), 3, ['sapphic'], { cost_text: 'Free' }),
    event(506, 'Board Game Night', evening(9), 4, ['geek'], { cost_text: 'Free' }),
    event(507, 'Bear Happy Hour', evening(10), 1, ['bear'], { cost_text: 'No cover' }),
    event(508, 'Sober Saturday Hike', evening(11), null, ['sober'], {
      ends_at_us: null,
      location_text: 'Cougar Mountain, Red Town trailhead',
      cost_text: 'Free',
    }),
    // Never in the default window: one over, one 60 days out.
    event(509, 'Trivia Night', daysAgo(2), 1, []),
    event(510, 'Bear Weekend Kickoff', evening(60), 1, ['bear']),
  ];

  return { scenes, neighborhoods, venues, organizations, places, services, pages, events };
}
