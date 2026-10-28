import { InjectionToken } from '@angular/core';
import { Observable } from 'rxjs';
import {
  Category,
  EventSeries,
  EventSeriesDetail,
  Neighborhood,
  PublicEvent,
  SceneTag,
  UpcomingEvents,
  UpcomingEventsQuery,
} from '../types/events.types';

export interface HealthInfo {
  status: string;
  version?: string;
}

export interface SiteInfo {
  display_name: string;
  website_url: string;
  logo_url: string;
}

// The CommunityFinder app-specific access seam. @honuware/ui provides the generic
// CrudAccess / AuthAccess / PhotoAccess; this adds the framework calls the library
// doesn't wrap — GET /api/health and GET /api/site_info — and the public events
// domain (EV2 §2.3). All of these are anonymous reads.
export interface CommunityAccess {
  getHealth(): Observable<HealthInfo>;
  getSiteInfo(): Observable<SiteInfo>;

  // GET /api/events/upcoming — approved events in the window (default now →
  // +42 days, at most 92), filtered; the reply says which window was served.
  getUpcomingEvents(query?: UpcomingEventsQuery): Observable<UpcomingEvents>;
  // GET /api/events/<id> — one approved event (a curator also gets pending
  // ones, with `status`); errors with a 404 HttpErrorResponse otherwise.
  getEvent(id: number): Observable<PublicEvent>;
  // GET /api/event_series — the active series, by title.
  getEventSeriesList(): Observable<EventSeries[]>;
  // GET /api/event_series/<slug> — any series, ended ones included, with its
  // upcoming events; a 404 HttpErrorResponse for an unknown slug.
  getEventSeries(slug: string): Observable<EventSeriesDetail>;

  // The shared vocabularies, through the framework's public generic table read
  // (GET /api/get_table_rows/<table>, EV1 OQ1 Option A): the enabled entries,
  // in display order.
  getCategories(): Observable<Category[]>;
  getSceneTags(): Observable<SceneTag[]>;
  getNeighborhoods(): Observable<Neighborhood[]>;
}

export const COMMUNITY_ACCESS = new InjectionToken<CommunityAccess>('COMMUNITY_ACCESS');
