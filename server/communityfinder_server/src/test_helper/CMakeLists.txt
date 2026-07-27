# The test_helper REPL/dashboard (dev tool). Everything folds into the exe — a
# LEAF executable (not a DAG component), so it is exempt from
# honuware_validate_layering (which only governs HONUWARE_COMPONENTS).
#
# Standing convention: each later phase adds simulation commands under commands/
# for its hard-to-engineer scenarios (Phase 4: verified users / expired sessions;
# Phase 10: past/expired events; …).
add_executable(communityfinder_test_helper
    "main.cpp"
    command_registry.h
    command_registry.cpp
    command_runner.h
    command_runner.cpp
    console_colors.h
    commands/utility_commands.h
    commands/utility_commands.cpp
    repl/repl.h
    repl/repl.cpp
    dashboard/dashboard.h
    dashboard/dashboard.cpp
)

# The test_helper root must be on the include path — files in commands/, repl/,
# dashboard/ include command_runner.h / console_colors.h / command_registry.h by
# test_helper-relative path.
target_include_directories(communityfinder_test_helper PRIVATE ${CMAKE_CURRENT_LIST_DIR})

# core = the app lib (+ honuware foundation/data/services/platform PUBLIC). honuware_testing
# supplies MakeTestMailHelper (the --nosend_real_email path) and is NOT pulled in by
# core transitively, so link it directly. Plus the TUI (ftxui/replxx), flags (absl),
# and pqxx.
target_link_libraries(communityfinder_test_helper
    ${ABSL_LIB}
    ${PQXX_LIB}
    ${FTXUI_LIB}
    ${REPLXX_LIB}
    communityfinder_core
    honuware_testing
)

if(WIN32)
    set_target_properties(communityfinder_test_helper PROPERTIES
        LINK_FLAGS "/PDB:communityfinder_test_helper.pdb")
endif()
