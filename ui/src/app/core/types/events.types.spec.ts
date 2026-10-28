import {
  buildEventsQueryString,
  categoriesFromRows,
  dataResultsToRecords,
  neighborhoodsFromRows,
  normalizeEvent,
  normalizeEventSeriesDetail,
  normalizeEventSeriesList,
  normalizeUpcomingEvents,
  sceneTagsFromRows,
} from './events.types';

// A full event as GET /api/events/<id> sends it (events_json.cpp): ids and
// times are numbers, a missing end / venue / series is "", the slug lists are
// real arrays.
const WIRE_EVENT = {
  id: 42,
  title: 'T4T Dance Party',
  description: 'Second Saturdays.',
  starts_at_us: 1793500000000000,
  ends_at_us: 1793510800000000,
  venue_id: 7,
  venue_name: 'The Cuff',
  venue_address: '1533 13th Ave, Seattle, WA 98122',
  neighborhood_slug: 'capitol-hill',
  location_text: '',
  url: 'https://example.org/t4t',
  cost_text: '10',
  category_slugs: ['dance', 'drag'],
  scene_tag_slugs: ['trans'],
  series_id: 3,
  series_slug: 't4t',
  series_title: 'T4T',
  origin: 'scanned',
};

describe('events.types', () => {
  describe('normalizeEvent', () => {
    it('keeps a full event as it is', () => {
      expect(normalizeEvent(WIRE_EVENT)).toEqual(WIRE_EVENT);
    });

    it('maps the server\'s "" for "none" to null', () => {
      const event = normalizeEvent({ ...WIRE_EVENT, ends_at_us: '', venue_id: '', series_id: '' });
      expect(event.ends_at_us).toBeNull();
      expect(event.venue_id).toBeNull();
      expect(event.series_id).toBeNull();
    });

    it('defaults whatever is missing', () => {
      const event = normalizeEvent({ id: 1, title: 'Bare', starts_at_us: 5 });
      expect(event.category_slugs).toEqual([]);
      expect(event.scene_tag_slugs).toEqual([]);
      expect(event.description).toBe('');
      expect(event.venue_address).toBe('');
      expect(event.ends_at_us).toBeNull();
      expect(normalizeEvent(null).id).toBe(0);
    });

    it('coerces numbers sent as strings and keeps free text as text', () => {
      const event = normalizeEvent({ ...WIRE_EVENT, id: '42', starts_at_us: '1793500000000000' });
      expect(event.id).toBe(42);
      expect(event.starts_at_us).toBe(1793500000000000);
      expect(event.cost_text).toBe('10');
    });

    it("carries a curator's status and notes only when the server sent them", () => {
      expect('status' in normalizeEvent(WIRE_EVENT)).toBe(false);
      const preview = normalizeEvent({ ...WIRE_EVENT, status: 'pending', review_notes: '' });
      expect(preview.status).toBe('pending');
      expect(preview.review_notes).toBe('');
    });
  });

  it('unwraps the upcoming list and its served window', () => {
    const upcoming = normalizeUpcomingEvents({
      items: [WIRE_EVENT, { ...WIRE_EVENT, id: 43 }],
      from_us: 100,
      to_us: 200,
    });
    expect(upcoming.items.map((event) => event.id)).toEqual([42, 43]);
    expect(upcoming.from_us).toBe(100);
    expect(upcoming.to_us).toBe(200);
    expect(normalizeUpcomingEvents({}).items).toEqual([]);
  });

  it('unwraps the series list and a series page', () => {
    const wireSeries = {
      id: 3,
      title: 'T4T',
      slug: 't4t',
      description: '',
      cadence_text: 'Second Saturdays',
      url: '',
      venue_id: '',
      venue_name: '',
      neighborhood_slug: '',
      status: 'open',
      last_verified_at_us: '',
      category_slugs: ['dance'],
      scene_tag_slugs: ['trans'],
    };
    const list = normalizeEventSeriesList({ items: [wireSeries] });
    expect(list).toHaveLength(1);
    expect(list[0].venue_id).toBeNull();
    expect(list[0].last_verified_at_us).toBeNull();

    const detail = normalizeEventSeriesDetail({ series: wireSeries, upcoming: [WIRE_EVENT] });
    expect(detail.series.slug).toBe('t4t');
    expect(detail.upcoming.map((event) => event.id)).toEqual([42]);
    expect(normalizeEventSeriesDetail({ series: wireSeries }).upcoming).toEqual([]);
  });

  describe('vocabulary rows', () => {
    // The generic table read: columns sorted by name, every value a string,
    // booleans as Postgres "t" / "f".
    const categoryRows = {
      sortedColumnNames: ['description', 'display_name', 'enabled', 'id', 'slug', 'sort_order'],
      dataTable: [
        ['', 'Drag', 't', '2', 'drag', '20'],
        ['', 'Retired', 'f', '9', 'retired', '5'],
        ['', 'Bar & club nights', 't', '1', 'bar', '10'],
        ['', 'Also twenty', 't', '3', 'tie', '20'],
      ],
    };

    it('reads rows into records by column name', () => {
      expect(dataResultsToRecords(categoryRows)[0]).toEqual({
        description: '',
        display_name: 'Drag',
        enabled: 't',
        id: '2',
        slug: 'drag',
        sort_order: '20',
      });
    });

    it('keeps the enabled categories, in sort order then id', () => {
      expect(categoriesFromRows(categoryRows)).toEqual([
        { id: 1, slug: 'bar', display_name: 'Bar & club nights', description: '', sort_order: 10 },
        { id: 2, slug: 'drag', display_name: 'Drag', description: '', sort_order: 20 },
        { id: 3, slug: 'tie', display_name: 'Also twenty', description: '', sort_order: 20 },
      ]);
    });

    it('reads scene tags with their coverage and neighborhoods with their region', () => {
      const sceneTags = sceneTagsFromRows({
        sortedColumnNames: ['coverage', 'display_name', 'enabled', 'id', 'slug', 'sort_order'],
        dataTable: [
          ['link_out', 'Trans', 't', '10', 'trans', '100'],
          ['full', 'Bears', 't', '1', 'bear', '10'],
        ],
      });
      expect(sceneTags.map((tag) => [tag.slug, tag.coverage])).toEqual([
        ['bear', 'full'],
        ['trans', 'link_out'],
      ]);

      const neighborhoods = neighborhoodsFromRows({
        sortedColumnNames: ['display_name', 'enabled', 'id', 'region', 'slug', 'sort_order'],
        dataTable: [['Bellevue', 'true', '17', 'eastside', 'bellevue', '170']],
      });
      expect(neighborhoods).toEqual([
        { id: 17, slug: 'bellevue', display_name: 'Bellevue', region: 'eastside', sort_order: 170 },
      ]);
    });

    it('tolerates an empty reply', () => {
      expect(categoriesFromRows({ sortedColumnNames: [], dataTable: [] })).toEqual([]);
    });
  });

  describe('buildEventsQueryString', () => {
    it('is empty with nothing set', () => {
      expect(buildEventsQueryString({})).toBe('');
      expect(buildEventsQueryString({ category: '', venue_id: 0 })).toBe('');
    });

    it('writes the set fields in a fixed order', () => {
      expect(
        buildEventsQueryString({
          series_id: 3,
          scene: 'bear',
          to_us: 200,
          from_us: 100,
          category: 'drag',
          neighborhood: 'capitol-hill',
          venue_id: 7,
        }),
      ).toBe(
        '?from_us=100&to_us=200&category=drag&scene=bear&neighborhood=capitol-hill&venue_id=7&series_id=3',
      );
    });

    it('encodes what needs encoding', () => {
      expect(buildEventsQueryString({ category: 'a&b c' })).toBe('?category=a%26b%20c');
    });
  });
});
