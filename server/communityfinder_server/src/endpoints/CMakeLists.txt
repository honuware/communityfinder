# The app endpoint registration table (web_app.cpp) — anchors the framework
# endpoints (via honuware's RegisterFrameworkEndpoints) plus the app's own endpoints
# (none yet in the skeleton; account endpoints arrive Phase 4, events Phase 10).
target_sources(communityfinder_core PRIVATE
    endpoint_registrations.h
    web_app.cpp
)

# App-side HTTP tests for the framework account endpoints in CF's build (Phase 4).
# register is app-side (its PostRegisterHook seam) + the open-registration policy is
# CF-specific; login/verify/me/remember/set_user_info/update_user_password are
# covered by the honuware component suite composed against CF's schema.
target_sources(communityfinder_test_cases PUBLIC
    register_test.cpp
    crud_test.cpp
    roles_permissions_test.cpp
)
