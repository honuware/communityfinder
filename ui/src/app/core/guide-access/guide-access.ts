import { InjectionToken } from '@angular/core';
import { Observable } from 'rxjs';
import {
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
} from '../types/guide.types';

// The guide's access seam (GD3 §1): GD2's anonymous /api/guide/* reads, plus
// the two reads from Stream A's public events API that the guide's venue and
// scene pages show. A detail read for an unknown slug errors with a 404
// HttpErrorResponse, for the page to show "not found".
//
// Every list leaves out retired listings (closed, moved) unless it says
// otherwise; a retired listing's own page still loads by slug.
export interface GuideAccess {
  // GET /api/guide/scenes — the enabled scenes in display order, with counts.
  getScenes(): Observable<GuideScene[]>;
  // GET /api/guide/scenes/<slug> — a scene's landing.
  getScene(slug: string): Observable<GuideSceneDetail>;

  // GET /api/guide/venues?scene&neighborhood&include_closed — by name.
  getVenues(filters?: GuideVenueFilters): Observable<GuideVenue[]>;
  // GET /api/guide/venues/<slug> — any status (a closed venue's page is its
  // graveyard entry).
  getVenue(slug: string): Observable<GuideVenueDetail>;

  // GET /api/guide/orgs?kind&scene&neighborhood — active organizations, by name.
  getOrganizations(filters?: GuideOrganizationFilters): Observable<GuideOrganization[]>;
  // GET /api/guide/orgs/<slug> — any status.
  getOrganization(slug: string): Observable<GuideOrganizationDetail>;

  // GET /api/guide/places — active places, by name.
  getPlaces(): Observable<GuidePlace[]>;
  // GET /api/guide/places/<slug> — any status.
  getPlace(slug: string): Observable<GuidePlaceDetail>;

  // GET /api/guide/services?kind — active services of one kind (none = every
  // kind), by name.
  getServices(kind?: GuideServiceKind): Observable<GuideService[]>;

  // GET /api/guide/neighborhoods — the enabled neighborhoods in display order,
  // with counts.
  getNeighborhoods(): Observable<GuideNeighborhood[]>;
  // GET /api/guide/neighborhoods/<slug> — a neighborhood's page.
  getNeighborhood(slug: string): Observable<GuideNeighborhoodDetail>;

  // GET /api/guide/graveyard — the closed venues and organizations.
  getGraveyard(): Observable<GuideGraveyard>;

  // GET /api/guide/pages/<slug> — a published page (a manage_guide curator
  // also gets unpublished ones, marked published: false).
  getGuidePage(slug: string): Observable<GuidePage>;
  // GET /api/guide/pages?section= — a section's published pages, by title.
  getGuidePagesBySection(section: GuidePageSection): Observable<GuidePageSummary[]>;

  // Stream A's GET /api/events/upcoming?venue_id= / ?scene= — the approved
  // events in its default window (the next 42 days), in start order. Pages
  // show a failure here as a quiet empty list, never an error.
  getVenueEvents(venueId: number): Observable<GuideEvent[]>;
  getSceneEvents(sceneSlug: string): Observable<GuideEvent[]>;
}

export const GUIDE_ACCESS = new InjectionToken<GuideAccess>('GUIDE_ACCESS');
