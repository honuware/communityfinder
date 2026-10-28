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
    make_app_tables.h
    make_app_tables.cpp
    make_database_info.h
    make_database_info.cpp
    neighborhoods.h
    neighborhoods.cpp
    scene_tags.h
    scene_tags.cpp
    series.h
    series.cpp
    series_category_assignments.h
    series_category_assignments.cpp
    series_scene_tag_assignments.h
    series_scene_tag_assignments.cpp
    venues.h
    venues.cpp
)

target_sources(communityfinder_test_cases PUBLIC
    make_database_info_test.cpp
)
