import { inject, Injectable } from '@angular/core';
import { map, Observable } from 'rxjs';
import { HONUWARE_CRUD_ACCESS } from '@honuware/ui/access';
import { EventsAdminAccess, ReviewAction, ReviewEventResult } from './events-admin-access';

// Offline mock — the `local` configuration wires this so `ng serve` runs with no
// backend. A review sets the event row's status in the in-memory CRUD store,
// so the editor's pending queue behaves as it does live. (The public events
// pages read their own demo dataset, so an offline approval doesn't show up
// on them.)
@Injectable()
export class EventsAdminMockAccess implements EventsAdminAccess {
  private crud = inject(HONUWARE_CRUD_ACCESS);

  reviewEvent(eventId: number, action: ReviewAction, notes: string): Observable<ReviewEventResult> {
    const status = action === 'approve' ? 'approved' : 'rejected';
    return this.crud
      .updateItem({
        table_name: 'events',
        column_name: 'id',
        column_value: String(eventId),
        value: { status, review_notes: notes },
      })
      .pipe(map(() => ({ event_id: eventId, status })));
  }
}
