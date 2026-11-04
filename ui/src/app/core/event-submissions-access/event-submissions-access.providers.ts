import { Provider } from '@angular/core';
import { EVENT_SUBMISSIONS_ACCESS } from './event-submissions-access';
import { EventSubmissionsHttpAccess } from './event-submissions-access.http';
import { EventSubmissionsMockAccess } from './event-submissions-access.mock';

// Provide the EventSubmissionsAccess seam — the offline mock (local) or the HTTP impl.
export function provideEventSubmissionsAccess(useMock: boolean): Provider {
  return useMock
    ? { provide: EVENT_SUBMISSIONS_ACCESS, useClass: EventSubmissionsMockAccess }
    : { provide: EVENT_SUBMISSIONS_ACCESS, useClass: EventSubmissionsHttpAccess };
}
