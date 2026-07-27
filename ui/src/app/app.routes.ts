import { Routes } from '@angular/router';
import {
  AuthGuard,
  NoAuthGuard,
  LoginComponent,
  RegisterComponent,
  VerifyComponent,
} from '@honuware/ui/auth';
import { Home } from './pages/home/home';
import { AccountPage } from './pages/account/account';
import { ChangePasswordPage } from './pages/account/change-password';

export const routes: Routes = [
  { path: '', component: Home },

  // Auth pages from @honuware/ui — NoAuthGuard bounces already-signed-in users
  // back to home. /verify carries no guard (the email link lands here regardless).
  { path: 'login', component: LoginComponent, canActivate: [NoAuthGuard] },
  { path: 'register', component: RegisterComponent, canActivate: [NoAuthGuard] },
  { path: 'verify', component: VerifyComponent },

  // The signed-in user portal. AuthGuard forwards anonymous visitors to /login;
  // forced-password-change users are steered to /account/password by AUTH_ROUTES.
  { path: 'account', component: AccountPage, canActivate: [AuthGuard] },
  { path: 'account/password', component: ChangePasswordPage, canActivate: [AuthGuard] },

  // The /admin CRUD editor arrives in Phase 7; events routes in Phase 11.
  { path: '**', redirectTo: '' },
];
