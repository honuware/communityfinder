import { DOCUMENT } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { of } from 'rxjs';
import { vi } from 'vitest';
import { GuideMockAccess } from '../../core/guide-access/guide-access.mock';
import { normalizeGuidePage } from '../../core/types/guide.types';
import { link, openGuide, text, texts, useGuideTestClock } from './guide-testing';

describe('GuideArticlePage', () => {
  useGuideTestClock();

  it('renders the markdown body, its section, related listings and freshness', async () => {
    const { element, waitFor } = await openGuide('/guide/pages/leather');
    expect(text(element.querySelector('h1'))).toBe('Leather & kink');
    expect(link(element, 'Scenes')?.getAttribute('href')).toBe('/guide/sections/scenes');

    await waitFor('.guide-article__body a');
    // Markdown, rendered: links, not [brackets](parens).
    expect(element.querySelector('.guide-article__body')?.textContent).not.toContain('](');
    expect(
      Array.from(element.querySelectorAll('.guide-article__body a')).map((anchor) =>
        anchor.getAttribute('href'),
      ),
    ).toEqual(['/guide/venues/iron-and-oak', '/guide/orgs/pacific-leather-collective']);

    // The listings that share its scene tag.
    expect(texts(element, '.guide-article__related li')).toEqual([
      'Iron & Oak Venue · 18+',
      'Pacific Leather Collective Social meetup · 18+',
    ]);
    expect(text(element.querySelector('app-freshness-footer'))).toBe('Verified September 2026');
    expect(element.querySelector('.guide-article__preview')).toBeNull();
    expect(TestBed.inject(DOCUMENT).title).toBe('Leather & kink · CommunityFinder');
  });

  it('follows a link in the body without reloading the app', async () => {
    const { waitFor, settle, harness, router } = await openGuide('/guide/pages/getting-tested');
    const services = (await waitFor('.guide-article__body a')) as HTMLAnchorElement;
    expect(services.getAttribute('href')).toBe('/guide/services');

    const click = new MouseEvent('click', { bubbles: true, cancelable: true });
    services.dispatchEvent(click);
    await settle();

    // The in-app-links directive took it: no page load, a router navigation.
    expect(click.defaultPrevented).toBe(true);
    expect(router.url).toBe('/guide/services');
    expect(text((harness.routeNativeElement as HTMLElement).querySelector('h1'))).toBe('Services');
  });

  it('is "not found" for a draft or a page scheduled for later', async () => {
    const draft = await openGuide('/guide/pages/insurance-and-costs');
    expect(text(draft.element.querySelector('h1'))).toBe('Page not found');
    expect(TestBed.inject(DOCUMENT).title).toBe('Page not found · CommunityFinder');

    await draft.harness.navigateByUrl('/guide/pages/beat-the-freeze');
    expect(text(draft.element.querySelector('h1'))).toBe('Page not found');
  });

  it('asks before showing an 18+ page', async () => {
    const { element, harness, waitFor } = await openGuide('/guide/pages/etiquette-basics');
    expect(text(element.querySelector('h1'))).toBe('18+ section');
    expect(element.querySelector('.guide-article__body')).toBeNull();
    element.querySelector<HTMLButtonElement>('.adult-gate__accept')!.click();
    harness.detectChanges();
    expect(text(element.querySelector('h1'))).toBe('Etiquette basics');
    expect(text(await waitFor('.guide-article__body p'))).toBe('A mock 18+ page, behind the gate.');
  });

  it("marks a curator's preview of an unpublished page", async () => {
    vi.spyOn(GuideMockAccess.prototype, 'getGuidePage').mockReturnValue(
      of(
        normalizeGuidePage({
          slug: 'insurance-and-costs',
          title: 'Insurance and costs',
          section: 'health',
          body_markdown: 'Not written yet.',
          draft: true,
          published: false,
          status: 'open',
        }),
      ),
    );
    const { element } = await openGuide('/guide/pages/insurance-and-costs');
    expect(text(element.querySelector('.guide-article__preview'))).toBe(
      "visibility Curator preview: this page isn't published.",
    );
    expect(text(element.querySelector('h1'))).toBe('Insurance and costs');
  });
});
