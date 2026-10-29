import { HttpErrorResponse } from '@angular/common/http';
import { Component, computed, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { ActivatedRoute, RouterLink } from '@angular/router';
import { catchError, map, of, switchMap, tap } from 'rxjs';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import {
  EMPTY_VOCABULARIES,
  EventVocabularies,
  EventVocabulariesService,
} from '../../core/services/event-vocabularies.service';
import { EventSeriesDetail } from '../../core/types/events.types';
import { EventsFeedFilters, seriesPagePath } from '../../core/utils/event-links';
import { PageMeta } from '../../core/utils/page-meta';
import { FreshnessFooter } from '../../shared/components/freshness-footer/freshness-footer';
import { EventRow } from './event-row';
import { tagChips } from './events-view';
import { SubscribeLink } from './subscribe-link';

// /events/series/:slug — a series' durable page ("Bear Happy Hour —
// Thursdays, 6–9 PM at The Lantern Room"): its description, its next dates,
// a subscribe link for just this series (OQ2), and GD1 §7's freshness footer.
// An ended series keeps its page, marked as ended, with nothing upcoming.
@Component({
  selector: 'app-series-detail',
  imports: [
    RouterLink,
    MatButtonModule,
    MatIconModule,
    MatProgressSpinnerModule,
    EventRow,
    FreshnessFooter,
    SubscribeLink,
  ],
  templateUrl: './series-detail.html',
  styleUrl: './series-detail.scss',
})
export class SeriesDetailPage implements OnInit {
  private access = inject(COMMUNITY_ACCESS);
  private vocabularyService = inject(EventVocabulariesService);
  private route = inject(ActivatedRoute);
  private pageMeta = inject(PageMeta);
  private destroyRef = inject(DestroyRef);

  readonly loading = signal(true);
  readonly notFound = signal(false);
  readonly failed = signal(false);
  readonly detail = signal<EventSeriesDetail | null>(null);
  readonly vocabularies = signal<EventVocabularies>(EMPTY_VOCABULARIES);

  readonly ended = computed(() => {
    const status = this.detail()?.series.status;
    return status === 'closed' || status === 'moved';
  });
  readonly whenAndWhere = computed(() => {
    const series = this.detail()?.series;
    if (!series) {
      return '';
    }
    const at = series.venue_name ? `at ${series.venue_name}` : '';
    return [series.cadence_text, at].filter(Boolean).join(' ');
  });
  readonly feedFilters = computed<EventsFeedFilters>(() => ({
    series_id: this.detail()?.series.id,
  }));
  readonly chips = computed(() => {
    const series = this.detail()?.series;
    return series ? tagChips(series, this.vocabularies()) : [];
  });

  ngOnInit(): void {
    this.vocabularyService.vocabularies$
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((vocabularies) => this.vocabularies.set(vocabularies));
    this.route.paramMap
      .pipe(
        map((params) => params.get('slug') ?? ''),
        tap(() => {
          this.loading.set(true);
          this.notFound.set(false);
          this.failed.set(false);
          this.detail.set(null);
        }),
        switchMap((slug) =>
          this.access.getEventSeries(slug).pipe(
            map((detail) => ({ detail, status: 200 })),
            catchError((error: HttpErrorResponse) => of({ detail: null, status: error.status })),
          ),
        ),
        takeUntilDestroyed(this.destroyRef),
      )
      .subscribe(({ detail, status }) => {
        this.loading.set(false);
        if (detail) {
          this.detail.set(detail);
          const { series } = detail;
          this.pageMeta.set({
            title: [series.title, this.whenAndWhere()].filter(Boolean).join(' — '),
            description: series.description || this.whenAndWhere() || series.title,
            canonicalPath: seriesPagePath(series.slug),
          });
        } else if (status === 404) {
          this.notFound.set(true);
          this.pageMeta.set({
            title: 'Series not found',
            description: 'This series is not listed.',
            canonicalPath: '/events/series',
          });
        } else {
          this.failed.set(true);
        }
      });
  }
}
