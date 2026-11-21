import { inject, Injectable, Injector } from '@angular/core';
import { forkJoin, map, Observable, of, switchMap, throwError } from 'rxjs';
import { FilterPair, HONUWARE_CRUD_ACCESS } from '@honuware/ui/access';
import { AuthService } from '@honuware/ui/auth';
import { SubmissionProblem } from '../event-submissions-access/event-submissions-access';
import { GallerySettingsMockStore } from '../mock/gallery-settings-mock-store';
import { PastEventSettingsMockStore } from '../mock/past-event-settings-mock-store';
import { SubmissionSettingsMockStore } from '../mock/submission-settings-mock-store';
import { dataResultsToRecords, EventStatus } from '../types/events.types';
import { CALENDAR_DEFAULT_DURATION_US } from '../utils/event-links';
import { nowUs } from '../utils/events-date.utils';
import {
  clampCuratorFinderLimit,
  CuratorEventMatch,
  EventsAdminAccess,
  GALLERY_ALT_TEXT_MAX,
  GALLERY_CAPTION_MAX,
  GALLERY_CREDIT_MAX,
  GALLERY_FULL,
  GalleryProblem,
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
} from './events-admin-access';

// Offline mock — the `local` configuration wires this so `ng serve` runs with no
// backend. A review sets the event rows' status in the in-memory CRUD store,
// so the editor's pending queue behaves as it does live, and a submitted
// event's reason and message land on its submission. The decision "email" is
// sent (decision_emailed) once a submission's last pending date is decided,
// once. The settings live in the shared SubmissionSettingsMockStore,
// PastEventSettingsMockStore and GallerySettingsMockStore. A gallery row goes
// into the CRUD store by the server's checks and order, its consent record
// the signed-in curator. (The public events pages read their own demo
// dataset, so an offline approval or gallery photo doesn't show up on them;
// a past-events window does.)
@Injectable()
export class EventsAdminMockAccess implements EventsAdminAccess {
  private crud = inject(HONUWARE_CRUD_ACCESS);
  // Looked up only when a gallery row needs its consent record, so the
  // settings cards' specs needn't provide a session.
  private injector = inject(Injector);
  private settingsStore = inject(SubmissionSettingsMockStore);
  private pastSettingsStore = inject(PastEventSettingsMockStore);
  private gallerySettingsStore = inject(GallerySettingsMockStore);

  reviewEvent(
    eventId: number,
    action: ReviewAction,
    decision: ReviewDecision,
  ): Observable<ReviewEventResult> {
    const status = statusFor(action);
    return this.rows('events', [{ column_name: 'id', column_value: String(eventId) }]).pipe(
      switchMap(([event]) =>
        this.decide(String(eventId), status, decision).pipe(
          switchMap(() =>
            event?.['submission_id']
              ? this.afterDecision(event['submission_id'], action, decision)
              : of(false),
          ),
        ),
      ),
      map((emailed) => ({ event_id: eventId, status, decision_emailed: emailed })),
    );
  }

  reviewSubmission(
    submissionId: number,
    action: ReviewAction,
    decision: ReviewDecision,
  ): Observable<ReviewSubmissionResult> {
    const id = String(submissionId);
    const status = statusFor(action);
    return forkJoin({
      submission: this.rows('event_submissions', [{ column_name: 'id', column_value: id }]),
      pending: this.rows('events', [
        { column_name: 'submission_id', column_value: id },
        { column_name: 'status', column_value: 'pending' },
      ]),
    }).pipe(
      switchMap(({ submission, pending }) => {
        if (submission.length === 0) {
          return throwError(() => problem(404, 'SUBMISSION_NOT_FOUND', 'Submission not found'));
        }
        if (pending.length === 0) {
          return throwError(() =>
            problem(400, 'NOTHING_PENDING', 'Nothing in this submission is waiting for review'),
          );
        }
        const decidedIds = pending.map((event) => event['id']);
        return forkJoin(decidedIds.map((eventId) => this.decide(eventId, status, decision))).pipe(
          switchMap(() => this.afterDecision(id, action, decision)),
          map((emailed) => ({
            submission_id: submissionId,
            status,
            decided_event_ids: decidedIds.map(Number),
            decision_emailed: emailed,
          })),
        );
      }),
    );
  }

  getSubmissionSettings(): Observable<SubmissionSettings> {
    return of(this.settingsStore.settings());
  }

  saveSubmissionSettings(update: SubmissionSettingsUpdate): Observable<SubmissionSettings> {
    const refused = this.settingsStore.save(update);
    return refused ? throwError(() => refused) : of(this.settingsStore.settings());
  }

