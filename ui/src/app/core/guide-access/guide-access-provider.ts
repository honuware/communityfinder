import { Provider } from '@angular/core';
import { GUIDE_ACCESS } from './guide-access';
import { GuideHttpAccess } from './guide-access.http';

// The guide's data access, as its lazy routes provide it: the real server's
// (CM3 Phase 2 §14, OQ16). The `local` build configuration replaces this file
// with guide-access-provider.mock.ts, so the guide's demo data is in that
// build only. Its own file, not app-access-providers.ts, so the guide's access
// stays in the guide's chunk.
export function provideGuideAppAccess(): Provider {
  return { provide: GUIDE_ACCESS, useClass: GuideHttpAccess };
}
