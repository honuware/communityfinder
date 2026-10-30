import { TestBed } from '@angular/core/testing';
import { provideRouter } from '@angular/router';
import { GuideVocabularies } from '../../../core/services/guide-vocabularies.service';
import {
  normalizeGuideNeighborhood,
  normalizeGuideOrganization,
  normalizeGuidePlace,
  normalizeGuideScene,
  normalizeGuideService,
  normalizeGuideVenue,
} from '../../../core/types/guide.types';
import {
  EntityCard,
  EntityCardItem,
  organizationCardItem,
  placeCardItem,
  serviceCardItem,
  venueCardItem,
} from './entity-card';

// Mid-October 2026 in Seattle.
const OCTOBER_2026_US = Date.UTC(2026, 9, 15, 19) * 1000;

const VOCABULARIES: GuideVocabularies = {
  scenes: [normalizeGuideScene({ slug: 'bear', display_name: 'Bears' })],
  neighborhoods: [
    normalizeGuideNeighborhood({ slug: 'capitol-hill', display_name: 'Capitol Hill' }),
  ],
};

function render(item: EntityCardItem) {
  const fixture = TestBed.createComponent(EntityCard);
  fixture.componentRef.setInput('item', item);
  fixture.componentRef.setInput('vocabularies', VOCABULARIES);
  fixture.detectChanges();
  const element = fixture.nativeElement as HTMLElement;
  const text = (selector: string) =>
    element.querySelector(selector)?.textContent?.replace(/\s+/g, ' ').trim() ?? null;
  return {
    element,
    text,
    name: () => element.querySelector('.entity-card__name')!,
    badges: () =>
      Array.from(element.querySelectorAll('.entity-card__badge')).map((badge) =>
        badge.textContent?.trim(),
      ),
  };
}

describe('EntityCard', () => {
  beforeEach(() => TestBed.configureTestingModule({ providers: [provideRouter([])] }));

  it('shows a venue: its page, neighborhood, description, chips and stamp', () => {
    const card = render(
      venueCardItem(
        normalizeGuideVenue({
          slug: 'the-lantern-room',
          name: 'The Lantern Room',
          description: 'A neighborhood bar.',
          neighborhood_slug: 'capitol-hill',
          scene_tag_slugs: ['bear'],
          status: 'open',
          last_verified_at_us: OCTOBER_2026_US,
        }),
      ),
    );
    expect(card.name().getAttribute('href')).toBe('/guide/venues/the-lantern-room');
    expect(card.text('.entity-card__name')).toBe('The Lantern Room');
    expect(card.badges()).toEqual([]);
    expect(card.text('.entity-card__meta')).toBe('Capitol Hill');
    expect(card.text('.entity-card__description')).toBe('A neighborhood bar.');
    expect(card.text('.scene-chips__chip')).toBe('Bears');
    expect(card.text('.entity-card__verified')).toBe('Verified October 2026');
  });

  it('shows a closed organization faded, with its badge and closure note', () => {
    const card = render(
      organizationCardItem(
        normalizeGuideOrganization({
          slug: 'night-owls-book-club',
          name: 'Night Owls Book Club',
          org_kind: 'club-hobby',
          neighborhood_slug: 'capitol-hill',
          closure_note: 'Stopped meeting in 2025.',
          status: 'closed',
          last_verified_at_us: OCTOBER_2026_US,
        }),
      ),
    );
    expect(card.name().getAttribute('href')).toBe('/guide/orgs/night-owls-book-club');
    expect(card.element.querySelector('.entity-card--retired')).not.toBeNull();
    expect(card.badges()).toEqual(['Closed']);
    expect(card.element.querySelector('.entity-card__badge--closed')).not.toBeNull();
    expect(card.text('.entity-card__meta')).toBe('Club & hobby · Capitol Hill');
    expect(card.text('.entity-card__closure-note')).toBe('Stopped meeting in 2025.');
  });

  it("keeps an open listing's closure note to itself", () => {
    const card = render(
      organizationCardItem(normalizeGuideOrganization({ status: 'open', closure_note: 'Old.' })),
    );
    expect(card.element.querySelector('.entity-card__closure-note')).toBeNull();
    expect(card.element.querySelector('.entity-card--retired')).toBeNull();
  });

  it('marks a seasonal, never-verified, 18+ place as such', () => {
    const card = render(
      placeCardItem(
        normalizeGuidePlace({
          slug: 'denny-blaine-park',
          name: 'Denny Blaine Park',
          place_kind: 'park-beach',
          adult: true,
          status: 'seasonal',
          last_verified_at_us: '',
        }),
      ),
    );
    expect(card.name().getAttribute('href')).toBe('/guide/places/denny-blaine-park');
    expect(card.badges()).toEqual(['Seasonal', '18+']);
    expect(card.text('.entity-card__meta')).toBe('Park & beach');
    expect(card.text('.entity-card__verified')).toBe('Not yet verified');
  });

  it('links a service out to its own site, since it has no page here', () => {
    const service = normalizeGuideService({
      slug: 'northside-barbers',
      name: 'Northside Barbers',
      service_kind: 'everyday-service',
      url: 'https://example.org/barbers',
      neighborhood_slug: 'capitol-hill',
      status: 'open',
    });
    const card = render(serviceCardItem(service));
    expect(card.name().getAttribute('href')).toBe('https://example.org/barbers');
    expect(card.name().getAttribute('target')).toBe('_blank');
    expect(card.name().getAttribute('rel')).toBe('noopener noreferrer');
    expect(card.text('.entity-card__meta')).toBe('Everyday services · Capitol Hill');
    expect(card.element.querySelector('.scene-chips')).toBeNull();

    // No site either: the name is plain text.
    const unlinked = render(serviceCardItem({ ...service, url: '' }));
    expect(unlinked.name().tagName).toBe('SPAN');
  });
});
