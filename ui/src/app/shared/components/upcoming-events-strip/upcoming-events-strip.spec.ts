import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { of, throwError } from 'rxjs';
import { vi } from 'vitest';
import { COMMUNITY_ACCESS } from '../../../core/community-access/community-access';
import { provideCommunityAccess } from '../../../core/community-access/community-access.providers';
import { pacificWallClockToUs } from '../../../core/utils/events-date.utils';
import { UpcomingEventsStrip } from './upcoming-events-strip';

// Wed Aug 12, 2026, 3:00 PM Pacific: the mock dataset's reference week.
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);

function configure() {
  TestBed.configureTestingModule({
    imports: [UpcomingEventsStrip],
    providers: [provideRouter([]), provideCommunityAccess(true)],
  });
}

function render(): HTMLElement {
  const fixture = TestBed.createComponent(UpcomingEventsStrip);
  fixture.detectChanges();
  return fixture.nativeElement as HTMLElement;
}

describe('UpcomingEventsStrip', () => {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date(NOW_US / 1000));
    configure();
  });
  afterEach(() => vi.useRealTimers());

  it('shows the next five events and a link to the rest', () => {
    const element = render();
    const items = element.querySelectorAll('.upcoming-strip__item');
    expect(items.length).toBe(5);
    expect(items[0].querySelector('.upcoming-strip__name')?.textContent?.trim()).toBe(
      'Karaoke Night',
    );
    expect(items[0].querySelector('.upcoming-strip__name')?.getAttribute('href')).toBe(
      '/events/101',
    );
    expect(items[0].querySelector('.upcoming-strip__when')?.textContent).toMatch(/^Wed, Aug 12 · /);
    expect(items[0].querySelector('.upcoming-strip__place')?.textContent?.trim()).toBe(
      'The Lantern Room',
    );
    expect(element.querySelector('.upcoming-strip__all')?.getAttribute('href')).toBe('/events');
  });

  it('renders nothing when nothing is coming up', () => {
    vi.spyOn(TestBed.inject(COMMUNITY_ACCESS), 'getUpcomingEvents').mockReturnValue(
      of({ items: [], from_us: 0, to_us: 0 }),
    );
    expect(render().children.length).toBe(0);
  });

  it('renders nothing when the events could not be loaded', () => {
    vi.spyOn(TestBed.inject(COMMUNITY_ACCESS), 'getUpcomingEvents').mockReturnValue(
      throwError(() => new Error('offline')),
    );
    expect(render().children.length).toBe(0);
  });
});
