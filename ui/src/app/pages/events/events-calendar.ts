import { Component, computed, DestroyRef, DOCUMENT, inject, OnInit, signal } from '@angular/core';
import { takeUntilDestroyed } from '@angular/core/rxjs-interop';
import { ActivatedRoute, Router, RouterLink } from '@angular/router';
import { combineLatest, Subscription } from 'rxjs';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { MatProgressSpinnerModule } from '@angular/material/progress-spinner';
import { MatSlideToggleModule } from '@angular/material/slide-toggle';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import { EventSettingsService } from '../../core/services/event-settings.service';
import { EventSettings, PublicEvent, US_PER_DAY } from '../../core/types/events.types';
import { eventHasEnded, eventPagePath } from '../../core/utils/event-links';
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

// The visitor's "Show past events" choice, kept on their device. A UI
// preference only; nothing about it reaches the server.
export const SHOW_PAST_STORAGE_KEY = 'cf_calendar_show_past';

// /events/calendar?month=2026-08 — a Pacific month grid with each day's
// events as compact chips. Each month is its own fetch, covering the whole
// grid (leading and trailing days included).
//
// Past events (EV4 §10, OQ7/OQ9) are hidden unless "Show past events" is on:
// a visitor's own choice when they've made one, else the site's default
// (off unless an admin turns it on). Shown, they're muted, and the page goes
// back as far as the past-events window reaches; hidden, it starts at this
// month. On a phone the grid stacks into a list of the days that have events.
@Component({
  selector: 'app-events-calendar',
  imports: [
    RouterLink,
    MatButtonModule,
    MatIconModule,
    MatProgressSpinnerModule,
    MatSlideToggleModule,
  ],
  templateUrl: './events-calendar.html',
  styleUrl: './events-calendar.scss',
})
export class EventsCalendarPage implements OnInit {
  private access = inject(COMMUNITY_ACCESS);
  private settingsService = inject(EventSettingsService);
  private route = inject(ActivatedRoute);
  private router = inject(Router);
  private pageMeta = inject(PageMeta);
  private destroyRef = inject(DestroyRef);
  private storage = localStorageOf(inject(DOCUMENT));
  private fetch?: Subscription;
  private settingsApplied = false;

  readonly weekdays = ['Sun', 'Mon', 'Tue', 'Wed', 'Thu', 'Fri', 'Sat'];
  readonly currentMonth = pacificMonthOf(nowUs());

  readonly month = signal<CalendarMonth>(this.currentMonth);
  readonly loading = signal(true);
  readonly failed = signal(false);
  readonly events = signal<PublicEvent[]>([]);
  readonly showPast = signal(false);
  // The month the past-events window starts in; this month until the
  // settings arrive.
  private readonly earliestMonth = signal<CalendarMonth>(this.currentMonth);

  readonly title = computed(() => {
    const { year, month } = this.month();
    return formatMonthYear(pacificWallClockToUs(year, month, 15, 12));
  });
  readonly weeks = computed(() => buildMonthGrid(this.month(), nowUs()));
  readonly shownEvents = computed(() => {
    const now = nowUs();
    return this.showPast()
      ? this.events()
      : this.events().filter((event) => !eventHasEnded(event, now));
  });
  readonly eventsByDay = computed(() => bucketEventsByDay(this.shownEvents()));
  readonly firstMonth = computed(() =>
    this.showPast() ? this.earliestMonth() : this.currentMonth,
  );
  readonly canGoBack = computed(() => compareMonths(this.month(), this.firstMonth()) > 0);
  readonly isCurrentMonth = computed(() => compareMonths(this.month(), this.currentMonth) === 0);

  ngOnInit(): void {
    combineLatest([this.settingsService.settings$, this.route.queryParamMap])
      .pipe(takeUntilDestroyed(this.destroyRef))
      .subscribe(([settings, params]) => {
        this.applySettings(settings);
        const month = parseMonthParam(params.get('month')) ?? this.currentMonth;
        this.month.set(compareMonths(month, this.firstMonth()) < 0 ? this.firstMonth() : month);
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

  // Remembered on this device. Hiding the past from a month before this one
  // comes back to this month, where there's something left to see.
  setShowPast(show: boolean): void {
    this.showPast.set(show);
    try {
      this.storage?.setItem(SHOW_PAST_STORAGE_KEY, String(show));
    } catch {
      // Storage is off (some private modes): the choice holds for this visit.
    }
    if (!show && compareMonths(this.month(), this.currentMonth) < 0) {
      this.thisMonth();
    }
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

  hasEnded(event: PublicEvent): boolean {
    return eventHasEnded(event, nowUs());
  }

  // Once: the visitor's own choice wins over the site's default.
  private applySettings(settings: EventSettings): void {
    if (this.settingsApplied) {
      return;
    }
    this.settingsApplied = true;
    this.earliestMonth.set(
      pacificMonthOf(nowUs() - settings.archived_events_visible_days * US_PER_DAY),
    );
    this.showPast.set(readShowPast(this.storage) ?? settings.events_show_past_by_default);
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

function localStorageOf(document: Document): Storage | null {
  try {
    return document.defaultView?.localStorage ?? null;
  } catch {
    return null;
  }
}

// The stored choice, or null when there isn't one (or storage is off).
function readShowPast(storage: Storage | null): boolean | null {
  try {
    const stored = storage?.getItem(SHOW_PAST_STORAGE_KEY);
    return stored === 'true' ? true : stored === 'false' ? false : null;
  } catch {
    return null;
  }
}
