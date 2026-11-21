// Local environment: fully offline. The `local` configuration also swaps the
// access providers for the mocks (app-access-providers.mock.ts and the guide's,
// angular.json's fileReplacements; CM3 Phase 2 §14): @honuware/ui's in-memory
// mock access and CommunityFinder's own, so `ng serve` (which defaults to this
// configuration) runs with NO backend. This is the zero-ceremony
// UI-development path.
export const environment = {
  production: false,
  useMock: true,
  apiBase: '/api',
};
