// Every date and time on the events pages, in one place (EV2's time pin). Event
// times are real UTC instants in microseconds (`_us`), and they're always shown
// in Seattle time, America/Los_Angeles, wherever the viewer is. Nothing here
// reads the browser's own time zone: Intl does the zone math, so a visitor in
// Tokyo sees the same "Friday, Aug 14 · 10:00 PM" as one on Capitol Hill.
//
// "Day" means a Pacific calendar day throughout, keyed "yyyy-mm-dd".

export const EVENTS_TIME_ZONE = 'America/Los_Angeles';

// [fromUs, toUs): `toUs` is the first instant NOT in the range.
export interface UsRange {
  fromUs: number;
  toUs: number;
}

// A Pacific wall-clock reading. Months are 1–12.
export interface PacificDateTime {
  year: number;
  month: number;
  day: number;
  hour: number;
  minute: number;
  second: number;
}

// When "tonight" ends: bar nights spill past midnight.
export const TONIGHT_ENDS_AT_HOUR = 4;

const PARTS_FORMAT = new Intl.DateTimeFormat('en-US', {
  timeZone: EVENTS_TIME_ZONE,
  year: 'numeric',
  month: '2-digit',
  day: '2-digit',
  hour: '2-digit',
  minute: '2-digit',
  second: '2-digit',
  hourCycle: 'h23',
});

const TIME_FORMAT = new Intl.DateTimeFormat('en-US', {
  timeZone: EVENTS_TIME_ZONE,
  hour: 'numeric',
  minute: '2-digit',
});

const DATE_LINE_FORMAT = new Intl.DateTimeFormat('en-US', {
  timeZone: EVENTS_TIME_ZONE,
  weekday: 'long',
  month: 'short',
  day: 'numeric',
});

export function usToDate(us: number): Date {
  return new Date(Math.floor(us / 1000));
}

export function dateToUs(date: Date): number {
  return date.getTime() * 1000;
}

export function nowUs(): number {
  return Date.now() * 1000;
}

export function pacificDateTime(us: number): PacificDateTime {
  const parts = PARTS_FORMAT.formatToParts(usToDate(us));
  const part = (type: Intl.DateTimeFormatPartTypes) =>
    Number(parts.find((p) => p.type === type)?.value ?? 0);
  return {
    year: part('year'),
    month: part('month'),
    day: part('day'),
    hour: part('hour'),
    minute: part('minute'),
    second: part('second'),
  };
}

// Pacific's offset from UTC at an instant, in ms (−7h in summer, −8h in winter).
function pacificOffsetMs(utcMs: number): number {
  const wall = pacificDateTime(utcMs * 1000);
  const wallAsUtcMs = Date.UTC(
    wall.year,
    wall.month - 1,
    wall.day,
    wall.hour,
    wall.minute,
    wall.second,
  );
  return wallAsUtcMs - (utcMs - (utcMs % 1000));
}

// A Pacific wall-clock time → its `_us` instant. Out-of-range parts roll over
// (day 32 is the 1st of next month, month 13 is next January), so "tomorrow"
// is just `day + 1`. Two passes settle the offset across a DST change. A wall
// time that doesn't exist (2:30 AM on the spring-forward night) lands an hour
// off; nothing here asks for one — the boundaries used are midnight and 4 AM.
export function pacificWallClockToUs(
  year: number,
  month: number,
  day: number,
  hour = 0,
  minute = 0,
): number {
  const wallAsUtcMs = Date.UTC(year, month - 1, day, hour, minute);
  let utcMs = wallAsUtcMs - pacificOffsetMs(wallAsUtcMs);
  utcMs = wallAsUtcMs - pacificOffsetMs(utcMs);
  return utcMs * 1000;
}

// Intl puts a narrow no-break space before AM/PM; a plain space keeps the
// text predictable for pages and specs.
function plainSpaces(text: string): string {
  return text.replace(/[  ]/g, ' ');
}

// "10:00 PM"
export function formatEventTime(us: number): string {
  return plainSpaces(TIME_FORMAT.format(usToDate(us)));
}

// "Friday, Aug 14" — the list's day header and the detail page's date line.
export function formatEventDateLine(us: number): string {
  return plainSpaces(DATE_LINE_FORMAT.format(usToDate(us)));
}

// "2026-08-14": the Pacific day an instant falls on. A 10 PM Friday event is
// Friday here, though it's already Saturday in UTC.
export function pacificDayKey(us: number): string {
  const { year, month, day } = pacificDateTime(us);
  return `${year}-${String(month).padStart(2, '0')}-${String(day).padStart(2, '0')}`;
}

// 0 = Sunday … 6 = Saturday, for the Pacific day an instant falls on.
export function pacificDayOfWeek(us: number): number {
  const { year, month, day } = pacificDateTime(us);
  return new Date(Date.UTC(year, month - 1, day)).getUTCDay();
}

// Tonight (`/events?when=tonight`): now through the next 4:00 AM Pacific. In
// the evening that's 4 AM tomorrow; at 1 AM it's 4 AM today, since it's still
// last night.
export function tonightRangeUs(now: number): UsRange {
  const { year, month, day } = pacificDateTime(now);
  let toUs = pacificWallClockToUs(year, month, day, TONIGHT_ENDS_AT_HOUR);
  if (toUs <= now) {
    toUs = pacificWallClockToUs(year, month, day + 1, TONIGHT_ENDS_AT_HOUR);
  }
  return { fromUs: now, toUs };
}

// This weekend (`/events?when=weekend`): the coming Friday 00:00 through
// Sunday 24:00 Pacific. From Friday through Sunday it's the rest of this one.
export function weekendRangeUs(now: number): UsRange {
  const { year, month, day } = pacificDateTime(now);
  const dayOfWeek = pacificDayOfWeek(now);
  const insideWeekend = dayOfWeek === 5 || dayOfWeek === 6 || dayOfWeek === 0;
  // Days from today back to this weekend's Friday, or forward to the next one.
  const fridayOffset = insideWeekend ? -((dayOfWeek + 2) % 7) : (5 - dayOfWeek + 7) % 7;
  const friday = day + fridayOffset;
  return {
    fromUs: insideWeekend ? now : pacificWallClockToUs(year, month, friday),
    toUs: pacificWallClockToUs(year, month, friday + 3),
  };
}

// A Pacific calendar month (1–12): its 1st at 00:00 through the next month's.
// The calendar page widens this to its leading and trailing grid days.
export function monthRangeUs(year: number, month: number): UsRange {
  return {
    fromUs: pacificWallClockToUs(year, month, 1),
    toUs: pacificWallClockToUs(year, month + 1, 1),
  };
}
