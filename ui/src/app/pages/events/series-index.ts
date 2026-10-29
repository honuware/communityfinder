import { Component, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { RouterLink } from '@angular/router';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import { EventSeries } from '../../core/types/events.types';
import { seriesPagePath } from '../../core/utils/event-links';
import { PageMeta } from '../../core/utils/page-meta';

// /events/series — the recurring series that are still running, by title.
// Ended and moved ones keep their own pages but leave this index.
@Component({
  selector: 'app-series-index',
  imports: [RouterLink, MatButtonModule, MatIconModule, MatProgressSpinnerModule],
  templateUrl: './series-index.html',
  styleUrl: './series-index.scss',
})
export class SeriesIndexPage implements OnInit {
  private access = inject(COMMUNITY_ACCESS);
  private pageMeta = inject(PageMeta);
  private destroyRef = inject(DestroyRef);

  readonly loading = signal(true);
  readonly failed = signal(false);
  readonly series = signal<EventSeries[]>([]);

  ngOnInit(): void {
    this.pageMeta.set({
      title: 'Recurring events',
      description:
        'Weekly and monthly events around Seattle and King County: the nights, meetups and ' +
        'gatherings that keep coming back.',
      canonicalPath: '/events/series',
    });
    this.access
      .getEventSeriesList()
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (series) => {
          this.series.set(series);
          this.loading.set(false);
        },
        error: () => {
          this.failed.set(true);
          this.loading.set(false);
        },
      });
  }

  seriesPath(series: EventSeries): string {
    return seriesPagePath(series.slug);
  }

  // "Thursdays, 6–9 PM · The Lantern Room"
  whenAndWhere(series: EventSeries): string {
    return [series.cadence_text, series.venue_name].filter(Boolean).join(' · ');
  }
}
