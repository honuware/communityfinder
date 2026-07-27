import { Injectable } from '@angular/core';
import { Observable, of } from 'rxjs';
import { CommunityAccess, HealthInfo, SiteInfo } from './community-access';

// Offline mock — the `local` configuration wires this so `ng serve` runs with no
// backend. Returns a healthy status and stub branding.
@Injectable()
export class CommunityMockAccess implements CommunityAccess {
  getHealth(): Observable<HealthInfo> {
    return of({ status: 'ok', version: 'mock' });
  }

  getSiteInfo(): Observable<SiteInfo> {
    return of({
      display_name: 'CommunityFinder (mock)',
      website_url: 'http://localhost:4201/',
      logo_url: '',
    });
  }
}
