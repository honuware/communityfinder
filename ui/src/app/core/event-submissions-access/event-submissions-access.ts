import { HttpErrorResponse } from '@angular/common/http';
import { InjectionToken } from '@angular/core';
import { Observable } from 'rxjs';
import { VideoRef } from '../types/events.types';

// The submitter's side of the submission pipeline (EV3 §4.1, §4.6): the form,
// "My submissions", withdraw, and whether the form is open. Field names mirror
// the wire. Times are `_us` UTC instants.

export type SubmissionRelationship = 'organizer' | 'venue' | 'performer' | 'attendee' | 'other';

// One date: when it starts, and optionally when it ends (at most 4 days later).
export interface SubmissionDateInput {
  starts_at_us: number;
  ends_at_us?: number;
}

// POST /api/event_submissions. Optional fields left out mean "not given".
// `website` is the hidden honeypot: only a bot fills it in.
export interface SubmissionRequest {
  title: string;
  dates: SubmissionDateInput[];
  venue_id?: number;
  location_text?: string;
  description?: string;
  url?: string;
  cost_text?: string;
  category_slugs?: string[];
  scene_tag_slugs?: string[];
  regular_thing?: boolean;
  relationship?: SubmissionRelationship;
  submitter_note?: string;
  // CM3: one YouTube or Vimeo link, stored canonical on every date. Refused
  // with BAD_VIDEO_URL or VIDEO_HOST_NOT_SUPPORTED on field "video_url".
  video_url?: string;
  agreed: boolean;
  website?: string;
}

// CM3 §4.4: a flyer is a photo, never a vector, from a stranger.
export type FlyerImageType = 'jpeg' | 'png';

// POST /api/event_submissions/<id>/photo/<type>: the dates it went on (every
// one still waiting).
export interface FlyerResult {
  submission_id: number;
  event_ids: number[];
}

// An approved event that looks like one of the submitted dates.
export interface PossibleDuplicate {
  // Which submitted date it looks like, counting from 0.
  date_index: number;
  event_id: number;
  title: string;
  starts_at_us: number;
}

// The honeypot's answer looks like success, with submission_id 0.
export interface SubmissionResult {
  submission_id: number;
  event_ids: number[];
  ignored_category_slugs: string[];
  ignored_scene_tag_slugs: string[];
  possible_duplicates: PossibleDuplicate[];
}

// waiting = pending · listed = approved · not_listed = a curator turned it
// down · withdrawn = the submitter took it back · past = archived (EV4).
export type SubmissionDateState = 'waiting' | 'listed' | 'not_listed' | 'withdrawn' | 'past';

export interface MySubmissionDate {
  event_id: number;
  title: string;
  starts_at_us: number;
  ends_at_us: number | null;
  state: SubmissionDateState;
  // CM3: whether the date has a photo of its own (the flyer, or one a
  // curator added; not its venue's), and its video, or null.
  has_photo: boolean;
  video: VideoRef | null;
}

// GET /api/event_submissions/mine: newest first, dates in start order. No
// curator note is ever included.
export interface MySubmission {
  id: number;
  created_us: number;
  relationship: SubmissionRelationship;
  regular_thing: boolean;
  submitter_note: string;
  // '' until a curator gives one.
  reject_reason: string;
  message_to_submitter: string;
  dates: MySubmissionDate[];
}

export interface WithdrawResult {
  submission_id: number;
  withdrawn_event_ids: number[];
}

// A refusal from the submission routes, read from the server's problem body:
// `error_code` names it (TITLE_LENGTH, RATE_LIMITED, SUBMISSIONS_PAUSED, …),
// `field` is the form field it's about (by its JSON name), `date_index` the
// date, and `detail` is the plain-language message to show.
export interface SubmissionProblem {
  status: number;
  error_code: string;
  field: string;
  date_index: number | null;
  detail: string;
}

export const SUBMISSIONS_PAUSED = 'SUBMISSIONS_PAUSED';
export const EMAIL_NOT_VERIFIED = 'EMAIL_NOT_VERIFIED';
export const RATE_LIMITED = 'RATE_LIMITED';

const FALLBACK_DETAIL = 'Something went wrong. Please try again in a moment.';

// Any error from these routes → a SubmissionProblem. A problem passes through
// as it is, so the mock can throw one directly.
export function toSubmissionProblem(error: unknown): SubmissionProblem {
  if (isSubmissionProblem(error)) {
    return error;
  }
  const status = error instanceof HttpErrorResponse ? error.status : 0;
  const body = error instanceof HttpErrorResponse ? error.error : null;
  const raw = body !== null && typeof body === 'object' ? (body as Record<string, unknown>) : {};
  const dateIndex = raw['date_index'];
  return {
    status,
    error_code: typeof raw['error_code'] === 'string' ? raw['error_code'] : '',
    field: typeof raw['field'] === 'string' ? raw['field'] : '',
    date_index: typeof dateIndex === 'number' ? dateIndex : null,
    detail: typeof raw['detail'] === 'string' && raw['detail'] ? raw['detail'] : FALLBACK_DETAIL,
  };
}

function isSubmissionProblem(error: unknown): error is SubmissionProblem {
  return (
    error !== null &&
    typeof error === 'object' &&
    'error_code' in error &&
    'detail' in error &&
    !(error instanceof HttpErrorResponse)
  );
}

// Every call errors with a SubmissionProblem, never a raw HTTP error.
export interface EventSubmissionsAccess {
  // GET /api/event_submissions/status — public; false while curators have
  // paused submissions.
  getStatus(): Observable<boolean>;
  // POST /api/event_submissions — signed in with a confirmed email.
  submit(request: SubmissionRequest): Observable<SubmissionResult>;
  // GET /api/event_submissions/mine — signed in.
  getMine(): Observable<MySubmission[]>;
  // POST /api/event_submissions/<id>/withdraw — your own, while a date waits.
  withdraw(submissionId: number): Observable<WithdrawResult>;
  // POST /api/event_submissions/<id>/photo/<type> — your own, while a date
  // waits: the image's bytes become every waiting date's photo (CM3 §4.4).
  // NOTHING_PENDING, BAD_IMAGE_TYPE, IMAGE_TOO_LARGE, BAD_IMAGE.
  attachFlyer(
    submissionId: number,
    imageData: ArrayBuffer,
    imageType: FlyerImageType,
  ): Observable<FlyerResult>;
}

export const EVENT_SUBMISSIONS_ACCESS = new InjectionToken<EventSubmissionsAccess>(
  'EVENT_SUBMISSIONS_ACCESS',
);
