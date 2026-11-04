import { DOCUMENT } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { provideRouter, Router } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { of, throwError } from 'rxjs';
import { vi } from 'vitest';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import { provideCommunityAccess } from '../../core/community-access/community-access.providers';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import { EventsListPage } from './events-list';
import routes from './events.routes';

// Wed Aug 12, 2026, 3:00 PM Pacific: the mock dataset's reference week.
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);

function configure() {
  TestBed.configureTestingModule({
    providers: [
      provideRouter([{ path: 'events', children: routes }]),
      provideNoopAnimations(),
      provideCommunityAccess(true),
      { provide: HONUWARE_API_BASE, useValue: '/api' },
    ],
  });
}

async function open(url: string) {
  const harness = await RouterTestingHarness.create();
  await harness.navigateByUrl(url, EventsListPage);
  return { harness, element: harness.routeNativeElement as HTMLElement };
}

function titles(element: HTMLElement): string[] {
  return Array.from(element.querySelectorAll('.event-row__title')).map(
    (link) => link.textContent?.trim() ?? '',
  );
}

function button(element: HTMLElement, label: string): HTMLButtonElement {
  return Array.from(element.querySelectorAll('button')).find(
    (candidate) => candidate.textContent?.trim() === label,
  )!;
}

describe('EventsListPage', () => {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date(NOW_US / 1000));
    configure();
  });
  afterEach(() => vi.useRealTimers());

  it('lists the next six weeks by day, tonight first', async () => {
    const { element } = await open('/events');
    const days = Array.from(element.querySelectorAll('.events-list__day-title')).map((heading) =>
      heading.textContent?.trim(),
    );
    expect(days[0]).toBe('Wednesday, Aug 12');
    expect(days[1]).toBe('Thursday, Aug 13');
    expect(titles(element).length).toBe(14);
    expect(titles(element).slice(0, 2)).toEqual(['Karaoke Night', 'Late Show: Drag Revue']);
    // The pending and past events never show.
    expect(titles(element)).not.toContain('Open Mic (awaiting review)');
    expect(titles(element)).not.toContain('Trivia Night');
  });

  it('narrows to tonight from the URL, and marks the preset pressed', async () => {
    const { element } = await open('/events?when=tonight');
    expect(titles(element)).toEqual(['Karaoke Night', 'Late Show: Drag Revue']);
    expect(button(element, 'Tonight').getAttribute('aria-pressed')).toBe('true');
    expect(button(element, 'This weekend').getAttribute('aria-pressed')).toBe('false');
  });

  it('filters by scene, offers that view as a feed, and names the page after it', async () => {
    const { harness, element } = await open('/events?scene=bear&when=weekend');
    // Bear Happy Hour is on Thursdays, so not this weekend.
    expect(titles(element)).toEqual([]);
    expect(element.textContent).toContain('No events match these filters.');

    await harness.navigateByUrl('/events?scene=bear');
    const bears = harness.routeNativeElement as HTMLElement;
    expect(titles(bears)).toEqual(['Bear Happy Hour', 'Bear Happy Hour', 'Bear Happy Hour']);
    const document = TestBed.inject(DOCUMENT);
    const subscribe = bears.querySelector('.subscribe-link__subscribe')!;
    expect(subscribe.getAttribute('href')).toBe(
      `webcal://${document.location.host}/api/events/feed.ics?scene=bear`,
    );
    expect(bears.textContent).toContain('Get Bears events in your calendar');
    expect(document.title).toBe('Bears events · CommunityFinder');
    // Tonight / This weekend stay out of the canonical address; the scene stays in.
    expect(document.head.querySelector('link[rel="canonical"]')?.getAttribute('href')).toBe(
      `${document.location.origin}/events?scene=bear`,
    );
  });

  it('keeps every filter change in the URL', async () => {
    const { harness, element } = await open('/events?category=drag');
    const router = TestBed.inject(Router);
    button(element, 'Tonight').click();
    await harness.fixture.whenStable();
    expect(router.url).toBe('/events?when=tonight&category=drag');
    // Picking the active preset again clears it.
    button(element, 'Tonight').click();
    await harness.fixture.whenStable();
    expect(router.url).toBe('/events?category=drag');
    button(element, 'Clear filters').click();
    await harness.fixture.whenStable();
    expect(router.url).toBe('/events');
    harness.detectChanges();
    expect(titles(element).length).toBe(14);
  });

  it('offers the submit form in its header', async () => {
    const { element } = await open('/events');
    const submit = element.querySelector('.events-list__header .events-list__submit')!;
    expect(submit.textContent?.replace(/\s+/g, ' ').trim()).toBe('add Submit an event');
    expect(submit.getAttribute('href')).toBe('/events/submit');
  });

  it('says so when nothing is on the calendar at all, and asks for a submission', async () => {
    vi.spyOn(TestBed.inject(COMMUNITY_ACCESS), 'getUpcomingEvents').mockReturnValue(
      of({ items: [], from_us: 0, to_us: 0 }),
    );
    const { element } = await open('/events?when=tonight');
    expect(element.textContent).toContain("Nothing's on the calendar yet.");
    expect(element.textContent).not.toContain('No events match these filters.');
    const empty = element.querySelector('.events-list__empty')!;
    expect(empty.textContent).toContain('Know of something?');
    expect(empty.querySelector('a')?.getAttribute('href')).toBe('/events/submit');
  });

  it('says so when the events could not be loaded', async () => {
    vi.spyOn(TestBed.inject(COMMUNITY_ACCESS), 'getUpcomingEvents').mockReturnValue(
      throwError(() => new Error('offline')),
    );
    const { element } = await open('/events');
    expect(element.textContent).toContain("Events couldn't be loaded.");
  });

  // EV4 §10. From NOW the mock's past events are Trivia Night (Aug 9), Bear
  // Happy Hour (Aug 6), the farewell brunch (May 4) and the coat drive
  // (Jan 24); the ball over a year ago is past the window.
  describe('the past list', () => {
    const PAST_TITLES = [
      'Trivia Night',
      'Bear Happy Hour',
      'Drag Brunch Club: Farewell Brunch',
      'Winter Coat Drive',
    ];

    it('lists what already happened, newest first, by month, muted', async () => {
      const upcoming = vi.spyOn(TestBed.inject(COMMUNITY_ACCESS), 'getUpcomingEvents');
      const { element } = await open('/events?view=past');
      expect(element.querySelector('.events-list__title')?.textContent?.trim()).toBe('Past events');
      const months = Array.from(element.querySelectorAll('.events-list__day-title')).map(
        (heading) => heading.textContent?.trim(),
      );
      expect(months).toEqual(['August 2026', 'May 2026', 'January 2026']);
      expect(titles(element)).toEqual(PAST_TITLES);
      expect(element.querySelectorAll('.event-row--past').length).toBe(4);
      // Each row names its day, since the headers are months.
      expect(element.querySelector('.event-row__when')?.textContent).toContain('Aug 9');
      // The presets and the feed are for what's coming.
      expect(button(element, 'Tonight')).toBeUndefined();
      expect(element.querySelector('app-subscribe-link')).toBeNull();
      expect(upcoming).not.toHaveBeenCalled();
      const document = TestBed.inject(DOCUMENT);
      expect(document.title).toBe('Past events · CommunityFinder');
      expect(document.head.querySelector('link[rel="canonical"]')?.getAttribute('href')).toBe(
        `${document.location.origin}/events?view=past`,
      );
    });

    it('is a link away from the upcoming list, filters and all, and back', async () => {
      const { harness, element } = await open('/events?scene=bear&when=tonight');
      const toPast = element.querySelector('.events-list__other-view')!;
      expect(toPast.textContent).toContain('Past events');
      expect(toPast.getAttribute('href')).toBe('/events?view=past&scene=bear');
      // The upcoming rows aren't muted.
      await harness.navigateByUrl('/events');
      const upcoming = harness.routeNativeElement as HTMLElement;
      expect(titles(upcoming).length).toBe(14);
      expect(upcoming.querySelectorAll('.event-row--past').length).toBe(0);

      await harness.navigateByUrl('/events?view=past&scene=bear');
      const back = (harness.routeNativeElement as HTMLElement).querySelector(
        '.events-list__other-view',
      )!;
      expect(back.textContent).toContain('Upcoming');
      expect(back.getAttribute('href')).toBe('/events?scene=bear');
    });

    it('filters on the server, and keeps to the past list as the filters change', async () => {
      const fetch = vi.spyOn(TestBed.inject(COMMUNITY_ACCESS), 'getPastEvents');
      const { harness, element } = await open('/events?view=past&scene=bear');
      expect(fetch).toHaveBeenCalledWith({
        offset: 0,
        limit: 20,
        category: '',
        scene: 'bear',
        neighborhood: '',
      });
      expect(titles(element)).toEqual(['Bear Happy Hour']);
      const document = TestBed.inject(DOCUMENT);
      expect(document.title).toBe('Past Bears events · CommunityFinder');
      expect(document.head.querySelector('link[rel="canonical"]')?.getAttribute('href')).toBe(
        `${document.location.origin}/events?view=past&scene=bear`,
      );

      button(element, 'Clear filters').click();
      await harness.fixture.whenStable();
      expect(TestBed.inject(Router).url).toBe('/events?view=past');
      harness.detectChanges();
      expect(titles(harness.routeNativeElement as HTMLElement)).toEqual(PAST_TITLES);

      await harness.navigateByUrl('/events?view=past&category=protest');
      expect((harness.routeNativeElement as HTMLElement).textContent).toContain(
        'No past events match these filters.',
      );
    });

    it('fetches a page at a time, the next on "Show more"', async () => {
      const access = TestBed.inject(COMMUNITY_ACCESS);
      const real = access.getPastEvents.bind(access);
      const fetch = vi
        .spyOn(access, 'getPastEvents')
        .mockImplementation((query) => real({ ...query, limit: 3 }));
      const { harness, element } = await open('/events?view=past');
      expect(titles(element)).toEqual(PAST_TITLES.slice(0, 3));

      button(element, 'Show more').click();
      harness.detectChanges();
      expect(fetch).toHaveBeenLastCalledWith(expect.objectContaining({ offset: 3 }));
      expect(titles(element)).toEqual(PAST_TITLES);
      expect(button(element, 'Show more')).toBeUndefined();
    });

    it('says so when there is nothing, or it could not be loaded', async () => {
      const fetch = vi
        .spyOn(TestBed.inject(COMMUNITY_ACCESS), 'getPastEvents')
        .mockReturnValue(of({ items: [], has_more: false, since_us: 0 }));
      const { harness, element } = await open('/events?view=past');
      expect(element.textContent).toContain('No past events to show yet.');

      fetch.mockReturnValue(throwError(() => new Error('offline')));
      await harness.navigateByUrl('/events?view=past&category=bar');
      expect((harness.routeNativeElement as HTMLElement).textContent).toContain(
        "Events couldn't be loaded.",
      );
    });
  });
});
