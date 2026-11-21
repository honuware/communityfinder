import { Routes } from '@angular/router';
import { AdminGuard } from '@honuware/ui/auth';
import { ManageEventsGuard } from '../../core/guards/manage-events.guard';
import {
  TableViewPageComponent,
  TableEditPageComponent,
  TableNewPageComponent,
} from '@honuware/ui/crud';
import { AdminDashboard } from './admin-dashboard';
import { AdminPage } from './admin';
import { ManageEventsPage } from './manage-events';
import { ManageRolesPage } from './manage-roles';
import { ManageUsersPage } from './manage-users';
import { SiteSettingsPage } from './site-settings';
import { SubmissionSettingsPage } from './submission-settings';

// Mounted at /admin, behind AuthGuard + AdminAreaGuard: admins, events
// curators (CM3 Phase 2 §12.5) and guide curators (CM3 OQ24).
//   /admin        → the dashboard landing (also @honuware/ui/crud's default
//                   adminHome, so an editor page's "back" returns here). It
//                   shows each person only the tiles they can use.
//   /admin/tables → "Manage Data": the table-picker shell hosting the generic CRUD
//                   editor pages. The child paths sit directly under 'tables' so the
//                   full URLs match the library's default basePath '/admin/tables'
//                   ('/admin/tables/:tableName/view/:pageSize/:pageOffset', …) — no
//                   CRUD_EDITOR_ROUTES override needed. Open to everyone the
//                   door lets in: get_db_schema's root_tables are cut to what
//                   the caller's permissions reach, so an events curator's
//                   picker lists the events tables and a guide curator's the
//                   guide tables (CommunityFinderCrudTest.EachCuratorsTableList…).
//   The Events pages: events curators and admins (ManageEventsGuard).
//   Users, Roles and Site settings stay admin-only (AdminGuard).
const routes: Routes = [
  { path: '', component: AdminDashboard },
  // Bespoke "assign people to a role" page (the Roles & Permissions dashboard tile).
  { path: 'roles', component: ManageRolesPage, canActivate: [AdminGuard] },
  // Bespoke Users page — search / edit / delete accounts (the Users dashboard tile).
  { path: 'users', component: ManageUsersPage, canActivate: [AdminGuard] },
  // Site settings (CM3 Phase 2 §13.3, OQ21): the upload limit, for the whole
  // site, so admins only (the server checks admin_portal too).
  { path: 'settings', component: SiteSettingsPage, canActivate: [AdminGuard] },
  // Bespoke event editor + review queue (the Events dashboard tile; EV2 §3.7).
  // Admins and events curators; the server checks manage_events on every call.
  { path: 'events', component: ManageEventsPage, canActivate: [ManageEventsGuard] },
  // The Event settings page (EV3 §5.6, EV4 §10.4): the submission rate limits
  // and pause switch, and the past-event settings. Ahead of any future
  // /admin/events/:id route.
  {
    path: 'events/settings',
    component: SubmissionSettingsPage,
    canActivate: [ManageEventsGuard],
  },
  {
    path: 'tables',
    component: AdminPage,
    children: [
      { path: ':tableName/view/:pageSize/:pageOffset', component: TableViewPageComponent },
      { path: ':tableName/edit/:id', component: TableEditPageComponent },
      { path: ':tableName/new', component: TableNewPageComponent },
    ],
  },
];

export default routes;
