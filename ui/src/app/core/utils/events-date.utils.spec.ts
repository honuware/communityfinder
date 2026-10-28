import {
  formatEventDateLine,
  formatEventTime,
  monthRangeUs,
  pacificDateTime,
  pacificDayKey,
  pacificDayOfWeek,
  pacificWallClockToUs,
  tonightRangeUs,
  usToDate,
  weekendRangeUs,
} from './events-date.utils';

// A UTC instant in `_us`. Expected values are written as UTC with the Pacific
// offset worked in by hand: PDT is UTC−7 (Mar 8 – Nov 1, 2026), PST is UTC−8.
function utc(year: number, month: number, day: number, hour = 0, minute = 0): number {
  return Date.UTC(year, month - 1, day, hour, minute) * 1000;
}

const HOUR_US = 3_600 * 1_000_000;

// Fri Aug 14, 2026, 10:00 PM PDT — already Saturday 05:00 in UTC.
const FRIDAY_10PM = utc(2026, 8, 15, 5);

describe('events-date.utils', () => {
  describe('conversions', () => {
    it('turns _us into a Date and back', () => {
      expect(usToDate(FRIDAY_10PM).toISOString()).toBe('2026-08-15T05:00:00.000Z');
    });

    it('reads the Pacific wall clock, not UTC', () => {
      expect(pacificDateTime(FRIDAY_10PM)).toEqual({
        year: 2026,
        month: 8,
        day: 14,
        hour: 22,
        minute: 0,
        second: 0,
      });
      expect(pacificDayOfWeek(FRIDAY_10PM)).toBe(5);
    });

    it('turns a Pacific wall-clock time into its instant, in summer and winter', () => {
      expect(pacificWallClockToUs(2026, 8, 14, 22)).toBe(FRIDAY_10PM);
      expect(pacificWallClockToUs(2026, 12, 24, 19, 30)).toBe(utc(2026, 12, 25, 3, 30));
    });

    it('rolls an overflowing day or month into the next', () => {
      expect(pacificWallClockToUs(2026, 8, 32)).toBe(pacificWallClockToUs(2026, 9, 1));
      expect(pacificWallClockToUs(2026, 13, 1)).toBe(pacificWallClockToUs(2027, 1, 1));
    });

    it('lands on the right side of both 2026 DST changes', () => {
      // Spring forward Sun Mar 8: midnight is still PST, 4 AM is PDT.
      expect(pacificWallClockToUs(2026, 3, 8, 0)).toBe(utc(2026, 3, 8, 8));
      expect(pacificWallClockToUs(2026, 3, 8, 4)).toBe(utc(2026, 3, 8, 11));
      // Fall back Sun Nov 1: midnight is still PDT, 4 AM is PST.
      expect(pacificWallClockToUs(2026, 11, 1, 0)).toBe(utc(2026, 11, 1, 7));
      expect(pacificWallClockToUs(2026, 11, 1, 4)).toBe(utc(2026, 11, 1, 12));
    });
  });

  describe('formatting', () => {
    it('shows the Pacific time with a plain space before AM/PM', () => {
      expect(formatEventTime(FRIDAY_10PM)).toBe('10:00 PM');
      expect(formatEventTime(pacificWallClockToUs(2026, 8, 15, 9, 5))).toBe('9:05 AM');
    });

    it('shows the Pacific date line', () => {
      expect(formatEventDateLine(FRIDAY_10PM)).toBe('Friday, Aug 14');
    });

    it('keys a 10 PM event by its Pacific day, not its UTC day', () => {
      expect(pacificDayKey(FRIDAY_10PM)).toBe('2026-08-14');
      expect(usToDate(FRIDAY_10PM).getUTCDate()).toBe(15);
      expect(pacificDayKey(pacificWallClockToUs(2026, 1, 5, 0, 0))).toBe('2026-01-05');
    });
  });

  describe('tonightRangeUs', () => {
    it('runs from now to 4 AM tomorrow in the evening', () => {
      const sevenPm = pacificWallClockToUs(2026, 8, 14, 19);
      expect(tonightRangeUs(sevenPm)).toEqual({ fromUs: sevenPm, toUs: utc(2026, 8, 15, 11) });
    });

    it('still means last night after midnight, ending at 4 AM today', () => {
      const oneThirtyAm = pacificWallClockToUs(2026, 8, 15, 1, 30);
      expect(tonightRangeUs(oneThirtyAm).toUs).toBe(utc(2026, 8, 15, 11));
      const threeFiftyNine = pacificWallClockToUs(2026, 8, 15, 3, 59);
      expect(tonightRangeUs(threeFiftyNine).toUs).toBe(utc(2026, 8, 15, 11));
    });

    it('moves to the next night at 4 AM exactly', () => {
      const fourAm = pacificWallClockToUs(2026, 8, 15, 4);
      expect(tonightRangeUs(fourAm).toUs).toBe(utc(2026, 8, 16, 11));
    });

    it('is an hour shorter on the spring-forward night and an hour longer in the fall', () => {
      const springEightPm = pacificWallClockToUs(2026, 3, 7, 20);
      const spring = tonightRangeUs(springEightPm);
      expect(spring.toUs).toBe(utc(2026, 3, 8, 11));
      expect(spring.toUs - spring.fromUs).toBe(7 * HOUR_US);

      const fallEightPm = pacificWallClockToUs(2026, 10, 31, 20);
      const fall = tonightRangeUs(fallEightPm);
      expect(fall.toUs).toBe(utc(2026, 11, 1, 12));
      expect(fall.toUs - fall.fromUs).toBe(9 * HOUR_US);
    });
  });

  describe('weekendRangeUs', () => {
    // The week of Mon Aug 10 – Sun Aug 16, 2026: the weekend is Fri Aug 14
    // 00:00 PDT (07:00 UTC) through Mon Aug 17 00:00 PDT.
    const FRIDAY_START = utc(2026, 8, 14, 7);
    const MONDAY_START = utc(2026, 8, 17, 7);

    it('looks ahead to Friday from every weekday', () => {
      for (const day of [10, 11, 12, 13]) {
        const noon = pacificWallClockToUs(2026, 8, day, 12);
        expect(weekendRangeUs(noon)).toEqual({ fromUs: FRIDAY_START, toUs: MONDAY_START });
      }
      // Late Thursday is still before the weekend.
      const lateThursday = pacificWallClockToUs(2026, 8, 13, 23, 59);
      expect(weekendRangeUs(lateThursday).fromUs).toBe(FRIDAY_START);
    });

    it('is the rest of the weekend from Friday through Sunday', () => {
      const dayAndHour: Array<[number, number]> = [
        [14, 0],
        [14, 10],
        [15, 13],
        [16, 9],
        [16, 23],
      ];
      for (const [day, hour] of dayAndHour) {
        const now = pacificWallClockToUs(2026, 8, day, hour);
        expect(weekendRangeUs(now)).toEqual({ fromUs: now, toUs: MONDAY_START });
      }
    });

    it('moves to next weekend once Monday starts', () => {
      const earlyMonday = pacificWallClockToUs(2026, 8, 17, 0, 30);
      expect(weekendRangeUs(earlyMonday)).toEqual({
        fromUs: utc(2026, 8, 21, 7),
        toUs: utc(2026, 8, 24, 7),
      });
    });

    it('spans the fall-back weekend: Fri Oct 30 PDT to Mon Nov 2 PST', () => {
      const wednesday = pacificWallClockToUs(2026, 10, 28, 12);
      expect(weekendRangeUs(wednesday)).toEqual({
        fromUs: utc(2026, 10, 30, 7),
        toUs: utc(2026, 11, 2, 8),
      });
    });

    it('reaches across a month end', () => {
      // Thu Dec 31, 2026 → Fri Jan 1 – Mon Jan 4, 2027 (PST).
      const newYearsEve = pacificWallClockToUs(2026, 12, 31, 12);
      expect(weekendRangeUs(newYearsEve)).toEqual({
        fromUs: utc(2027, 1, 1, 8),
        toUs: utc(2027, 1, 4, 8),
      });
    });
  });

  describe('monthRangeUs', () => {
    it('runs from the 1st to the next 1st, across a DST change', () => {
      expect(monthRangeUs(2026, 3)).toEqual({
        fromUs: utc(2026, 3, 1, 8),
        toUs: utc(2026, 4, 1, 7),
      });
    });

    it('rolls December into January', () => {
      expect(monthRangeUs(2026, 12)).toEqual({
        fromUs: utc(2026, 12, 1, 8),
        toUs: utc(2027, 1, 1, 8),
      });
    });
  });
});
