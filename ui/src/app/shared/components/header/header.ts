import { Component, computed, inject, signal, OnDestroy } from '@angular/core';
import { RouterLink } from '@angular/router';
import { Router } from '@angular/router';
import { MatToolbarModule } from '@angular/material/toolbar';
import { MatButtonModule } from '@angular/material/button';
import { MatMenuModule } from '@angular/material/menu';
import { MatIconModule } from '@angular/material/icon';
import { Subject, takeUntil } from 'rxjs';
import { AuthService, AuthData } from '@honuware/ui/auth';
import { PhotoUrlBuilder } from '@honuware/ui/access';
import { SiteConfigService } from '../../../core/services/site-config.service';

// The app shell header: the community display name on the left; on the right,
// either Login/Register links (logged out) or a user chip — avatar + name with a
// dropdown (My account / Admin / Logout). Reads the live AuthData stream so it
// flips the moment silent login resolves or the user logs in/out.
@Component({
  selector: 'app-header',
  imports: [RouterLink, MatToolbarModule, MatButtonModule, MatMenuModule, MatIconModule],
  templateUrl: './header.html',
  styleUrl: './header.scss',
})
export class Header implements OnDestroy {
  private auth = inject(AuthService);
  private photoUrls = inject(PhotoUrlBuilder);
  private siteConfig = inject(SiteConfigService);
  private router = inject(Router);
  private destroy$ = new Subject<void>();

  private authData = signal<AuthData>({ isAuth: false });

  // Narrowed logged-in user (or null) — lets the template read personId/name/isAdmin
  // without re-narrowing the union on every field access.
  readonly user = computed(() => {
    const data = this.authData();
    return data.isAuth ? data : null;
  });

  constructor() {
    this.auth.authData$
      .pipe(takeUntil(this.destroy$))
      .subscribe((data) => this.authData.set(data));
  }

  ngOnDestroy(): void {
    this.destroy$.next();
    this.destroy$.complete();
  }

  get siteName(): string {
    return this.siteConfig.config.displayName;
  }

  avatarUrl(personId: number): string {
    return this.photoUrls.scaledPhotoUrl('people', personId, 300, 300);
  }

  // People without an uploaded photo 404 the avatar URL — hide the broken image
  // so the neutral background chip shows instead of a broken-image glyph.
  onAvatarError(event: Event): void {
    (event.target as HTMLImageElement).style.visibility = 'hidden';
  }

  logout(): void {
    this.auth.logout().subscribe({
      complete: () => this.router.navigateByUrl('/'),
    });
  }
}
