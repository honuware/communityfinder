import { normalizeEvent } from '../../core/types/events.types';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import {
  addMonths,
  bucketEventsByDay,
  buildMonthGrid,
  compareMonths,
  gridRangeUs,
  monthParam,
  pacificMonthOf,
  parseMonthParam,
} from './calendar-grid';

function utc(year: number, month: number, day: number, hour = 0): number {
  return Date.UTC(year, month - 1, day, hour) * 1000;
}

const AUGUST_2026 = { year: 2026, month: 8 };
const NOON_AUG_12 = pacificWallClockToUs(2026, 8, 12, 12);

describe('calendar-grid', () => {
  it('reads and writes the month query param', () => {
    expect(parseMonthParam('2026-08')).toEqual(AUGUST_2026);
    expect(parseMonthParam('2026-13')).toBeNull();
    expect(parseMonthParam('august')).toBeNull();
    expect(parseMonthParam(null)).toBeNull();
    expect(monthParam({ year: 2027, month: 1 })).toBe('2027-01');
  });

  it('steps months across a year end and compares them', () => {
    expect(addMonths({ year: 2026, month: 12 }, 1)).toEqual({ year: 2027, month: 1 });
    expect(addMonths({ year: 2027, month: 1 }, -1)).toEqual({ year: 2026, month: 12 });
    expect(compareMonths(AUGUST_2026, { year: 2026, month: 9 })).toBeLessThan(0);
    expect(compareMonths(AUGUST_2026, AUGUST_2026)).toBe(0);
    // 8 PM on Aug 31 Pacific is already September in UTC.
    expect(pacificMonthOf(pacificWallClockToUs(2026, 8, 31, 20))).toEqual(AUGUST_2026);
  });

  it('lays August 2026 out Sunday-first, with the leading and trailing days', () => {
    const weeks = buildMonthGrid(AUGUST_2026, NOON_AUG_12);
    // Aug 1 is a Saturday and Aug 31 a Monday: Sun Jul 26 through Sat Sep 5.
    expect(weeks).toHaveLength(6);
    expect(weeks.every((week) => week.length === 7)).toBe(true);
    expect(weeks[0][0]).toMatchObject({ key: '2026-07-26', day: 26, inMonth: false });
    expect(weeks[0][6]).toMatchObject({
      key: '2026-08-01',
      inMonth: true,
      dateLabel: 'Sat, Aug 1',
    });
    expect(weeks[5][6]).toMatchObject({ key: '2026-09-05', inMonth: false });
    const today = weeks.flat().filter((day) => day.isToday);
    expect(today.map((day) => day.key)).toEqual(['2026-08-12']);
  });

  it('needs only four weeks for February 2026, which starts on a Sunday', () => {
    const weeks = buildMonthGrid({ year: 2026, month: 2 }, NOON_AUG_12);
    expect(weeks).toHaveLength(4);
    expect(weeks[0][0].key).toBe('2026-02-01');
    expect(weeks[3][6].key).toBe('2026-02-28');
    expect(weeks.flat().every((day) => day.inMonth && !day.isToday)).toBe(true);
  });

  it('spans the whole grid in Pacific time, across the fall-back change', () => {
    expect(gridRangeUs(AUGUST_2026)).toEqual({
      fromUs: utc(2026, 7, 26, 7),
      toUs: utc(2026, 9, 6, 7),
    });
    // Sun Nov 1 00:00 PDT through Sun Dec 6 00:00 PST.
    expect(gridRangeUs({ year: 2026, month: 11 })).toEqual({
      fromUs: utc(2026, 11, 1, 7),
      toUs: utc(2026, 12, 6, 8),
    });
  });

  it('buckets events by their Pacific day, trailing days included', () => {
    const lateJuly31 = normalizeEvent({
      id: 1,
      starts_at_us: pacificWallClockToUs(2026, 7, 31, 23),
    });
    const sept2 = normalizeEvent({ id: 2, starts_at_us: pacificWallClockToUs(2026, 9, 2, 19) });
    const sept2Later = normalizeEvent({
      id: 3,
      starts_at_us: pacificWallClockToUs(2026, 9, 2, 21),
    });
    const buckets = bucketEventsByDay([lateJuly31, sept2, sept2Later]);
    expect(buckets.get('2026-07-31')?.map((e) => e.id)).toEqual([1]);
    expect(buckets.get('2026-08-01')).toBeUndefined();
    expect(buckets.get('2026-09-02')?.map((e) => e.id)).toEqual([2, 3]);
  });
});
