import { InjectionToken } from '@angular/core';
import { Observable } from 'rxjs';
import {
  Category,
  EventDetail,
  EventSeries,
  EventSeriesDetail,
  EventSettings,
  Neighborhood,
  PastEvents,
  PastEventsQuery,
  PublicEvent,
  SceneTag,
  UpcomingEvents,
  UpcomingEventsQuery,
  VenueChoice,
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

// CM3 Phase 2 §12: a visitor's request to take a photo down. `page_path` is
// the page's path on this site ("/events/12"); `photo` says which ("photo 2
// of 4"), or '' when the message says; `website` is the honeypot, always ''
// from a person.
export interface PhotoRemovalRequest {
  page_path: string;
  page_title: string;
  photo: string;
  message: string;
  reply_email: string;
  website: string;
}

// The server's limits (photo_removal_request_helper.h), so the form can say
// so before sending.
export const PHOTO_REMOVAL_MESSAGE_MAX = 2000;
export const PHOTO_REMOVAL_EMAIL_MAX = 254;
export const PHOTO_REMOVAL_RATE_LIMITED = 'RATE_LIMITED';

// The CommunityFinder app-specific access seam. @honuware/ui provides the generic
// CrudAccess / AuthAccess / PhotoAccess; this adds the framework calls the library
// doesn't wrap — GET /api/health and GET /api/site_info — and the public events
// domain (EV2 §2.3). All of these are anonymous; all but the photo-removal
// request are reads.
export interface CommunityAccess {
  getHealth(): Observable<HealthInfo>;
  getSiteInfo(): Observable<SiteInfo>;

  // POST /api/photo_removal_requests — public, rate-limited per visitor. It
  // emails the site and its curators. Errors with an HttpErrorResponse: a
  // 400 names the `field` and `error_code`, a 429 is RATE_LIMITED, a 503 is
  // nothing sent.
  requestPhotoRemoval(request: PhotoRemovalRequest): Observable<void>;

  // GET /api/events/upcoming — public (approved or archived) events in the
  // window (default now → +42 days, at most 92, back no further than the
  // past-events window), filtered; the reply says which window was served.
  getUpcomingEvents(query?: UpcomingEventsQuery): Observable<UpcomingEvents>;
  // GET /api/events/past — public events that have started, newest first, a
  // page at a time (EV4 §10.2), filtered like the upcoming list.
  getPastEvents(query?: PastEventsQuery): Observable<PastEvents>;
  // GET /api/events/settings — the past-events window and the calendar's
  // default; the defaults when the call fails is the caller's choice.
  getEventSettings(): Observable<EventSettings>;
  // GET /api/events/<id> — one public event, past ones within the window
  // included (a curator also gets pending ones, and older ones, with
  // `status`), with its gallery (CM3); errors with a 404 HttpErrorResponse
  // otherwise.
  getEvent(id: number): Observable<EventDetail>;
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
  // The venues a submitter can pick (EV3 §5.2), through the same public read.
  getVenueChoices(): Observable<VenueChoice[]>;
}

export const COMMUNITY_ACCESS = new InjectionToken<CommunityAccess>('COMMUNITY_ACCESS');
