import { Component, inject } from '@angular/core';
import { MatToolbarModule } from '@angular/material/toolbar';
import { MatButtonModule } from '@angular/material/button';
import { SiteConfigService } from '../../../core/services/site-config.service';

// The app shell header: the community display name on the left, login/register
// placeholders on the right (wired to hw-login / hw-register in Phase 5; the
// logged-in user chip lands in Phase 5 too).
@Component({
  selector: 'app-header',
  imports: [MatToolbarModule, MatButtonModule],
  templateUrl: './header.html',
  styleUrl: './header.scss',
})
export class Header {
  private siteConfig = inject(SiteConfigService);

  get siteName(): string {
    return this.siteConfig.config.displayName;
  }
}
