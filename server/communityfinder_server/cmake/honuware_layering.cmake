# honuware_layering.cmake — component layer enforcement (adapted from honuware for
# the CommunityFinder app-superset DAG).
#
# Makes the component layer diagram a build-time invariant: a target may only link
# targets BELOW it. CMake otherwise permits static-library cycles and never
# complains about an upward edge, so without this the layering is only a convention.
#
#   honuware_add_component(NAME <c> [DEPENDS <lower components...>])
#       Creates the component's library target; validates+links any DEPENDS.
#   honuware_validate_layering()
#       Called ONCE after every add_subdirectory: re-checks each component's ACTUAL
#       resolved LINK_LIBRARIES against the DAG, catching an illegal edge no matter
#       which CMakeLists declared it.
#
# The DAG below is the single source of truth for the layer diagram; changing an
# allow-list is the deliberate act of changing the architecture.

cmake_policy(SET CMP0057 NEW)

# --- The layer DAG -----------------------------------------------------------
# Layer order (low -> high): foundation -> data -> services -> platform ->
#   communityfinder_core -> testing -> test_cases. honuware_square is a side branch
#   on foundation only; CommunityFinder's core deliberately does NOT link it (no
#   payments yet), but honuware_testing does (a Square service double), so it stays
#   in testing's allow-list. honuware_scheduler is the generic scheduler engine
#   (foundation only), consumed by communityfinder_test_cases (the job catalog,
#   Phase 12) + the communityfinder_helper executable.
set(HONUWARE_ALLOW_honuware_foundation        "")
set(HONUWARE_ALLOW_honuware_data              honuware_foundation)
set(HONUWARE_ALLOW_honuware_services          honuware_data honuware_foundation)
set(HONUWARE_ALLOW_honuware_square            honuware_foundation)
set(HONUWARE_ALLOW_honuware_scheduler         honuware_foundation)
set(HONUWARE_ALLOW_honuware_platform          honuware_services honuware_data honuware_foundation)
set(HONUWARE_ALLOW_communityfinder_core       honuware_platform honuware_services honuware_data honuware_foundation)
set(HONUWARE_ALLOW_honuware_testing           communityfinder_core honuware_platform honuware_services honuware_data honuware_square honuware_foundation)
set(HONUWARE_ALLOW_communityfinder_test_cases honuware_testing communityfinder_core honuware_scheduler honuware_platform honuware_services honuware_data honuware_square honuware_foundation)

# The complete set of DAG-governed targets. Anything NOT here (Conan imported
# targets, honuware_tests, the leaf executables/helper libs) is ignored.
set(HONUWARE_COMPONENTS
    honuware_foundation
    honuware_data
    honuware_services
    honuware_square
    honuware_scheduler
    honuware_platform
    communityfinder_core
    honuware_testing
    communityfinder_test_cases)

# --- Internal: check a target's declared/resolved deps against its allow-list --
function(_honuware_check_component_deps name deps)
    if(NOT DEFINED HONUWARE_ALLOW_${name})
        message(FATAL_ERROR
            "honuware layering: '${name}' has no allow-list. Add it to "
            "HONUWARE_COMPONENTS with an explicit HONUWARE_ALLOW_${name} in "
            "cmake/honuware_layering.cmake.")
    endif()
    set(allowed ${HONUWARE_ALLOW_${name}})
    foreach(dep IN LISTS deps)
        if(dep IN_LIST HONUWARE_COMPONENTS)
            if(NOT dep IN_LIST allowed)
                message(FATAL_ERROR
                    "\n==== honuware layering violation ====\n"
                    "  '${name}' may not depend on '${dep}'.\n"
                    "  '${name}' is only permitted to link: ${allowed}\n"
                    "  This edge would break the component layer DAG. Either remove "
                    "the dependency, or — if the layer diagram is genuinely changing "
                    "— update HONUWARE_ALLOW_${name} in cmake/honuware_layering.cmake.\n"
                    "=====================================")
            endif()
        endif()
    endforeach()
endfunction()

# --- Declare a layered component (create target; validate+link DEPENDS) -------
function(honuware_add_component)
    cmake_parse_arguments(HAC "" "NAME" "DEPENDS" ${ARGN})
    if(NOT HAC_NAME)
        message(FATAL_ERROR "honuware_add_component: NAME <component> is required")
    endif()
    if(NOT DEFINED HONUWARE_ALLOW_${HAC_NAME})
        message(FATAL_ERROR
            "honuware_add_component: '${HAC_NAME}' is not a declared honuware "
            "layered component. Add it to HONUWARE_COMPONENTS and give it an "
            "explicit allow-list in cmake/honuware_layering.cmake first.")
    endif()

    add_library(${HAC_NAME} "")

    if(HAC_DEPENDS)
        _honuware_check_component_deps(${HAC_NAME} "${HAC_DEPENDS}")
        target_link_libraries(${HAC_NAME} PUBLIC ${HAC_DEPENDS})
    endif()
endfunction()

# --- Validate the fully-resolved graph (run last) -----------------------------
function(honuware_validate_layering)
    foreach(component IN LISTS HONUWARE_COMPONENTS)
        if(NOT TARGET ${component})
            continue()
        endif()
        get_target_property(deps ${component} LINK_LIBRARIES)
        if(NOT deps)
            continue()
        endif()
        _honuware_check_component_deps(${component} "${deps}")
    endforeach()
    message(STATUS
        "honuware: component layer DAG validated across "
        "[${HONUWARE_COMPONENTS}]")
endfunction()
