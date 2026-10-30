import { Provider } from '@angular/core';
import { GUIDE_ACCESS } from './guide-access';
import { GuideHttpAccess } from './guide-access.http';
import { GuideMockAccess } from './guide-access.mock';

// Provide the GuideAccess seam — the offline mock (local) or the HTTP impl.
export function provideGuideAccess(useMock: boolean): Provider {
  return useMock
    ? { provide: GUIDE_ACCESS, useClass: GuideMockAccess }
    : { provide: GUIDE_ACCESS, useClass: GuideHttpAccess };
}
