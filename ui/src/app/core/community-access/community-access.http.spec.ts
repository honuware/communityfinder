import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { TestBed } from '@angular/core/testing';
import { firstValueFrom, Observable } from 'rxjs';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { CommunityHttpAccess } from './community-access.http';

function setUp() {
  TestBed.configureTestingModule({
    providers: [
      provideHttpClient(),
      provideHttpClientTesting(),
      { provide: HONUWARE_API_BASE, useValue: '/api' },
      CommunityHttpAccess,
    ],
  });
  return {
    access: TestBed.inject(CommunityHttpAccess),
    http: TestBed.inject(HttpTestingController),
  };
}

// Subscribes, answers the one request it makes with `body`, and returns the
// request's URL and what the call resolved to.
async function answer<T>(
  http: HttpTestingController,
  call: Observable<T>,
  body: unknown,
): Promise<{ url: string; withCredentials: boolean; method: string; result: T }> {
  const result = firstValueFrom(call);
  const request = http.expectOne(() => true);
  request.flush(body as object);
  return {
    url: request.request.urlWithParams,
    withCredentials: request.request.withCredentials,
    method: request.request.method,
    result: await result,
  };
}

const WIRE_EVENT = {
  id: 42,
  title: 'T4T Dance Party',
  starts_at_us: 1793500000000000,
  ends_at_us: '',
  venue_id: '',
  series_id: 3,
  category_slugs: ['dance'],
  scene_tag_slugs: [],
};

describe('CommunityHttpAccess (events)', () => {
  afterEach(() => TestBed.inject(HttpTestingController).verify());

  describe('getUpcomingEvents', () => {
    it('asks for the default window with no query', async () => {
      const { access, http } = setUp();
      const { url, method, withCredentials, result } = await answer(
        http,
        access.getUpcomingEvents(),
        { items: [WIRE_EVENT], from_us: 100, to_us: 200 },
      );
      expect(method).toBe('GET');
      expect(url).toBe('/api/events/upcoming');
      expect(withCredentials).toBe(true);
      expect(result.items[0].ends_at_us).toBeNull();
      expect(result.items[0].venue_id).toBeNull();
      expect(result.from_us).toBe(100);
    });

    it('passes the window and filters through', async () => {
      const { access, http } = setUp();
      const { url } = await answer(
        http,
        access.getUpcomingEvents({
          from_us: 100,
          to_us: 200,
          category: 'drag',
          scene: 'bear',
          neighborhood: 'capitol-hill',
        }),
        { items: [], from_us: 100, to_us: 200 },
      );
      expect(url).toBe(
        '/api/events/upcoming?from_us=100&to_us=200&category=drag&scene=bear&neighborhood=capitol-hill',
      );
    });

    it('filters by venue or series', async () => {
      const { access, http } = setUp();
      const { url } = await answer(http, access.getUpcomingEvents({ venue_id: 7, series_id: 3 }), {
        items: [],
      });
      expect(url).toBe('/api/events/upcoming?venue_id=7&series_id=3');
    });
  });

  it('reads one event', async () => {
    const { access, http } = setUp();
    const { url, result } = await answer(http, access.getEvent(42), WIRE_EVENT);
    expect(url).toBe('/api/events/42');
    expect(result.id).toBe(42);
    expect(result.series_id).toBe(3);
    expect(result.description).toBe('');
  });

  it('passes a 404 through for the page to show "not found"', async () => {
    const { access, http } = setUp();
    const result = firstValueFrom(access.getEvent(999));
    http
      .expectOne('/api/events/999')
      .flush(
        { status: 404, title: 'Not Found', detail: 'Event not found' },
        { status: 404, statusText: 'Not Found' },
      );
    await expect(result).rejects.toMatchObject({ status: 404 });
  });

  it('reads the series list and a series page', async () => {
    const { access, http } = setUp();
    const series = { id: 3, title: 'T4T', slug: 't4t', status: 'open' };
    const list = await answer(http, access.getEventSeriesList(), { items: [series] });
    expect(list.url).toBe('/api/event_series');
    expect(list.result.map((s) => s.slug)).toEqual(['t4t']);

    const page = await answer(http, access.getEventSeries('t4t'), {
      series,
      upcoming: [WIRE_EVENT],
    });
    expect(page.url).toBe('/api/event_series/t4t');
    expect(page.result.series.title).toBe('T4T');
    expect(page.result.upcoming.map((event) => event.id)).toEqual([42]);
  });

  it('encodes a series slug', async () => {
    const { access, http } = setUp();
    const { url } = await answer(http, access.getEventSeries('a b/c'), { series: {} });
    expect(url).toBe('/api/event_series/a%20b%2Fc');
  });

  describe('vocabularies', () => {
    it('reads categories through the generic table read: enabled, in display order', async () => {
      const { access, http } = setUp();
      const { url, withCredentials, result } = await answer(http, access.getCategories(), {
        sortedColumnNames: ['display_name', 'enabled', 'id', 'slug', 'sort_order'],
        dataTable: [
          ['Drag', 't', '2', 'drag', '20'],
          ['Off', 'f', '3', 'off', '1'],
          ['Bar & club nights', 't', '1', 'bar', '10'],
        ],
      });
      expect(url).toBe('/api/get_table_rows/categories');
      expect(withCredentials).toBe(true);
      expect(result.map((category) => category.slug)).toEqual(['bar', 'drag']);
    });

    it('reads scene tags and neighborhoods from their tables', async () => {
      const { access, http } = setUp();
      const sceneTags = await answer(http, access.getSceneTags(), {
        sortedColumnNames: ['coverage', 'enabled', 'id', 'slug', 'sort_order'],
        dataTable: [['link_out', 't', '1', 'trans', '10']],
      });
      expect(sceneTags.url).toBe('/api/get_table_rows/scene_tags');
      expect(sceneTags.result[0].coverage).toBe('link_out');

      const neighborhoods = await answer(http, access.getNeighborhoods(), {
        sortedColumnNames: ['enabled', 'id', 'region', 'slug', 'sort_order'],
        dataTable: [['t', '1', 'seattle-core', 'capitol-hill', '10']],
      });
      expect(neighborhoods.url).toBe('/api/get_table_rows/neighborhoods');
      expect(neighborhoods.result[0].region).toBe('seattle-core');
    });
  });
});
