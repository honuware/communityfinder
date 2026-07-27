import { Component, inject } from '@angular/core';
import { SiteConfigService } from '../../../core/services/site-config.service';

@Component({
  selector: 'app-footer',
  imports: [],
  templateUrl: './footer.html',
  styleUrl: './footer.scss',
})
export class Footer {
  private siteConfig = inject(SiteConfigService);
  readonly year = new Date().getFullYear();

  get siteName(): string {
    return this.siteConfig.config.displayName;
  }
}
