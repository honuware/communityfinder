import { Component, computed, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { ActivatedRoute, Params, Router, RouterLink } from '@angular/router';
import { Subscription } from 'rxjs';
import { MatButtonModule } from '@angular/material/button';
import { MatFormFieldModule } from '@angular/material/form-field';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { MatSelectModule } from '@angular/material/select';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import {
  EMPTY_VOCABULARIES,
  EventVocabularies,
  EventVocabulariesService,
  vocabularyLabel,
} from '../../core/services/event-vocabularies.service';
import {
  buildEventsQueryString,
  PAST_EVENTS_PAGE_SIZE,
  PublicEvent,
} from '../../core/types/events.types';
import { EventsFeedFilters } from '../../core/utils/event-links';
import { nowUs } from '../../core/utils/events-date.utils';
import { PageMeta } from '../../core/utils/page-meta';
import { EventRow } from './event-row';
import {
  EMPTY_LIST_FILTERS,
  EventsView,
  eventsViewFromParams,
  filterEvents,
  groupEventsByDay,
  groupEventsByMonth,
  hasListFilters,
  ListFilters,
  listFiltersFromParams,
  listFiltersToQueryParams,
  WhenPreset,
} from './events-view';
import { SubscribeLink } from './subscribe-link';

// /events — the upcoming list, grouped by day, with the filter bar and the
// Tonight / This Weekend presets. Every filter lives in the URL
// (/events?when=tonight&scene=bear), so a view can be shared, bookmarked and
// gone back to. The page fetches the API's default 42-day window once and
// filters it in the browser; `rawItemCount` tells "nothing on" apart from
// "your filters hid everything".
//
// /events?view=past — the past list (EV4 §10): what already happened, newest
// first, grouped by month, as far back as the site keeps events. It's paged,
// so the server filters it and "Show more" fetches the next page. The presets
// and the subscribe link are for what's coming, so it leaves them out.
@Component({
  selector: 'app-events-list',
  imports: [
    RouterLink,
    MatButtonModule,
    MatFormFieldModule,
    MatIconModule,
    MatProgressSpinnerModule,
    MatSelectModule,
    EventRow,
    SubscribeLink,
  ],
  templateUrl: './events-list.html',
  styleUrl: './events-list.scss',
})
export class EventsListPage implements OnInit {
  private access = inject(COMMUNITY_ACCESS);
  private vocabularyService = inject(EventVocabulariesService);
  private route = inject(ActivatedRoute);
  private router = inject(Router);
  private pageMeta = inject(PageMeta);
  private destroyRef = inject(DestroyRef);
  private upcomingRequested = false;
  private pastFetch?: Subscription;

  readonly presets: ReadonlyArray<{ when: WhenPreset; label: string }> = [
    { when: 'tonight', label: 'Tonight' },
    { when: 'weekend', label: 'This weekend' },
  ];

  readonly view = signal<EventsView>('upcoming');
  readonly filters = signal<ListFilters>(EMPTY_LIST_FILTERS);
  readonly vocabularies = signal<EventVocabularies>(EMPTY_VOCABULARIES);

  // The upcoming list: one fetch, filtered here.
  private readonly upcomingLoading = signal(true);
  private readonly upcomingFailed = signal(false);
  readonly events = signal<PublicEvent[]>([]);

  // The past list: the pages fetched so far for the current filters.
  private readonly pastLoading = signal(true);
  private readonly pastFailed = signal(false);
  readonly pastEvents = signal<PublicEvent[]>([]);
  readonly pastHasMore = signal(false);
  readonly loadingMore = signal(false);
  readonly moreFailed = signal(false);

  readonly isPast = computed(() => this.view() === 'past');
  readonly loading = computed(() => (this.isPast() ? this.pastLoading() : this.upcomingLoading()));
  readonly failed = computed(() => (this.isPast() ? this.pastFailed() : this.upcomingFailed()));
  readonly rawItemCount = computed(() => this.events().length);
  readonly groups = computed(() =>
    this.isPast()
      ? groupEventsByMonth(this.pastEvents())
      : groupEventsByDay(filterEvents(this.events(), this.filters(), nowUs())),
  );
  // The category / scene / neighborhood; Tonight and This weekend don't
  // apply to the past list.
  readonly filtered = computed(() =>
    hasListFilters(this.isPast() ? { ...this.filters(), when: '' } : this.filters()),
  );
  // The other list, with the same category / scene / neighborhood.
  readonly otherViewParams = computed<Params>(() => {
    const params = listFiltersToQueryParams({ ...this.filters(), when: '' });
    return this.isPast() ? params : { view: 'past', ...params };
  });
  readonly feedFilters = computed<EventsFeedFilters>(() => {
    const { category, scene, neighborhood } = this.filters();
    return { category, scene, neighborhood };
  });
  // "Drag, Capitol Hill events" — the feed's own naming (EV2 §1.5).
  readonly feedLabel = computed(() => {
    const names = this.filterNames();
    return names.length ? `${names.join(', ')} events` : 'all events';
  });

  ngOnInit(): void {
    this.vocabularyService.vocabularies$
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((vocabularies) => {
        this.vocabularies.set(vocabularies);
        this.updatePageMeta();
      });
    this.route.queryParamMap.pipe(takeUntilDestroyed(this.destroyRef)).subscribe((params) => {
      this.view.set(eventsViewFromParams(params));
      this.filters.set(listFiltersFromParams(params));
      if (this.isPast()) {
        this.loadPast(0);
      } else {
        this.loadUpcoming();
      }
      this.updatePageMeta();
    });
  }

  setFilter(key: 'category' | 'scene' | 'neighborhood', value: string): void {
    this.navigate({ ...this.filters(), [key]: value });
  }

  // A preset chip toggles: picking the active one again clears it.
  togglePreset(when: WhenPreset): void {
    this.navigate({ ...this.filters(), when: this.filters().when === when ? '' : when });
  }

  clearFilters(): void {
    this.navigate(EMPTY_LIST_FILTERS);
  }

  showMore(): void {
    this.loadPast(this.pastEvents().length);
  }

  private navigate(filters: ListFilters): void {
    const params = listFiltersToQueryParams(this.isPast() ? { ...filters, when: '' } : filters);
    this.router.navigate([], {
      relativeTo: this.route,
      queryParams: this.isPast() ? { view: 'past', ...params } : params,
    });
  }

  // Once per visit: the filters apply in the browser.
  private loadUpcoming(): void {
    if (this.upcomingRequested) {
      return;
    }
    this.upcomingRequested = true;
    this.access
      .getUpcomingEvents()
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (upcoming) => {
          this.events.set(upcoming.items);
          this.upcomingLoading.set(false);
        },
        error: () => {
          this.upcomingFailed.set(true);
          this.upcomingLoading.set(false);
        },
      });
  }

  // The page from `offset` on; 0 starts the list over (new filters).
  private loadPast(offset: number): void {
    const { category, scene, neighborhood } = this.filters();
    this.pastFetch?.unsubscribe();
    if (offset === 0) {
      this.pastLoading.set(true);
      this.pastFailed.set(false);
    } else {
      this.loadingMore.set(true);
    }
    this.moreFailed.set(false);
    this.pastFetch = this.access
      .getPastEvents({ offset, limit: PAST_EVENTS_PAGE_SIZE, category, scene, neighborhood })
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (page) => {
          this.pastEvents.set(offset === 0 ? page.items : [...this.pastEvents(), ...page.items]);
          this.pastHasMore.set(page.has_more);
          this.pastLoading.set(false);
          this.loadingMore.set(false);
        },
        error: () => {
          if (offset === 0) {
            this.pastEvents.set([]);
            this.pastHasMore.set(false);
            this.pastFailed.set(true);
            this.pastLoading.set(false);
          } else {
            this.moreFailed.set(true);
            this.loadingMore.set(false);
          }
        },
      });
  }

  private filterNames(): string[] {
    const { category, scene, neighborhood } = this.filters();
    const { categories, sceneTags, neighborhoods } = this.vocabularies();
    return [
      category && vocabularyLabel(categories, category),
      scene && vocabularyLabel(sceneTags, scene),
      neighborhood && vocabularyLabel(neighborhoods, neighborhood),
    ].filter((name): name is string => !!name);
  }

  // "Drag, Capitol Hill events tonight". The canonical address keeps the
  // category / scene / neighborhood (each is a page worth finding) but not
  // Tonight or This Weekend, which mean a different day tomorrow.
  private updatePageMeta(): void {
    const names = this.filterNames().join(', ');
    const { category, scene, neighborhood } = this.filters();
    const query = buildEventsQueryString({ category, scene, neighborhood });
    if (this.isPast()) {
      this.pageMeta.set({
        title: `Past ${names ? `${names} ` : ''}events`,
        description:
          `${names ? `${names} events` : 'Events'} that already happened around Seattle and ` +
          'King County, newest first.',
        canonicalPath: `/events?view=past${query.replace('?', '&')}`,
      });
      return;
    }
    const when = { tonight: ' tonight', weekend: ' this weekend', '': '' }[this.filters().when];
    this.pageMeta.set({
      title: `${names ? `${names} events` : 'Events'}${when}`,
      description:
        `Upcoming ${names ? `${names} ` : ''}events${when} around Seattle and King County: ` +
        'dates, times, places, and a link to add each one to your calendar.',
      canonicalPath: `/events${query}`,
    });
  }
}
