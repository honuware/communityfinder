import { DOCUMENT } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { link, openGuide, text, texts, useGuideTestClock } from './guide-testing';

function facts(element: HTMLElement): Record<string, string> {
  const terms = texts(element, '.guide-page__facts dt');
  const values = texts(element, '.guide-page__facts dd');
  return Object.fromEntries(terms.map((term, index) => [term, values[index]]));
}

describe('OrganizationProfilePage', () => {
  useGuideTestClock();

  it('shows what an organization is, when and where it meets, and how to join', async () => {
    const { element } = await openGuide('/guide/orgs/harbor-voices-chorus');
    expect(text(element.querySelector('h1'))).toBe('Harbor Voices Chorus');
    expect(text(element.querySelector('.guide-page__lead'))).toBe('Chorus & performance');
    expect(facts(element)).toEqual({
      When: 'Rehearsals Monday evenings',
      Where: 'Capitol Hill',
      'How to join': 'Harbor Voices Chorus open_in_new',
    });
    expect(link(element, 'Harbor Voices Chorus open_in_new')?.getAttribute('href')).toBe(
      'https://example.org/harbor-voices-chorus',
    );
    // Verified 90 days ago.
    expect(text(element.querySelector('app-freshness-footer'))).toBe('Verified July 2026');
    expect(TestBed.inject(DOCUMENT).title).toBe('Harbor Voices Chorus · CommunityFinder');
  });

  it('marks a seasonal league, one with no single neighborhood, and shows its links', async () => {
    const { element } = await openGuide('/guide/orgs/cascade-queer-softball');
    expect(text(element.querySelector('.guide-page__banner'))).toBe(
      'Seasonal — active part of the year.',
    );
    expect(facts(element)['Where']).toBe('Countywide, or it moves around');
    expect(texts(element, '.scene-chips__chip')).toEqual(['Jocks & sports']);
    expect(texts(element, '.link-out-card__label')).toEqual(['League sign-ups']);
    expect(text(element.querySelector('app-freshness-footer'))).toBe(
      'Seasonal Verified September 2026',
    );
  });

  it('keeps closed and moved organizations up, saying which', async () => {
    const closed = await openGuide('/guide/orgs/night-owls-book-club');
    expect(text(closed.element.querySelector('.organization-profile__closed'))).toBe(
      'Closed — Wound down after its last meeting; members moved to a library group.',
    );

    await closed.harness.navigateByUrl('/guide/orgs/eastlake-run-club');
    expect(text(closed.element.querySelector('h1'))).toBe('Eastlake Run Club');
    expect(text(closed.element.querySelector('.organization-profile__closed'))).toBe('Moved');
  });

  it('asks before showing an 18+ organization', async () => {
    const { element } = await openGuide('/guide/orgs/pacific-leather-collective');
    expect(text(element.querySelector('h1'))).toBe('18+ section');
    expect(element.textContent).not.toContain('Pacific Leather Collective');
  });

  it('is a "not found" page for an unknown organization', async () => {
    const { element } = await openGuide('/guide/orgs/nope');
    expect(text(element.querySelector('h1'))).toBe('Organization not found');
  });
});
