// Default (production) environment. `ng build` (production) uses this file as-is;
// the development / local configurations file-replace it (see angular.json).
export const environment = {
  production: true,
  // When true, the app wires @honuware/ui's in-memory mock access instead of the
  // real HTTP access (zero-backend UI development). false here — prod talks to the
  // real API.
  useMock: false,
  // Base path for the honuware API. `/api` is the library default; in dev it is
  // proxied to the server (proxy.conf.json), in prod it is same-origin.
  apiBase: '/api',
};
