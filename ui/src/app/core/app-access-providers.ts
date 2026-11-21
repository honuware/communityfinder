import { Provider } from '@angular/core';
import { provideHonuwareAccess } from '@honuware/ui/access';
import { ADMIN_USERS_ACCESS } from './admin-users-access/admin-users-access';
import { AdminUsersHttpAccess } from './admin-users-access/admin-users-access.http';
import { COMMUNITY_ACCESS } from './community-access/community-access';
import { CommunityHttpAccess } from './community-access/community-access.http';
import { EVENT_SUBMISSIONS_ACCESS } from './event-submissions-access/event-submissions-access';
import { EventSubmissionsHttpAccess } from './event-submissions-access/event-submissions-access.http';
import { EVENTS_ADMIN_ACCESS } from './events-admin-access/events-admin-access';
import { EventsAdminHttpAccess } from './events-admin-access/events-admin-access.http';

// The app's data access, as app.config.ts provides it: the real server's, and
// nothing else (CM3 Phase 2 §14, OQ16). The `local` build configuration
// replaces this file with app-access-providers.mock.ts (angular.json's
// fileReplacements), so the offline demo's code and data are in that build
// only, never in production's. Keep the two files' exports the same.
//
//   - @honuware/ui's Crud / Auth / Photo access;
//   - CommunityFinder's own: the public site (health, site_info, the events
//     pages, photo-removal requests), the admin's users, the curator's
//     events, and the submitter's side.
//
// The guide's seam isn't here: its lazy routes provide it, from
// guide-access/guide-access-provider.ts, swapped the same way (GD3 OQ7 (d)).
// Specs don't use either file: they import the mocks they want directly.
export function provideAppAccess(): Provider[] {
  return [
    ...provideHonuwareAccess(),
    { provide: COMMUNITY_ACCESS, useClass: CommunityHttpAccess },
    { provide: ADMIN_USERS_ACCESS, useClass: AdminUsersHttpAccess },
    { provide: EVENTS_ADMIN_ACCESS, useClass: EventsAdminHttpAccess },
    { provide: EVENT_SUBMISSIONS_ACCESS, useClass: EventSubmissionsHttpAccess },
  ];
}
