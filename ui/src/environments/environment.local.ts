// Local environment: fully offline. Wires @honuware/ui's in-memory mock access +
// CommunityAccess mock, so `ng serve` (which defaults to this configuration) runs
// with NO backend. This is the zero-ceremony UI-development path.
export const environment = {
  production: false,
  useMock: true,
  apiBase: '/api',
};
