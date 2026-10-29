#pragma once

class WebApp;

namespace Endpoints {

// The app's configuration of honuware's framework-endpoint seams
// (WebApp::SetService). main.cpp calls this once at startup, and the tests call
// it on their test WebApp, so both exercise the same configuration.
//
// Today that is one seam: PublicPhotoTables = {events, venues} (EV1 §5.6) plus
// the guide's {organizations, places, guide_pages} (GD2 §4.10), the tables whose
// scaled photos a signed-out visitor may read through /api/get_scaled_photo.
// With nothing registered, every photo read needs a login.
void RegisterAppServices(WebApp& webApp);

}  // namespace Endpoints
