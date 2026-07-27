# App schema composition: make_app_tables (empty — no app tables yet) +
# make_database_info (honuware MakeFrameworkTables ++ app tables). App table DDL +
# column constants land here from Phase 10 (the events domain).
target_sources(communityfinder_core PRIVATE
    make_app_tables.h
    make_app_tables.cpp
    make_database_info.h
    make_database_info.cpp
)

target_sources(communityfinder_test_cases PUBLIC
    make_database_info_test.cpp
)
