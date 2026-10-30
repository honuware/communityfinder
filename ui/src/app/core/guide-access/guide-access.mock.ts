import { HttpErrorResponse } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable, of, throwError } from 'rxjs';
import { GuideAccess } from './guide-access';
import {
  buildGuideMockData,
  GuideMockData,
  GuideMockNeighborhood,
  GuideMockSceneTag,
  isGuidePagePublished,
} from '../mock/guide-mock-data';
import {
  GuideEvent,
  GuideFreshness,
  GuideGraveyard,
  GuideNeighborhood,
  GuideNeighborhoodDetail,
  GuideOrganization,
  GuideOrganizationDetail,
  GuideOrganizationFilters,
  GuidePage,
  GuidePageSection,
  GuidePageSummary,
  GuidePlace,
  GuidePlaceDetail,
  GuideScene,
  GuideSceneDetail,
  GuideService,
  GuideServiceKind,
  GuideVenue,
  GuideVenueDetail,
  GuideVenueFilters,
} from '../types/guide.types';

// Stream A's default upcoming window (EV1's behavior pin), which the guide's
// event reads get because they don't pass one.
const UPCOMING_WINDOW_US = 42 * 86_400 * 1_000_000;

function nowUs(): number {
  return Date.now() * 1000;
}

function httpError(status: number, title: string, detail: string): Observable<never> {
  return throwError(
    () =>
      new HttpErrorResponse({
        status,
        statusText: title,
        error: { status, title, detail },
      }),
  );
}

function notFound(detail: string): Observable<never> {
  return httpError(404, 'Not Found', detail);
}

// Closed and moved listings are retired: out of every list but the graveyard
// (GD2 OQ10 (a)).
function isActive(listing: GuideFreshness): boolean {
  return listing.status !== 'closed' && listing.status !== 'moved';
}

function byName<T extends { name: string }>(id: (item: T) => number) {
  return (a: T, b: T) => a.name.localeCompare(b.name) || id(a) - id(b);
}

const venueOrder = byName<GuideVenue>((venue) => venue.venue_id);
const organizationOrder = byName<GuideOrganization>((organization) => organization.id);
const placeOrder = byName<GuidePlace>((place) => place.id);
const serviceOrder = byName<GuideService>((service) => service.id);

// Offline mock — the `local` configuration wires this so `ng serve` runs with
// no backend. Serves the guide demo dataset (core/mock/guide-mock-data) by
// GD2's rules: retired listings out of the lists, published pages only,
// counts that match what each page lists, 404s for unknown slugs. The dataset
// is built once, relative to when the app started; the publish rule and the
// events window are applied as of each call.
@Injectable()
export class GuideMockAccess implements GuideAccess {
  private readonly data: GuideMockData = buildGuideMockData(nowUs());

  getScenes(): Observable<GuideScene[]> {
    return of(this.data.scenes.map(({ scene }) => this.sceneSummary(scene)));
  }

  getScene(slug: string): Observable<GuideSceneDetail> {
    const record = this.data.scenes.find(({ scene }) => scene.slug === slug);
    if (!record) {
      return notFound('Scene not found');
    }
    const tagged = <T extends { scene_tag_slugs: string[] }>(item: T) =>
      item.scene_tag_slugs.includes(slug);
    return of({
      scene: this.sceneSummary(record.scene),
      venues: this.activeVenues().filter(tagged),
      organizations: this.activeOrganizations().filter(tagged),
      places: this.activePlaces().filter(tagged),
      links: structuredClone(record.links),
      intro_page: this.publishedPageInSection(slug, 'scenes'),
    });
  }

  getVenues(filters: GuideVenueFilters = {}): Observable<GuideVenue[]> {
    return of(
      this.allVenues()
        .filter((venue) => filters.include_closed || isActive(venue))
        .filter((venue) => !filters.scene || venue.scene_tag_slugs.includes(filters.scene))
        .filter(
          (venue) => !filters.neighborhood || venue.neighborhood_slug === filters.neighborhood,
        ),
    );
  }

  getVenue(slug: string): Observable<GuideVenueDetail> {
    const record = this.data.venues.find(({ venue }) => venue.slug === slug);
    return record ? of(structuredClone(record)) : notFound('Venue not found');
  }

  getOrganizations(filters: GuideOrganizationFilters = {}): Observable<GuideOrganization[]> {
    return of(
      this.activeOrganizations()
        .filter((organization) => !filters.kind || organization.org_kind === filters.kind)
        .filter(
          (organization) => !filters.scene || organization.scene_tag_slugs.includes(filters.scene),
        )
        .filter(
          (organization) =>
            !filters.neighborhood || organization.neighborhood_slug === filters.neighborhood,
        ),
    );
  }

  getOrganization(slug: string): Observable<GuideOrganizationDetail> {
    const record = this.data.organizations.find(({ organization }) => organization.slug === slug);
    return record ? of(structuredClone(record)) : notFound('Organization not found');
  }

  getPlaces(): Observable<GuidePlace[]> {
    return of(this.activePlaces());
  }

  getPlace(slug: string): Observable<GuidePlaceDetail> {
    const record = this.data.places.find(({ place }) => place.slug === slug);
    return record ? of(structuredClone(record)) : notFound('Place not found');
  }

  getServices(kind?: GuideServiceKind): Observable<GuideService[]> {
    return of(
      this.data.services
        .filter(isActive)
        .filter((service) => !kind || service.service_kind === kind)
        .sort(serviceOrder)
        .map((service) => structuredClone(service)),
    );
  }

