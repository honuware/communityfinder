import { Component, computed, inject, input } from '@angular/core';
import { toObservable, toSignal } from '@angular/core/rxjs-interop';
import { RouterLink } from '@angular/router';
import { catchError, map, Observable, of, startWith, switchMap } from 'rxjs';
import { GUIDE_ACCESS } from '../../core/guide-access/guide-access';
import { GuideEvent } from '../../core/types/guide.types';
import { formatGuideEventStart } from '../../core/utils/guide-date.utils';

// "Coming up" on a venue or scene page: the next few approved events there,
// read from Stream A's public events API through the guide's own seam (GD3's
// cross-stream pin). A failed read looks just like an empty one: one quiet
// line, never an error, so the page around it never breaks over it.
@Component({
  selector: 'app-guide-upcoming-events',
  imports: [RouterLink],
  templateUrl: './guide-upcoming-events.html',
  styleUrl: './guide-upcoming-events.scss',
})
export class GuideUpcomingEvents {
  private access = inject(GUIDE_ACCESS);

  // One of the two.
  readonly venueId = input<number | null>(null);
  readonly sceneSlug = input<string | null>(null);

  readonly limit = 5;

  private readonly source = computed(() => ({
    venueId: this.venueId(),
    sceneSlug: this.sceneSlug(),
  }));

  // null while loading.
  readonly events = toSignal(
    toObservable(this.source).pipe(
      switchMap(({ venueId, sceneSlug }) =>
        this.fetch(venueId, sceneSlug).pipe(
          map((events) => events.slice(0, this.limit)),
          catchError(() => of<GuideEvent[]>([])),
          startWith(null),
        ),
      ),
    ),
    { initialValue: null },
  );

  when(event: GuideEvent): string {
    return formatGuideEventStart(event.starts_at_us);
  }

  // On a scene page, where it is; on a venue page that goes without saying.
  place(event: GuideEvent): string {
    return this.sceneSlug() ? event.venue_name || event.location_text : '';
  }

  private fetch(venueId: number | null, sceneSlug: string | null): Observable<GuideEvent[]> {
    if (venueId) {
      return this.access.getVenueEvents(venueId);
    }
    return sceneSlug ? this.access.getSceneEvents(sceneSlug) : of([]);
  }
}
