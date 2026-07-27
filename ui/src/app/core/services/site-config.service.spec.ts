import { TestBed } from '@angular/core/testing';
import { firstValueFrom, of, throwError } from 'rxjs';
import { COMMUNITY_ACCESS, CommunityAccess } from '../community-access/community-access';
import { DEFAULT_SITE_CONFIG, SiteConfigService } from './site-config.service';

function makeService(access: CommunityAccess | null): SiteConfigService {
  TestBed.configureTestingModule({
    providers: access ? [{ provide: COMMUNITY_ACCESS, useValue: access }] : [],
  });
  return TestBed.inject(SiteConfigService);
}

describe('SiteConfigService', () => {
  it('starts with the default (fallback) config', () => {
    const svc = makeService(null);
    expect(svc.config.displayName).toBe(DEFAULT_SITE_CONFIG.displayName);
  });

  it('merges non-empty site_info fields over the defaults on load()', async () => {
    const svc = makeService({
      getHealth: () => of({ status: 'ok' }),
      getSiteInfo: () =>
        of({ display_name: 'Seattle Pride Finder', website_url: 'https://x.test/', logo_url: '' }),
    });
    await firstValueFrom(svc.load());
    expect(svc.config.displayName).toBe('Seattle Pride Finder');
    expect(svc.config.websiteUrl).toBe('https://x.test/');
    expect(svc.config.logoUrl).toBe('');
  });

  it('does not overwrite defaults with empty API values', async () => {
    const svc = makeService({
      getHealth: () => of({ status: 'ok' }),
      getSiteInfo: () => of({ display_name: '', website_url: '', logo_url: '' }),
    });
    await firstValueFrom(svc.load());
    expect(svc.config.displayName).toBe(DEFAULT_SITE_CONFIG.displayName);
  });

  it('keeps the fallback and never rejects when the fetch errors', async () => {
    const svc = makeService({
      getHealth: () => of({ status: 'ok' }),
      getSiteInfo: () => throwError(() => new Error('boom')),
    });
    // Must resolve (not reject) — a branding blip cannot block boot.
    await firstValueFrom(svc.load());
    expect(svc.config.displayName).toBe(DEFAULT_SITE_CONFIG.displayName);
  });

  it('returns the fallback when no access token is wired', async () => {
    const svc = makeService(null);
    await firstValueFrom(svc.load());
    expect(svc.config.displayName).toBe(DEFAULT_SITE_CONFIG.displayName);
  });
});
