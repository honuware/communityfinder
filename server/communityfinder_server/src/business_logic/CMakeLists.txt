# App config seams: database name, secret keys/values (brand defaults), service
# account config, iCal brand config. These are the per-app values honuware's
# framework reads through its generic seams.
target_sources(communityfinder_core PRIVATE
    app_database_config.h
    app_secret_keys.h
    app_secret_values.h
    app_secret_values.cpp
    app_service_account_config.h
    app_ical_config.h
)

target_sources(communityfinder_test_cases PUBLIC
    app_secret_values_test.cpp
)

add_subdirectory(migration)
