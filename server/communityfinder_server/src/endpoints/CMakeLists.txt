# The app endpoint registration table (web_app.cpp) — anchors the framework
# endpoints (via honuware's RegisterFrameworkEndpoints) plus the app's own endpoints
# (none yet in the skeleton; account endpoints arrive Phase 4, events Phase 10).
target_sources(communityfinder_core PRIVATE
    endpoint_registrations.h
    web_app.cpp
)
