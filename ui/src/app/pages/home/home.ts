import { Component, inject, OnInit, signal } from '@angular/core';
import { RouterLink } from '@angular/router';
import { MatButtonModule } from '@angular/material/button';
import { MatIconModule } from '@angular/material/icon';
import { COMMUNITY_ACCESS } from '../../core/community-access/community-access';
import { SiteConfigService } from '../../core/services/site-config.service';
import { UpcomingEventsStrip } from '../../shared/components/upcoming-events-strip/upcoming-events-strip';

// The home page: a welcome with the way into events and the guide, a row of
// quick links, the upcoming-events strip, and the live /api/health payload in
// small print (the "simple dummy call that gets shown").
@Component({
  selector: 'app-home',
  imports: [RouterLink, MatButtonModule, MatIconModule, UpcomingEventsStrip],
  templateUrl: './home.html',
  styleUrl: './home.scss',
})
export class Home implements OnInit {
  private access = inject(COMMUNITY_ACCESS);
  private siteConfig = inject(SiteConfigService);

  readonly siteName = signal(this.siteConfig.config.displayName);
  readonly health = signal<string>('checking…');
  readonly version = signal<string>('');

  readonly quickLinks: ReadonlyArray<{
    label: string;
    icon: string;
    path: string;
    queryParams?: Record<string, string>;
  }> = [
    { label: 'Tonight', icon: 'nightlife', path: '/events', queryParams: { when: 'tonight' } },
    { label: 'This weekend', icon: 'weekend', path: '/events', queryParams: { when: 'weekend' } },
    { label: 'Recurring', icon: 'repeat', path: '/events/series' },
    { label: 'Venues', icon: 'storefront', path: '/guide/venues' },
    { label: 'Organizations', icon: 'groups', path: '/guide/orgs' },
    { label: 'New to Seattle', icon: 'explore', path: '/guide/sections/new-to-seattle' },
  ];

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