  getPastEventSettings(): Observable<PastEventSettings> {
    return of(this.pastSettingsStore.settings());
  }

  savePastEventSettings(update: PastEventSettingsUpdate): Observable<PastEventSettings> {
    const refused = this.pastSettingsStore.save(update);
    return refused ? throwError(() => refused) : of(this.pastSettingsStore.settings());
  }

  getGallerySettings(): Observable<GallerySettings> {
    return of(this.gallerySettingsStore.settings());
  }

  saveGallerySettings(maxPhotos: number): Observable<GallerySettings> {
    const refused = this.gallerySettingsStore.save(maxPhotos);
    return refused ? throwError(() => refused) : of(this.gallerySettingsStore.settings());
  }

  // EventGalleryHelper::AddGalleryPhoto's checks, in its order: one parent
  // that exists, the alt text, the lengths, the consent box, then the cap.
  // A new row goes one past the highest sort_order in its gallery (0 first).
  addGalleryPhoto(photo: NewGalleryPhoto): Observable<{ id: number }> {
    const parent =
      photo.event_id !== undefined && photo.series_id === undefined
        ? { table: 'events', column: 'event_id', id: String(photo.event_id) }
        : photo.series_id !== undefined && photo.event_id === undefined
          ? { table: 'series', column: 'series_id', id: String(photo.series_id) }
          : null;
    if (!parent) {
      return throwError(() => galleryProblem('ONE_PARENT_REQUIRED', 'event_id'));
    }
    const altText = photo.alt_text.trim();
    const caption = (photo.caption ?? '').trim();
    const credit = (photo.credit ?? '').trim();
    const authData = this.injector.get(AuthService).authData;
    return forkJoin({
      parentRows: this.rows(parent.table, [{ column_name: 'id', column_value: parent.id }]),
      gallery: this.rows('event_gallery_photos', [
        { column_name: parent.column, column_value: parent.id },
      ]),
    }).pipe(
      switchMap(({ parentRows, gallery }) => {
        const refused =
          parentRows.length === 0
            ? galleryProblem('PARENT_NOT_FOUND', parent.column)
            : !altText
              ? galleryProblem('ALT_TEXT_REQUIRED', 'alt_text')
              : characters(altText) > GALLERY_ALT_TEXT_MAX
                ? galleryProblem('TOO_LONG', 'alt_text')
                : characters(caption) > GALLERY_CAPTION_MAX
                  ? galleryProblem('TOO_LONG', 'caption')
                  : characters(credit) > GALLERY_CREDIT_MAX
                    ? galleryProblem('TOO_LONG', 'credit')
                    : !photo.consent_confirmed
                      ? galleryProblem('CONSENT_REQUIRED', 'consent_confirmed')
                      : gallery.length >= this.gallerySettingsStore.maxPhotos
                        ? {
                            ...galleryProblem(GALLERY_FULL, parent.column),
                            max_photos: this.gallerySettingsStore.maxPhotos,
                          }
                        : null;
        if (refused) {
          return throwError(() => refused);
        }
        const now = String(nowUs());
        const nextSortOrder = gallery.reduce(
          (next, row) => Math.max(next, (Number(row['sort_order']) || 0) + 1),
          0,
        );
        return this.crud
          .addItemFetchPrimaryKey({
            table_name: 'event_gallery_photos',
            value: {
              event_id: parent.table === 'events' ? parent.id : '',
              series_id: parent.table === 'series' ? parent.id : '',
              sort_order: String(nextSortOrder),
              alt_text: altText,
              caption,
              credit,
              consent_confirmed_by_person_id: authData.isAuth ? String(authData.personId) : '',
              consent_confirmed_us: now,
              created_us: now,
              updated_us: now,
            },
          })
          .pipe(map((id) => ({ id: Number(id) })));
      }),
    );
  }

