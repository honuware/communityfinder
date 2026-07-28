import { HttpClient } from '@angular/common/http';
import { inject, Injectable } from '@angular/core';
import { map, Observable } from 'rxjs';
import { HONUWARE_API_BASE } from '@honuware/ui/access';
import {
  AdminUsersAccess,
  CreateUserInput,
  CreateUserResult,
} from './admin-users-access';

// The real HTTP implementation. `withCredentials` carries the admin's session cookie;
// the CsrfInterceptor stamps the X-CSRF-Token header on these POSTs. The server gates
// both routes on the `admin_portal` permission.
@Injectable()
export class AdminUsersHttpAccess implements AdminUsersAccess {
  private http = inject(HttpClient);
  private apiBase = inject(HONUWARE_API_BASE);

  createUser(input: CreateUserInput): Observable<CreateUserResult> {
    return this.http.post<CreateUserResult>(`${this.apiBase}/admin/create_user`, input, {
      withCredentials: true,
    });
  }

  resetPassword(email: string): Observable<void> {
    return this.http
      .post(`${this.apiBase}/admin/reset_password`, { email }, { withCredentials: true })
      .pipe(map(() => void 0));
  }
}
