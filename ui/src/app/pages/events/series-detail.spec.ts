import { DOCUMENT } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { vi } from 'vitest';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { provideCommunityAccess } from '../../core/community-access/community-access.providers';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import routes from './events.routes';
import { SeriesDetailPage } from './series-detail';

// Wed Aug 12, 2026, 3:00 PM Pacific: the mock dataset's reference week. Bear
// Happy Hour was verified 20 days ago (July), Drag Brunch Club closed and
// was last verified 120 days ago (April), and Sober Saturday Hike is seasonal
// and never verified.
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);

async function open(url: string): Promise<HTMLElement> {
  TestBed.configureTestingModule({
    providers: [
      provideRouter([{ path: 'events', children: routes }]),
      provideNoopAnimations(),
      provideCommunityAccess(true),
      { provide: HONUWARE_API_BASE, useValue: '/api' },
    ],
  });
  const harness = await RouterTestingHarness.create();
  await harness.navigateByUrl(url, SeriesDetailPage);
  return harness.routeNativeElement as HTMLElement;
}

function text(element: Element | null): string {
  return element?.textContent?.replace(/\s+/g, ' ').trim() ?? '';
}

describe('SeriesDetailPage', () => {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date(NOW_US / 1000));
  });
  afterEach(() => vi.useRealTimers());

  it("shows a running series' dates, its feed, and when it was last checked", async () => {
    const element = await open('/events/series/bear-happy-hour');
    const document = TestBed.inject(DOCUMENT);
    expect(text(element.querySelector('h1'))).toBe('Bear Happy Hour');
    expect(text(element.querySelector('.series-detail__when'))).toBe(
      'Thursdays, 6–9 PM at The Lantern Room',
    );
    const dates = Array.from(element.querySelectorAll('.event-row__when')).map(text);
    expect(dates).toEqual([
      'Thu, Aug 13 · 6:00 PM – 9:00 PM',
      'Thu, Aug 20 · 6:00 PM – 9:00 PM',
      'Thu, Aug 27 · 6:00 PM – 9:00 PM',
    ]);
    expect(element.querySelector('.subscribe-link__subscribe')?.getAttribute('href')).toBe(
      `webcal://${document.location.host}/api/events/feed.ics?series_id=11`,
    );
    expect(text(element.querySelector('app-freshness-footer'))).toBe('Verified July 2026');
    expect(document.title).toBe(
      'Bear Happy Hour — Thursdays, 6–9 PM at The Lantern Room · CommunityFinder',
    );
  });

  it('keeps an ended series up, marked as ended, with no feed', async () => {
    const element = await open('/events/series/drag-brunch-club');
    expect(text(element.querySelector('.series-detail__ended'))).toBe(
      'This series has ended. Its page stays up for the record.',
    );
    expect(text(element.querySelector('.series-detail__none'))).toBe(
      'No upcoming dates are listed.',
    );
    expect(element.querySelector('app-subscribe-link')).toBeNull();
    expect(text(element.querySelector('app-freshness-footer'))).toBe('Ended Verified April 2026');
  });

  it('marks a seasonal series as seasonal, not ended', async () => {
    const element = await open('/events/series/sober-saturday-hike');
    expect(element.querySelector('.series-detail__ended')).toBeNull();
    expect(element.querySelector('app-subscribe-link')).not.toBeNull();
    expect(text(element.querySelector('app-freshness-footer'))).toBe('Seasonal Not yet verified');
  });

  it('is a "not found" page for an unknown series', async () => {
    const element = await open('/events/series/nope');
    expect(text(element.querySelector('h1'))).toBe('Series not found');
  });
});
