import { DOCUMENT } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { openGuide, text, texts, useGuideTestClock } from './guide-testing';

describe('NeighborhoodPage', () => {
  useGuideTestClock();

  it("shows a neighborhood's rundown, then what's listed there", async () => {
    const { element, waitFor } = await openGuide('/guide/neighborhoods/capitol-hill');
    expect(text(element.querySelector('h1'))).toBe('Capitol Hill');
    expect(text(element.querySelector('.guide-page__lead'))).toBe('Seattle');

    const rundownLink = await waitFor('.neighborhood-page__rundown a');
    expect(rundownLink.getAttribute('href')).toBe('/guide/venues/the-lantern-room');

    expect(texts(element, '.guide-page__section-title')).toEqual([
      'Venues',
      'Organizations',
      'Places',
    ]);
    // The closed bar here is in the graveyard, not on this page.
    expect(texts(element, '.entity-card__name')).toEqual([
      'Iron & Oak',
      'The Lantern Room',
      'Clear Path Recovery Circle',
      'Harbor Voices Chorus',
      'Pacific Leather Collective',
      'Rainbow Mural Wall',
    ]);
    // The rundown was published 20 days ago.
    expect(text(element.querySelector('app-freshness-footer'))).toBe('Verified October 2026');
    expect(TestBed.inject(DOCUMENT).title).toBe('Capitol Hill · CommunityFinder');
  });

  it("says the rundown is being written when there isn't one yet", async () => {
    const { element } = await openGuide('/guide/neighborhoods/ballard');
    expect(text(element.querySelector('.neighborhood-page__no-rundown'))).toBe(
      'The rundown for this neighborhood is being written.',
    );
    expect(texts(element, '.entity-card__name')).toEqual(['Driftwood Hall']);
    expect(element.querySelector('app-freshness-footer')).toBeNull();
  });

  it('says so when nothing is listed in a neighborhood', async () => {
    const { element } = await openGuide('/guide/neighborhoods/kirkland');
    expect(texts(element, '.guide-page__empty')).toContain('Nothing is listed here yet.');
  });

  it('is a "not found" page for an unknown neighborhood', async () => {
    const { element } = await openGuide('/guide/neighborhoods/nope');
    expect(text(element.querySelector('h1'))).toBe('Neighborhood not found');
  });
});
