// Default (production) environment. `ng build` (production) uses this file as-is;
// the development / local configurations file-replace it (see angular.json).
export const environment = {
  production: true,
  // Whether this build is the offline demo. Since CM3 Phase 2 §14 it wires
  // nothing: the `local` configuration's fileReplacements swap the access
  // providers for the mocks, so production carries no mock code. false here —
  // prod talks to the real API.
  useMock: false,
  // Base path for the honuware API. `/api` is the library default; in dev it is
  // proxied to the server (proxy.conf.json), in prod it is same-origin.
  apiBase: '/api',
};
