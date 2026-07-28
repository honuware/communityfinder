import { Provider } from '@angular/core';
import { ADMIN_USERS_ACCESS } from './admin-users-access';
import { AdminUsersHttpAccess } from './admin-users-access.http';
import { AdminUsersMockAccess } from './admin-users-access.mock';

// Provide the AdminUsersAccess seam — the offline mock (local) or the HTTP impl.
export function provideAdminUsersAccess(useMock: boolean): Provider {
  return useMock
    ? { provide: ADMIN_USERS_ACCESS, useClass: AdminUsersMockAccess }
    : { provide: ADMIN_USERS_ACCESS, useClass: AdminUsersHttpAccess };
}
