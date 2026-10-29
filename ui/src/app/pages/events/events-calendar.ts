import { Component, computed, DestroyRef, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { ActivatedRoute, Router, RouterLink } from '@angular/router';
import { Subscription } from 'rxjs';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import { PublicEvent } from '../../core/types/events.types';
import { eventPagePath } from '../../core/utils/event-links';
import {
  formatEventTime,
  formatMonthYear,
  nowUs,
  pacificWallClockToUs,
} from '../../core/utils/events-date.utils';
import { PageMeta } from '../../core/utils/page-meta';
import {
  addMonths,
  bucketEventsByDay,
  buildMonthGrid,
  CalendarMonth,
  compareMonths,
  gridRangeUs,
  monthParam,
  pacificMonthOf,
  parseMonthParam,
} from './calendar-grid';

// /events/calendar?month=2026-08 — a Pacific month grid with each day's
// events as compact chips. Each month is its own fetch, covering the whole
// grid (leading and trailing days included). The server serves nothing more
// than a day old, so months before this one have nothing to show and the
// page doesn't go back to them. On a phone the grid stacks into a list of
// the days that have events.
@Component({
  selector: 'app-events-calendar',
  imports: [RouterLink, MatButtonModule, MatIconModule, MatProgressSpinnerModule],
  templateUrl: './events-calendar.html',
  styleUrl: './events-calendar.scss',
})
export class EventsCalendarPage implements OnInit {
  private access = inject(COMMUNITY_ACCESS);
  private route = inject(ActivatedRoute);
  private router = inject(Router);
  private pageMeta = inject(PageMeta);
  private destroyRef = inject(DestroyRef);
  private fetch?: Subscription;

  readonly weekdays = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
  readonly currentMonth = pacificMonthOf(nowUs());

  readonly month = signal<CalendarMonth>(this.currentMonth);
  readonly loading = signal(true);
  readonly failed = signal(false);
  readonly events = signal<PublicEvent[]>([]);

  readonly title = computed(() => {
    const { year, month } = this.month();
    return formatMonthYear(pacificWallClockToUs(year, month, 15, 12));
  });
  readonly weeks = computed(() => buildMonthGrid(this.month(), nowUs()));
  readonly eventsByDay = computed(() => bucketEventsByDay(this.events()));
  readonly canGoBack = computed(() => compareMonths(this.month(), this.currentMonth) > 0);

  ngOnInit(): void {
    this.route.queryParamMap.pipe(takeUntilDestroyed(this.destroyRef)).subscribe((params) => {
      const month = parseMonthParam(params.get('month')) ?? this.currentMonth;
      this.month.set(compareMonths(month, this.currentMonth) < 0 ? this.currentMonth : month);
      this.load();
    });
  }

  previousMonth(): void {
    this.goTo(addMonths(this.month(), -1));
  }

  nextMonth(): void {
    this.goTo(addMonths(this.month(), 1));
  }

  thisMonth(): void {
    this.goTo(this.currentMonth);
  }

  dayEvents(key: string): PublicEvent[] {
    return this.eventsByDay().get(key) ?? [];
  }

  eventPath(event: PublicEvent): string {
    return eventPagePath(event.id);
  }

  eventTime(event: PublicEvent): string {
    return formatEventTime(event.starts_at_us);
  }

  private goTo(month: CalendarMonth): void {
    this.router.navigate([], {
      relativeTo: this.route,
      queryParams: {
        month: compareMonths(month, this.currentMonth) === 0 ? null : monthParam(month),
      },
    });
  }

  private load(): void {
    const range = gridRangeUs(this.month());
    this.loading.set(true);
    this.failed.set(false);
    this.pageMeta.set({
      title: `Events calendar — ${this.title()}`,
      description: `Events around Seattle and King County in ${this.title()}, day by day.`,
      canonicalPath: `/events/calendar?month=${monthParam(this.month())}`,
    });
    this.fetch?.unsubscribe();
    this.fetch = this.access
      .getUpcomingEvents({ from_us: range.fromUs, to_us: range.toUs })
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe({
        next: (upcoming) => {
          this.events.set(upcoming.items);
          this.loading.set(false);
        },
        error: () => {
          this.events.set([]);
          this.failed.set(true);
          this.loading.set(false);
        },
      });
  }
}
