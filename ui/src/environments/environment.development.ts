// Development environment: real HTTP access, but unoptimized + source-mapped, and
// `/api` proxied to the running server (see proxy.conf.json). Used by
// `ng serve -c development` and `ng build --configuration development`.
export const environment = {
  production: false,
  useMock: false,
  apiBase: '/api',
};
