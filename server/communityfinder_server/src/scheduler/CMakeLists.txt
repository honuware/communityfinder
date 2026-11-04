# EV4 §6 — communityfinder_helper: CF's job catalog over honuware's scheduler
# engine (honuware_scheduler, a foundation-only side branch).

# The catalog compiles into the app test bag too, so its test can link it; the
# bag links the engine for the ScheduledJob type (the layering allow-list
# already permits communityfinder_test_cases -> honuware_scheduler).
target_sources(communityfinder_test_cases PUBLIC
    communityfinder_job_catalog.h
    communityfinder_job_catalog.cpp
    communityfinder_job_catalog_test.cpp
)
target_link_libraries(communityfinder_test_cases PUBLIC honuware_scheduler)

# The helper is a leaf executable, outside the layer DAG (like the server and
# database_helper). communityfinder_core supplies the service-account constant
# and logging/env/HTTP through the honuware components it links.
add_executable(communityfinder_helper
    "main.cpp"
    "communityfinder_job_catalog.h"
    "communityfinder_job_catalog.cpp"
)

target_link_libraries(communityfinder_helper
    ${ABSL_LIB}
    communityfinder_core
    honuware_scheduler)

if(WIN32)
    set_target_properties(communityfinder_helper PROPERTIES
        LINK_FLAGS "/PDB:communityfinder_helper.pdb")
endif()
