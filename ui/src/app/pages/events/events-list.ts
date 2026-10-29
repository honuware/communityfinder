import { Component, computed, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { ActivatedRoute, Router, RouterLink } from '@angular/router';
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
import { PublicEvent, buildEventsQueryString } from '../../core/types/events.types';
import { EventsFeedFilters } from '../../core/utils/event-links';
import { nowUs } from '../../core/utils/events-date.utils';
import { PageMeta } from '../../core/utils/page-meta';
import { EventRow } from './event-row';
import {
  EMPTY_LIST_FILTERS,
  filterEvents,
  groupEventsByDay,
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

  readonly presets: ReadonlyArray<{ when: WhenPreset; label: string }> = [
    { when: 'tonight', label: 'Tonight' },
    { when: 'weekend', label: 'This weekend' },
  ];

  readonly loading = signal(true);
  readonly failed = signal(false);
  readonly events = signal<PublicEvent[]>([]);
  readonly filters = signal<ListFilters>(EMPTY_LIST_FILTERS);
  readonly vocabularies = signal<EventVocabularies>(EMPTY_VOCABULARIES);

  readonly rawItemCount = computed(() => this.events().length);
  readonly groups = computed(() =>
    groupEventsByDay(filterEvents(this.events(), this.filters(), nowUs())),
  );
  readonly filtered = computed(() => hasListFilters(this.filters()));
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
    this.access
      .getUpcomingEvents()
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (upcoming) => {
          this.events.set(upcoming.items);
          this.loading.set(false);
        },
        error: () => {
          this.failed.set(true);
          this.loading.set(false);
        },
      });
    this.vocabularyService.vocabularies$
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe((vocabularies) => {
        this.vocabularies.set(vocabularies);
        this.updatePageMeta();
      });
    this.route.queryParamMap.pipe(takeUntilDestroyed(this.destroyRef)).subscribe((params) => {
      this.filters.set(listFiltersFromParams(params));
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

  private navigate(filters: ListFilters): void {
    this.router.navigate([], {
      relativeTo: this.route,
      queryParams: listFiltersToQueryParams(filters),
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
    const when = { tonight: ' tonight', weekend: ' this weekend', '': '' }[this.filters().when];
    const { category, scene, neighborhood } = this.filters();
    this.pageMeta.set({
      title: `${names ? `${names} events` : 'Events'}${when}`,
      description:
        `Upcoming ${names ? `${names} ` : ''}events${when} around Seattle and King County: ` +
        'dates, times, places, and a link to add each one to your calendar.',
      canonicalPath: `/events${buildEventsQueryString({ category, scene, neighborhood })}`,
    });
  }
}
