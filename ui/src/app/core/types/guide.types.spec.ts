import {
  buildGuideQueryString,
  normalizeGuideEvents,
  normalizeGuidePage,
  normalizeGuideSceneDetail,
  normalizeGuideService,
  normalizeGuideVenue,
  normalizeGuideVenues,
} from './guide.types';

// A full venue as GET /api/guide/venues/<slug> sends it (guide_json.cpp): ids
// and times are numbers, checkboxes are real booleans, text stays text even
// when it's all digits, and a date that was never set is "".
const WIRE_VENUE = {
  venue_id: 7,
  profile_id: 12,
  slug: 'example-bar',
  name: '1999',
  description: 'A bar.',
  address: '1400 E Pine St, Seattle, WA 98122',
  url: 'https://example.org/bar',
  neighborhood_slug: 'capitol-hill',
  crowd_vibe: 'Bears after work.',
  price_band: '$$',
  adult: false,
  closure_note: '',
  scene_tag_slugs: ['bear', 'leather'],
  status: 'open',
  last_verified_at_us: 1790000000000000,
  venue_last_verified_at_us: 1790000000000000,
  profile_last_verified_at_us: 1791000000000000,
};

const WIRE_PAGE = {
  id: 701,
  slug: 'leather',
  title: 'Leather & kink',
  section: 'scenes',
  body_markdown: 'Start at [the bar](/guide/venues/example-bar).',
  adult: false,
  draft: false,
  publish_at_us: 1790000000000000,
  published: true,
  scene_tag_slugs: ['leather'],
  status: 'open',
  last_verified_at_us: '',
};

describe('guide.types', () => {
  describe('normalizeGuideVenue', () => {
    it('keeps a full venue as it is', () => {
      expect(normalizeGuideVenue(WIRE_VENUE)).toEqual(WIRE_VENUE);
    });

    it('maps the server\'s "" for "never verified" to null', () => {
      const venue = normalizeGuideVenue({
        ...WIRE_VENUE,
        last_verified_at_us: '',
        profile_last_verified_at_us: '',
      });
      expect(venue.last_verified_at_us).toBeNull();
      expect(venue.profile_last_verified_at_us).toBeNull();
      expect(venue.venue_last_verified_at_us).toBe(1790000000000000);
    });

    it('defaults whatever is missing', () => {
      const venue = normalizeGuideVenue({});
      expect(venue.slug).toBe('');
      expect(venue.venue_id).toBe(0);
      expect(venue.adult).toBe(false);
      expect(venue.scene_tag_slugs).toEqual([]);
      expect(venue.last_verified_at_us).toBeNull();
    });
  });

  it('keeps an all-digit phone and name as text, and is_health as a boolean', () => {
    const service = normalizeGuideService({
      id: 601,
      name: '1999',
      phone: '2065550100',
      is_health: false,
      status: 'open',
    });
    expect(service.phone).toBe('2065550100');
    expect(service.name).toBe('1999');
    expect(service.is_health).toBe(false);
    expect(normalizeGuideService({ is_health: true }).is_health).toBe(true);
  });

  describe('pages', () => {
    it('reads a page: an unset publish time or verification is null', () => {
      const page = normalizeGuidePage({ ...WIRE_PAGE, publish_at_us: '', draft: true });
      expect(page.publish_at_us).toBeNull();
      expect(page.last_verified_at_us).toBeNull();
      expect(page.draft).toBe(true);
      expect(page.body_markdown).toBe(WIRE_PAGE.body_markdown);
    });

    it('keeps a missing intro page null, and reads one that is there', () => {
      const bare = normalizeGuideSceneDetail({ scene: { slug: 'bear' }, intro_page: null });
      expect(bare.intro_page).toBeNull();
      expect(bare.venues).toEqual([]);
      expect(bare.links).toEqual([]);

      const withIntro = normalizeGuideSceneDetail({
        scene: { slug: 'leather' },
        intro_page: WIRE_PAGE,
      });
      expect(withIntro.intro_page?.slug).toBe('leather');
      expect(withIntro.intro_page?.published).toBe(true);
    });
  });

  it("reads a list's items, and nothing from a reply without them", () => {
    expect(normalizeGuideVenues({ items: [WIRE_VENUE] }).map((venue) => venue.slug)).toEqual([
      'example-bar',
    ]);
    expect(normalizeGuideVenues({})).toEqual([]);
    expect(normalizeGuideVenues(null)).toEqual([]);
  });

  it("keeps only the guide's fields from Stream A's upcoming events", () => {
    const [event] = normalizeGuideEvents({
      items: [
        {
          id: 42,
          title: 'Bear Happy Hour',
          starts_at_us: 1793500000000000,
          ends_at_us: '',
          venue_id: '',
          location_text: 'A trailhead',
          series_id: 3,
          origin: 'manual',
        },
      ],
      from_us: 1,
      to_us: 2,
    });
    expect(event).toEqual({
      id: 42,
      title: 'Bear Happy Hour',
      starts_at_us: 1793500000000000,
      ends_at_us: null,
      venue_id: null,
      venue_name: '',
      location_text: 'A trailhead',
      cost_text: '',
      scene_tag_slugs: [],
    });
  });

  describe('buildGuideQueryString', () => {
    it('is empty when nothing is set', () => {
      expect(buildGuideQueryString([])).toBe('');
      expect(
        buildGuideQueryString([
          ['scene', undefined],
          ['neighborhood', ''],
          ['include_closed', false],
        ]),
      ).toBe('');
    });

    it('keeps the order given, sends a yes as "true", and encodes values', () => {
      expect(
        buildGuideQueryString([
          ['scene', 'bear'],
          ['neighborhood', 'a b&c'],
          ['include_closed', true],
          ['venue_id', 7],
        ]),
      ).toBe('?scene=bear&neighborhood=a%20b%26c&include_closed=true&venue_id=7');
    });
  });
});
