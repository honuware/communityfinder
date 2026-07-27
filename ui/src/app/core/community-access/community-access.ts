import { InjectionToken } from '@angular/core';
import { Observable } from 'rxjs';

export interface HealthInfo {
  status: string;
  version?: string;
}

export interface SiteInfo {
  display_name: string;
  website_url: string;
  logo_url: string;
}

// The CommunityFinder app-specific access seam. @honuware/ui provides the generic
// CrudAccess / AuthAccess / PhotoAccess; this adds the framework calls the library
// doesn't wrap — GET /api/health and GET /api/site_info. Later phases extend it
// with the events-domain calls.
export interface CommunityAccess {
  getHealth(): Observable<HealthInfo>;
  getSiteInfo(): Observable<SiteInfo>;
}

export const COMMUNITY_ACCESS = new InjectionToken<CommunityAccess>('COMMUNITY_ACCESS');
