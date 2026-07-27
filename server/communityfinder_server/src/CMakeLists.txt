# The app targets keep the src/ root on their include path so app code includes app
# headers by the src-relative prefix (business_logic/..., db_schema/...,
# endpoints/...). The honuware components own their own include roots (set in the
# FetchContent'd honuware tree), so an app -> app-only path resolves here while the
# honuware framework headers resolve there.
target_include_directories(communityfinder_core PUBLIC ${CMAKE_CURRENT_LIST_DIR})
target_include_directories(communityfinder_test_cases PUBLIC ${CMAKE_CURRENT_LIST_DIR})

add_subdirectory(business_logic)
add_subdirectory(db_schema)
add_subdirectory(endpoints)
add_subdirectory(database_helper)
add_subdirectory(test_helper)

# The app library links the honuware components it uses PUBLIC, so every consumer
# (server / tests) transparently gets foundation/data/services/platform sources,
# headers, and Conan link deps. honuware_square is deliberately NOT linked
# (CommunityFinder has no payments yet); add it here if/when a Square client lands.
# Link order is cosmetic — CMake derives it from the acyclic inter-target graph.
target_link_libraries(communityfinder_core PUBLIC
    honuware_foundation
    honuware_data
    honuware_services
    honuware_platform)
