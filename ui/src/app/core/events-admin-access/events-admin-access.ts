import { InjectionToken } from '@angular/core';
import { Observable } from 'rxjs';
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

// The curator's bespoke events actions the generic CrudAccess doesn't cover:
// reviewing a pending event or a whole submission (EV1 §5.5, EV3 §4.2), the
// submission settings (EV3 §4.5–4.6) and the past-event settings (EV4 §10.3).
// Entering and editing events goes through HONUWARE_CRUD_ACCESS. Mirrors the
// AdminUsersAccess seam. Every route is manage_events gated. Errors from the
// settings and review-submission calls come back as SubmissionProblems.
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
}

export const EVENTS_ADMIN_ACCESS = new InjectionToken<EventsAdminAccess>('EVENTS_ADMIN_ACCESS');
