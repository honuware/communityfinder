import { of } from 'rxjs';
import { vi } from 'vitest';
import { GuideMockAccess } from '../../core/guide-access/guide-access.mock';
import { normalizeGuidePage } from '../../core/types/guide.types';
import { link, openGuide, text, texts, useGuideTestClock } from './guide-testing';

describe('PlacePage', () => {
  useGuideTestClock();

  it('shows a place: its kind, neighborhood, address, links and freshness', async () => {
    const { element } = await openGuide('/guide/places/rainbow-mural-wall');
    expect(text(element.querySelector('h1'))).toBe('Rainbow Mural Wall');
    expect(text(element.querySelector('.guide-page__lead'))).toBe('Landmark · Capitol Hill');
    expect(text(element.querySelector('.place-page__address'))).toBe('E Pike St and 11th Ave');
    expect(texts(element, '.link-out-card__label')).toEqual(['Mural project']);
    // No article with its slug: no "Read more".
    expect(element.querySelector('.place-page__article')).toBeNull();
    // Verified 100 days ago.
    expect(text(element.querySelector('app-freshness-footer'))).toBe('Verified July 2026');
  });

  it("links the place's own article when one is published", async () => {
    const getGuidePage = GuideMockAccess.prototype.getGuidePage;
    vi.spyOn(GuideMockAccess.prototype, 'getGuidePage').mockImplementation(function (
      this: GuideMockAccess,
      slug: string,
    ) {
      return slug === 'rainbow-mural-wall'
        ? of(normalizeGuidePage({ slug, title: 'The mural, wall by wall', published: true }))
        : getGuidePage.call(this, slug);
    });
    const { element } = await openGuide('/guide/places/rainbow-mural-wall');
    expect(link(element, 'The mural, wall by wall')?.getAttribute('href')).toBe(
      '/guide/pages/rainbow-mural-wall',
    );
  });

  it('asks before showing an 18+ place, then shows it', async () => {
    const { element, harness } = await openGuide('/guide/places/denny-blaine-park');
    expect(text(element.querySelector('h1'))).toBe('18+ section');
    element.querySelector<HTMLButtonElement>('.adult-gate__accept')!.click();
    harness.detectChanges();
    expect(text(element.querySelector('h1'))).toBe('Denny Blaine Park');
    expect(text(element.querySelector('.guide-page__lead'))).toBe('Park & beach');
  });

  it('is a "not found" page for an unknown place', async () => {
    const { element } = await openGuide('/guide/places/nope');
    expect(text(element.querySelector('h1'))).toBe('Place not found');
  });
});
