#pragma once

namespace Endpoints {

// Anchor for the app endpoint registration table in web_app.cpp.
//
// web_app.cpp holds an `anchor = &Endpoints::X` store for every app endpoint (none
// yet in the skeleton) and calls honuware's RegisterFrameworkEndpoints(). Those
// references force the linker to retain each endpoint's translation unit so its
// self-registering RoutingBase runs (otherwise a static-library object is dropped
// and the route silently 404s at runtime — worse at -O2, see web_app.cpp).
//
// Every WebApp host (main.cpp for the server, the test main) calls
// RegisterAllEndpoints() once before RoutingBase::AddRoutes(). That call is an
// external reference into web_app.cpp, forcing its object — and every endpoint TU
// it anchors — into the final link.
void RegisterAllEndpoints();

}  // namespace Endpoints
