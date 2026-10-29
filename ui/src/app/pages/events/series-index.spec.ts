import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { vi } from 'vitest';
import { provideCommunityAccess } from '../../core/community-access/community-access.providers';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import routes from './events.routes';
import { SeriesIndexPage } from './series-index';

// Wed Aug 12, 2026, 3:00 PM Pacific: the mock dataset's reference week.
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);

function text(element: Element): string {
  return element.textContent?.replace(/\s+/g, ' ').trim() ?? '';
}

describe('SeriesIndexPage', () => {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date(NOW_US / 1000));
  });
  afterEach(() => vi.useRealTimers());

  it('lists the running series by title, without the ended one', async () => {
    TestBed.configureTestingModule({
      providers: [
        provideRouter([{ path: 'events', children: routes }]),
        provideNoopAnimations(),
        provideCommunityAccess(true),
      ],
    });
    const harness = await RouterTestingHarness.create();
    await harness.navigateByUrl('/events/series', SeriesIndexPage);
    const element = harness.routeNativeElement as HTMLElement;
    const items = Array.from(element.querySelectorAll('.series-index__item')).map((item) => [
      text(item.querySelector('.series-index__name')!),
      text(item.querySelector('.series-index__when')!),
    ]);
    expect(items).toEqual([
      ['Bear Happy Hour', 'Thursdays, 6–9 PM · The Lantern Room'],
      ['Sober Saturday Hike', 'One Saturday a month, spring through fall'],
    ]);
    expect(element.querySelector('.series-index__name')?.getAttribute('href')).toBe(
      '/events/series/bear-happy-hour',
    );
  });
});
