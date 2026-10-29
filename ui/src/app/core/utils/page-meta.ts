import { DOCUMENT, inject, Injectable } from '@angular/core';
import { Meta, Title } from '@angular/platform-browser';
import { NavigationStart, Router } from '@angular/router';
import { filter } from 'rxjs';
import { SiteConfigService } from '../services/site-config.service';
import { PublicEvent } from '../types/events.types';
import { buildEventJsonLd, JsonLd } from './event-json-ld';
import { absoluteUrl, eventPagePath } from './event-links';
import { formatEventShortDate, formatEventTime } from './events-date.utils';

// One page's metadata: the browser-tab title, the search-result description,
// the canonical address, the social-card (Open Graph) tags and, for an event,
// its schema.org JSON-LD. EV2 OQ5: event pages set all of it now; RT4 adopts
// and extends this helper for the rest of the site rather than redoing it.
//
// Honest limit (EV2's metadata pin): link-preview scrapers (Discord, iMessage,
// Facebook) don't run JavaScript, so these tags reach them only once RT4 adds
// prerendering. Google and Bing render JavaScript and see them now.
export interface PageMetaInput {
  // The page's own title; " · {site name}" is added.
  title: string;
  description: string;
  // Root-relative ("/events/42"); made absolute against the site's origin.
  canonicalPath: string;
  // Absolute.
  imageUrl?: string;
  jsonLd?: JsonLd;
}

// A search-result snippet is about this long before it's cut.
const DESCRIPTION_MAX_LENGTH = 160;
const JSON_LD_SCRIPT_ID = 'page-json-ld';
const SOCIAL_TAGS = ['og:title', 'og:description', 'og:url', 'og:type', 'og:site_name', 'og:image'];
const NAME_TAGS = ['description', 'twitter:card'];

// Text cut to a snippet on a word boundary, with an ellipsis.
export function snippet(text: string, maxLength = DESCRIPTION_MAX_LENGTH): string {
  const flat = text.replace(/\s+/g, ' ').trim();
  if (flat.length <= maxLength) {
    return flat;
  }
  const cut = flat.slice(0, maxLength - 1);
  const lastSpace = cut.lastIndexOf(' ');
  return `${(lastSpace > maxLength / 2 ? cut.slice(0, lastSpace) : cut).trimEnd()}…`;
}

@Injectable({ providedIn: 'root' })
export class PageMeta {
  private titleService = inject(Title);
  private meta = inject(Meta);
  private document = inject(DOCUMENT);
  private siteConfig = inject(SiteConfigService);

  constructor() {
    // A page that sets no metadata of its own (home, account) must not keep
    // the last event's: every navigation starts from the site defaults, and
    // each events page sets its own once its data is in.
    inject(Router, { optional: true })
      ?.events.pipe(filter((event) => event instanceof NavigationStart))
      .subscribe(() => this.reset());
  }

  get origin(): string {
    return this.document.location.origin;
  }

  set(input: PageMetaInput): void {
    const siteName = this.siteConfig.config.displayName;
    const url = absoluteUrl(this.origin, input.canonicalPath);
    const description = snippet(input.description);
    this.titleService.setTitle(`${input.title} · ${siteName}`);
    this.meta.updateTag({ name: 'description', content: description });
    this.setCanonical(url);
    this.meta.updateTag({ property: 'og:title', content: input.title });
    this.meta.updateTag({ property: 'og:description', content: description });
    this.meta.updateTag({ property: 'og:url', content: url });
    this.meta.updateTag({ property: 'og:type', content: 'website' });
    this.meta.updateTag({ property: 'og:site_name', content: siteName });
    if (input.imageUrl) {
      this.meta.updateTag({ property: 'og:image', content: input.imageUrl });
      this.meta.updateTag({ name: 'twitter:card', content: 'summary_large_image' });
    } else {
      this.meta.removeTag('property="og:image"');
      this.meta.updateTag({ name: 'twitter:card', content: 'summary' });
    }
    this.setJsonLd(input.jsonLd);
  }

  // An event's page: "T4T Dance Party — Sat, Aug 15", its description (or
  // when and where, if it has none), its photo, and its JSON-LD. A curator's
  // preview of an unapproved event gets no JSON-LD.
  setEventPage(event: PublicEvent, imageUrl?: string): void {
    const path = eventPagePath(event.id);
    const isPublic = event.status === undefined || event.status === 'approved';
    const when = `${formatEventShortDate(event.starts_at_us)}, ${formatEventTime(event.starts_at_us)}`;
    const where = event.venue_name || event.location_text;
    this.set({
      title: `${event.title} — ${formatEventShortDate(event.starts_at_us)}`,
      description: event.description || [when, where].filter(Boolean).join(' · '),
      canonicalPath: path,
      imageUrl,
      jsonLd: isPublic
        ? buildEventJsonLd(event, absoluteUrl(this.origin, path), imageUrl)
        : undefined,
    });
  }

  // Back to the site defaults: the bare site name, no page tags.
  reset(): void {
    this.titleService.setTitle(this.siteConfig.config.displayName);
    for (const property of SOCIAL_TAGS) {
      this.meta.removeTag(`property="${property}"`);
    }
    for (const name of NAME_TAGS) {
      this.meta.removeTag(`name="${name}"`);
    }
    this.document.head.querySelector('link[rel="canonical"]')?.remove();
    this.setJsonLd(undefined);
  }

  private setCanonical(url: string): void {
    let link = this.document.head.querySelector<HTMLLinkElement>('link[rel="canonical"]');
    if (!link) {
      link = this.document.createElement('link');
      link.setAttribute('rel', 'canonical');
      this.document.head.appendChild(link);
    }
    link.setAttribute('href', url);
  }

  private setJsonLd(jsonLd: JsonLd | undefined): void {
    this.document.getElementById(JSON_LD_SCRIPT_ID)?.remove();
    if (!jsonLd) {
      return;
    }
    const script = this.document.createElement('script');
    script.id = JSON_LD_SCRIPT_ID;
    script.type = 'application/ld+json';
    // "<" can't close the script tag from inside the JSON.
    script.textContent = JSON.stringify(jsonLd).replace(/</g, '\\u003c');
    this.document.head.appendChild(script);
  }
}
