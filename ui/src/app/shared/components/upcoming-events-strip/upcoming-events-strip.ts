import { Component, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { RouterLink } from '@angular/router';
import { COMMUNITY_ACCESS } from '../../../core/community-access/community-access';
import { PublicEvent } from '../../../core/types/events.types';
import { eventPagePath } from '../../../core/utils/event-links';
import { formatEventShortDate, formatEventTime } from '../../../core/utils/events-date.utils';

// The next few events, compact, for the home page (EV2 OQ4 / §3.9): date and
// time, title, place, and a link to the full list. Self-contained: it does
// its own fetch, and renders nothing at all when there's nothing to show or
// the fetch fails, so it can sit on any page without an empty box. RT1's
// home-page restyle mounts it into its own slot.
@Component({
  selector: 'app-upcoming-events-strip',
  imports: [RouterLink],
  templateUrl: './upcoming-events-strip.html',
  styleUrl: './upcoming-events-strip.scss',
})
export class UpcomingEventsStrip implements OnInit {
  private access = inject(COMMUNITY_ACCESS);
  private destroyRef = inject(DestroyRef);

  readonly limit = 5;
  readonly events = signal<PublicEvent[]>([]);

  ngOnInit(): void {
    this.access
      .getUpcomingEvents()
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (upcoming) => this.events.set(upcoming.items.slice(0, this.limit)),
        error: () => this.events.set([]),
      });
  }

  eventPath(event: PublicEvent): string {
    return eventPagePath(event.id);
  }

  // "Fri, Aug 14 · 10:00 PM"
  when(event: PublicEvent): string {
    return `${formatEventShortDate(event.starts_at_us)} · ${formatEventTime(event.starts_at_us)}`;
  }

  place(event: PublicEvent): string {
    return event.venue_name || event.location_text;
  }
}
