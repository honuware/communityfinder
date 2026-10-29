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

const SHORT_DATE_FORMAT = new Intl.DateTimeFormat('en-US', {
  timeZone: EVENTS_TIME_ZONE,
  weekday: 'short',
  month: 'short',
  day: 'numeric',
});

const MONTH_YEAR_FORMAT = new Intl.DateTimeFormat('en-US', {
  timeZone: EVENTS_TIME_ZONE,
  month: 'long',
  year: 'numeric',
});

// An end time on the next morning up to this hour reads as the same night
// ("10:00 PM – 2:00 AM"); anything later names its day.
const SAME_NIGHT_ENDS_BY_HOUR = 6;

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

// "Fri, Aug 14" — a row's date where there's no day header above it.
export function formatEventShortDate(us: number): string {
  return plainSpaces(SHORT_DATE_FORMAT.format(usToDate(us)));
}

// "August 2026" — the calendar's heading and the "Verified" footer.
export function formatMonthYear(us: number): string {
  return plainSpaces(MONTH_YEAR_FORMAT.format(usToDate(us)));
}

// "10:00 PM", "10:00 PM – 2:00 AM", or "10:00 AM – Sun, Aug 16, 6:00 PM": the
// end names its day only when it isn't the same day or night as the start.
export function formatEventTimeRange(startsAtUs: number, endsAtUs: number | null): string {
  const start = formatEventTime(startsAtUs);
  if (endsAtUs === null || endsAtUs <= startsAtUs) {
    return start;
  }
  const end = formatEventTime(endsAtUs);
  const startDay = pacificDateTime(startsAtUs);
  const endDay = pacificDayKey(endsAtUs);
  const nextMorning = pacificWallClockToUs(
    startDay.year,
    startDay.month,
    startDay.day + 1,
    SAME_NIGHT_ENDS_BY_HOUR,
  );
  if (endDay === pacificDayKey(startsAtUs) || endsAtUs <= nextMorning) {
    return `${start} – ${end}`;
  }
  return `${start} – ${formatEventShortDate(endsAtUs)}, ${end}`;
}

function twoDigits(value: number): string {
  return String(value).padStart(2, '0');
}

// "2026-08-14": the Pacific day an instant falls on. A 10 PM Friday event is
// Friday here, though it's already Saturday in UTC. Also the value of an
// <input type="date">.
export function pacificDayKey(us: number): string {
  const { year, month, day } = pacificDateTime(us);
  return `${year}-${twoDigits(month)}-${twoDigits(day)}`;
}

// "22:00": the Pacific time of day, as an <input type="time"> holds it.
export function pacificTimeOfDay(us: number): string {
  const { hour, minute } = pacificDateTime(us);
  return `${twoDigits(hour)}:${twoDigits(minute)}`;
}

// The two form inputs ("2026-08-14", "22:00") → their `_us` instant, or null
// when either is blank or malformed.
export function pacificInputsToUs(dateText: string, timeText: string): number | null {
  const date = /^(\d{4})-(\d{2})-(\d{2})$/.exec(dateText.trim());
  const time = /^(\d{1,2}):(\d{2})$/.exec(timeText.trim());
  if (!date || !time) {
    return null;
  }
  const [year, month, day, hour, minute] = [date[1], date[2], date[3], time[1], time[2]].map(
    Number,
  );
  if (month < 1 || month > 12 || day < 1 || day > 31 || hour > 23 || minute > 59) {
    return null;
  }
  return pacificWallClockToUs(year, month, day, hour, minute);
}

// "2026-08-14T22:00:00-07:00": ISO 8601 with the Pacific offset, for
// schema.org's startDate / endDate.
export function formatPacificIso(us: number): string {
  const wall = pacificDateTime(us);
  const offsetMinutes = Math.round(pacificOffsetMs(Math.floor(us / 1000)) / 60_000);
  const sign = offsetMinutes < 0 ? '-' : '+';
  const absolute = Math.abs(offsetMinutes);
  return (
    `${wall.year}-${twoDigits(wall.month)}-${twoDigits(wall.day)}` +
    `T${twoDigits(wall.hour)}:${twoDigits(wall.minute)}:${twoDigits(wall.second)}` +
    `${sign}${twoDigits(Math.floor(absolute / 60))}:${twoDigits(absolute % 60)}`
  );
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
