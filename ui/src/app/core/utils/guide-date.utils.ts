// The guide's date formats: GD1 §5's "Verified {Month} {Year}" stamp, where a
// card or a list shows it inline (pages end with the shared freshness footer
// instead), and an upcoming event's start. Stream B's own copy rather than an
// import of Stream A's events-date utilities (GD3 OQ5); it dates in Seattle
// time just as they do, so a card and its page's footer never name different
// months, and an event's time matches its own page.

const GUIDE_TIME_ZONE = 'America/Los_Angeles';

const MONTH_YEAR_FORMAT = new Intl.DateTimeFormat('en-US', {
  timeZone: GUIDE_TIME_ZONE,
  month: 'long',
  year: 'numeric',
});

const EVENT_DAY_FORMAT = new Intl.DateTimeFormat('en-US', {
  timeZone: GUIDE_TIME_ZONE,
  weekday: 'short',
  month: 'short',
  day: 'numeric',
});

const EVENT_TIME_FORMAT = new Intl.DateTimeFormat('en-US', {
  timeZone: GUIDE_TIME_ZONE,
  hour: 'numeric',
  minute: '2-digit',
});

function usToDate(us: number): Date {
  return new Date(Math.floor(us / 1000));
}

// Intl puts a narrow no-break space before "PM" in newer ICU versions; plain
// spaces keep the text the same everywhere.
function plainSpaces(text: string): string {
  return text.replace(/[  ]/g, ' ');
}

// "October 2026"
export function formatGuideMonthYear(us: number): string {
  return MONTH_YEAR_FORMAT.format(usToDate(us));
}

// "Fri, Aug 14 · 9:00 PM": an event's start, for the "coming up" lists on
// venue and scene pages.
export function formatGuideEventStart(us: number): string {
  const date = usToDate(us);
  return plainSpaces(`${EVENT_DAY_FORMAT.format(date)} · ${EVENT_TIME_FORMAT.format(date)}`);
}

// "Verified October 2026", or "Not yet verified" in the same quiet style:
// a missing date is shown honestly, never hidden.
export function guideVerifiedText(lastVerifiedAtUs: number | null): string {
  return lastVerifiedAtUs === null
    ? 'Not yet verified'
    : `Verified ${formatGuideMonthYear(lastVerifiedAtUs)}`;
}
