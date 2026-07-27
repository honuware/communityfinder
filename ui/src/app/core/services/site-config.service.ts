import { inject, Injectable } from '@angular/core';
import { Observable, of } from 'rxjs';
import { catchError, map, tap } from 'rxjs/operators';
import { COMMUNITY_ACCESS, SiteInfo } from '../community-access/community-access';

// The runtime-branding surface the shell reads. The API (GET /api/site_info)
// supplies per-community display name / website / logo; the rest is app-static.
// Ported from knottyyoga's SiteConfigService pattern.
export interface SiteConfig {
  displayName: string;
  websiteUrl: string;
  // Per-community logo URL. Empty string ⇒ the shell falls back to logoAltText.
  logoUrl: string;
  logoAltText: string;
  taglineLines: string[];
}

// The hardcoded fallback — used until (and if) the /api/site_info fetch resolves,
// so render is never blocked on the network.
export const DEFAULT_SITE_CONFIG: SiteConfig = {
  displayName: 'CommunityFinder',
  websiteUrl: '',
  logoUrl: '',
  logoAltText: 'CommunityFinder',
  taglineLines: ['Find your community.'],
};

@Injectable({ providedIn: 'root' })
export class SiteConfigService {
  // Optional so the service always constructs (returning the fallback) even in a
  // shallow spec that renders header/footer without the access token wired.
  private access = inject(COMMUNITY_ACCESS, { optional: true });

  private _config: SiteConfig = { ...DEFAULT_SITE_CONFIG };

  // Synchronous snapshot — starts as the fallback, replaced in place once load()
  // resolves, so components can read it directly at any time.
  get config(): SiteConfig {
    return this._config;
  }

  // App-initializer hook: fetch the resolved community's public branding and merge
  // the non-empty fields over the defaults. NEVER rejects — a failed/empty fetch
  // leaves the fallback in place (logged), so a branding blip never blocks boot.
  load(): Observable<void> {
    if (!this.access) {
      return of(void 0);
    }
    return this.access.getSiteInfo().pipe(
      tap((info) => this.applySiteInfo(info)),
      map(() => void 0),
      catchError((err) => {
        console.warn('[SiteConfig] /api/site_info failed; using fallback branding', err);
        return of(void 0);
      })
    );
  }

  private applySiteInfo(info: SiteInfo): void {
    const next: SiteConfig = { ...this._config };
    // Only override when the API supplied a non-empty value, so a blank field does
    // not wipe a sensible default.
    if (info.display_name) next.displayName = info.display_name;
    if (info.website_url) next.websiteUrl = info.website_url;
    // logoUrl may legitimately be '' (no logo) — store as-is; the shell uses the
    // alt text when it is blank.
    next.logoUrl = info.logo_url ?? '';
    this._config = next;
  }
}
