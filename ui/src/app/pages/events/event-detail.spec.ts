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
    // No photo uploaded: no og:image, no <img>.
    expect(document.head.querySelector('meta[property="og:image"]')).toBeNull();
    expect(element.querySelector('.event-detail__photo')).toBeNull();
    // Not a curator: no edit link.
    expect(link(element, 'edit Edit this event')).toBeUndefined();
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

  it('shows the photo, and gives it to og:image, once the photo check finds one', async () => {
    const { photo } = configure();
    await firstValueFrom(photo.uploadPhoto('events', 103, new ArrayBuffer(8), 'image/jpeg'));
    const { element } = await open('/events/103');
    const document = TestBed.inject(DOCUMENT);
    const photoUrl = `${document.location.origin}/api/get_scaled_photo/events/103/1200/630`;
    expect(element.querySelector('.event-detail__photo')?.getAttribute('src')).toBe(photoUrl);
    expect(document.head.querySelector('meta[property="og:image"]')?.getAttribute('content')).toBe(
      photoUrl,
    );
    expect(jsonLd(document).image).toEqual([photoUrl]);
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
