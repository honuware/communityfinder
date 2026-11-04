import { inject, Injectable } from '@angular/core';
import { forkJoin, map, Observable, of, switchMap, throwError } from 'rxjs';
import { FilterPair, HONUWARE_CRUD_ACCESS } from '@honuware/ui/access';
import { SubmissionProblem } from '../event-submissions-access/event-submissions-access';
import { PastEventSettingsMockStore } from '../mock/past-event-settings-mock-store';
import { SubmissionSettingsMockStore } from '../mock/submission-settings-mock-store';
import { dataResultsToRecords } from '../types/events.types';
import { nowUs } from '../utils/events-date.utils';
import {
  EventsAdminAccess,
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
// once. The settings live in the shared SubmissionSettingsMockStore and
// PastEventSettingsMockStore. (The public events pages read their own demo
// dataset, so an offline approval doesn't show up on them; a past-events
// window does.)
@Injectable()
export class EventsAdminMockAccess implements EventsAdminAccess {
  private crud = inject(HONUWARE_CRUD_ACCESS);
  private settingsStore = inject(SubmissionSettingsMockStore);
  private pastSettingsStore = inject(PastEventSettingsMockStore);

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

function statusFor(action: ReviewAction): 'approved' | 'rejected' {
  return action === 'approve' ? 'approved' : 'rejected';
}

function problem(status: number, errorCode: string, detail: string): SubmissionProblem {
  return { status, error_code: errorCode, field: '', date_index: null, detail };
}