  // EventHelper::FindForCurators over the CRUD store: every status; with a
  // search, the titles containing it (any case), those not yet over first,
  // soonest first, then the rest, latest first; without, the next ones not yet
  // over. An event with no end is over two hours after it starts.
  findEvents(search: string, limit?: number): Observable<CuratorEventMatch[]> {
    const text = search.trim().toLocaleLowerCase();
    const count = clampCuratorFinderLimit(limit);
    return forkJoin({
      events: this.allRows('events'),
      venues: this.allRows('venues'),
      series: this.allRows('series'),
    }).pipe(
      map(({ events, venues, series }) => {
        const now = nowUs();
        const venueNames = new Map(venues.map((row) => [row['id'], row['name'] ?? '']));
        const seriesTitles = new Map(series.map((row) => [row['id'], row['title'] ?? '']));
        const notOver = (row: Record<string, string>) =>
          (row['ends_at_us']
            ? Number(row['ends_at_us'])
            : Number(row['starts_at_us']) + CALENDAR_DEFAULT_DURATION_US) >= now;
        const soonestFirst = (a: Record<string, string>, b: Record<string, string>) =>
          Number(a['starts_at_us']) - Number(b['starts_at_us']) ||
          Number(a['id']) - Number(b['id']);
        const latestFirst = (a: Record<string, string>, b: Record<string, string>) =>
          Number(b['starts_at_us']) - Number(a['starts_at_us']) ||
          Number(a['id']) - Number(b['id']);
        const matches = events.filter((row) =>
          (row['title'] ?? '').toLocaleLowerCase().includes(text),
        );
        const chosen = text
          ? [
              ...matches.filter(notOver).sort(soonestFirst),
              ...matches.filter((row) => !notOver(row)).sort(latestFirst),
            ]
          : events.filter(notOver).sort(soonestFirst);
        return chosen.slice(0, count).map((row): CuratorEventMatch => ({
          id: Number(row['id']),
          title: row['title'] ?? '',
          starts_at_us: Number(row['starts_at_us']),
          ends_at_us: row['ends_at_us'] ? Number(row['ends_at_us']) : null,
          venue_name: venueNames.get(row['venue_id']) ?? '',
          location_text: row['location_text'] ?? '',
          series_title: seriesTitles.get(row['series_id']) ?? '',
          status: (row['status'] ?? 'pending') as EventStatus,
        }));
      }),
    );
  }

  private allRows(table: string): Observable<Record<string, string>[]> {
    return this.crud
      .getFilteredTableRows(table, 'id', true, FINDER_PAGE_SIZE, 0, [])
      .pipe(map(dataResultsToRecords));
  }

  private decide(eventId: string, status: string, decision: ReviewDecision): Observable<void> {
    return this.crud.updateItem({
      table_name: 'events',
      column_name: 'id',
      column_value: eventId,
      value: { status, review_notes: decision.notes },
    });
  }

  // The server's rules after a submitted date is decided: a reason is stored
  // only with a rejection, a message only when one is sent, and the email goes
  // once nothing is pending, if it hasn't gone already.
  private afterDecision(
    submissionId: string,
    action: ReviewAction,
    decision: ReviewDecision,
  ): Observable<boolean> {
    const changes: Record<string, string> = {};
    if (action === 'reject' && decision.reason) {
      changes['reject_reason'] = decision.reason;
    }
    if (decision.message_to_submitter !== undefined) {
      changes['message_to_submitter'] = decision.message_to_submitter;
    }
    const store$ = Object.keys(changes).length
      ? this.updateSubmission(submissionId, changes)
      : of(undefined);
    return store$.pipe(
      switchMap(() =>
        forkJoin({
          submission: this.rows('event_submissions', [
            { column_name: 'id', column_value: submissionId },
          ]),
          pending: this.rows('events', [
            { column_name: 'submission_id', column_value: submissionId },
            { column_name: 'status', column_value: 'pending' },
          ]),
        }),
      ),
      switchMap(({ submission, pending }) => {
        if (pending.length > 0 || !submission[0] || submission[0]['decision_notified_us']) {
          return of(false);
        }
        return this.updateSubmission(submissionId, { decision_notified_us: String(nowUs()) }).pipe(
          map(() => true),
        );
      }),
    );
  }

  private updateSubmission(submissionId: string, value: Record<string, string>): Observable<void> {
    return this.crud.updateItem({
      table_name: 'event_submissions',
      column_name: 'id',
      column_value: submissionId,
      value: { ...value, updated_us: String(nowUs()) },
    });
  }

  private rows(table: string, filters: FilterPair[]): Observable<Record<string, string>[]> {
    return this.crud.getRowByValues(table, filters).pipe(map(dataResultsToRecords));
  }
}

// Every row the demo has, for the finder: its tables are far smaller.
const FINDER_PAGE_SIZE = 1000;

function statusFor(action: ReviewAction): 'approved' | 'rejected' {
  return action === 'approve' ? 'approved' : 'rejected';
}

function problem(status: number, errorCode: string, detail: string): SubmissionProblem {
  return { status, error_code: errorCode, field: '', date_index: null, detail };
}

// The server's gallery refusal: a 400 whose detail is the code itself.
function galleryProblem(errorCode: string, field: string): GalleryProblem {
  return {
    status: 400,
    error_code: errorCode,
    field,
    date_index: null,
    detail: errorCode,
    max_photos: null,
  };
}

// Characters, not UTF-16 units, as the server counts them.
function characters(text: string): number {
  return [...text].length;
}
