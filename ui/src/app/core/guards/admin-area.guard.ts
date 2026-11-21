import { inject } from '@angular/core';
import { CanActivateFn, Router } from '@angular/router';
import { AUTH_ROUTES, AuthData, AuthService } from '@honuware/ui/auth';
import { canManageEvents } from './manage-events.guard';

// The permission the guide's tables need (GD2 OQ3): the guide's ten tables,
// plus venues.
export const MANAGE_GUIDE_PERMISSION = 'manage_guide';

// Whether this person curates the guide: an admin, or anyone holding
// manage_guide.
export function canManageGuide(authData: AuthData): boolean {
  return (
    authData.isAuth &&
    (authData.isAdmin || (authData.permissions ?? []).includes(MANAGE_GUIDE_PERMISSION))
  );
}

// CM3 OQ24: whether this person may use the admin area at all: an admin, an
// events curator, or a guide curator. The server checks the permission on
// every call; this decides what the site offers.
export function canUseAdminArea(authData: AuthData): boolean {
  return canManageEvents(authData) || canManageGuide(authData);
}

// The admin area's door (/admin).
// - Everyone it lets in gets the dashboard and Manage Data, whose table list
//   the server cuts to what each person can reach.
// - ManageEventsGuard keeps the Events pages to events curators.
// - AdminGuard keeps Users, Roles and Site settings to admins.
// Anyone else goes where a signed-out visitor would, as AdminGuard sends them.
export const AdminAreaGuard: CanActivateFn = () => {
  const authService = inject(AuthService);
  if (canUseAdminArea(authService.authData)) {
    return true;
  }
  return inject(Router).parseUrl(inject(AUTH_ROUTES).postLogoutPath);
};
