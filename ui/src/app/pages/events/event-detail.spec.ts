import { DOCUMENT } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { RouterTestingHarness } from '@angular/router/testing';
import { provideNoopAnimations } from '@angular/platform-browser/animations';
import { firstValueFrom, of } from 'rxjs';
import { vi } from 'vitest';
import { HONUWARE_API_BASE, PhotoAccess } from '@honuware/ui/access';
import { AuthService } from '@honuware/ui/auth';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import { provideCommunityAccess } from '../../core/community-access/community-access.providers';
import { pacificWallClockToUs } from '../../core/utils/events-date.utils';
import { EventDetailPage } from './event-detail';
import routes from './events.routes';

// Wed Aug 12, 2026, 3:00 PM Pacific: the mock dataset's reference week.
// Leather Night (103) is Fri Aug 14, 9 PM – 2 AM at The Lantern Room.
const NOW_US = pacificWallClockToUs(2026, 8, 12, 15);

const CURATOR = {
  isAuth: true,
  personId: 1,
  firstName: 'Demo',
  lastName: 'Admin',
  email: 'admin@communityfinder.local',
  createdAt: '0',
  isAdmin: true,
  roles: ['admin'],
  permissions: ['admin_portal', 'manage_events'],
  mustChangePassword: false,
};

function configure(authData: object = { isAuth: false }) {
  let photo!: PhotoAccess;
  TestBed.configureTestingModule({
    providers: [
      provideRouter([{ path: 'events', children: routes }]),
      provideNoopAnimations(),
      provideHonuwareAccessMock({}, (mocks) => (photo = mocks.photo)),
      provideCommunityAccess(true),
      { provide: HONUWARE_API_BASE, useValue: '/api' },
      { provide: AuthService, useValue: { authData$: of(authData) } },
    ],
  });
  return { photo };
}

async function open(url: string) {
  const harness = await RouterTestingHarness.create();
  await harness.navigateByUrl(url, EventDetailPage);
  return { harness, element: harness.routeNativeElement as HTMLElement };
}

function link(element: HTMLElement, label: string): HTMLAnchorElement | undefined {
  return Array.from(element.querySelectorAll('a')).find(
    (anchor) => anchor.textContent?.replace(/\s+/g, ' ').trim() === label,
  );
}

function jsonLd(document: Document) {
  const script = document.getElementById('page-json-ld');
  return script ? JSON.parse(script.textContent ?? '') : null;
}

