import {
  ApplicationConfig,
  inject,
  provideAppInitializer,
  provideBrowserGlobalErrorListeners,
  provideZoneChangeDetection,
} from '@angular/core';
import {
  HTTP_INTERCEPTORS,
  provideHttpClient,
  withInterceptorsFromDi,
} from '@angular/common/http';
import { provideAnimations } from '@angular/platform-browser/animations';
import { provideRouter, withInMemoryScrolling } from '@angular/router';
import { CsrfInterceptor, HONUWARE_API_BASE, provideHonuwareAccess } from '@honuware/ui/access';
import {
  AUTH_ROUTES,
  AuthRoutes,
  AuthService,
  ErrorInterceptor,
  tryTokenLoginInitializer,
} from '@honuware/ui/auth';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';

import { routes } from './app.routes';
import { environment } from '../environments/environment';
import { provideCommunityAccess } from './core/community-access/community-access.providers';
import { SiteConfigService } from './core/services/site-config.service';
import { HONUWARE_MOCK_OPTIONS } from './core/mock/honuware-mock';

export const appConfig: ApplicationConfig = {
  providers: [
    // @honuware/ui's components use ZONE-BASED change detection (plain subscribes +
    // field updates — no signals / markForCheck), so the app must run with NgZone.
    // Angular 21 is otherwise effectively zoneless even with zone.js in polyfills, and
    // their passive async loads (e.g. the admin CRUD table view fetching rows) would
    // render only after an unrelated interaction forces a CD tick. Matches knottyyoga.
    provideZoneChangeDetection(),
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
    // the DI swap the plan calls for. In mock mode we seed a demo admin + a small
    // CRUD schema/rows so the admin editor is fully navigable offline.
    ...(environment.useMock
      ? provideHonuwareAccessMock(HONUWARE_MOCK_OPTIONS)
      : provideHonuwareAccess()),
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
    // CF's auth navigation targets: guards + the auth pages redirect through these
    // instead of @honuware/ui's defaults. mustChangePasswordPath points at CF's own
    // change-password page; the allowlist bounds post-login returnUrl redirects.
    {
      provide: AUTH_ROUTES,
      useValue: {
        loginPath: '/login',
        registerPath: '/register',
        postLogoutPath: '/',
        mustChangePasswordPath: '/account/password',
        returnUrlAllowlist: ['/account', '/'],
      } satisfies AuthRoutes,
    },
    provideRouter(routes, withInMemoryScrolling({ scrollPositionRestoration: 'enabled' })),
  ],
};
