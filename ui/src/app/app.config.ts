import {
  ApplicationConfig,
  inject,
  provideAppInitializer,
  provideBrowserGlobalErrorListeners,
} from '@angular/core';
import {
  HTTP_INTERCEPTORS,
  provideHttpClient,
  withInterceptorsFromDi,
} from '@angular/common/http';
import { provideAnimations } from '@angular/platform-browser/animations';
import { provideRouter, withInMemoryScrolling } from '@angular/router';
import { CsrfInterceptor, HONUWARE_API_BASE, provideHonuwareAccess } from '@honuware/ui/access';
import { AuthService, ErrorInterceptor, tryTokenLoginInitializer } from '@honuware/ui/auth';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';

import { routes } from './app.routes';
import { environment } from '../environments/environment';
import { provideCommunityAccess } from './core/community-access/community-access.providers';
import { SiteConfigService } from './core/services/site-config.service';

export const appConfig: ApplicationConfig = {
  providers: [
    provideBrowserGlobalErrorListeners(),
    provideAnimations(),
    provideHttpClient(withInterceptorsFromDi()),
    // The honuware API base (default `/api`; proxied to the server in dev).
    { provide: HONUWARE_API_BASE, useValue: environment.apiBase },
    // CsrfInterceptor first so every state-changing request is stamped with the
    // X-CSRF-Token header before ErrorInterceptor sees the response.
    { provide: HTTP_INTERCEPTORS, useClass: CsrfInterceptor, multi: true },
    { provide: HTTP_INTERCEPTORS, useClass: ErrorInterceptor, multi: true },
    // @honuware/ui's Crud/Auth/Photo access — the in-memory mock offline (the
    // `local` build config sets useMock), the real HTTP impls otherwise. This is
    // the DI swap the plan calls for.
    ...(environment.useMock ? provideHonuwareAccessMock() : provideHonuwareAccess()),
    // CommunityFinder's app-specific access (health + site_info).
    provideCommunityAccess(environment.useMock),
    // Bootstrap-time silent login (me() → on 401 remember() → me()). Registered
    // after the interceptors so its calls are CSRF-stamped + error-handled.
    provideAppInitializer(() => {
      const authService = inject(AuthService);
      return tryTokenLoginInitializer(authService)();
    }),
    // Fetch the community branding before first render (never rejects).
    provideAppInitializer(() => inject(SiteConfigService).load()),
    provideRouter(routes, withInMemoryScrolling({ scrollPositionRestoration: 'enabled' })),
  ],
};
