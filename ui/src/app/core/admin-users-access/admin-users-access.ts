import { InjectionToken } from '@angular/core';
import { Observable } from 'rxjs';

export interface CreateUserInput {
  first_name: string;
  last_name: string;
  email: string;
}

// The server reuses an existing account by email rather than duplicating it, so the
// result reports whether it created a new one (`already_exists`) and echoes the
// resolved names (the stored ones when an existing account was reused).
export interface CreateUserResult {
  person_id: number;
  already_exists: boolean;
  first_name: string;
  last_name: string;
}

// The admin user-management access seam — the two bespoke, admin-gated actions the
// generic CrudAccess doesn't cover: creating an account (which emails the person a
// temporary password to sign in and set their own) and resetting a password (which
// emails a fresh temporary password + forces a change on next login). Search / edit /
// delete stay on HONUWARE_CRUD_ACCESS; these are the endpoints @honuware/ui has no
// wrapper for. Mirrors the CommunityAccess seam (health / site_info).
export interface AdminUsersAccess {
  // POST /api/admin/create_user — admin_portal gated.
  createUser(input: CreateUserInput): Observable<CreateUserResult>;
  // POST /api/admin/reset_password — admin_portal gated.
  resetPassword(email: string): Observable<void>;
}

export const ADMIN_USERS_ACCESS = new InjectionToken<AdminUsersAccess>('ADMIN_USERS_ACCESS');
