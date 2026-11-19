import { HttpErrorResponse } from '@angular/common/http';
import { InjectionToken } from '@angular/core';
import { Observable } from 'rxjs';
import {
  SubmissionProblem,
  toSubmissionProblem,
} from '../event-submissions-access/event-submissions-access';
import { EventStatus } from '../types/events.types';

export type ReviewAction = 'approve' | 'reject';

// Why a submitted event wasn't listed (EV3 "Review"). The submitter sees it,
// in plain words, on "My submissions" and in the decision email.
export type RejectReason =
  'duplicate' | 'not-a-fit' | 'missing-details' | 'past' | 'spam' | 'other';

export const REJECT_REASONS: ReadonlyArray<{ value: RejectReason; label: string }> = [
  { value: 'duplicate', label: 'Already listed' },
  { value: 'not-a-fit', label: 'Not a fit' },
  { value: 'missing-details', label: 'Missing details' },
  { value: 'past', label: 'Already over' },
  { value: 'spam', label: 'Spam' },
  { value: 'other', label: 'Other' },
];

// What a curator decides alongside approve or reject. `notes` is the
// curator-only record (review_notes). The reason and the message to the
// submitter are for submitted events; the server ignores them on others.
export interface ReviewDecision {
  notes: string;
  reason?: RejectReason;
  message_to_submitter?: string;
}

// POST /api/admin/review_event/<id>. `decision_emailed` says whether this
// decision finished a submission and its email went out.
export interface ReviewEventResult {
  event_id: number;
  status: EventStatus;
  decision_emailed: boolean;
}

// POST /api/admin/review_submission/<id>: every pending date at once.
export interface ReviewSubmissionResult {
  submission_id: number;
  status: EventStatus;
  decided_event_ids: number[];
  decision_emailed: boolean;
}

// One whole-number setting as a settings GET sends it, with its default and
// allowed range, so the page never hard-codes them.
export interface RangedSetting {
  value: number;
  default: number;
  min: number;
  max: number;
}

// One rate limit.
export type SubmissionLimitSetting = RangedSetting;

export type SubmissionLimitName = 'max_pending_dates' | 'max_submissions_per_day';

// GET /api/admin/event_submission_settings (EV3 §4.5, §4.6).
export interface SubmissionSettings {
  max_pending_dates: SubmissionLimitSetting;
  max_submissions_per_day: SubmissionLimitSetting;
  submissions_paused: { value: boolean; default: boolean };
}

// PUT: only the fields present are saved; a refused save saves nothing.
export interface SubmissionSettingsUpdate {
  max_pending_dates?: number;
  max_submissions_per_day?: number;
  submissions_paused?: boolean;
}

// GET /api/admin/past_event_settings (EV4 §10.3): how many days an event stays
// on the site after it ends, and whether the calendar shows past events until
// a visitor says otherwise.
export interface PastEventSettings {
  archived_events_visible_days: RangedSetting;
  events_show_past_by_default: { value: boolean; default: boolean };
}

// PUT: only the fields present are saved; a refused save saves nothing.
export interface PastEventSettingsUpdate {
  archived_events_visible_days?: number;
  events_show_past_by_default?: boolean;
}

// GET /api/admin/event_gallery_settings (CM3 OQ5): how many photos one
// gallery may hold. Lowering it never removes photos; it only stops new ones.
export interface GallerySettings {
  event_gallery_max_photos: RangedSetting;
}

// POST /api/admin/event_gallery_photos (CM3 §4.2): a new gallery row, on an
// event or a series (exactly one). The server stamps who confirmed consent,
// and when, from the curator's session. The row's photo is uploaded next.
export interface NewGalleryPhoto {
  event_id?: number;
  series_id?: number;
  alt_text: string;
  caption?: string;
  credit?: string;
  consent_confirmed: boolean;
}

export const GALLERY_FULL = 'GALLERY_FULL';

// A refusal of a gallery row: the submission routes' problem shape, plus the
// cap that was reached on GALLERY_FULL. The server's `detail` is only the
// code, so pages word it with galleryProblemMessage.
export interface GalleryProblem extends SubmissionProblem {
  max_photos: number | null;
}

