# App schema composition: one header + source pair per app table (column constants
# + DDL), make_app_tables (registers them in FK order), and make_database_info
# (honuware MakeFrameworkTables ++ app tables).
target_sources(communityfinder_core PRIVATE
    categories.h
    categories.cpp
    event_category_assignments.h
    event_category_assignments.cpp
    event_scene_tag_assignments.h
    event_scene_tag_assignments.cpp
    event_sources.h
    event_sources.cpp
    events.h
    events.cpp
    freshness.h
    guide_links.h
    guide_links.cpp
    guide_page_scene_tag_assignments.h
    guide_page_scene_tag_assignments.cpp
    guide_pages.h
    guide_pages.cpp
    make_app_tables.h
    make_app_tables.cpp
    make_database_info.h
    make_database_info.cpp
    neighborhoods.h
    neighborhoods.cpp
    organization_scene_tag_assignments.h
    organization_scene_tag_assignments.cpp
    organizations.h
    organizations.cpp
    place_scene_tag_assignments.h
    place_scene_tag_assignments.cpp
    places.h
    places.cpp
    scene_tags.h
    scene_tags.cpp
    series.h
    series.cpp
    series_category_assignments.h
    series_category_assignments.cpp
    series_scene_tag_assignments.h
    series_scene_tag_assignments.cpp
    service_listings.h
    service_listings.cpp
    venue_profile_scene_tag_assignments.h
    venue_profile_scene_tag_assignments.cpp
    venue_profiles.h
    venue_profiles.cpp
    venues.h
    venues.cpp
)

target_sources(communityfinder_test_cases PUBLIC
    make_database_info_test.cpp
)
