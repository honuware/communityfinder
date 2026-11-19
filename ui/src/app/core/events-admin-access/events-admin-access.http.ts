import { HttpClient } from '@angular/common/http';
import { inject, Injectable } from '@angular/core';
import { catchError, Observable, throwError } from 'rxjs';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { toSubmissionProblem } from '../event-submissions-access/event-submissions-access';
import {
  EventsAdminAccess,
  GallerySettings,
  NewGalleryPhoto,
  PastEventSettings,
  PastEventSettingsUpdate,
  ReviewAction,
  ReviewDecision,
  ReviewEventResult,
  ReviewSubmissionResult,
  SubmissionSettings,
  SubmissionSettingsUpdate,
  toGalleryProblem,
} from './events-admin-access';

// The real HTTP implementation. `withCredentials` carries the curator's session
// cookie; the CsrfInterceptor stamps the X-CSRF-Token header on the POSTs and
// the PUT. The server checks the `manage_events` permission before it reads
// the body.
@Injectable()
export class EventsAdminHttpAccess implements EventsAdminAccess {
  private http = inject(HttpClient);
  private apiBase = inject(HONUWARE_API_BASE);

  reviewEvent(
    eventId: number,
    action: ReviewAction,
    decision: ReviewDecision,
  ): Observable<ReviewEventResult> {
    return this.http.post<ReviewEventResult>(
      `${this.apiBase}/admin/review_event/${eventId}`,
      { action, ...decision },
      { withCredentials: true },
    );
  }

  reviewSubmission(
    submissionId: number,
    action: ReviewAction,
    decision: ReviewDecision,
  ): Observable<ReviewSubmissionResult> {
    return this.http
      .post<ReviewSubmissionResult>(
        `${this.apiBase}/admin/review_submission/${submissionId}`,
        { action, ...decision },
        { withCredentials: true },
      )
      .pipe(catchError((error) => throwError(() => toSubmissionProblem(error))));
  }

  getSubmissionSettings(): Observable<SubmissionSettings> {
    return this.http
      .get<SubmissionSettings>(`${this.apiBase}/admin/event_submission_settings`, {
        withCredentials: true,
      })
      .pipe(catchError((error) => throwError(() => toSubmissionProblem(error))));
  }

  saveSubmissionSettings(update: SubmissionSettingsUpdate): Observable<SubmissionSettings> {
    return this.http
      .put<SubmissionSettings>(`${this.apiBase}/admin/event_submission_settings`, update, {
        withCredentials: true,
      })
      .pipe(catchError((error) => throwError(() => toSubmissionProblem(error))));
  }

  getPastEventSettings(): Observable<PastEventSettings> {
    return this.http
      .get<PastEventSettings>(`${this.apiBase}/admin/past_event_settings`, {
        withCredentials: true,
      })
      .pipe(catchError((error) => throwError(() => toSubmissionProblem(error))));
  }

  savePastEventSettings(update: PastEventSettingsUpdate): Observable<PastEventSettings> {
    return this.http
      .put<PastEventSettings>(`${this.apiBase}/admin/past_event_settings`, update, {
        withCredentials: true,
      })
      .pipe(catchError((error) => throwError(() => toSubmissionProblem(error))));
  }

  addGalleryPhoto(photo: NewGalleryPhoto): Observable<{ id: number }> {
    return this.http
      .post<{ id: number }>(`${this.apiBase}/admin/event_gallery_photos`, photo, {
        withCredentials: true,
      })
      .pipe(catchError((error) => throwError(() => toGalleryProblem(error))));
  }

  getGallerySettings(): Observable<GallerySettings> {
    return this.http
      .get<GallerySettings>(`${this.apiBase}/admin/event_gallery_settings`, {
        withCredentials: true,
      })
      .pipe(catchError((error) => throwError(() => toSubmissionProblem(error))));
  }

  saveGallerySettings(maxPhotos: number): Observable<GallerySettings> {
    return this.http
      .put<GallerySettings>(
        `${this.apiBase}/admin/event_gallery_settings`,
        { event_gallery_max_photos: maxPhotos },
        { withCredentials: true },
      )
      .pipe(catchError((error) => throwError(() => toSubmissionProblem(error))));
  }
}
