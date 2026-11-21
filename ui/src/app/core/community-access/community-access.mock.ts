import { HttpErrorResponse } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable, of, throwError } from 'rxjs';
import {
  CommunityAccess,
  HealthInfo,
  PHOTO_REMOVAL_EMAIL_MAX,
  PHOTO_REMOVAL_MESSAGE_MAX,
  PHOTO_REMOVAL_RATE_LIMITED,
  PhotoRemovalRequest,
  SiteInfo,
} from './community-access';
import {
  buildEventsMockData,
  MockEventRecord,
  mockGalleryFor,
  mockSeriesGallery,
} from '../mock/events-mock-data';
import { PastEventSettingsMockStore } from '../mock/past-event-settings-mock-store';
import {
  Category,
  EventDetail,
  EventSeries,
  EventSeriesDetail,
  EventSettings,
  EventStatus,
  Neighborhood,
  PAST_EVENTS_MAX_PAGE_SIZE,
  PAST_EVENTS_PAGE_SIZE,
  PastEvents,
  PastEventsQuery,
  PublicEvent,
  SceneTag,
  UPCOMING_DEFAULT_WINDOW_DAYS,
  UPCOMING_LOOK_BACK_DAYS,
  UPCOMING_MAX_WINDOW_DAYS,
  UpcomingEvents,
  UpcomingEventsQuery,
  US_PER_DAY,
  VenueChoice,
} from '../types/events.types';
import { CALENDAR_DEFAULT_DURATION_US } from '../utils/event-links';
import { nowUs } from '../utils/events-date.utils';

// The server's window clamp (EV1's EventHelper ClampUpcomingWindow), so the
// mock serves the same window the real API would: back as far as the
// past-events window (EV4 §10), and never less than a day.
function clampUpcomingWindow(now: number, fromUs: number, toUs: number, visibleDays: number) {
  const earliestUs = now - Math.max(UPCOMING_LOOK_BACK_DAYS, visibleDays) * US_PER_DAY;
  const from = fromUs > 0 ? Math.max(fromUs, earliestUs) : now;
  let to = toUs > 0 ? toUs : from + UPCOMING_DEFAULT_WINDOW_DAYS * US_PER_DAY;
  to = Math.min(to, from + UPCOMING_MAX_WINDOW_DAYS * US_PER_DAY);
  // A backwards request is an empty window, not an error.
  return { fromUs: from, toUs: Math.max(to, from) };
}

function notFound(detail: string): Observable<never> {
  return throwError(
    () =>
      new HttpErrorResponse({
        status: 404,
        statusText: 'Not Found',
        error: { status: 404, title: 'Not Found', detail },
      }),
  );
}

function refused(status: number, errorCode: string, field: string, detail: string) {
  return throwError(
    () =>
      new HttpErrorResponse({
        status,
        statusText: status === 429 ? 'Too Many Requests' : 'Bad Request',
        error: { status, detail, error_code: errorCode, ...(field ? { field } : {}) },
      }),
  );
}

// The server's main checks on a photo-removal request
// (CheckPhotoRemovalRequest), in its order, with its messages; null when it
// passes. Lengths are UTF-8 bytes, as the server counts them.
function photoRemovalProblem(request: PhotoRemovalRequest): Observable<never> | null {
  const path = request.page_path.trim();
  const message = request.message.trim();
  const email = request.reply_email.trim();
  if (!path) {
    return refused(400, 'REQUIRED', 'page_path', 'Which page is the photo on?');
  }
  if (!path.startsWith('/') || path.startsWith('//')) {
    return refused(400, 'BAD_PAGE', 'page_path', "The page must be one of this site's.");
  }
  if (!request.page_title.trim()) {
    return refused(400, 'REQUIRED', 'page_title', 'Which page is the photo on?');
  }
  if (utf8Length(message) > PHOTO_REMOVAL_MESSAGE_MAX) {
    return refused(
      400,
      'TOO_LONG',
      'message',
      `Please keep your message under ${PHOTO_REMOVAL_MESSAGE_MAX} characters.`,
    );
  }
  if (!request.photo.trim() && !message) {
    return refused(400, 'REQUIRED', 'message', 'Tell us which photo: its number or what it shows.');
  }
  if (utf8Length(email) > PHOTO_REMOVAL_EMAIL_MAX) {
    return refused(400, 'TOO_LONG', 'reply_email', 'That email address is too long.');
  }
  if (email && !/^[^\s@]+@[^\s@]+\.[^\s@]+$/.test(email)) {
    return refused(400, 'BAD_EMAIL', 'reply_email', "That doesn't look like an email address.");
  }
  return null;
}

