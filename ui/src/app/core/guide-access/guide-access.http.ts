import { HttpClient } from '@angular/common/http';
import { inject, Injectable } from '@angular/core';
import { map, Observable } from 'rxjs';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { GuideAccess } from './guide-access';
import {
  buildGuideQueryString,
  GuideEvent,
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
  normalizeGuideEvents,
  normalizeGuideGraveyard,
  normalizeGuideNeighborhoodDetail,
  normalizeGuideNeighborhoods,
  normalizeGuideOrganizationDetail,
  normalizeGuideOrganizations,
  normalizeGuidePage,
  normalizeGuidePageSummaries,
  normalizeGuidePlaceDetail,
  normalizeGuidePlaces,
  normalizeGuideSceneDetail,
  normalizeGuideScenes,
  normalizeGuideServices,
  normalizeGuideVenueDetail,
  normalizeGuideVenues,
} from '../types/guide.types';

// The real HTTP implementation, over GD2's /api/guide/* plus Stream A's public
// /api/events/upcoming. `withCredentials` matches the other access layers (the
// session cookie rides every request, which is how a manage_guide curator's
// preview of an unpublished page works); the base path comes from the
// library's HONUWARE_API_BASE token (default `/api`, proxied to the server in
// dev). Every reply goes through a normalize* function.
@Injectable()
export class GuideHttpAccess implements GuideAccess {
  private http = inject(HttpClient);
  private apiBase = inject(HONUWARE_API_BASE);

  getScenes(): Observable<GuideScene[]> {
    return this.read('/guide/scenes').pipe(map(normalizeGuideScenes));
  }

  getScene(slug: string): Observable<GuideSceneDetail> {
    return this.read(`/guide/scenes/${encodeURIComponent(slug)}`).pipe(
      map(normalizeGuideSceneDetail),
    );
  }

  getVenues(filters: GuideVenueFilters = {}): Observable<GuideVenue[]> {
    const query = buildGuideQueryString([
      ['scene', filters.scene],
      ['neighborhood', filters.neighborhood],
      ['include_closed', filters.include_closed],
    ]);
    return this.read(`/guide/venues${query}`).pipe(map(normalizeGuideVenues));
  }

  getVenue(slug: string): Observable<GuideVenueDetail> {
    return this.read(`/guide/venues/${encodeURIComponent(slug)}`).pipe(
      map(normalizeGuideVenueDetail),
    );
  }

  getOrganizations(filters: GuideOrganizationFilters = {}): Observable<GuideOrganization[]> {
    const query = buildGuideQueryString([
      ['kind', filters.kind],
      ['scene', filters.scene],
      ['neighborhood', filters.neighborhood],
    ]);
    return this.read(`/guide/orgs${query}`).pipe(map(normalizeGuideOrganizations));
  }

  getOrganization(slug: string): Observable<GuideOrganizationDetail> {
    return this.read(`/guide/orgs/${encodeURIComponent(slug)}`).pipe(
      map(normalizeGuideOrganizationDetail),
    );
  }

  getPlaces(): Observable<GuidePlace[]> {
    return this.read('/guide/places').pipe(map(normalizeGuidePlaces));
  }

  getPlace(slug: string): Observable<GuidePlaceDetail> {
    return this.read(`/guide/places/${encodeURIComponent(slug)}`).pipe(
      map(normalizeGuidePlaceDetail),
    );
  }

  getServices(kind?: GuideServiceKind): Observable<GuideService[]> {
    const query = buildGuideQueryString([['kind', kind]]);
    return this.read(`/guide/services${query}`).pipe(map(normalizeGuideServices));
  }

  getNeighborhoods(): Observable<GuideNeighborhood[]> {
    return this.read('/guide/neighborhoods').pipe(map(normalizeGuideNeighborhoods));
  }

  getNeighborhood(slug: string): Observable<GuideNeighborhoodDetail> {
    return this.read(`/guide/neighborhoods/${encodeURIComponent(slug)}`).pipe(
      map(normalizeGuideNeighborhoodDetail),
    );
  }

  getGraveyard(): Observable<GuideGraveyard> {
    return this.read('/guide/graveyard').pipe(map(normalizeGuideGraveyard));
  }

  getGuidePage(slug: string): Observable<GuidePage> {
    return this.read(`/guide/pages/${encodeURIComponent(slug)}`).pipe(map(normalizeGuidePage));
  }

  getGuidePagesBySection(section: GuidePageSection): Observable<GuidePageSummary[]> {
    const query = buildGuideQueryString([['section', section]]);
    return this.read(`/guide/pages${query}`).pipe(map(normalizeGuidePageSummaries));
  }

  getVenueEvents(venueId: number): Observable<GuideEvent[]> {
    const query = buildGuideQueryString([['venue_id', venueId]]);
    return this.read(`/events/upcoming${query}`).pipe(map(normalizeGuideEvents));
  }

  getSceneEvents(sceneSlug: string): Observable<GuideEvent[]> {
    const query = buildGuideQueryString([['scene', sceneSlug]]);
    return this.read(`/events/upcoming${query}`).pipe(map(normalizeGuideEvents));
  }

  private read(path: string): Observable<unknown> {
    return this.http.get<unknown>(`${this.apiBase}${path}`, { withCredentials: true });
  }
}
