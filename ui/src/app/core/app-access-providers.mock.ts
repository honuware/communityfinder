import { Provider } from '@angular/core';
import { PhotoUrlBuilder } from '@honuware/ui/access';
import { provideHonuwareAccessMock } from '@honuware/ui/testing';
import { ADMIN_USERS_ACCESS } from './admin-users-access/admin-users-access';
import { AdminUsersMockAccess } from './admin-users-access/admin-users-access.mock';
import { COMMUNITY_ACCESS } from './community-access/community-access';
import { CommunityMockAccess } from './community-access/community-access.mock';
import { EVENT_SUBMISSIONS_ACCESS } from './event-submissions-access/event-submissions-access';
import { EventSubmissionsMockAccess } from './event-submissions-access/event-submissions-access.mock';
import { EVENTS_ADMIN_ACCESS } from './events-admin-access/events-admin-access';
import { EventsAdminMockAccess } from './events-admin-access/events-admin-access.mock';
import { DemoPhotoUrlBuilder, seedDemoPhotos } from './mock/demo-photo-art';
import { HONUWARE_MOCK_OPTIONS } from './mock/honuware-mock';

// The `local` build configuration's app-access-providers.ts (angular.json's
// fileReplacements; CM3 Phase 2 §14): the offline demo, so `ng serve` runs with
// no backend. Same exports as the real file.
//
// @honuware/ui's in-memory mock access, seeded with a demo admin and a small
// CRUD schema and rows, so the admin editor is navigable offline. There's no
// photo scaler offline either, so the events photos are drawn (CM3's
// placeholder art: shapes and color fields, never people), and the photo store
// is told which demo rows have one. Then CommunityFinder's own mocks.
export function provideAppAccess(): Provider[] {
  return [
    ...provideHonuwareAccessMock(HONUWARE_MOCK_OPTIONS, seedDemoPhotos),
    { provide: PhotoUrlBuilder, useClass: DemoPhotoUrlBuilder },
    { provide: COMMUNITY_ACCESS, useClass: CommunityMockAccess },
    { provide: ADMIN_USERS_ACCESS, useClass: AdminUsersMockAccess },
    { provide: EVENTS_ADMIN_ACCESS, useClass: EventsAdminMockAccess },
    { provide: EVENT_SUBMISSIONS_ACCESS, useClass: EventSubmissionsMockAccess },
  ];
}