describe('EventDetailPage', () => {
  beforeEach(() => {
    vi.useFakeTimers({ toFake: ['Date'] });
    vi.setSystemTime(new Date(NOW_US / 1000));
  });
  afterEach(() => vi.useRealTimers());

  it('shows the event in full, with its calendar links and metadata', async () => {
    configure();
    const { element } = await open('/events/103');
    const document = TestBed.inject(DOCUMENT);
    const text = element.textContent?.replace(/\s+/g, ' ') ?? '';
    expect(element.querySelector('h1')?.textContent?.trim()).toBe('Leather Night');
    expect(text).toContain('Friday, Aug 14');
    expect(text).toContain('9:00 PM – 2:00 AM');
    expect(text).toContain('The Lantern Room');
    expect(text).toContain('1400 E Pine St, Seattle, WA 98122');
    expect(text).toContain('$10 cover');
    expect(text).toContain('Dress code encouraged, not enforced.');

    // EV2 §3.8: the venue's guide page, found by its id.
    expect(link(element, 'The Lantern Room')?.getAttribute('href')).toBe('/guide/venues/by-id/1');
    expect(link(element, 'Capitol Hill')?.getAttribute('href')).toBe(
      '/events?neighborhood=capitol-hill',
    );
    expect(link(element, 'Leather & kink')?.getAttribute('href')).toBe('/events?scene=leather');
    expect(link(element, 'event Add to calendar')?.getAttribute('href')).toBe(
      '/api/events/103/ical',
    );
    const google = new URL(link(element, 'Google Calendar')!.href);
    expect(google.origin + google.pathname).toBe('https://calendar.google.com/calendar/render');
    expect(google.searchParams.get('text')).toBe('Leather Night');
    expect(google.searchParams.get('dates')).toBe('20260815T040000Z/20260815T090000Z');

    expect(document.title).toBe('Leather Night — Fri, Aug 14 · CommunityFinder');
    expect(jsonLd(document)).toMatchObject({
      '@type': 'Event',
      name: 'Leather Night',
      startDate: '2026-08-14T21:00:00-07:00',
      url: `${document.location.origin}/events/103`,
    });
    // Not a curator: no edit link.
    expect(link(element, 'edit Edit this event')).toBeUndefined();
  });

  // CM3 §5.2, §5.5. Leather Night has its own photo, a gallery of four and a
  // video in the demo data.
  it('shows its own photo, then its video, then its gallery, without asking has_photo', async () => {
    const { photo } = configure();
    const hasPhoto = vi.spyOn(photo, 'hasPhoto');
    const { element } = await open('/events/103');
    const document = TestBed.inject(DOCUMENT);
    const origin = document.location.origin;
    const heroPath = '/api/get_scaled_photo/events/103/1200/630';

    const hero = element.querySelector('.event-detail__photo')!;
    expect(hero.getAttribute('src')).toBe(heroPath);
    expect(document.head.querySelector('meta[property="og:image"]')?.getAttribute('content')).toBe(
      `${origin}${heroPath}`,
    );
    // The main photo, then the gallery's first three, full size.
    expect(jsonLd(document).image).toEqual([
      `${origin}${heroPath}`,
      `${origin}/api/get_scaled_photo/event_gallery_photos/1/1600/1200`,
      `${origin}/api/get_scaled_photo/event_gallery_photos/2/1600/1200`,
      `${origin}/api/get_scaled_photo/event_gallery_photos/3/1600/1200`,
    ]);

    // The video is YouTube's own player from the start (CM3 OQ23), and the
    // event's photo appears only as the hero, never as a stand-in for the video.
    const tile = element.querySelector('.video-embed')!;
    expect(tile.querySelector('iframe')!.getAttribute('src')).toContain(
      'https://www.youtube-nocookie.com/embed/',
    );
    expect(element.querySelectorAll(`img[src="${heroPath}"]`).length).toBe(1);
    expect(element.querySelectorAll('.photo-gallery__thumb').length).toBe(4);

    const gallery = element.querySelector('.photo-gallery')!;
    const follows = (first: Element, second: Element) =>
      Boolean(first.compareDocumentPosition(second) & Node.DOCUMENT_POSITION_FOLLOWING);
    expect(follows(hero, tile)).toBe(true);
    expect(follows(tile, gallery)).toBe(true);
    expect(hasPhoto).not.toHaveBeenCalled();
  });

  it("borrows its series' photo, else its venue's; with none, no image at all", async () => {
    configure();
    const document = TestBed.inject(DOCUMENT);
    const hero = (element: HTMLElement) =>
      element.querySelector('.event-detail__photo')?.getAttribute('src') ?? null;
    // Bear Happy Hour: the series' photo.
    const { harness, element } = await open('/events/107');
    expect(hero(element)).toBe('/api/get_scaled_photo/series/11/1200/630');
    expect(element.querySelector('.photo-gallery')).toBeNull();
    expect(element.querySelector('app-video-embed')).toBeNull();

    // The Late Show at Driftwood Hall: the venue's.
    await harness.navigateByUrl('/events/102');
    expect(hero(harness.routeNativeElement as HTMLElement)).toBe(
      '/api/get_scaled_photo/venues/2/1200/630',
    );

    // Karaoke Night: nothing, and nothing pointing at a missing photo.
    await harness.navigateByUrl('/events/101');
    expect(hero(harness.routeNativeElement as HTMLElement)).toBeNull();
    expect(document.head.querySelector('meta[property="og:image"]')).toBeNull();
    expect(jsonLd(document).image).toBeUndefined();
  });

  it('links a series event to its series, with the tags it inherits', async () => {
    configure();
    const { element } = await open('/events/107');
    expect(link(element, 'Bear Happy Hour')?.getAttribute('href')).toBe(
      '/events/series/bear-happy-hour',
    );
    expect(link(element, 'Bears')?.getAttribute('href')).toBe('/events?scene=bear');
    expect(link(element, 'Bar & club nights')?.getAttribute('href')).toBe('/events?category=bar');
  });

  it('is a "not found" page for an unlisted event or a bad id', async () => {
    configure();
    const getEvent = vi.spyOn(TestBed.inject(COMMUNITY_ACCESS), 'getEvent');
    const { harness, element } = await open('/events/117');
    const document = TestBed.inject(DOCUMENT);
    expect(element.querySelector('h1')?.textContent?.trim()).toBe('Event not found');
    expect(document.title).toBe('Event not found · CommunityFinder');
    expect(jsonLd(document)).toBeNull();

    getEvent.mockClear();
    await harness.navigateByUrl('/events/not-a-number');
    expect((harness.routeNativeElement as HTMLElement).textContent).toContain('Event not found');
    expect(getEvent).not.toHaveBeenCalled();
  });

  // EV4 §3.3: an archived event's page still loads, says it's over, and stops
  // offering it for a calendar.
  it('says a past event has passed, without calendar links', async () => {
    configure();
    const { element } = await open('/events/116');
    const document = TestBed.inject(DOCUMENT);
    expect(element.querySelector('h1')?.textContent?.trim()).toBe('Trivia Night');
    expect(element.textContent).toContain('This event has passed.');
    expect(link(element, 'event Add to calendar')).toBeUndefined();
    expect(link(element, 'Google Calendar')).toBeUndefined();
    // Still a public page, so it keeps its JSON-LD.
    expect(jsonLd(document)).toMatchObject({ '@type': 'Event', name: 'Trivia Night' });
  });

  // The time decides, not the status: an approved event the sweep hasn't
  // archived yet is over once start + 2 hours (no end) is behind us.
  it('counts an event with no end as over two hours after it starts', async () => {
    configure();
    const access = TestBed.inject(COMMUNITY_ACCESS);
    const approved = await firstValueFrom(access.getEvent(101));
    const MINUTE_US = 60 * 1_000_000;
    const HOUR_US = 60 * MINUTE_US;
    const getEvent = vi.spyOn(access, 'getEvent');

    const startedAgo = (us: number) => ({
      ...approved,
      starts_at_us: NOW_US - us,
      ends_at_us: null,
    });

    getEvent.mockReturnValue(of(startedAgo(2 * HOUR_US - MINUTE_US)));
    const { harness, element } = await open('/events/101');
    expect(element.textContent).not.toContain('This event has passed.');
    expect(link(element, 'event Add to calendar')).toBeDefined();

    getEvent.mockReturnValue(of({ ...startedAgo(2 * HOUR_US + MINUTE_US), id: 102 }));
    await harness.navigateByUrl('/events/102');
    const later = harness.routeNativeElement as HTMLElement;
    expect(later.textContent).toContain('This event has passed.');
    expect(link(later, 'event Add to calendar')).toBeUndefined();
  });

  it('shows a curator an archived event as public, with no preview banner', async () => {
    configure(CURATOR);
    const access = TestBed.inject(COMMUNITY_ACCESS);
    const archived = await firstValueFrom(access.getEvent(116));
    vi.spyOn(access, 'getEvent').mockReturnValue(
      of({ ...archived, status: 'archived', review_notes: '' }),
    );
    const { element } = await open('/events/116');
    expect(element.textContent).not.toContain('Curator preview');
    expect(element.textContent).toContain('This event has passed.');
  });

  it('gives a curator a preview of an unapproved event, without calendar links or JSON-LD', async () => {
    configure(CURATOR);
    const access = TestBed.inject(COMMUNITY_ACCESS);
    const approved = await firstValueFrom(access.getEvent(101));
    vi.spyOn(access, 'getEvent').mockReturnValue(
      of({ ...approved, id: 117, title: 'Open Mic', status: 'pending', review_notes: '' }),
    );
    const { element } = await open('/events/117');
    const document = TestBed.inject(DOCUMENT);
    expect(element.textContent).toContain('Curator preview: this event is pending and not public.');
    expect(link(element, 'event Add to calendar')).toBeUndefined();
    expect(link(element, 'Google Calendar')).toBeUndefined();
    expect(jsonLd(document)).toBeNull();
    expect(link(element, 'edit Edit this event')?.getAttribute('href')).toBe(
      '/admin/events?edit=117',
    );
  });
});
