import { TestBed } from '@angular/core/testing';
import { provideRouter, Router } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { vi } from 'vitest';
import { of } from 'rxjs';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import { provideCommunityAccess } from '../../core/community-access/community-access.providers';
import { PastEventSettingsMockStore } from '../../core/mock/past-event-settings-mock-store';
import { normalizeEvent } from '../../core/types/events.types';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import { gridRangeUs } from './calendar-grid';
import { EventsCalendarPage, SHOW_PAST_STORAGE_KEY } from './events-calendar';
import routes from './events.routes';

// Wed Aug 12, 2026, 3:00 PM Pacific: the mock dataset's reference week. Its
// past events: Trivia Night (Aug 9), Bear Happy Hour (Aug 6), the farewell
// brunch (May 4), the coat drive (Jan 24), and the ball over a year ago.
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);

async function open(url: string, before: () => void = () => undefined) {
  TestBed.configureTestingModule({
    providers: [
      provideRouter([{ path: 'events', children: routes }]),
      provideNoopAnimations(),
      provideCommunityAccess(true),
    ],
  });
  before();
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

function showPastToggle(element: HTMLElement): HTMLButtonElement {
  return element.querySelector('.calendar__show-past button')!;
}

describe('EventsCalendarPage', () => {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date(NOW_US / 1000));
    localStorage.clear();
  });
  afterEach(() => {
    vi.useRealTimers();
    localStorage.clear();
  });

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

  describe('past events (EV4 §10)', () => {
    it('hides what is over until "Show past events" is on, then shows it muted, and remembers', async () => {
      const { harness, element } = await open('/events/calendar');
      expect(showPastToggle(element).getAttribute('aria-checked')).toBe('false');
      expect(dayTitles(element, '2026-08-09')).toEqual([]);
      expect(dayTitles(element, '2026-08-06')).toEqual([]);

      showPastToggle(element).click();
      harness.detectChanges();
      expect(dayTitles(element, '2026-08-09')).toEqual(['Trivia Night']);
      expect(dayTitles(element, '2026-08-06')).toEqual(['Bear Happy Hour']);
      const trivia = element.querySelector('[data-day="2026-08-09"] .calendar__event')!;
      expect(trivia.classList).toContain('calendar__event--past');
      const karaoke = element.querySelector('[data-day="2026-08-12"] .calendar__event')!;
      expect(karaoke.classList).not.toContain('calendar__event--past');
      expect(localStorage.getItem(SHOW_PAST_STORAGE_KEY)).toBe('true');
      // Now there's a past to go back to.
      expect(button(element, 'Previous month').disabled).toBe(false);
    });

    it('goes back as far as the past-events window, no further', async () => {
      const { harness, element } = await open('/events/calendar?month=2026-05', () =>
        localStorage.setItem(SHOW_PAST_STORAGE_KEY, 'true'),
      );
      expect(title(element)).toBe('May 2026');
      expect(dayTitles(element, '2026-05-04')).toEqual(['Drag Brunch Club: Farewell Brunch']);
      expect(button(element, 'This month')).toBeDefined();

      await harness.navigateByUrl('/events/calendar?month=2026-01');
      expect(dayTitles(element, '2026-01-24')).toEqual(['Winter Coat Drive']);

      // A year back from Aug 12, 2026 is in August 2025.
      await harness.navigateByUrl('/events/calendar?month=2025-03');
      expect(title(element)).toBe('August 2025');
      expect(button(element, 'Previous month').disabled).toBe(true);
    });

    it('comes back to this month when the past is hidden from an earlier one', async () => {
      const { harness, element } = await open('/events/calendar?month=2026-05', () =>
        localStorage.setItem(SHOW_PAST_STORAGE_KEY, 'true'),
      );
      showPastToggle(element).click();
      await harness.fixture.whenStable();
      harness.detectChanges();
      expect(TestBed.inject(Router).url).toBe('/events/calendar');
      expect(title(element)).toBe('August 2026');
      expect(localStorage.getItem(SHOW_PAST_STORAGE_KEY)).toBe('false');
    });

    it("starts from the site's default until the visitor chooses", async () => {
      const showByDefault = () =>
        TestBed.inject(PastEventSettingsMockStore).save({ events_show_past_by_default: true });
      const { element } = await open('/events/calendar', showByDefault);
      expect(showPastToggle(element).getAttribute('aria-checked')).toBe('true');
      expect(dayTitles(element, '2026-08-09')).toEqual(['Trivia Night']);
    });

    it("keeps the visitor's own choice over the site's default", async () => {
      const { element } = await open('/events/calendar', () => {
        TestBed.inject(PastEventSettingsMockStore).save({ events_show_past_by_default: true });
        localStorage.setItem(SHOW_PAST_STORAGE_KEY, 'false');
      });
      expect(showPastToggle(element).getAttribute('aria-checked')).toBe('false');
      expect(dayTitles(element, '2026-08-09')).toEqual([]);
    });

    it('says what to do when everything this month is over', async () => {
      const { harness, element, fetch } = await open('/events/calendar?month=2026-09');
      const over = normalizeEvent({
        id: 1,
        title: 'Already over',
        starts_at_us: NOW_US - 3 * 3_600 * 1_000_000,
      });
      fetch.mockReturnValue(of({ items: [over], from_us: 0, to_us: 0 }));
      await harness.navigateByUrl('/events/calendar');
      expect(element.textContent).toContain('Nothing else is on the calendar this month.');
      expect(element.textContent).not.toContain("Nothing's on the calendar this month yet.");
    });
  });
});
