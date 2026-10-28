import { HttpErrorResponse } from '@angular/common/http';
import { Injectable } from '@angular/core';
import { Observable, of, throwError } from 'rxjs';
import { CommunityAccess, HealthInfo, SiteInfo } from './community-access';
import { buildEventsMockData } from '../mock/events-mock-data';
import {
  Category,
  EventSeries,
  EventSeriesDetail,
  Neighborhood,
  PublicEvent,
  SceneTag,
  UPCOMING_DEFAULT_WINDOW_DAYS,
  UPCOMING_LOOK_BACK_DAYS,
  UPCOMING_MAX_WINDOW_DAYS,
  UpcomingEvents,
  UpcomingEventsQuery,
  US_PER_DAY,
} from '../types/events.types';
import { nowUs } from '../utils/events-date.utils';

// The server's window clamp (EV1's EventHelper ClampUpcomingWindow), so the
// mock serves the same window the real API would.
function clampUpcomingWindow(now: number, fromUs: number, toUs: number) {
  const earliestUs = now - UPCOMING_LOOK_BACK_DAYS * US_PER_DAY;
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

// Offline mock — the `local` configuration wires this so `ng serve` runs with no
// backend. Returns a healthy status and stub branding, and serves the events
// demo dataset (core/mock/events-mock-data) by the server's rules: approved
// events only, the same window clamp, the same filters, 404s for what isn't
// public. The dataset is built once, relative to when the app started.
@Injectable()
export class CommunityMockAccess implements CommunityAccess {
  private readonly data = buildEventsMockData(nowUs());

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

  getUpcomingEvents(query: UpcomingEventsQuery = {}): Observable<UpcomingEvents> {
    const window = clampUpcomingWindow(nowUs(), query.from_us ?? 0, query.to_us ?? 0);
    return of({
      items: this.upcoming(window.fromUs, window.toUs, query),
      from_us: window.fromUs,
      to_us: window.toUs,
    });
  }

  getEvent(id: number): Observable<PublicEvent> {
    const record = this.data.events.find(
      (candidate) => candidate.event.id === id && candidate.status === 'approved',
    );
    return record ? of(structuredClone(record.event)) : notFound('Event not found');
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
    const window = clampUpcomingWindow(nowUs(), 0, 0);
    return of({
      series: structuredClone(series),
      upcoming: this.upcoming(window.fromUs, window.toUs, { series_id: series.id }),
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

  // Approved events starting in [fromUs, toUs), filtered, in start order. The
  // events' slug lists are already the effective ones (own, else the series').
  private upcoming(fromUs: number, toUs: number, query: UpcomingEventsQuery): PublicEvent[] {
    return this.data.events
      .filter(({ status }) => status === 'approved')
      .map(({ event }) => event)
      .filter(
        (event) =>
          event.starts_at_us >= fromUs &&
          event.starts_at_us < toUs &&
          (!query.venue_id || event.venue_id === query.venue_id) &&
          (!query.series_id || event.series_id === query.series_id) &&
          (!query.neighborhood || event.neighborhood_slug === query.neighborhood) &&
          (!query.category || event.category_slugs.includes(query.category)) &&
          (!query.scene || event.scene_tag_slugs.includes(query.scene)),
      )
      .sort((a, b) => a.starts_at_us - b.starts_at_us || a.id - b.id)
      .map((event) => structuredClone(event));
  }
}
