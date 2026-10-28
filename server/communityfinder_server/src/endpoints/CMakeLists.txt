# The app endpoint registration table (web_app.cpp) — anchors the framework
# endpoints (via honuware's RegisterFrameworkEndpoints) plus the app's own
# endpoints: the Phase 9 account admin pair and the EV1 events surface.
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
    get_event_series.h
    get_event_series.cpp
    get_upcoming_events.h
    get_upcoming_events.cpp
    register_app_services.h
    register_app_services.cpp
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
    get_event_series_test.cpp
    get_upcoming_events_test.cpp
    register_app_services_test.cpp
)