function utf8Length(text: string): number {
  return new TextEncoder().encode(text).length;
}

// The server's per-visitor limit: three accepted requests an hour.
const PHOTO_REMOVALS_PER_HOUR = 3;
const HOUR_US = 3_600 * 1_000_000;

// The server's public statuses (EV4 OQ3): approved, and archived.
function isPublicStatus(status: EventStatus): boolean {
  return status === 'approved' || status === 'archived';
}

// The server's end rule: the end, or the start plus two hours without one.
function eventEndUs(event: PublicEvent): number {
  return event.ends_at_us ?? event.starts_at_us + CALENDAR_DEFAULT_DURATION_US;
}

function matchesQuery(event: PublicEvent, query: UpcomingEventsQuery | PastEventsQuery): boolean {
  return (
    (!query.venue_id || event.venue_id === query.venue_id) &&
    (!query.series_id || event.series_id === query.series_id) &&
    (!query.neighborhood || event.neighborhood_slug === query.neighborhood) &&
    (!query.category || event.category_slugs.includes(query.category)) &&
    (!query.scene || event.scene_tag_slugs.includes(query.scene))
  );
}

// Offline mock — the `local` configuration wires this so `ng serve` runs with no
// backend. Returns a healthy status and stub branding, and serves the events
// demo dataset (core/mock/events-mock-data) by the server's rules: approved and
// archived events only, hidden once they ended longer ago than the past-events
// window, the same window clamp, the same filters, the same paging, 404s for
// what isn't public. The dataset is built once, relative to when the app
// started. The window comes from the shared PastEventSettingsMockStore, which
// the admin mock saves to; a spec that builds the mock by hand gets its own.
@Injectable()
export class CommunityMockAccess implements CommunityAccess {
  private readonly data = buildEventsMockData(nowUs());

  constructor(
    private readonly pastEventSettings: PastEventSettingsMockStore = new PastEventSettingsMockStore(),
  ) {}

  getHealth(): Observable<HealthInfo> {
    return of({ status: 'ok', version: 'mock' });
  }

  getSiteInfo(): Observable<SiteInfo> {
    return of({
      display_name: 'CommunityFinder (mock)',
      website_url: 'http://localhost:4201/',
      logo_url: '',
    });
  }

  // What offline "sent": nothing leaves the browser. The honeypot is quietly
  // dropped, as the server drops it.
  readonly photoRemovalRequests: PhotoRemovalRequest[] = [];
  private photoRemovalTimesUs: number[] = [];

  requestPhotoRemoval(request: PhotoRemovalRequest): Observable<void> {
    if (request.website) {
      return of(undefined);
    }
    const problem = photoRemovalProblem(request);
    if (problem) {
      return problem;
    }
    const now = nowUs();
    this.photoRemovalTimesUs = this.photoRemovalTimesUs.filter((at) => at > now - HOUR_US);
    if (this.photoRemovalTimesUs.length >= PHOTO_REMOVALS_PER_HOUR) {
      return refused(
        429,
        PHOTO_REMOVAL_RATE_LIMITED,
        '',
        "You've sent several requests in the last hour. Please try again later, or email us.",
      );
    }
    this.photoRemovalTimesUs.push(now);
    this.photoRemovalRequests.push(structuredClone(request));
    return of(undefined);
  }

