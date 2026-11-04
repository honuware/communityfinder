import { inject, Injectable } from '@angular/core';
import { catchError, Observable, of, shareReplay } from 'rxjs';
import { COMMUNITY_ACCESS } from '../community-access/community-access';
import { DEFAULT_EVENT_SETTINGS, EventSettings } from '../types/events.types';

// The site's past-event settings (EV4 §10.3), fetched once per visit and shared
// by the events pages. Settings that fail to load are the defaults, so no page
// breaks over them.
@Injectable({ providedIn: 'root' })
export class EventSettingsService {
  private access = inject(COMMUNITY_ACCESS);

  readonly settings$: Observable<EventSettings> = this.access.getEventSettings().pipe(
    catchError(() => of(DEFAULT_EVENT_SETTINGS)),
    shareReplay(1),
  );
}
