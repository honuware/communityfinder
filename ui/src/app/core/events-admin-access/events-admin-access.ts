import { InjectionToken } from '@angular/core';
import { Observable } from 'rxjs';
import { EventStatus } from '../types/events.types';

export type ReviewAction = 'approve' | 'reject';

// POST /api/admin/review_event/<id> replies with the event's new status.
export interface ReviewEventResult {
  event_id: number;
  status: EventStatus;
}

// The curator's one bespoke events action the generic CrudAccess doesn't
// cover: approving or rejecting a pending event (EV1 §5.5), which also stamps
// who reviewed it, when, and the notes. Entering and editing events goes
// through HONUWARE_CRUD_ACCESS. Mirrors the AdminUsersAccess seam.
export interface EventsAdminAccess {
  // POST /api/admin/review_event/<id> — manage_events gated.
  reviewEvent(eventId: number, action: ReviewAction, notes: string): Observable<ReviewEventResult>;
}

export const EVENTS_ADMIN_ACCESS = new InjectionToken<EventsAdminAccess>('EVENTS_ADMIN_ACCESS');
