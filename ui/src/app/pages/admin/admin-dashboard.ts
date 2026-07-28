import { Component } from '@angular/core';
import { RouterLink } from '@angular/router';
import { MatCardModule } from '@angular/material/card';
import { MatIconModule } from '@angular/material/icon';

interface AdminTile {
  title: string;
  description: string;
  icon: string;
  link: string;
}

// The admin landing page (/admin): a grid of shortcut tiles into the admin functions.
// Each tile links into the generic CRUD editor ("Manage Data", /admin/tables/…) for
// the relevant table; the editor pages' "back" (adminHome) returns here.
@Component({
  selector: 'app-admin-dashboard',
  imports: [RouterLink, MatCardModule, MatIconModule],
  templateUrl: './admin-dashboard.html',
  styleUrl: './admin-dashboard.scss',
})
export class AdminDashboard {
  readonly tiles: readonly AdminTile[] = [
    {
      title: 'Roles & Permissions',
      description: 'Assign people to roles',
      icon: 'manage_accounts',
      // The bespoke role-assignment page (role → people list + person autocomplete).
      link: '/admin/roles',
    },
    {
      title: 'Users',
      description: 'Search, edit, and manage accounts',
      icon: 'people',
      link: '/admin/users',
    },
    {
      title: 'Manage Data',
      description: 'Browse and edit every admin table',
      icon: 'storage',
      link: '/admin/tables',
    },
  ];
}
