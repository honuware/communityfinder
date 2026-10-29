import { HttpClient } from '@angular/common/http';
import { inject, Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { EventsAdminAccess, ReviewAction, ReviewEventResult } from './events-admin-access';

// The real HTTP implementation. `withCredentials` carries the curator's session
// cookie; the CsrfInterceptor stamps the X-CSRF-Token header on the POST. The
// server checks the `manage_events` permission before it reads the body.
@Injectable()
export class EventsAdminHttpAccess implements EventsAdminAccess {
  private http = inject(HttpClient);
  private apiBase = inject(HONUWARE_API_BASE);

  reviewEvent(eventId: number, action: ReviewAction, notes: string): Observable<ReviewEventResult> {
    return this.http.post<ReviewEventResult>(
      `${this.apiBase}/admin/review_event/${eventId}`,
      { action, notes },
      { withCredentials: true },
    );
  }
}
