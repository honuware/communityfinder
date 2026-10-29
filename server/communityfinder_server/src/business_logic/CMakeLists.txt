# App config seams: database name, secret keys/values (brand defaults), service
# account config, iCal brand config. These are the per-app values honuware's
# framework reads through its generic seams. Plus create_app_tables: the app
# halves of create_database (DDL list + seed), kept in the library so tests can
# run them.
target_sources(communityfinder_core PRIVATE
    app_database_config.h
    app_secret_keys.h
    app_secret_values.h
    app_secret_values.cpp
    app_service_account_config.h
    app_ical_config.h
    create_app_tables.h
    create_app_tables.cpp
)

target_sources(communityfinder_test_cases PUBLIC
    app_secret_values_test.cpp
    create_app_tables_test.cpp
)

add_subdirectory(events)
add_subdirectory(guide)
add_subdirectory(migration)
