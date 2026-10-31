import { DOCUMENT } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { vi } from 'vitest';
import { GuideMockAccess } from '../../core/guide-access/guide-access.mock';
import { link, openGuide, text, texts, useGuideTestClock } from './guide-testing';

describe('SectionIndexPage', () => {
  useGuideTestClock();

  it("lists a section's published pages with their verified stamps, never the draft", async () => {
    const { element } = await openGuide('/guide/sections/health');
    expect(text(element.querySelector('h1'))).toBe('Health');
    // Verified 5 days ago.
    expect(texts(element, '.section-index__pages li')).toEqual([
      'Getting tested Verified October 2026',
    ]);
    expect(link(element, 'Getting tested')?.getAttribute('href')).toBe(
      '/guide/pages/getting-tested',
    );
    expect(TestBed.inject(DOCUMENT).title).toBe('Health · CommunityFinder');
  });

  it('says a section with nothing published yet is being written', async () => {
    // The Beat the Freeze page is scheduled for next week.
    const { element } = await openGuide('/guide/sections/freeze');
    expect(text(element.querySelector('h1'))).toBe('Beat the Freeze');
    expect(text(element.querySelector('.guide-page__empty'))).toBe(
      'Nothing here yet — this guide section is being written.',
    );
  });

  it('asks before showing the 18+ section', async () => {
    const { element, harness } = await openGuide('/guide/sections/adult');
    expect(text(element.querySelector('h1'))).toBe('18+ section');
    expect(element.querySelector('.section-index__pages')).toBeNull();
    element.querySelector<HTMLButtonElement>('.adult-gate__accept')!.click();
    harness.detectChanges();
    expect(texts(element, '.section-index__pages li')).toEqual([
      'Etiquette basics Verified October 2026 · 18+',
    ]);
  });

  it('is "not found" for a section that doesn\'t exist, without asking the server', async () => {
    const bySection = vi.spyOn(GuideMockAccess.prototype, 'getGuidePagesBySection');
    const { element } = await openGuide('/guide/sections/nope');
    expect(text(element.querySelector('h1'))).toBe('Section not found');
    expect(bySection).not.toHaveBeenCalled();
  });
});
