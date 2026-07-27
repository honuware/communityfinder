import { Component, inject, OnInit, signal } from '@angular/core';
import { MatCardModule } from '@angular/material/card';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import { SiteConfigService } from '../../core/services/site-config.service';

// The home page — the "simple dummy call that gets shown": the live /api/health
// payload + the community display name from SiteConfigService.
@Component({
  selector: 'app-home',
  imports: [MatCardModule],
  templateUrl: './home.html',
  styleUrl: './home.scss',
})
export class Home implements OnInit {
  private access = inject(COMMUNITY_ACCESS);
  private siteConfig = inject(SiteConfigService);

  readonly siteName = signal(this.siteConfig.config.displayName);
  readonly health = signal<string>('checking…');
  readonly version = signal<string>('');

  ngOnInit(): void {
    this.access.getHealth().subscribe({
      next: (h) => {
        this.health.set(h.status);
        this.version.set(h.version ?? '');
      },
      error: () => this.health.set('unavailable'),
    });
  }
}