  getNeighborhoods(): Observable<GuideNeighborhood[]> {
    return of(
      this.data.neighborhoods.map((neighborhood) => this.neighborhoodSummary(neighborhood)),
    );
  }

  getNeighborhood(slug: string): Observable<GuideNeighborhoodDetail> {
    const neighborhood = this.data.neighborhoods.find((candidate) => candidate.slug === slug);
    if (!neighborhood) {
      return notFound('Neighborhood not found');
    }
    const here = <T extends { neighborhood_slug: string }>(item: T) =>
      item.neighborhood_slug === slug;
    return of({
      neighborhood: this.neighborhoodSummary(neighborhood),
      venues: this.activeVenues().filter(here),
      organizations: this.activeOrganizations().filter(here),
      places: this.activePlaces().filter(here),
      rundown_page: this.publishedPageInSection(slug, 'new-to-seattle'),
    });
  }

  // Closed only: a moved listing points to its new home rather than being gone.
  getGraveyard(): Observable<GuideGraveyard> {
    return of({
      venues: this.allVenues().filter((venue) => venue.status === 'closed'),
      organizations: this.data.organizations
        .map(({ organization }) => organization)
        .filter((organization) => organization.status === 'closed')
        .sort(organizationOrder)
        .map((organization) => structuredClone(organization)),
    });
  }

  getGuidePage(slug: string): Observable<GuidePage> {
    const page = this.publishedPages().find((candidate) => candidate.slug === slug);
    return page ? of(page) : notFound('Guide page not found');
  }

  getGuidePagesBySection(section: GuidePageSection): Observable<GuidePageSummary[]> {
    if (!section) {
      return httpError(400, 'Validation Error', 'A section is required');
    }
    return of(
      this.publishedPages()
        .filter((page) => page.section === section)
        .sort((a, b) => a.title.localeCompare(b.title) || a.id - b.id)
        .map((page) => ({
          id: page.id,
          slug: page.slug,
          title: page.title,
          section: page.section,
          adult: page.adult,
          publish_at_us: page.publish_at_us,
          status: page.status,
          last_verified_at_us: page.last_verified_at_us,
        })),
    );
  }

  getVenueEvents(venueId: number): Observable<GuideEvent[]> {
    return of(this.upcomingEvents((event) => event.venue_id === venueId));
  }

  getSceneEvents(sceneSlug: string): Observable<GuideEvent[]> {
    return of(this.upcomingEvents((event) => event.scene_tag_slugs.includes(sceneSlug)));
  }

  // ---- the server's rules ----

  // Every venue, retired ones included, by name; copies.
  private allVenues(): GuideVenue[] {
    return this.data.venues
      .map(({ venue }) => venue)
      .sort(venueOrder)
      .map((venue) => structuredClone(venue));
  }

  private activeVenues(): GuideVenue[] {
    return this.allVenues().filter(isActive);
  }

  private activeOrganizations(): GuideOrganization[] {
    return this.data.organizations
      .map(({ organization }) => organization)
      .filter(isActive)
      .sort(organizationOrder)
      .map((organization) => structuredClone(organization));
  }

  private activePlaces(): GuidePlace[] {
    return this.data.places
      .map(({ place }) => place)
      .filter(isActive)
      .sort(placeOrder)
      .map((place) => structuredClone(place));
  }

  // A scene's counts are its landing page's list sizes.
  private sceneSummary(scene: GuideMockSceneTag): GuideScene {
    const tagged = (item: { scene_tag_slugs: string[] }) =>
      item.scene_tag_slugs.includes(scene.slug);
    return {
      ...scene,
      venue_count: this.activeVenues().filter(tagged).length,
      organization_count: this.activeOrganizations().filter(tagged).length,
      place_count: this.activePlaces().filter(tagged).length,
    };
  }

  private neighborhoodSummary(neighborhood: GuideMockNeighborhood): GuideNeighborhood {
    const here = (item: { neighborhood_slug: string }) =>
      item.neighborhood_slug === neighborhood.slug;
    return {
      ...neighborhood,
      venue_count: this.activeVenues().filter(here).length,
      organization_count: this.activeOrganizations().filter(here).length,
      place_count: this.activePlaces().filter(here).length,
    };
  }

  // The pages published as of now, marked so; copies.
  private publishedPages(): GuidePage[] {
    const now = nowUs();
    return this.data.pages
      .filter((page) => isGuidePagePublished(page, now))
      .map((page) => ({ ...structuredClone(page), published: true }));
  }

  // The slug conventions' lookup: a scene's intro, a neighborhood's rundown.
  private publishedPageInSection(slug: string, section: GuidePageSection): GuidePage | null {
    return (
      this.publishedPages().find((page) => page.slug === slug && page.section === section) ?? null
    );
  }

  // Stream A's rule: events starting in the default window, in start order.
  private upcomingEvents(matches: (event: GuideEvent) => boolean): GuideEvent[] {
    const now = nowUs();
    return this.data.events
      .filter(
        (event) =>
          event.starts_at_us >= now &&
          event.starts_at_us < now + UPCOMING_WINDOW_US &&
          matches(event),
      )
      .sort((a, b) => a.starts_at_us - b.starts_at_us || a.id - b.id)
      .map((event) => structuredClone(event));
  }
}
