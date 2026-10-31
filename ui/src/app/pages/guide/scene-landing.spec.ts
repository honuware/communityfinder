import { DOCUMENT } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { throwError } from 'rxjs';
import { vi } from 'vitest';
import { GuideMockAccess } from '../../core/guide-access/guide-access.mock';
import { openGuide, text, texts, useGuideTestClock } from './guide-testing';

describe('SceneLandingPage', () => {
  useGuideTestClock();

  it("shows a scene we cover: its intro, its listings, what's coming up, where it talks", async () => {
    const { element, waitFor } = await openGuide('/guide/scenes/leather');
    expect(text(element.querySelector('h1'))).toBe('Leather & kink');

    const intro = await waitFor('.scene-landing__intro a');
    expect(intro.getAttribute('href')).toBe('/guide/venues/iron-and-oak');

    expect(texts(element, '.guide-page__section-title')).toEqual([
      'Venues',
      'Organizations',
      'Where this scene talks',
    ]);
    expect(texts(element, '.entity-card__name')).toEqual([
      'Iron & Oak',
      'Pacific Leather Collective',
    ]);
    expect(texts(element, '.guide-upcoming__name')).toEqual(['Leather Night']);
    expect(element.querySelector('.guide-upcoming__all')?.getAttribute('href')).toBe(
      '/events?scene=leather',
    );
    expect(texts(element, '.link-out-card__label')).toEqual(['Leather scene chat']);
    expect(element.querySelector('.scene-landing__coverage')).toBeNull();
    // The intro page was published 30 days ago.
    expect(text(element.querySelector('app-freshness-footer'))).toBe('Verified September 2026');
    expect(TestBed.inject(DOCUMENT).title).toBe('Leather & kink · CommunityFinder');
  });

  it("points a link-out scene to its own curators, and lists only what's shared", async () => {
    const { element } = await openGuide('/guide/scenes/sapphic');
    expect(text(element.querySelector('.scene-landing__coverage'))).toContain(
      "We don't cover this scene ourselves.",
    );
    expect(texts(element, '.link-out-card__label')).toEqual([
      'Sapphic Seattle calendar',
      'Sapphic meetups',
    ]);
    expect(texts(element, '.guide-page__section-title')).toEqual([
      'Where this scene talks',
      'Shared spaces in this guide',
    ]);
    expect(texts(element, '.entity-card__name')).toEqual(['Columbia City Social Club']);
    expect(element.querySelector('app-guide-upcoming-events')).toBeNull();
  });

  it("renders a link-out scene's intro above its curators' links", async () => {
    const { element, waitFor } = await openGuide('/guide/scenes/trans');
    expect(text(await waitFor('.scene-landing__intro p'))).toContain('curated by its own people');
    expect(texts(element, '.link-out-card__label')).toEqual([
      'Trans community Discord',
      'Trans resource list',
    ]);
  });

  it("keeps the page up when the coming-up list can't be loaded", async () => {
    vi.spyOn(GuideMockAccess.prototype, 'getSceneEvents').mockReturnValue(
      throwError(() => new Error('offline')),
    );
    const { element } = await openGuide('/guide/scenes/bear');
    expect(text(element.querySelector('h1'))).toBe('Bears');
    expect(texts(element, '.entity-card__name')).toEqual(['The Lantern Room']);
    expect(text(element.querySelector('.guide-upcoming__none'))).toBe(
      'Nothing on the calendar here in the next six weeks.',
    );
  });

  it('says so when a scene has nothing listed yet', async () => {
    const { element } = await openGuide('/guide/scenes/furry');
    expect(text(element.querySelector('.guide-page__empty'))).toBe(
      'Nothing here yet — this guide section is being written.',
    );
  });

  it('is a "not found" page for an unknown scene', async () => {
    const { element } = await openGuide('/guide/scenes/nope');
    expect(text(element.querySelector('h1'))).toBe('Scene not found');
    expect(TestBed.inject(DOCUMENT).title).toBe('Scene not found · CommunityFinder');
  });
});
