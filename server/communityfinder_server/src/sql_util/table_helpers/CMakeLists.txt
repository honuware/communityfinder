# App table helpers: one class per app table (typed CRUD over db_schema), the layer
# between business_logic and db_schema. honuware's framework helpers live under
# the same sql_util/table_helpers/ include prefix in the platform component.
target_sources(communityfinder_core PRIVATE
    bigint_array_literal.h
    bigint_array_literal.cpp
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
    bigint_array_literal_test.cpp
    categories_test.cpp
    event_category_assignments_test.cpp
    event_scene_tag_assignments_test.cpp
    event_sources_test.cpp
    events_test.cpp
    neighborhoods_test.cpp
    scene_tags_test.cpp
    series_category_assignments_test.cpp
    series_scene_tag_assignments_test.cpp
    series_test.cpp
    venues_test.cpp
)
