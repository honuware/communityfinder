# The app endpoint registration table (web_app.cpp) — anchors the framework
# endpoints (via honuware's RegisterFrameworkEndpoints) plus the app's own
# endpoints: the Phase 9 account admin pair, the EV1 events surface and the EV2
# calendar (ICS) endpoints.
target_sources(communityfinder_core PRIVATE
    endpoint_registrations.h
    web_app.cpp
    admin_create_user.h
    admin_create_user.cpp
    admin_ingest_events.h
    admin_ingest_events.cpp
    admin_reset_password.h
    admin_reset_password.cpp
    admin_review_event.h
    admin_review_event.cpp
    events_json.h
    events_json.cpp
    get_event_detail.h
    get_event_detail.cpp
    get_event_ics.h
    get_event_ics.cpp
    get_event_series.h
    get_event_series.cpp
    get_events_ics_feed.h
    get_events_ics_feed.cpp
    get_upcoming_events.h
    get_upcoming_events.cpp
    register_app_services.h
    register_app_services.cpp
)

# Stream B — the guide's public read endpoints (GD2 §4): one TU per resource,
# its list and detail routes together, sharing guide_json.
target_sources(communityfinder_core PRIVATE
    guide_json.h
    guide_json.cpp
    get_guide_graveyard.h
    get_guide_graveyard.cpp
    get_guide_neighborhoods.h
    get_guide_neighborhoods.cpp
    get_guide_organizations.h
    get_guide_organizations.cpp
    get_guide_pages.h
    get_guide_pages.cpp
    get_guide_places.h
    get_guide_places.cpp
    get_guide_scenes.h
    get_guide_scenes.cpp
    get_guide_services.h
    get_guide_services.cpp
    get_guide_venues.h
    get_guide_venues.cpp
)

target_sources(communityfinder_test_cases PUBLIC
    guide_endpoint_test_support.h
    guide_endpoint_test_support.cpp
    guide_json_test.cpp
    get_guide_graveyard_test.cpp
    get_guide_neighborhoods_test.cpp
    get_guide_organizations_test.cpp
    get_guide_pages_test.cpp
    get_guide_places_test.cpp
    get_guide_scenes_test.cpp
    get_guide_services_test.cpp
    get_guide_venues_test.cpp
)

# App-side HTTP tests for the framework account endpoints in CF's build (Phase 4).
# register is app-side (its PostRegisterHook seam) + the open-registration policy is
# CF-specific; login/verify/me/remember/set_user_info/update_user_password are
# covered by the honuware component suite composed against CF's schema. Plus the
# EV1 events endpoints (one test file per endpoint, sharing
# events_endpoint_test_support) and the photo seam.
target_sources(communityfinder_test_cases PUBLIC
    register_test.cpp
    crud_test.cpp
    roles_permissions_test.cpp
    admin_users_test.cpp
    events_endpoint_test_support.h
    events_endpoint_test_support.cpp
    admin_ingest_events_test.cpp
    admin_review_event_test.cpp
    events_json_test.cpp
    get_event_detail_test.cpp
    get_event_ics_test.cpp
    get_event_series_test.cpp
    get_events_ics_feed_test.cpp
    get_upcoming_events_test.cpp
    register_app_services_test.cpp
)
