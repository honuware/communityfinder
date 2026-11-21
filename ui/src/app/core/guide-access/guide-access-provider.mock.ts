import { Provider } from '@angular/core';
import { GUIDE_ACCESS } from './guide-access';
import { GuideMockAccess } from './guide-access.mock';

// The `local` build configuration's guide-access-provider.ts (angular.json's
// fileReplacements; CM3 Phase 2 §14): the guide's offline demo data. Same
// export as the real file.
export function provideGuideAppAccess(): Provider {
  return { provide: GUIDE_ACCESS, useClass: GuideMockAccess };
}
