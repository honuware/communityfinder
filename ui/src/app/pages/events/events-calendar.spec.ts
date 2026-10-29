import { TestBed } from '@angular/core/testing';
import { provideRouter, Router } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { vi } from 'vitest';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import { provideCommunityAccess } from '../../core/community-access/community-access.providers';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import { gridRangeUs } from './calendar-grid';
import { EventsCalendarPage } from './events-calendar';
import routes from './events.routes';

// Wed Aug 12, 2026, 3:00 PM Pacific: the mock dataset's reference week.
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);

async function open(url: string) {
  TestBed.configureTestingModule({
    providers: [
      provideRouter([{ path: 'events', children: routes }]),
      provideNoopAnimations(),
      provideCommunityAccess(true),
    ],
  });
  const fetch = vi.spyOn(TestBed.inject(COMMUNITY_ACCESS), 'getUpcomingEvents');
  const harness = await RouterTestingHarness.create();
  await harness.navigateByUrl(url, EventsCalendarPage);
  return { harness, fetch, element: harness.routeNativeElement as HTMLElement };
}

function title(element: HTMLElement): string {
  return element.querySelector('.calendar__title')?.textContent?.trim() ?? '';
}

function dayTitles(element: HTMLElement, key: string): string[] {
  return Array.from(element.querySelectorAll(`[data-day="${key}"] .calendar__event`)).map(
    (chip) => chip.lastChild?.textContent?.trim() ?? '',
  );
}

function button(element: HTMLElement, label: string): HTMLButtonElement {
  return Array.from(element.querySelectorAll('button')).find(
    (candidate) =>
      candidate.getAttribute('aria-label') === label || candidate.textContent?.trim() === label,
  )!;
}

describe('EventsCalendarPage', () => {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date(NOW_US / 1000));
  });
  afterEach(() => vi.useRealTimers());

  it("shows this month's grid, each event on its Pacific day", async () => {
    const { element, fetch } = await open('/events/calendar');
    expect(title(element)).toBe('August 2026');
    expect(fetch).toHaveBeenCalledWith({
      from_us: gridRangeUs({ year: 2026, month: 8 }).fromUs,
      to_us: gridRangeUs({ year: 2026, month: 8 }).toUs,
    });
    expect(dayTitles(element, '2026-08-12')).toEqual(['Karaoke Night', 'Late Show: Drag Revue']);
    expect(dayTitles(element, '2026-08-13')).toEqual(['Bear Happy Hour']);
    expect(element.querySelector('[data-day="2026-08-12"]')?.classList).toContain(
      'calendar__day--today',
    );
    expect(element.querySelector('[data-day="2026-07-26"]')?.classList).toContain(
      'calendar__day--outside',
    );
    const chip = element.querySelector('[data-day="2026-08-13"] .calendar__event');
    expect(chip?.getAttribute('href')).toBe('/events/107');
    // Nothing before this month to go back to.
    expect(button(element, 'Previous month').disabled).toBe(true);
  });

  it("fills the trailing days with the next month's first events", async () => {
    const { element, fetch } = await open('/events/calendar?month=2026-09');
    expect(title(element)).toBe('September 2026');
    expect(fetch).toHaveBeenCalledWith({
      from_us: gridRangeUs({ year: 2026, month: 9 }).fromUs,
      to_us: gridRangeUs({ year: 2026, month: 9 }).toUs,
    });
    // Thu Oct 1 closes the September grid.
    expect(dayTitles(element, '2026-10-01')).toEqual(['Pride Planning Picnic']);
    expect(element.querySelector('[data-day="2026-10-01"]')?.classList).toContain(
      'calendar__day--outside',
    );
  });

  it('steps month to month in the URL, back no further than this one', async () => {
    const { harness, element } = await open('/events/calendar');
    const router = TestBed.inject(Router);
    button(element, 'Next month').click();
    await harness.fixture.whenStable();
    harness.detectChanges();
    expect(router.url).toBe('/events/calendar?month=2026-09');
    expect(title(element)).toBe('September 2026');
    expect(button(element, 'Previous month').disabled).toBe(false);

    button(element, 'Previous month').click();
    await harness.fixture.whenStable();
    harness.detectChanges();
    // This month has no ?month= of its own.
    expect(router.url).toBe('/events/calendar');
    expect(title(element)).toBe('August 2026');

    await harness.navigateByUrl('/events/calendar?month=2026-03');
    expect(title(element)).toBe('August 2026');
  });

  it('says so when a month has nothing on it yet', async () => {
    const { element } = await open('/events/calendar?month=2026-12');
    expect(element.textContent).toContain("Nothing's on the calendar this month yet.");
  });
});
