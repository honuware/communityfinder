import { HttpClient } from '@angular/common/http';
import { inject, Injectable } from '@angular/core';
import { catchError, map, Observable, throwError } from 'rxjs';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import {
  EventSubmissionsAccess,
  MySubmission,
  MySubmissionDate,
  SubmissionDateState,
  SubmissionRelationship,
  SubmissionRequest,
  SubmissionResult,
  toSubmissionProblem,
  WithdrawResult,
} from './event-submissions-access';

// The real HTTP implementation. `withCredentials` carries the session cookie;
// the CsrfInterceptor stamps the X-CSRF-Token header on the POSTs. Errors come
// back as SubmissionProblems, so pages read `error_code` / `field` /
// `date_index` without knowing the problem-body format.
@Injectable()
export class EventSubmissionsHttpAccess implements EventSubmissionsAccess {
  private http = inject(HttpClient);
  private apiBase = inject(HONUWARE_API_BASE);

  getStatus(): Observable<boolean> {
    return this.http
      .get<{ accepting_submissions?: unknown }>(`${this.apiBase}/event_submissions/status`, {
        withCredentials: true,
      })
      .pipe(
        map((status) => status?.accepting_submissions !== false),
        catchError((error) => throwError(() => toSubmissionProblem(error))),
      );
  }

  submit(request: SubmissionRequest): Observable<SubmissionResult> {
    return this.http
      .post<SubmissionResult>(`${this.apiBase}/event_submissions`, request, {
        withCredentials: true,
      })
      .pipe(catchError((error) => throwError(() => toSubmissionProblem(error))));
  }

  getMine(): Observable<MySubmission[]> {
    return this.http
      .get<{ items?: unknown }>(`${this.apiBase}/event_submissions/mine`, {
        withCredentials: true,
      })
      .pipe(
        map((reply) => (Array.isArray(reply?.items) ? reply.items.map(normalizeMySubmission) : [])),
        catchError((error) => throwError(() => toSubmissionProblem(error))),
      );
  }

  withdraw(submissionId: number): Observable<WithdrawResult> {
    return this.http
      .post<WithdrawResult>(
        `${this.apiBase}/event_submissions/${submissionId}/withdraw`,
        {},
        { withCredentials: true },
      )
      .pipe(catchError((error) => throwError(() => toSubmissionProblem(error))));
  }
}

type Raw = Record<string, unknown>;

function asRaw(value: unknown): Raw {
  return value !== null && typeof value === 'object' ? (value as Raw) : {};
}

function toText(value: unknown): string {
  return value === null || value === undefined ? '' : String(value);
}

function normalizeDate(value: unknown): MySubmissionDate {
  const raw = asRaw(value);
  const endsAtUs = raw['ends_at_us'];
  return {
    event_id: Number(raw['event_id']) || 0,
    title: toText(raw['title']),
    starts_at_us: Number(raw['starts_at_us']) || 0,
    ends_at_us: endsAtUs === null || endsAtUs === undefined ? null : Number(endsAtUs),
    state: toText(raw['state']) as SubmissionDateState,
  };
}

export function normalizeMySubmission(value: unknown): MySubmission {
  const raw = asRaw(value);
  return {
    id: Number(raw['id']) || 0,
    created_us: Number(raw['created_us']) || 0,
    relationship: (toText(raw['relationship']) || 'other') as SubmissionRelationship,
    regular_thing: raw['regular_thing'] === true,
    submitter_note: toText(raw['submitter_note']),
    reject_reason: toText(raw['reject_reason']),
    message_to_submitter: toText(raw['message_to_submitter']),
    dates: Array.isArray(raw['dates']) ? raw['dates'].map(normalizeDate) : [],
  };
}
