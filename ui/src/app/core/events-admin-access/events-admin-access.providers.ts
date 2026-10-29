import { Provider } from '@angular/core';
import { EVENTS_ADMIN_ACCESS } from './events-admin-access';
import { EventsAdminHttpAccess } from './events-admin-access.http';
import { EventsAdminMockAccess } from './events-admin-access.mock';

// Provide the EventsAdminAccess seam — the offline mock (local) or the HTTP impl.
export function provideEventsAdminAccess(useMock: boolean): Provider {
  return useMock
    ? { provide: EVENTS_ADMIN_ACCESS, useClass: EventsAdminMockAccess }
    : { provide: EVENTS_ADMIN_ACCESS, useClass: EventsAdminHttpAccess };
}
