import { DOCUMENT } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { throwError } from 'rxjs';
import { vi } from 'vitest';
import { GuideMockAccess } from '../../core/guide-access/guide-access.mock';
import { link, openGuide, text, texts, useGuideTestClock } from './guide-testing';

describe('GuideHomePage', () => {
  useGuideTestClock();

  it('tiles the scenes we cover, and gives the link-out scenes their own credited row', async () => {
    const { element } = await openGuide('/guide');
    expect(text(element.querySelector('h1'))).toBe('Guide');
    expect(texts(element, '.guide-home__scenes .guide-page__tile-name')).toEqual([
      'Bears',
      'Leather & kink',
      'Jocks & sports',
      'Sober',
      'Elders',
      'Geeks & gaymers',
      'Furries',
      'BIPOC',
    ]);
    expect(texts(element, '.guide-home__sister-scenes .guide-page__tile-name')).toEqual([
      'Sapphic',
      'Trans',
      'QTPOC',
    ]);
    // Each tile says what's listed, or that it's being written.
    expect(texts(element, '.guide-home__scenes .guide-page__tile-note').slice(0, 3)).toEqual([
      '1 venue',
      '1 venue · 1 organization',
      '1 organization',
    ]);
    expect(texts(element, '.guide-home__scenes .guide-page__tile-note')).toContain('Being written');
    expect(element.querySelector('.guide-home__sister-scenes a')?.getAttribute('href')).toBe(
      '/guide/scenes/sapphic',
    );
    expect(TestBed.inject(DOCUMENT).title).toBe('Guide · CommunityFinder');
  });

  it('links to the sections and directories', async () => {
    const { element } = await openGuide('/guide');
    expect(link(element, 'Health')?.getAttribute('href')).toBe('/guide/sections/health');
    expect(link(element, 'Services')?.getAttribute('href')).toBe('/guide/services');
    expect(link(element, 'Beat the Freeze')?.getAttribute('href')).toBe('/guide/sections/freeze');
    expect(link(element, '18+')?.getAttribute('href')).toBe('/guide/sections/adult');
    expect(
      Array.from(element.querySelectorAll('.guide-home__directories a')).map((anchor) =>
        anchor.getAttribute('href'),
      ),
    ).toEqual(['/guide/venues', '/guide/orgs', '/guide/neighborhoods', '/guide/graveyard']);
  });

  it("still links everywhere when the scenes can't be loaded", async () => {
    vi.spyOn(GuideMockAccess.prototype, 'getScenes').mockReturnValue(
      throwError(() => new Error('offline')),
    );
    const { element } = await openGuide('/guide');
    expect(element.textContent).toContain("The scenes couldn't be loaded.");
    expect(link(element, 'Health')).toBeDefined();
  });
});