export function toGalleryProblem(error: unknown): GalleryProblem {
  if (error !== null && typeof error === 'object' && 'max_photos' in error) {
    return error as GalleryProblem;
  }
  const body = error instanceof HttpErrorResponse ? error.error : null;
  const maxPhotos =
    body !== null && typeof body === 'object'
      ? (body as Record<string, unknown>)['max_photos']
      : null;
  return {
    ...toSubmissionProblem(error),
    max_photos: typeof maxPhotos === 'number' ? maxPhotos : null,
  };
}

// The gallery fields' limits, in characters (db_schema/event_gallery_photos).
export const GALLERY_ALT_TEXT_MAX = 300;
export const GALLERY_CAPTION_MAX = 300;
export const GALLERY_CREDIT_MAX = 120;

// A gallery refusal in plain words, by its code (EventGalleryHelper's).
export function galleryProblemMessage(problem: GalleryProblem): string {
  switch (problem.error_code) {
    case GALLERY_FULL:
      return problem.max_photos !== null
        ? `This gallery is full: it can hold ${problem.max_photos} ${problem.max_photos === 1 ? 'photo' : 'photos'}. ` +
            'Remove one first, or raise the limit on the Event settings page.'
        : 'This gallery is full. Remove a photo first, or raise the limit on the Event settings page.';
    case 'ALT_TEXT_REQUIRED':
      return 'Describe the photo for people who can’t see it.';
    case 'TOO_LONG':
      return problem.field === 'credit'
        ? `Keep the credit to ${GALLERY_CREDIT_MAX} characters.`
        : `Keep it to ${problem.field === 'caption' ? GALLERY_CAPTION_MAX : GALLERY_ALT_TEXT_MAX} characters.`;
    case 'CONSENT_REQUIRED':
      return 'Confirm that everyone recognizable in the photo agreed to be shown.';
    case 'PARENT_NOT_FOUND':
      return 'This event no longer exists.';
    case 'ONE_PARENT_REQUIRED':
      return 'A photo belongs to one event or one series.';
    default:
      return "The photo couldn't be added. Please try again.";
  }
}

// The curator's bespoke events actions the generic CrudAccess doesn't cover:
// reviewing a pending event or a whole submission (EV1 §5.5, EV3 §4.2), the
// submission settings (EV3 §4.5–4.6), the past-event settings (EV4 §10.3),
// and CM3's gallery rows and cap (§4.2). Entering and editing events, and
// editing, reordering and deleting gallery rows, go through
// HONUWARE_CRUD_ACCESS. Mirrors the AdminUsersAccess seam. Every route is
// manage_events gated. Errors from the settings and review-submission calls
// come back as SubmissionProblems; a gallery row's as a GalleryProblem.
export interface EventsAdminAccess {
  // POST /api/admin/review_event/<id>.
  reviewEvent(
    eventId: number,
    action: ReviewAction,
    decision: ReviewDecision,
  ): Observable<ReviewEventResult>;
  // POST /api/admin/review_submission/<id>.
  reviewSubmission(
    submissionId: number,
    action: ReviewAction,
    decision: ReviewDecision,
  ): Observable<ReviewSubmissionResult>;
  // GET /api/admin/event_submission_settings.
  getSubmissionSettings(): Observable<SubmissionSettings>;
  // PUT /api/admin/event_submission_settings; replies with the settings as saved.
  saveSubmissionSettings(update: SubmissionSettingsUpdate): Observable<SubmissionSettings>;
  // GET /api/admin/past_event_settings.
  getPastEventSettings(): Observable<PastEventSettings>;
  // PUT /api/admin/past_event_settings; replies with the settings as saved.
  savePastEventSettings(update: PastEventSettingsUpdate): Observable<PastEventSettings>;
  // POST /api/admin/event_gallery_photos → the new row's id.
  addGalleryPhoto(photo: NewGalleryPhoto): Observable<{ id: number }>;
  // GET /api/admin/event_gallery_settings.
  getGallerySettings(): Observable<GallerySettings>;
  // PUT /api/admin/event_gallery_settings; replies with the setting as saved.
  saveGallerySettings(maxPhotos: number): Observable<GallerySettings>;
}

export const EVENTS_ADMIN_ACCESS = new InjectionToken<EventsAdminAccess>('EVENTS_ADMIN_ACCESS');
