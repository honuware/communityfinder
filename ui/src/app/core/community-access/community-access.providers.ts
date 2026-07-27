import { Provider } from '@angular/core';
import { COMMUNITY_ACCESS } from './community-access';
import { CommunityHttpAccess } from './community-access.http';
import { CommunityMockAccess } from './community-access.mock';

// Provide the CommunityAccess seam — the offline mock (local) or the HTTP impl.
export function provideCommunityAccess(useMock: boolean): Provider {
  return useMock
    ? { provide: COMMUNITY_ACCESS, useClass: CommunityMockAccess }
    : { provide: COMMUNITY_ACCESS, useClass: CommunityHttpAccess };
}
