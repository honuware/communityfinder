import { Injectable } from '@angular/core';
import { Observable, of } from 'rxjs';
import {
  AdminUsersAccess,
  CreateUserInput,
  CreateUserResult,
} from './admin-users-access';

// Offline mock — the `local` configuration wires this so `ng serve` runs with no
// backend. Create returns a synthetic new account; reset succeeds silently. Neither
// sends a real email, so the admin page's confirmation text is exercised offline.
@Injectable()
export class AdminUsersMockAccess implements AdminUsersAccess {
  private nextId = 1000;

  createUser(input: CreateUserInput): Observable<CreateUserResult> {
    return of({
      person_id: this.nextId++,
      already_exists: false,
      first_name: input.first_name,
      last_name: input.last_name,
    });
  }

  resetPassword(_email: string): Observable<void> {
    return of(void 0);
  }
}
