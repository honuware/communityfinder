import { Component, computed, inject } from '@angular/core';
import { toSignal } from '@angular/core/rxjs-interop';
import { RouterLink } from '@angular/router';
import { MatCardModule } from '@angular/material/card';
import { MatIconModule } from '@angular/material/icon';
import { AuthData, AuthService } from '@honuware/ui/auth';
import { canManageGuide } from '../../core/guards/admin-area.guard';
import { canManageEvents } from '../../core/guards/manage-events.guard';

// Who a tile is for, matching its route's guard:
// - 'admins': Users, Roles and Site settings (AdminGuard);
// - 'eventsCurators': the Events pages (ManageEventsGuard);
// - 'everyone': Manage Data, open to all the door lets in.
type TileAudience = 'admins' | 'eventsCurators' | 'everyone';

interface AdminTile {
  title: string;
  description: string;
  icon: string;
  link: string;
  audience: TileAudience;
}

const TILES: readonly AdminTile[] = [
  {
    title: 'Roles & Permissions',
    description: 'Assign people to roles',
    icon: 'manage_accounts',
    // The bespoke role-assignment page (role → people list + person autocomplete).
    link: '/admin/roles',
    audience: 'admins',
  },
  {
    title: 'Users',
    description: 'Search, edit, and manage accounts',
    icon: 'people',
    link: '/admin/users',
    audience: 'admins',
  },
  {
    title: 'Events',
    description: 'Review, add, and edit events',
    icon: 'event',
    // The bespoke event editor + pending queue (EV2 §3.7).
    link: '/admin/events',
    audience: 'eventsCurators',
  },
  {
    title: 'Manage Data',
    description: 'Browse and edit every admin table',
    icon: 'storage',
    link: '/admin/tables',
    audience: 'everyone',
  },
  {
    title: 'Site settings',
    description: 'Settings for the whole site, like the upload limit',
    icon: 'settings',
    // CM3 Phase 2 §13.3 (OQ21).
    link: '/admin/settings',
    audience: 'admins',
  },
];

function canSee(tile: AdminTile, authData: AuthData): boolean {
  switch (tile.audience) {
    case 'admins':
      return authData.isAuth && authData.isAdmin;
    case 'eventsCurators':
      return canManageEvents(authData);
    case 'everyone':
      return true;
  }
}

// What Manage Data holds for this person: the server cuts the table list to
// what their permissions reach.
function manageDataDescription(authData: AuthData, fallback: string): string {
  if (authData.isAuth && authData.isAdmin) {
    return fallback;
  }
  const events = canManageEvents(authData);
  const guide = canManageGuide(authData);
  if (events && guide) {
    return 'Browse and edit the events and guide tables';
  }
  return guide ? 'Browse and edit the guide tables' : 'Browse and edit the events tables';
}

// The admin landing page (/admin): a grid of shortcut tiles into the admin
// functions; the editor pages' "back" (adminHome) returns here. Admins, events
// curators and guide curators get here (AdminAreaGuard), and each sees only
// the tiles they can use:
// - an events curator gets Events and Manage Data (CM3 Phase 2 §12.5);
// - a guide curator gets Manage Data (CM3 OQ24);
// Manage Data's description says which tables, since the server cuts the list.
@Component({
  selector: 'app-admin-dashboard',
  imports: [RouterLink, MatCardModule, MatIconModule],
  templateUrl: './admin-dashboard.html',
  styleUrl: './admin-dashboard.scss',
})
export class AdminDashboard {
  private authService = inject(AuthService);
  private readonly authData = toSignal(this.authService.authData$, {
    initialValue: this.authService.authData,
  });

  readonly tiles = computed(() => {
    const authData = this.authData();
    return TILES.filter((tile) => canSee(tile, authData)).map((tile) => ({
      ...tile,
      description:
        tile.link === '/admin/tables'
          ? manageDataDescription(authData, tile.description)
          : tile.description,
    }));
  });
}
