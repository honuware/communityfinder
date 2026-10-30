import { firstValueFrom, Observable } from 'rxjs';
import { GuideMockAccess } from './guide-access.mock';
import { buildGuideMockData } from '../mock/guide-mock-data';
import {
  normalizeGuideEvent,
  normalizeGuideLink,
  normalizeGuideOrganization,
  normalizeGuidePage,
  normalizeGuidePlace,
  normalizeGuideService,
  normalizeGuideVenue,
} from '../types/guide.types';

const US_PER_DAY = 86_400 * 1_000_000;

// Thu Oct 29, 2026, 1 PM Pacific (20:00 UTC).
const NOW_US = Date.UTC(2026, 9, 29, 20) * 1000;

// The mock reads Date.now() when it's built and on every call.
function mockAt(now: number): GuideMockAccess {
  vi.setSystemTime(new Date(now / 1000));
  return new GuideMockAccess();
}

function slugs(items: Array<{ slug: string }>): string[] {
  return items.map((item) => item.slug);
}

describe('GuideMockAccess', () => {
  beforeEach(() => vi.useFakeTimers({ toFake: ['Date'] }));
  afterEach(() => vi.useRealTimers());

  describe('pages', () => {
    it('never returns the draft or the page scheduled for next week', async () => {
      const access = mockAt(NOW_US);
      for (const slug of ['insurance-and-costs', 'beat-the-freeze']) {
        await expect(firstValueFrom(access.getGuidePage(slug))).rejects.toMatchObject({
          status: 404,
        });
      }
      expect(slugs(await firstValueFrom(access.getGuidePagesBySection('health')))).toEqual([
        'getting-tested',
      ]);
      expect(await firstValueFrom(access.getGuidePagesBySection('freeze'))).toEqual([]);

      const published = await firstValueFrom(access.getGuidePage('getting-tested'));
      expect(published.published).toBe(true);
      expect(published.draft).toBe(false);
    });

    it('serves the scheduled page once its time has come', async () => {
      const access = mockAt(NOW_US);
      vi.setSystemTime(new Date((NOW_US + 8 * US_PER_DAY) / 1000));
      const page = await firstValueFrom(access.getGuidePage('beat-the-freeze'));
      expect(page.published).toBe(true);
    });

    it('lists a section by title, as summaries without a body, and needs a section', async () => {
      const access = mockAt(NOW_US);
      const [summary] = await firstValueFrom(access.getGuidePagesBySection('adult'));
      expect(summary.slug).toBe('etiquette-basics');
      expect(summary.adult).toBe(true);
      expect('body_markdown' in summary).toBe(false);
      await expect(
        firstValueFrom(access.getGuidePagesBySection('' as 'health')),
      ).rejects.toMatchObject({ status: 400 });
    });
  });

  describe('retired listings', () => {
    it('the graveyard returns exactly the closed venues and organizations', async () => {
      const graveyard = await firstValueFrom(mockAt(NOW_US).getGraveyard());
      expect(slugs(graveyard.venues)).toEqual(['the-wren']);
      expect(slugs(graveyard.organizations)).toEqual(['night-owls-book-club']);
      for (const listing of [...graveyard.venues, ...graveyard.organizations]) {
        expect(listing.status).toBe('closed');
        expect(listing.closure_note).not.toBe('');
      }
    });

    it('directories leave out closed and moved listings, but their pages still load', async () => {
      const access = mockAt(NOW_US);
      expect(slugs(await firstValueFrom(access.getVenues()))).not.toContain('the-wren');
      const organizations = slugs(await firstValueFrom(access.getOrganizations()));
      expect(organizations).not.toContain('night-owls-book-club');
      expect(organizations).not.toContain('eastlake-run-club');
      // Seasonal isn't retired.
      expect(organizations).toContain('cascade-queer-softball');

      expect((await firstValueFrom(access.getVenue('the-wren'))).venue.status).toBe('closed');
      expect(
        (await firstValueFrom(access.getOrganization('eastlake-run-club'))).organization.status,
      ).toBe('moved');
    });

    it('include_closed brings closed venues back, so the by-id resolver finds any venue', async () => {
      const venues = await firstValueFrom(mockAt(NOW_US).getVenues({ include_closed: true }));
      expect(slugs(venues)).toContain('the-wren');
      const venueIds = venues.map((venue) => venue.venue_id);
      expect(new Set(venueIds).size).toBe(venueIds.length);
      expect(venueIds.every((id) => id > 0)).toBe(true);
    });
  });

  it('filters as the server does, ordering by name', async () => {
    const access = mockAt(NOW_US);
    expect(slugs(await firstValueFrom(access.getVenues({ scene: 'leather' })))).toEqual([
      'iron-and-oak',
    ]);
    expect(slugs(await firstValueFrom(access.getVenues({ neighborhood: 'capitol-hill' })))).toEqual(
      ['iron-and-oak', 'the-lantern-room'],
    );
    expect(slugs(await firstValueFrom(access.getVenues({ scene: 'no-such-scene' })))).toEqual([]);
    expect(slugs(await firstValueFrom(access.getOrganizations({ kind: 'recovery' })))).toEqual([
      'clear-path-recovery-circle',
    ]);
    expect(slugs(await firstValueFrom(access.getOrganizations({ scene: 'jock' })))).toEqual([
      'cascade-queer-softball',
    ]);
    expect(
      slugs(await firstValueFrom(access.getOrganizations({ neighborhood: 'bellevue' }))),
    ).toEqual(['lakeside-queer-professionals', 'rolling-dice-gamers']);
    expect(slugs(await firstValueFrom(access.getServices('everyday-service')))).toEqual([
      'northside-barbers',
    ]);
    const names = (await firstValueFrom(access.getServices())).map((service) => service.name);
    expect(names).toEqual([...names].sort((a, b) => a.localeCompare(b)));
  });

  describe('scenes and neighborhoods', () => {
    it('a scene count is what its landing lists', async () => {
      const access = mockAt(NOW_US);
      const scenes = await firstValueFrom(access.getScenes());
      expect(scenes.length).toBe(11);
      for (const scene of scenes) {
        const detail = await firstValueFrom(access.getScene(scene.slug));
        expect(detail.venues.length).toBe(scene.venue_count);
        expect(detail.organizations.length).toBe(scene.organization_count);
        expect(detail.places.length).toBe(scene.place_count);
      }
      // The closed bear bar isn't counted.
      expect(scenes.find((scene) => scene.slug === 'bear')?.venue_count).toBe(1);
    });

    it('has both postures, and a link-out scene has its intro and links', async () => {
      const access = mockAt(NOW_US);
      const scenes = await firstValueFrom(access.getScenes());
      expect(
        scenes.filter((scene) => scene.coverage === 'link_out').map((scene) => scene.slug),
      ).toEqual(['sapphic', 'trans', 'qtpoc']);

      const trans = await firstValueFrom(access.getScene('trans'));
      expect(trans.intro_page?.slug).toBe('trans');
      expect(trans.links.length).toBe(2);
      const leather = await firstValueFrom(access.getScene('leather'));
      expect(leather.intro_page?.section).toBe('scenes');
      expect((await firstValueFrom(access.getScene('bear'))).intro_page).toBeNull();
    });

    it('a neighborhood count is what its page lists; the rundown follows the slug', async () => {
      const access = mockAt(NOW_US);
      const neighborhoods = await firstValueFrom(access.getNeighborhoods());
      expect(neighborhoods.length).toBe(25);
      expect(
        neighborhoods.filter((neighborhood) => neighborhood.region === 'eastside').length,
      ).toBe(6);
      for (const neighborhood of neighborhoods) {
        const detail = await firstValueFrom(access.getNeighborhood(neighborhood.slug));
        expect(detail.venues.length).toBe(neighborhood.venue_count);
        expect(detail.organizations.length).toBe(neighborhood.organization_count);
        expect(detail.places.length).toBe(neighborhood.place_count);
      }
      const capitolHill = await firstValueFrom(access.getNeighborhood('capitol-hill'));
      expect(capitolHill.rundown_page?.slug).toBe('capitol-hill');
      expect((await firstValueFrom(access.getNeighborhood('ballard'))).rundown_page).toBeNull();
    });
  });

  it('is a 404 for an unknown slug', async () => {
    const access = mockAt(NOW_US);
    const calls: Array<Observable<unknown>> = [
      access.getScene('nope'),
      access.getVenue('nope'),
      access.getOrganization('nope'),
      access.getPlace('nope'),
      access.getNeighborhood('nope'),
      access.getGuidePage('nope'),
    ];
    for (const call of calls) {
      await expect(firstValueFrom(call)).rejects.toMatchObject({ status: 404 });
    }
  });

  it('has the 18+ layer: a venue, an organization, Denny Blaine and a page', async () => {
    const access = mockAt(NOW_US);
    expect((await firstValueFrom(access.getVenue('iron-and-oak'))).venue.adult).toBe(true);
    expect(
      (await firstValueFrom(access.getOrganization('pacific-leather-collective'))).organization
        .adult,
    ).toBe(true);
    const place = (await firstValueFrom(access.getPlace('denny-blaine-park'))).place;
    expect(place.adult).toBe(true);
    expect(place.place_kind).toBe('park-beach');
    expect((await firstValueFrom(access.getGuidePage('etiquette-basics'))).adult).toBe(true);
  });

  it('mixes health and everyday services', async () => {
    const services = await firstValueFrom(mockAt(NOW_US).getServices());
    expect(services.length).toBe(6);
    expect(services.filter((service) => service.is_health).length).toBe(5);
    expect(services.every((service) => typeof service.phone === 'string')).toBe(true);
  });

  it('serves a venue or scene its events in the default window, in start order', async () => {
    const access = mockAt(NOW_US);
    const ids = (events: Array<{ id: number }>) => events.map((event) => event.id);
    // 509 is over and 510 is 60 days out.
    expect(ids(await firstValueFrom(access.getVenueEvents(1)))).toEqual([501, 503, 507]);
    expect(ids(await firstValueFrom(access.getSceneEvents('bear')))).toEqual([503, 507]);
    expect(ids(await firstValueFrom(access.getSceneEvents('sober')))).toEqual([508]);
    expect(await firstValueFrom(access.getVenueEvents(5))).toEqual([]);
  });

  it('hands out copies, so a page editing a listing cannot change the data', async () => {
    const access = mockAt(NOW_US);
    const first = await firstValueFrom(access.getVenue('the-lantern-room'));
    first.venue.name = 'Edited';
    first.venue.scene_tag_slugs.push('edited');
    first.links.length = 0;
    const second = await firstValueFrom(access.getVenue('the-lantern-room'));
    expect(second.venue.name).toBe('The Lantern Room');
    expect(second.venue.scene_tag_slugs).not.toContain('edited');
    expect(second.links.length).toBe(1);
  });

  describe('the dataset', () => {
    it('uses only vocabulary that exists', () => {
      const data = buildGuideMockData(NOW_US);
      const sceneTags = data.scenes.map(({ scene }) => scene.slug);
      const neighborhoods = data.neighborhoods.map((neighborhood) => neighborhood.slug);
      const listings = [
        ...data.venues.map(({ venue }) => venue),
        ...data.organizations.map(({ organization }) => organization),
        ...data.places.map(({ place }) => place),
      ];
      for (const listing of listings) {
        listing.scene_tag_slugs.forEach((slug) => expect(sceneTags).toContain(slug));
        if (listing.neighborhood_slug) expect(neighborhoods).toContain(listing.neighborhood_slug);
      }
      for (const service of data.services) {
        if (service.neighborhood_slug) expect(neighborhoods).toContain(service.neighborhood_slug);
      }
    });

    it('is already in the shape the HTTP access produces', () => {
      // Parity: normalizing mock data changes nothing, so pages see the same
      // values offline and against the server.
      const data = buildGuideMockData(NOW_US);
      for (const { venue, links } of data.venues) {
        expect(normalizeGuideVenue(venue)).toEqual(venue);
        links.forEach((link) => expect(normalizeGuideLink(link)).toEqual(link));
      }
      for (const { organization } of data.organizations) {
        expect(normalizeGuideOrganization(organization)).toEqual(organization);
      }
      for (const { place } of data.places) {
        expect(normalizeGuidePlace(place)).toEqual(place);
      }
      data.services.forEach((service) => expect(normalizeGuideService(service)).toEqual(service));
      data.pages.forEach((page) => expect(normalizeGuidePage(page)).toEqual(page));
      data.events.forEach((event) => expect(normalizeGuideEvent(event)).toEqual(event));
    });

    it('stamps a venue with the older of its two verifications, or none if either is missing', () => {
      const data = buildGuideMockData(NOW_US);
      for (const { venue } of data.venues) {
        const { venue_last_verified_at_us: venueAt, profile_last_verified_at_us: profileAt } =
          venue;
        const expected =
          venueAt === null || profileAt === null ? null : Math.min(venueAt, profileAt);
        expect(venue.last_verified_at_us).toBe(expected);
      }
      const driftwood = data.venues.find(({ venue }) => venue.slug === 'driftwood-hall')?.venue;
      expect(driftwood?.last_verified_at_us).toBeNull();
    });
  });
});
