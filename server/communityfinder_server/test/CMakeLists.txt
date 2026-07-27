# The test executable compiles only its own main.cpp; every test case lives in a
# linked library (communityfinder_test_cases for app tests, honuware_tests for the
# component tests).
add_executable(communityfinder_tests "src/main.cpp")

if(WIN32)
    set_target_properties(communityfinder_tests PROPERTIES LINK_FLAGS "/PDB:communityfinder_tests.pdb")
endif()

# The app test-case bag uses the honuware harness pervasively.
target_link_libraries(communityfinder_test_cases PUBLIC honuware_testing)

# The app test-case bag references the app library pervasively (every test calls
# into app code), so declare the edge explicitly — CMake then orders core AFTER
# test_cases on the GNU ld link line (static-lib symbol resolution needs
# dependencies to follow dependents).
target_link_libraries(communityfinder_test_cases PUBLIC communityfinder_core)

# The exe links the app test bag + app core + the harness + the honuware COMPONENT
# test bag (honuware_tests — the component *_test.cpp compile into it, and we link
# it to keep RUNNING them against the app's composed schema). CMake derives the
# static-lib order from the dependency graph.
target_link_libraries(communityfinder_tests
    ${GTEST_LIB} communityfinder_core honuware_testing communityfinder_test_cases honuware_tests)
