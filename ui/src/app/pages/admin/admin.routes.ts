import { Routes } from '@angular/router';
import {
  TableViewPageComponent,
  TableEditPageComponent,
  TableNewPageComponent,
} from '@honuware/ui/crud';
import { AdminDashboard } from './admin-dashboard';
import { AdminPage } from './admin';
import { ManageRolesPage } from './manage-roles';
import { ManageUsersPage } from './manage-users';

// Mounted at /admin (behind AuthGuard + AdminGuard).
//   /admin        → the dashboard landing (also @honuware/ui/crud's default
//                   adminHome, so an editor page's "back" returns here).
//   /admin/tables → "Manage Data": the table-picker shell hosting the generic CRUD
//                   editor pages. The child paths sit directly under 'tables' so the
//                   full URLs match the library's default basePath '/admin/tables'
//                   ('/admin/tables/:tableName/view/:pageSize/:pageOffset', …) — no
//                   CRUD_EDITOR_ROUTES override needed.
const routes: Routes = [
  { path: '', component: AdminDashboard },
  // Bespoke "assign people to a role" page (the Roles & Permissions dashboard tile).
  { path: 'roles', component: ManageRolesPage },
  // Bespoke Users page — search / edit / delete accounts (the Users dashboard tile).
  { path: 'users', component: ManageUsersPage },
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
