import { DOCUMENT } from '@angular/core';
import { TestBed } from '@angular/core/testing';
import { provideRouter, Router } from '@angular/router';
import { normalizeEvent, PublicEvent } from '../types/events.types';
import { buildEventJsonLd } from './event-json-ld';
import { pacificWallClockToUs } from './events-date.utils';
import { PageMeta, snippet } from './page-meta';

const PHOTO = 'https://communityfinder.example/api/get_scaled_photo/events/42/1200/630';

function event(fields: Partial<PublicEvent> = {}): PublicEvent {
  return {
    ...normalizeEvent({
      id: 42,
      title: 'Leather Night',
      description: 'Dress code encouraged, not enforced.',
      starts_at_us: pacificWallClockToUs(2026, 8, 14, 21),
      venue_name: 'The Lantern Room',
    }),
    ...fields,
  };
}

function setUp() {
  TestBed.configureTestingModule({
    providers: [provideRouter([{ path: '**', children: [] }])],
  });
  return { pageMeta: TestBed.inject(PageMeta), document: TestBed.inject(DOCUMENT) };
}

// What the page's <head> says, tag by tag.
function head(document: Document) {
  const content = (selector: string) =>
    document.head.querySelector(selector)?.getAttribute('content') ?? null;
  const jsonLd = document.getElementById('page-json-ld')?.textContent;
  return {
    title: document.title,
    description: content('meta[name="description"]'),
    canonical: document.head.querySelector('link[rel="canonical"]')?.getAttribute('href') ?? null,
    ogTitle: content('meta[property="og:title"]'),
    ogDescription: content('meta[property="og:description"]'),
    ogUrl: content('meta[property="og:url"]'),
    ogType: content('meta[property="og:type"]'),
    ogSiteName: content('meta[property="og:site_name"]'),
    ogImage: content('meta[property="og:image"]'),
    twitterCard: content('meta[name="twitter:card"]'),
    jsonLd: jsonLd ? JSON.parse(jsonLd) : null,
    canonicalCount: document.head.querySelectorAll('link[rel="canonical"]').length,
    descriptionCount: document.head.querySelectorAll('meta[name="description"]').length,
  };
}

describe('PageMeta', () => {
  afterEach(() => TestBed.inject(PageMeta).reset());

  it("sets an event page's full tag set, photo included", () => {
    const { pageMeta, document } = setUp();
    pageMeta.setEventPage(event(), PHOTO);
    const url = `${document.location.origin}/events/42`;
    expect(head(document)).toEqual({
      title: 'Leather Night — Fri, Aug 14 · CommunityFinder',
      description: 'Dress code encouraged, not enforced.',
      canonical: url,
      ogTitle: 'Leather Night — Fri, Aug 14',
      ogDescription: 'Dress code encouraged, not enforced.',
      ogUrl: url,
      ogType: 'website',
      ogSiteName: 'CommunityFinder',
      ogImage: PHOTO,
      twitterCard: 'summary_large_image',
      jsonLd: buildEventJsonLd(event(), url, PHOTO),
      canonicalCount: 1,
      descriptionCount: 1,
    });
  });

  it('sets no og:image for an event without a photo, and describes it by when and where', () => {
    const { pageMeta, document } = setUp();
    pageMeta.setEventPage(event({ description: '' }));
    const tags = head(document);
    expect(tags.ogImage).toBeNull();
    expect(tags.twitterCard).toBe('summary');
    expect(tags.description).toBe('Fri, Aug 14, 9:00 PM · The Lantern Room');
    expect(tags.jsonLd?.['image']).toBeUndefined();
  });

  it("gives a curator's preview of an unapproved event no JSON-LD", () => {
    const { pageMeta, document } = setUp();
    pageMeta.setEventPage(event({ status: 'pending', review_notes: '' }));
    expect(head(document).jsonLd).toBeNull();
    pageMeta.setEventPage(event({ status: 'approved', review_notes: '' }));
    expect(head(document).jsonLd).not.toBeNull();
  });

  it("overwrites the last page's tags, leaving nothing stale", () => {
    const { pageMeta, document } = setUp();
    pageMeta.setEventPage(event(), PHOTO);
    pageMeta.set({
      title: 'Events tonight',
      description: 'Upcoming events.',
      canonicalPath: '/events',
    });
    const tags = head(document);
    expect(tags.title).toBe('Events tonight · CommunityFinder');
    expect(tags.canonical).toBe(`${document.location.origin}/events`);
    expect(tags.ogImage).toBeNull();
    expect(tags.jsonLd).toBeNull();
    expect(tags.canonicalCount).toBe(1);
    expect(tags.descriptionCount).toBe(1);
  });

  it('starts every navigation from the site defaults', async () => {
    const { pageMeta, document } = setUp();
    pageMeta.setEventPage(event(), PHOTO);
    await TestBed.inject(Router).navigateByUrl('/account');
    const tags = head(document);
    expect(tags.title).toBe('CommunityFinder');
    expect(tags.description).toBeNull();
    expect(tags.canonical).toBeNull();
    expect(tags.ogTitle).toBeNull();
    expect(tags.ogImage).toBeNull();
    expect(tags.jsonLd).toBeNull();
  });

  it('keeps a script-closing "<" out of the JSON-LD', () => {
    const { pageMeta, document } = setUp();
    pageMeta.setEventPage(event({ title: '</script><b>' }));
    const script = document.getElementById('page-json-ld')!;
    expect(script.textContent).not.toContain('</script>');
    expect(JSON.parse(script.textContent!).name).toBe('</script><b>');
  });

  it('cuts a description to a snippet on a word boundary', () => {
    expect(snippet('  Two\n\nlines  ')).toBe('Two lines');
    const long = 'word '.repeat(60);
    const cut = snippet(long);
    expect(cut.length).toBeLessThanOrEqual(160);
    expect(cut.endsWith('word…')).toBe(true);
  });
});
