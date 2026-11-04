import { HttpClient } from '@angular/common/http';
import { inject, Injectable } from '@angular/core';
import { map, Observable } from 'rxjs';
import { DataResults, HONUWARE_API_BASE } from '@honuware/ui/access';
import { CommunityAccess, HealthInfo, SiteInfo } from './community-access';
import {
  buildEventsQueryString,
  buildPastEventsQueryString,
  categoriesFromRows,
  Category,
  EventSeries,
  EventSeriesDetail,
  EventSettings,
  Neighborhood,
  neighborhoodsFromRows,
  normalizeEvent,
  normalizeEventSeriesDetail,
  normalizeEventSeriesList,
  normalizeEventSettings,
  normalizePastEvents,
  normalizeUpcomingEvents,
  PastEvents,
  PastEventsQuery,
  PublicEvent,
  SceneTag,
  sceneTagsFromRows,
  UpcomingEvents,
  UpcomingEventsQuery,
  VenueChoice,
  venueChoicesFromRows,
} from '../types/events.types';

// The real HTTP implementation. `withCredentials` matches the library's HTTP
// access (the session cookie rides every request, which is how a curator's
// preview of a pending event works); the base path comes from the library's
// HONUWARE_API_BASE token (default `/api`, proxied to the server in dev).
// Every reply goes through a normalize* function, so pages get defaulted,
// typed values rather than the server's "" for "none".
@Injectable()
export class CommunityHttpAccess implements CommunityAccess {
  private http = inject(HttpClient);
  private apiBase = inject(HONUWARE_API_BASE);

  getHealth(): Observable<HealthInfo> {
    return this.http.get<HealthInfo>(`${this.apiBase}/health`, { withCredentials: true });
  }

  getSiteInfo(): Observable<SiteInfo> {
    return this.http.get<SiteInfo>(`${this.apiBase}/site_info`, { withCredentials: true });
  }

  getUpcomingEvents(query: UpcomingEventsQuery = {}): Observable<UpcomingEvents> {
    return this.http
      .get<unknown>(`${this.apiBase}/events/upcoming${buildEventsQueryString(query)}`, {
        withCredentials: true,
      })
      .pipe(map(normalizeUpcomingEvents));
  }

  getPastEvents(query: PastEventsQuery = {}): Observable<PastEvents> {
    return this.http
      .get<unknown>(`${this.apiBase}/events/past${buildPastEventsQueryString(query)}`, {
        withCredentials: true,
      })
      .pipe(map(normalizePastEvents));
  }

  getEventSettings(): Observable<EventSettings> {
    return this.http
      .get<unknown>(`${this.apiBase}/events/settings`, { withCredentials: true })
      .pipe(map(normalizeEventSettings));
  }

  getEvent(id: number): Observable<PublicEvent> {
    return this.http
      .get<unknown>(`${this.apiBase}/events/${id}`, { withCredentials: true })
      .pipe(map(normalizeEvent));
  }

  getEventSeriesList(): Observable<EventSeries[]> {
    return this.http
      .get<unknown>(`${this.apiBase}/event_series`, { withCredentials: true })
      .pipe(map(normalizeEventSeriesList));
  }

  getEventSeries(slug: string): Observable<EventSeriesDetail> {
    return this.http
      .get<unknown>(`${this.apiBase}/event_series/${encodeURIComponent(slug)}`, {
        withCredentials: true,
      })
      .pipe(map(normalizeEventSeriesDetail));
  }

  getCategories(): Observable<Category[]> {
    return this.getTableRows('categories').pipe(map(categoriesFromRows));
  }

  getSceneTags(): Observable<SceneTag[]> {
    return this.getTableRows('scene_tags').pipe(map(sceneTagsFromRows));
  }

  getNeighborhoods(): Observable<Neighborhood[]> {
    return this.getTableRows('neighborhoods').pipe(map(neighborhoodsFromRows));
  }

  getVenueChoices(): Observable<VenueChoice[]> {
    return this.getTableRows('venues').pipe(map(venueChoicesFromRows));
  }

  // The same endpoint as the library's CrudAccess.getTableRows, called directly:
  // the library's access proxy runs its calls one at a time, and these public
  // reads have no reason to queue behind sign-in or admin traffic.
  private getTableRows(tableName: string): Observable<DataResults> {
    return this.http.get<DataResults>(`${this.apiBase}/get_table_rows/${tableName}`, {
      withCredentials: true,
    });
  }
}