  getUpcomingEvents(query: UpcomingEventsQuery = {}): Observable<UpcomingEvents> {
    const window = clampUpcomingWindow(
      nowUs(),
      query.from_us ?? 0,
      query.to_us ?? 0,
      this.pastEventSettings.visibleDays,
    );
    return of({
      items: this.upcoming(window.fromUs, window.toUs, query),
      from_us: window.fromUs,
      to_us: window.toUs,
    });
  }

  // Newest first; ties by id, newest first too.
  getPastEvents(query: PastEventsQuery = {}): Observable<PastEvents> {
    const now = nowUs();
    const requested = query.limit && query.limit > 0 ? query.limit : PAST_EVENTS_PAGE_SIZE;
    const limit = Math.min(Math.max(requested, 1), PAST_EVENTS_MAX_PAGE_SIZE);
    const offset = Math.max(query.offset ?? 0, 0);
    const matches = this.visible()
      .filter((event) => event.starts_at_us < now && matchesQuery(event, query))
      .sort((a, b) => b.starts_at_us - a.starts_at_us || b.id - a.id);
    return of({
      items: matches.slice(offset, offset + limit).map((event) => structuredClone(event)),
      has_more: matches.length > offset + limit,
      since_us: this.cutoffUs(now),
    });
  }

  getEventSettings(): Observable<EventSettings> {
    return of(this.pastEventSettings.publicSettings());
  }

  // With its gallery (CM3): its own, else its series'. Only public events
  // are served here, so the gallery is always the visible one.
  getEvent(id: number): Observable<EventDetail> {
    const event = this.visible().find((candidate) => candidate.id === id);
    return event
      ? of({ ...structuredClone(event), gallery: mockGalleryFor(event.id, event.series_id) })
      : notFound('Event not found');
  }

  getEventSeriesList(): Observable<EventSeries[]> {
    const active = this.data.series
      .filter((series) => series.status !== 'closed' && series.status !== 'moved')
      .sort((a, b) => a.title.localeCompare(b.title) || a.id - b.id);
    return of(structuredClone(active));
  }

  getEventSeries(slug: string): Observable<EventSeriesDetail> {
    const series = this.data.series.find((candidate) => candidate.slug === slug);
    if (!series) {
      return notFound('Series not found');
    }
    const window = clampUpcomingWindow(nowUs(), 0, 0, this.pastEventSettings.visibleDays);
    return of({
      series: structuredClone(series),
      upcoming: this.upcoming(window.fromUs, window.toUs, { series_id: series.id }),
      gallery: mockSeriesGallery(series.id),
    });
  }

  getCategories(): Observable<Category[]> {
    return of(structuredClone(this.data.categories));
  }

  getSceneTags(): Observable<SceneTag[]> {
    return of(structuredClone(this.data.sceneTags));
  }

  getNeighborhoods(): Observable<Neighborhood[]> {
    return of(structuredClone(this.data.neighborhoods));
  }

  getVenueChoices(): Observable<VenueChoice[]> {
    return of(
      this.data.venues
        .map(({ id, name, address }) => ({ id, name, address }))
        .sort((a, b) => a.name.localeCompare(b.name)),
    );
  }

  // Visible events starting in [fromUs, toUs), filtered, in start order. The
  // events' slug lists are already the effective ones (own, else the series').
  private upcoming(fromUs: number, toUs: number, query: UpcomingEventsQuery): PublicEvent[] {
    return this.visible()
      .filter(
        (event) =>
          event.starts_at_us >= fromUs && event.starts_at_us < toUs && matchesQuery(event, query),
      )
      .sort((a, b) => a.starts_at_us - b.starts_at_us || a.id - b.id)
      .map((event) => structuredClone(event));
  }

  // What the public can see (EventHelper's IsPublicEvent): a public status,
  // and not over for longer than the window.
  private visible(): PublicEvent[] {
    const cutoffUs = this.cutoffUs(nowUs());
    return this.data.events
      .filter(
        ({ status, event }: MockEventRecord) =>
          isPublicStatus(status) && eventEndUs(event) >= cutoffUs,
      )
      .map(({ event }) => event);
  }

  private cutoffUs(now: number): number {
    return now - this.pastEventSettings.visibleDays * US_PER_DAY;
  }
}
