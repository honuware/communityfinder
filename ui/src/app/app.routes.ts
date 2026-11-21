import { Routes } from '@angular/router';
import {
  AuthGuard,
  NoAuthGuard,
  LoginComponent,
  RegisterComponent,
  VerifyComponent,
} from '@honuware/ui/auth';
import { AdminAreaGuard } from './core/guards/admin-area.guard';
import { Home } from './pages/home/home';
import { AccountPage } from './pages/account/account';
import { ChangePasswordPage } from './pages/account/change-password';
import { MySubmissionsPage } from './pages/account/my-submissions';

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
  // The member's own event submissions (EV3 §5.3).
  { path: 'account/submissions', component: MySubmissionsPage, canActivate: [AuthGuard] },

  // The public events pages (EV2 §3): list, calendar, series, and each event.
  // Lazy-loaded; anonymous.
  { path: 'events', loadChildren: () => import('./pages/events/events.routes') },

  // The guide (GD3 §3): scenes, venues, organizations, places, services,
  // neighborhoods, the graveyard and articles. Lazy-loaded with its own data
  // access and markdown renderer; anonymous.
  { path: 'guide', loadChildren: () => import('./pages/guide/guide.routes') },

  // The admin area (the dashboard, the Events pages, and @honuware/ui/crud's
  // editor), lazy-loaded so its pages stay out of the initial bundle. AuthGuard
  // sends anonymous visitors to /login. AdminAreaGuard lets in admins, events
  // curators (CM3 Phase 2 §12.5) and guide curators (CM3 OQ24), and sends
  // anyone else home. Each page inside carries its own narrower guard
  // (admin.routes.ts).
  {
    path: 'admin',
    canActivate: [AuthGuard, AdminAreaGuard],
    loadChildren: () => import('./pages/admin/admin.routes'),
  },

  { path: '**', redirectTo: '' },
];
