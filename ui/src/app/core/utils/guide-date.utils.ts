// The guide's one date format: GD1 §5's "Verified {Month} {Year}" stamp, where
// a card or a list shows it inline (pages end with the shared freshness
// footer instead). Stream B's own copy rather than an import of Stream A's
// events-date utilities (GD3 OQ5); it dates in Seattle time just as they do,
// so a card and its page's footer never name different months.

const GUIDE_TIME_ZONE = 'America/Los_Angeles';

const MONTH_YEAR_FORMAT = new Intl.DateTimeFormat('en-US', {
  timeZone: GUIDE_TIME_ZONE,
  month: 'long',
  year: 'numeric',
});

// "October 2026"
export function formatGuideMonthYear(us: number): string {
  return MONTH_YEAR_FORMAT.format(new Date(Math.floor(us / 1000)));
}

// "Verified October 2026", or "Not yet verified" in the same quiet style:
// a missing date is shown honestly, never hidden.
export function guideVerifiedText(lastVerifiedAtUs: number | null): string {
  return lastVerifiedAtUs === null
    ? 'Not yet verified'
    : `Verified ${formatGuideMonthYear(lastVerifiedAtUs)}`;
}
