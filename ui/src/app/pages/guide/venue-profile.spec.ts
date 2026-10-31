import { DOCUMENT } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { link, openGuide, text, texts, useGuideTestClock } from './guide-testing';

describe('VenueProfilePage', () => {
  useGuideTestClock();

  it('shows an open venue in full', async () => {
    const { element } = await openGuide('/guide/venues/the-lantern-room');
    expect(text(element.querySelector('h1'))).toBe('The Lantern Room');
    expect(text(element.querySelector('.guide-page__lead'))).toBe('Capitol Hill');
    expect(element.querySelector('.guide-page__banner')).toBeNull();
    expect(text(element.querySelector('.venue-profile__crowd'))).toBe(
      'Bears and their friends after work; louder and younger after 10.',
    );
    expect(texts(element, '.guide-page__facts dt')).toEqual(['Address', 'Website', 'Price']);
    expect(texts(element, '.guide-page__facts dd')).toContain('1400 E Pine St, Seattle, WA 98122');
    const website = link(element, 'The Lantern Room open_in_new');
    expect(website?.getAttribute('href')).toBe('https://example.org/lantern-room');
    expect(website?.getAttribute('target')).toBe('_blank');
    expect(texts(element, '.scene-chips__chip')).toEqual(['Bears']);
    expect(texts(element, '.link-out-card__label')).toEqual(['The Lantern Room on Instagram']);
    // Within the next six weeks, at this venue, in order.
    expect(texts(element, '.guide-upcoming__name')).toEqual([
      'Karaoke Night',
      'Bear Happy Hour',
      'Bear Happy Hour',
    ]);
    // The older of its two checks: the profile, 35 days ago.
    expect(text(element.querySelector('app-freshness-footer'))).toBe('Verified September 2026');
    expect(TestBed.inject(DOCUMENT).title).toBe('The Lantern Room · CommunityFinder');
  });

  it("shows the venue's photo, and hides it when there isn't one", async () => {
    const { element, harness } = await openGuide('/guide/venues/the-lantern-room');
    const photo = element.querySelector('.guide-page__photo')!;
    expect(photo.getAttribute('src')).toMatch(/\/api\/get_scaled_photo\/venues\/1\/1200\/675$/);
    photo.dispatchEvent(new Event('error'));
    harness.detectChanges();
    expect(element.querySelector('.guide-page__photo')).toBeNull();
  });

  it('keeps a closed venue up as its graveyard entry, with the closure note', async () => {
    const { element } = await openGuide('/guide/venues/the-wren');
    expect(text(element.querySelector('h1'))).toBe('The Wren');
    expect(text(element.querySelector('.venue-profile__closed'))).toBe(
      'Closed — Closed after twelve years when the building was sold.',
    );
    expect(element.querySelector('app-guide-upcoming-events')).toBeNull();
    expect(text(element.querySelector('app-freshness-footer'))).toBe('Closed Verified April 2026');
  });

  it('asks before showing an 18+ venue', async () => {
    const { element, harness } = await openGuide('/guide/venues/iron-and-oak');
    expect(text(element.querySelector('h1'))).toBe('18+ section');
    expect(element.textContent).not.toContain('dress code');
    element.querySelector<HTMLButtonElement>('.adult-gate__accept')!.click();
    harness.detectChanges();
    expect(text(element.querySelector('h1'))).toBe('Iron & Oak');
    expect(text(element.querySelector('.venue-profile__crowd'))).toContain('dress code');
  });

  it('is a "not found" page for an unknown venue', async () => {
    const { element } = await openGuide('/guide/venues/nope');
    expect(text(element.querySelector('h1'))).toBe('Venue not found');
  });
});

describe('/guide/venues/by-id/:venueId', () => {
  useGuideTestClock();

  it("redirects an event's venue id to the venue's page", async () => {
    const { element, router } = await openGuide('/guide/venues/by-id/1');
    expect(router.url).toBe('/guide/venues/the-lantern-room');
    expect(text(element.querySelector('h1'))).toBe('The Lantern Room');
  });

  it('finds a venue that has since closed, so an old event still lands on its page', async () => {
    const { router } = await openGuide('/guide/venues/by-id/5');
    expect(router.url).toBe('/guide/venues/the-wren');
  });

  it("says so when the venue isn't in the guide", async () => {
    const { element, router } = await openGuide('/guide/venues/by-id/99');
    expect(router.url).toBe('/guide/venues/by-id/99');
    expect(text(element.querySelector('h1'))).toBe('Venue not found');
    expect(TestBed.inject(DOCUMENT).title).toBe('Venue not found · CommunityFinder');
  });
});
