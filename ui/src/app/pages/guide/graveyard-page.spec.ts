import { DOCUMENT } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { openGuide, text, texts, useGuideTestClock } from './guide-testing';

describe('GraveyardPage', () => {
  useGuideTestClock();

  it('lists the closed venues and organizations with their closure notes, as history', async () => {
    const { element } = await openGuide('/guide/graveyard');
    expect(text(element.querySelector('h1'))).toBe('Closed venues and organizations');
    expect(texts(element, '.guide-page__section-title')).toEqual(['Venues', 'Organizations']);
    expect(texts(element, '.entity-card__name')).toEqual(['The Wren', 'Night Owls Book Club']);
    expect(texts(element, '.entity-card__closure-note')).toEqual([
      'Closed after twelve years when the building was sold.',
      'Wound down after its last meeting; members moved to a library group.',
    ]);
    // Each still links to its own page.
    expect(element.querySelector('.entity-card__name')?.getAttribute('href')).toBe(
      '/guide/venues/the-wren',
    );
    // A moved organization lives on elsewhere: not here.
    expect(element.textContent).not.toContain('Eastlake Run Club');
    expect(TestBed.inject(DOCUMENT).title).toBe(
      'Closed venues and organizations · CommunityFinder',
    );
  });
});
