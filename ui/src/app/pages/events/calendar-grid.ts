import { PublicEvent } from '../../core/types/events.types';
import {
  formatEventShortDate,
  pacificDateTime,
  pacificDayKey,
  pacificWallClockToUs,
  UsRange,
} from '../../core/utils/events-date.utils';

// The month calendar's view model: a Sunday-first grid of Pacific calendar
// days, including the leading and trailing days of the neighboring months
// that fill its first and last weeks, and the events bucketed onto it.

// Months are 1–12.
export interface CalendarMonth {
  year: number;
  month: number;
}

export interface CalendarDay {
  // "2026-08-14"
  key: string;
  day: number;
  // false for the leading / trailing days of the neighboring months.
  inMonth: boolean;
  isToday: boolean;
  // "Fri, Aug 14": the day's label when the grid stacks into a list.
  dateLabel: string;
}

const DAYS_PER_WEEK = 7;

// Plain calendar arithmetic on a date, via UTC so no zone gets involved.
function calendarDate(year: number, month: number, day: number): Date {
  return new Date(Date.UTC(year, month - 1, day));
}

function dayKey(date: Date): string {
  const month = String(date.getUTCMonth() + 1).padStart(2, '0');
  const day = String(date.getUTCDate()).padStart(2, '0');
  return `${date.getUTCFullYear()}-${month}-${day}`;
}

// "2026-08" → August 2026; null for anything else.
export function parseMonthParam(text: string | null): CalendarMonth | null {
  const match = /^(\d{4})-(\d{2})$/.exec(text ?? '');
  if (!match) {
    return null;
  }
  const month = Number(match[2]);
  return month >= 1 && month <= 12 ? { year: Number(match[1]), month } : null;
}

export function monthParam({ year, month }: CalendarMonth): string {
  return `${year}-${String(month).padStart(2, '0')}`;
}

export function addMonths({ year, month }: CalendarMonth, delta: number): CalendarMonth {
  const date = calendarDate(year, month + delta, 1);
  return { year: date.getUTCFullYear(), month: date.getUTCMonth() + 1 };
}

export function compareMonths(a: CalendarMonth, b: CalendarMonth): number {
  return a.year - b.year || a.month - b.month;
}

export function pacificMonthOf(us: number): CalendarMonth {
  const { year, month } = pacificDateTime(us);
  return { year, month };
}

// The grid's first day (the Sunday on or before the 1st) and its day count
// (whole weeks through the Saturday on or after the last day).
function gridSpan({ year, month }: CalendarMonth): { first: Date; dayCount: number } {
  const firstOfMonth = calendarDate(year, month, 1);
  const lastOfMonth = calendarDate(year, month + 1, 0);
  const first = calendarDate(year, month, 1 - firstOfMonth.getUTCDay());
  const last = calendarDate(year, month + 1, DAYS_PER_WEEK - 1 - lastOfMonth.getUTCDay());
  const dayCount = Math.round((last.getTime() - first.getTime()) / 86_400_000) + 1;
  return { first, dayCount };
}

// The weeks of the month, each Sunday through Saturday.
export function buildMonthGrid(calendarMonth: CalendarMonth, now: number): CalendarDay[][] {
  const { first, dayCount } = gridSpan(calendarMonth);
  const todayKey = pacificDayKey(now);
  const weeks: CalendarDay[][] = [];
  for (let index = 0; index < dayCount; index++) {
    const date = calendarDate(
      first.getUTCFullYear(),
      first.getUTCMonth() + 1,
      first.getUTCDate() + index,
    );
    const year = date.getUTCFullYear();
    const month = date.getUTCMonth() + 1;
    const day = date.getUTCDate();
    const key = dayKey(date);
    if (index % DAYS_PER_WEEK === 0) {
      weeks.push([]);
    }
    weeks[weeks.length - 1].push({
      key,
      day,
      inMonth: year === calendarMonth.year && month === calendarMonth.month,
      isToday: key === todayKey,
      dateLabel: formatEventShortDate(pacificWallClockToUs(year, month, day, 12)),
    });
  }
  return weeks;
}

// The grid's whole span in `_us`: its first day at 00:00 Pacific through the
// day after its last. At most six weeks, inside the API's 92-day cap.
export function gridRangeUs(calendarMonth: CalendarMonth): UsRange {
  const { first, dayCount } = gridSpan(calendarMonth);
  const [year, month, day] = [first.getUTCFullYear(), first.getUTCMonth() + 1, first.getUTCDate()];
  return {
    fromUs: pacificWallClockToUs(year, month, day),
    toUs: pacificWallClockToUs(year, month, day + dayCount),
  };
}

// Events by the Pacific day they start on, each day in start order.
export function bucketEventsByDay(events: PublicEvent[]): Map<string, PublicEvent[]> {
  const buckets = new Map<string, PublicEvent[]>();
  for (const event of events) {
    const key = pacificDayKey(event.starts_at_us);
    buckets.set(key, [...(buckets.get(key) ?? []), event]);
  }
  return buckets;
}
