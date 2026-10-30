import { provideHttpClient } from '@angular/common/http';
import { HttpTestingController, provideHttpClientTesting } from '@angular/common/http/testing';
import { TestBed } from '@angular/core/testing';
import { firstValueFrom, Observable } from 'rxjs';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { GuideHttpAccess } from './guide-access.http';

function setUp() {
  TestBed.configureTestingModule({
    providers: [
      provideHttpClient(),
      provideHttpClientTesting(),
      { provide: HONUWARE_API_BASE, useValue: '/api' },
      GuideHttpAccess,
    ],
  });
  return {
    access: TestBed.inject(GuideHttpAccess),
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

const WIRE_VENUE = {
  venue_id: 7,
  profile_id: 12,
  slug: 'example-bar',
  name: 'Example Bar',
  adult: false,
  scene_tag_slugs: ['leather'],
  status: 'open',
  last_verified_at_us: '',
};

// Every method, with the request it should make. All are anonymous GETs.
const REQUESTS: Array<[string, (access: GuideHttpAccess) => Observable<unknown>, string]> = [
  ['getScenes', (access) => access.getScenes(), '/api/guide/scenes'],
  ['getScene', (access) => access.getScene('leather'), '/api/guide/scenes/leather'],
  ['getVenues', (access) => access.getVenues(), '/api/guide/venues'],
  ['getVenue', (access) => access.getVenue('example-bar'), '/api/guide/venues/example-bar'],
  ['getOrganizations', (access) => access.getOrganizations(), '/api/guide/orgs'],
  ['getOrganization', (access) => access.getOrganization('a-league'), '/api/guide/orgs/a-league'],
  ['getPlaces', (access) => access.getPlaces(), '/api/guide/places'],
  ['getPlace', (access) => access.getPlace('a-beach'), '/api/guide/places/a-beach'],
  ['getServices', (access) => access.getServices(), '/api/guide/services'],
  ['getNeighborhoods', (access) => access.getNeighborhoods(), '/api/guide/neighborhoods'],
  [
    'getNeighborhood',
    (access) => access.getNeighborhood('ballard'),
    '/api/guide/neighborhoods/ballard',
  ],
  ['getGraveyard', (access) => access.getGraveyard(), '/api/guide/graveyard'],
  ['getGuidePage', (access) => access.getGuidePage('leather'), '/api/guide/pages/leather'],
  [
    'getGuidePagesBySection',
    (access) => access.getGuidePagesBySection('health'),
    '/api/guide/pages?section=health',
  ],
  ['getVenueEvents', (access) => access.getVenueEvents(7), '/api/events/upcoming?venue_id=7'],
  ['getSceneEvents', (access) => access.getSceneEvents('bear'), '/api/events/upcoming?scene=bear'],
];

describe('GuideHttpAccess', () => {
  afterEach(() => TestBed.inject(HttpTestingController).verify());

  it.each(REQUESTS)('%s asks for the right URL, with credentials', async (_name, call, url) => {
    const { access, http } = setUp();
    const request = await answer(http, call(access), {});
    expect(request.method).toBe('GET');
    expect(request.url).toBe(url);
    expect(request.withCredentials).toBe(true);
  });

  describe('venue filters', () => {
    it('passes each filter through, in a fixed order', async () => {
      const { access, http } = setUp();
      const { url } = await answer(
        http,
        access.getVenues({ include_closed: true, neighborhood: 'capitol-hill', scene: 'leather' }),
        { items: [] },
      );
      expect(url).toBe(
        '/api/guide/venues?scene=leather&neighborhood=capitol-hill&include_closed=true',
      );
    });

    it('leaves include_closed off unless it is asked for', async () => {
      const { access, http } = setUp();
      const { url } = await answer(
        http,
        access.getVenues({ scene: 'bear', include_closed: false }),
        { items: [] },
      );
      expect(url).toBe('/api/guide/venues?scene=bear');
    });

    it('reads the items', async () => {
      const { access, http } = setUp();
      const { result } = await answer(http, access.getVenues(), { items: [WIRE_VENUE] });
      expect(result.map((venue) => venue.venue_id)).toEqual([7]);
      expect(result[0].last_verified_at_us).toBeNull();
    });
  });

  it('passes the organization and service filters through', async () => {
    const { access, http } = setUp();
    const organizations = await answer(
      http,
      access.getOrganizations({ kind: 'sports-league', scene: 'jock', neighborhood: 'ballard' }),
      { items: [] },
    );
    expect(organizations.url).toBe(
      '/api/guide/orgs?kind=sports-league&scene=jock&neighborhood=ballard',
    );

    const services = await answer(http, access.getServices('sti-testing'), { items: [] });
    expect(services.url).toBe('/api/guide/services?kind=sti-testing');
  });

  it('encodes a slug', async () => {
    const { access, http } = setUp();
    const { url } = await answer(http, access.getVenue('a b/c'), {});
    expect(url).toBe('/api/guide/venues/a%20b%2Fc');
  });

  it('reads a scene landing, its intro page null until one is written', async () => {
    const { access, http } = setUp();
    const { result } = await answer(http, access.getScene('leather'), {
      scene: { slug: 'leather', coverage: 'full', venue_count: 1 },
      venues: [WIRE_VENUE],
      organizations: [],
      places: [],
      links: [{ id: 3, label: 'Chat', url: 'https://example.org', platform: 'discord' }],
      intro_page: null,
    });
    expect(result.scene.venue_count).toBe(1);
    expect(result.venues[0].slug).toBe('example-bar');
    expect(result.links[0].platform).toBe('discord');
    expect(result.intro_page).toBeNull();
  });

  it('reads the graveyard', async () => {
    const { access, http } = setUp();
    const { result } = await answer(http, access.getGraveyard(), {
      venues: [{ ...WIRE_VENUE, status: 'closed', closure_note: 'Gone.' }],
      organizations: [],
    });
    expect(result.venues[0].closure_note).toBe('Gone.');
    expect(result.organizations).toEqual([]);
  });

  it('reads the upcoming events at a venue', async () => {
    const { access, http } = setUp();
    const { result } = await answer(http, access.getVenueEvents(7), {
      items: [{ id: 42, title: 'Karaoke', starts_at_us: 100, venue_id: 7 }],
      from_us: 1,
      to_us: 2,
    });
    expect(result.map((event) => event.id)).toEqual([42]);
    expect(result[0].venue_id).toBe(7);
  });

  it('passes a 404 through for the page to show "not found"', async () => {
    const { access, http } = setUp();
    const result = firstValueFrom(access.getGuidePage('draft-page'));
    http
      .expectOne('/api/guide/pages/draft-page')
      .flush(
        { status: 404, title: 'Not Found', detail: 'Guide page not found' },
        { status: 404, statusText: 'Not Found' },
      );
    await expect(result).rejects.toMatchObject({ status: 404 });
  });
});
