import { firstValueFrom } from 'rxjs';
import { CommunityMockAccess } from './community-access.mock';
import { buildEventsMockData } from '../mock/events-mock-data';
import {
  normalizeEvent,
  normalizeEventSeries,
  PublicEvent,
  US_PER_DAY,
} from '../types/events.types';
import { pacificWallClockToUs, tonightRangeUs, weekendRangeUs } from '../utils/events-date.utils';

// Wed Aug 12, 2026, 3:00 PM Pacific. From here the dataset has tonight's two
// events, the Aug 14–16 weekend, Bear Happy Hour on the next three Thursdays,
// and the picnic 50 days out (past the default 42).
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);

// The mock reads Date.now() when it's built and on every call.
function mockAt(now: number): CommunityMockAccess {
  vi.setSystemTime(new Date(now / 1000));
  return new CommunityMockAccess();
}

function ids(events: PublicEvent[]): number[] {
  return events.map((event) => event.id);
}

const PAST_EVENT = 116;
const PENDING_EVENT = 117;
const PICNIC_50_DAYS_OUT = 115;

describe('CommunityMockAccess (events)', () => {
  beforeEach(() => vi.useFakeTimers({ toFake: ['Date'] }));
  afterEach(() => vi.useRealTimers());

  describe('getUpcomingEvents', () => {
    it('serves approved events in the default 42-day window, in start order', async () => {
      const upcoming = await firstValueFrom(mockAt(NOW_US).getUpcomingEvents());
      expect(upcoming.from_us).toBe(NOW_US);
      expect(upcoming.to_us).toBe(NOW_US + 42 * US_PER_DAY);
      expect(upcoming.items.length).toBe(14);
      expect(ids(upcoming.items)).not.toContain(PAST_EVENT);
      expect(ids(upcoming.items)).not.toContain(PENDING_EVENT);
      expect(ids(upcoming.items)).not.toContain(PICNIC_50_DAYS_OUT);
      const starts = upcoming.items.map((event) => event.starts_at_us);
      expect(starts).toEqual([...starts].sort((a, b) => a - b));
      expect(starts.every((start) => start >= NOW_US)).toBe(true);
      // The public shape: no review fields.
      expect(upcoming.items.some((event) => 'status' in event)).toBe(false);
    });

    it('has events tonight and this weekend', async () => {
      const items = (await firstValueFrom(mockAt(NOW_US).getUpcomingEvents())).items;
      const within = ({ fromUs, toUs }: { fromUs: number; toUs: number }) =>
        items.filter((event) => event.starts_at_us >= fromUs && event.starts_at_us < toUs);
      expect(within(tonightRangeUs(NOW_US)).length).toBeGreaterThanOrEqual(2);
      expect(within(weekendRangeUs(NOW_US)).length).toBeGreaterThanOrEqual(4);
    });

    it('clamps the window as the server does: one day back, 92 days at most', async () => {
      const access = mockAt(NOW_US);
      const wide = await firstValueFrom(
        access.getUpcomingEvents({
          from_us: NOW_US - 5 * US_PER_DAY,
          to_us: NOW_US + 60 * US_PER_DAY,
        }),
      );
      expect(wide.from_us).toBe(NOW_US - US_PER_DAY);
      expect(ids(wide.items)).toContain(PICNIC_50_DAYS_OUT);
      expect(ids(wide.items)).not.toContain(PAST_EVENT);
      expect(ids(wide.items)).not.toContain(PENDING_EVENT);

      const capped = await firstValueFrom(
        access.getUpcomingEvents({ to_us: NOW_US + 200 * US_PER_DAY }),
      );
      expect(capped.to_us).toBe(NOW_US + 92 * US_PER_DAY);

      const backwards = await firstValueFrom(
        access.getUpcomingEvents({ from_us: NOW_US + US_PER_DAY, to_us: NOW_US }),
      );
      expect(backwards.items).toEqual([]);
    });

    it('filters as the server does, on effective (inherited) slugs', async () => {
      const access = mockAt(NOW_US);
      const query = async (q: Parameters<CommunityMockAccess['getUpcomingEvents']>[0]) =>
        ids((await firstValueFrom(access.getUpcomingEvents(q))).items);

      // Bear Happy Hour's instances have no slugs of their own; they inherit bar / bear.
      expect(await query({ scene: 'bear' })).toEqual([107, 108, 109]);
      expect(await query({ category: 'bar' })).toEqual(
        expect.arrayContaining([101, 103, 107, 108, 109]),
      );
      expect(await query({ series_id: 11 })).toEqual([107, 108, 109]);
      expect(await query({ scene: 'sober' })).toEqual([110, 111]);
      expect(await query({ neighborhood: 'ballard' })).toEqual([102, 105]);
      expect(await query({ venue_id: 4 })).toEqual([106, 112]);
      expect(await query({ category: 'drag', neighborhood: 'bellevue' })).toEqual([106]);
      expect(await query({ category: 'protest' })).toEqual([]);
    });

    it('hands out copies, so a page editing an event cannot change the data', async () => {
      const access = mockAt(NOW_US);
      const first = await firstValueFrom(access.getUpcomingEvents());
      first.items[0].title = 'Edited';
      first.items[0].category_slugs.push('edited');
      const second = await firstValueFrom(access.getUpcomingEvents());
      expect(second.items[0].title).not.toBe('Edited');
      expect(second.items[0].category_slugs).not.toContain('edited');
    });
  });

  describe('getEvent', () => {
    it('serves an approved event, even a past one', async () => {
      const access = mockAt(NOW_US);
      expect((await firstValueFrom(access.getEvent(101))).title).toBe('Karaoke Night');
      expect((await firstValueFrom(access.getEvent(PAST_EVENT))).id).toBe(PAST_EVENT);
    });

    it('is a 404 for a pending or unknown event', async () => {
      const access = mockAt(NOW_US);
      await expect(firstValueFrom(access.getEvent(PENDING_EVENT))).rejects.toMatchObject({
        status: 404,
      });
      await expect(firstValueFrom(access.getEvent(999))).rejects.toMatchObject({ status: 404 });
    });
  });

  describe('series', () => {
    it('lists the active series by title, leaving out the ended one', async () => {
      const list = await firstValueFrom(mockAt(NOW_US).getEventSeriesList());
      expect(list.map((series) => series.slug)).toEqual(['bear-happy-hour', 'sober-saturday-hike']);
    });

    it('serves a series page with its upcoming events, ended series included', async () => {
      const access = mockAt(NOW_US);
      const bears = await firstValueFrom(access.getEventSeries('bear-happy-hour'));
      expect(bears.series.title).toBe('Bear Happy Hour');
      expect(ids(bears.upcoming)).toEqual([107, 108, 109]);

      const ended = await firstValueFrom(access.getEventSeries('drag-brunch-club'));
      expect(ended.series.status).toBe('closed');
      expect(ended.upcoming).toEqual([]);

      await expect(firstValueFrom(access.getEventSeries('nope'))).rejects.toMatchObject({
        status: 404,
      });
    });
  });

  describe('vocabularies', () => {
    it("serves GD1's vocabularies: 12 categories, 11 scene tags (3 link-out), 25 neighborhoods", async () => {
      const access = mockAt(NOW_US);
      const categories = await firstValueFrom(access.getCategories());
      const sceneTags = await firstValueFrom(access.getSceneTags());
      const neighborhoods = await firstValueFrom(access.getNeighborhoods());
      expect(categories.length).toBe(12);
      expect(sceneTags.length).toBe(11);
      expect(neighborhoods.length).toBe(25);
      expect(sceneTags.filter((tag) => tag.coverage === 'link_out').map((tag) => tag.slug)).toEqual(
        ['sapphic', 'trans', 'qtpoc'],
      );
      expect(categories[0].slug).toBe('bar');
      expect(neighborhoods[0].slug).toBe('capitol-hill');
      for (const list of [categories, sceneTags, neighborhoods]) {
        const orders = list.map((entry) => entry.sort_order);
        expect(orders).toEqual([...orders].sort((a, b) => a - b));
      }
    });
  });

  describe('the dataset', () => {
    it('uses only vocabulary that exists', () => {
      const data = buildEventsMockData(NOW_US);
      const categories = data.categories.map((category) => category.slug);
      const sceneTags = data.sceneTags.map((tag) => tag.slug);
      const neighborhoods = data.neighborhoods.map((neighborhood) => neighborhood.slug);
      for (const { event } of data.events) {
        event.category_slugs.forEach((slug) => expect(categories).toContain(slug));
        event.scene_tag_slugs.forEach((slug) => expect(sceneTags).toContain(slug));
        if (event.neighborhood_slug) expect(neighborhoods).toContain(event.neighborhood_slug);
      }
    });

    it('is already in the shape the HTTP access produces', () => {
      // Parity: normalizing a mock event or series changes nothing, so pages
      // see the same values offline and against the server.
      const data = buildEventsMockData(NOW_US);
      for (const { event } of data.events) {
        expect(normalizeEvent(event)).toEqual(event);
      }
      for (const series of data.series) {
        expect(normalizeEventSeries(series)).toEqual(series);
      }
    });

    it('stays current: any evening of the week has events tonight and nothing stale', async () => {
      // Mon Aug 10 – Sun Aug 16, 2026, at 6 PM Pacific.
      for (let day = 10; day <= 16; day++) {
        const evening = pacificWallClockToUs(2026, 8, day, 18);
        const items = (await firstValueFrom(mockAt(evening).getUpcomingEvents())).items;
        const tonight = tonightRangeUs(evening);
        const tonightCount = items.filter(
          (event) => event.starts_at_us >= tonight.fromUs && event.starts_at_us < tonight.toUs,
        ).length;
        expect(tonightCount).toBeGreaterThanOrEqual(2);
        expect(items.every((event) => event.starts_at_us >= evening)).toBe(true);
        expect(ids(items)).not.toContain(PENDING_EVENT);
      }
    });
  });
});
