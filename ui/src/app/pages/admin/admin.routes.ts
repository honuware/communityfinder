import { Routes } from '@angular/router';
import {
  TableViewPageComponent,
  TableEditPageComponent,
  TableNewPageComponent,
} from '@honuware/ui/crud';
import { AdminPage } from './admin';

// Mounted at /admin (see app.routes, behind AuthGuard + AdminGuard). The child paths
// match @honuware/ui/crud's DEFAULT_CRUD_EDITOR_ROUTES (basePath '/admin/tables',
// adminHome '/admin'), so the editor pages' internal navigation (new/edit/delete/
// pagination/back) resolves without a CRUD_EDITOR_ROUTES override. The view route
// carries pageSize + pageOffset as PATH segments, not query params.
const routes: Routes = [
  {
    path: '',
    component: AdminPage,
    children: [
      { path: 'tables/:tableName/view/:pageSize/:pageOffset', component: TableViewPageComponent },
      { path: 'tables/:tableName/edit/:id', component: TableEditPageComponent },
      { path: 'tables/:tableName/new', component: TableNewPageComponent },
    ],
  },
];

export default routes;
