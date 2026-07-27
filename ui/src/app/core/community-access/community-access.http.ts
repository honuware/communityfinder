import { HttpClient } from '@angular/common/http';
import { inject, Injectable } from '@angular/core';
import { Observable } from 'rxjs';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import { CommunityAccess, HealthInfo, SiteInfo } from './community-access';

// The real HTTP implementation. `withCredentials` matches the library's HTTP
// access (the session cookie rides every request); the base path comes from the
// library's HONUWARE_API_BASE token (default `/api`, proxied to the server in dev).
@Injectable()
export class CommunityHttpAccess implements CommunityAccess {
  private http = inject(HttpClient);
  private apiBase = inject(HONUWARE_API_BASE);

  getHealth(): Observable<HealthInfo> {
    return this.http.get<HealthInfo>(`${this.apiBase}/health`, { withCredentials: true });
  }

  getSiteInfo(): Observable<SiteInfo> {
    return this.http.get<SiteInfo>(`${this.apiBase}/site_info`, { withCredentials: true });
  }
}
