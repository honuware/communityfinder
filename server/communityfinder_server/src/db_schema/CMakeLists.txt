# App schema composition: one header + source pair per app table (column constants
# + DDL), make_app_tables (registers them in FK order), and make_database_info
# (honuware MakeFrameworkTables ++ app tables).
target_sources(communityfinder_core PRIVATE
    categories.h
    categories.cpp
    freshness.h
    make_app_tables.h
    make_app_tables.cpp
    make_database_info.h
    make_database_info.cpp
    neighborhoods.h
    neighborhoods.cpp
    scene_tags.h
    scene_tags.cpp
    venues.h
    venues.cpp
)

target_sources(communityfinder_test_cases PUBLIC
    make_database_info_test.cpp
)
