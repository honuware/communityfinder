import { formatGuideEventStart, formatGuideMonthYear, guideVerifiedText } from './guide-date.utils';

describe('guide-date.utils', () => {
  // New Year's Eve, 8 PM Seattle time (PST, UTC-8): already January in UTC
  // and in Tokyo.
  const NEW_YEARS_EVE_US = Date.UTC(2027, 0, 1, 4) * 1000;

  it('names the month in Seattle time, wherever the viewer is', () => {
    expect(formatGuideMonthYear(NEW_YEARS_EVE_US)).toBe('December 2026');
    expect(formatGuideMonthYear(NEW_YEARS_EVE_US + 4 * 3600 * 1_000_000)).toBe('January 2027');
  });

  it('stamps a verified listing, and says so plainly when it never was', () => {
    expect(guideVerifiedText(NEW_YEARS_EVE_US)).toBe('Verified December 2026');
    expect(guideVerifiedText(null)).toBe('Not yet verified');
  });

  it("gives an event's start in Seattle time, with plain spaces", () => {
    // 04:00 UTC on Sat Aug 15 is 9 PM Friday in Seattle (PDT, UTC-7).
    expect(formatGuideEventStart(Date.UTC(2026, 7, 15, 4) * 1000)).toBe('Fri, Aug 14 · 9:00 PM');
    expect(formatGuideEventStart(NEW_YEARS_EVE_US)).toBe('Thu, Dec 31 · 8:00 PM');
  });
});
