import { Routes } from '@angular/router';
import { Home } from './pages/home/home';

export const routes: Routes = [
  { path: '', component: Home },
  // Auth routes (/login, /register, /verify) onto hw-* pages arrive in Phase 5;
  // the /admin CRUD editor in Phase 7; events routes in Phase 11.
  { path: '**', redirectTo: '' },
];
