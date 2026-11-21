import { inject } from '@angular/core';
import { CanActivateFn, Router } from '@angular/router';
import { AUTH_ROUTES, AuthData, AuthService } from '@honuware/ui/auth';

// The permission the events pages and the curator's tables need (EV1 §2.6).
export const MANAGE_EVENTS_PERMISSION = 'manage_events';

// CM3 Phase 2 §12.5 (OQ19): whether this person curates events: an admin, or
// anyone holding manage_events (the Curator role). The server checks the
// permission on every call; this decides what the site offers.
export function canManageEvents(authData: AuthData): boolean {
  return (
    authData.isAuth &&
    (authData.isAdmin || (authData.permissions ?? []).includes(MANAGE_EVENTS_PERMISSION))
  );
}

// The Events pages (/admin/events, /admin/events/settings): admins and events
// curators. Since OQ24, a guide curator also gets into /admin (AdminAreaGuard),
// and this keeps them out of the events pages. Anyone else goes where a
// signed-out visitor would, as AdminGuard sends them.
export const ManageEventsGuard: CanActivateFn = () => {
  const authService = inject(AuthService);
  if (canManageEvents(authService.authData)) {
    return true;
  }
  return inject(Router).parseUrl(inject(AUTH_ROUTES).postLogoutPath);
};
