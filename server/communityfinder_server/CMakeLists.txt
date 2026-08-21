cmake_minimum_required(VERSION 3.24)
project(communityfinder)

set(CMAKE_CXX_STANDARD 20)

# CMP0167 (FindBoost removal) only exists in CMake 3.30+, and cmake_policy(SET) on
# an unknown policy is a hard ERROR. Guard it so configure works on the 3.25 that
# Debian bookworm (the gcc:14.2.0 docker/CI builder) installs. On <3.30 the policy
# doesn't exist and FindBoost is the only lookup path anyway.
if(POLICY CMP0167)
    cmake_policy(SET CMP0167 OLD)
endif()

# NOTE: this block is intentionally INERT (carried over verbatim from knottyyoga).
# `!WIN32` is an undefined CMake variable, so the branch is ALWAYS false and never
# runs — the compiler, C++ standard, and flags all come from the Conan toolchain +
# CMAKE_CXX_STANDARD above. Do NOT "fix" this to `if(NOT WIN32)`: that activates the
# dead `set(CMAKE_C_COMPILER "/usr/bin/gcc")`, which points at a path that does not
# exist in the gcc:14.2.0 image (gcc lives under /usr/local), breaking honuware's
# project() during FetchContent with "not a full path to an existing compiler tool".
if(!WIN32)
    set(CMAKE_C_COMPILER "/usr/bin/gcc")
    set(CMAKE_CXX_COMPILER "/usr/bin/g++")
    add_definitions(-D_GLIBCXX_USE_CXX11_ABI=1)
    add_compile_options(-std=c++20)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++20")
endif()

if(WIN32)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /std:c++20 /EHsc /MDd /Zi /Od /wd4267 /wd4244 /wd4996 /D_DEBUG /DDEBUG /DEBUG /D_WIN32_WINNT=0x0601")
    set(PDB_NAME "${LINK_FLAGS} /DEBUG /PDB")
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS 1)

# Reject any target_link_libraries item that is not an actual CMake target, so a
# bare/typo'd library name fails configuration instead of silently resolving to a
# system library. Must be set BEFORE any target is created.
set(CMAKE_LINK_LIBRARIES_ONLY_TARGETS ON)

# The app-superset component layer DAG + the honuware_add_component /
# honuware_validate_layering helpers that make it a build-time invariant.
include(${CMAKE_CURRENT_LIST_DIR}/cmake/honuware_layering.cmake)

# Static libpq (pulled transitively by libpqxx) auto-detects GSSAPI from system
# headers; the Conan recipe exposes no option to declare it, so link it ourselves.
# Appended at the very end of the link line (after all Conan static archives) —
# GNU ld processes left-to-right and would discard gssapi_krb5 symbols otherwise.
if(NOT WIN32)
    set(CMAKE_CXX_STANDARD_LIBRARIES "${CMAKE_CXX_STANDARD_LIBRARIES} -lgssapi_krb5")
endif()

# The APP targets that sit above the honuware stack:
#   communityfinder_core       = THE APP LIBRARY (app config seams, schema/migration
#                                composition, app endpoints + the web_app.cpp routing
#                                table). Links every honuware component it uses PUBLIC
#                                (edges in src/CMakeLists.txt). Top of the DAG.
#   communityfinder_test_cases = the APP test-case bag (app *_test.cpp only). The
#                                component test cases build into honuware_tests
#                                (honuware sets HONUWARE_TESTS_TARGET in its own
#                                scope); the app test exe links honuware_tests to
#                                keep running them (test/CMakeLists.txt).
# Both are declared through the app-superset DAG; honuware_validate_layering() at the
# end enforces it. (database_helper / test_helper / scheduler executables are added
# in later phases.)
honuware_add_component(NAME communityfinder_core)
honuware_add_component(NAME communityfinder_test_cases)

# The conan_provider.cmake language-service provider is DEFERRED and won't run until
# something is requested, so ConanLibImports.cmake won't exist yet. A benign
# find_package for a piece of Boost forces it.
find_package(Boost 1.83 REQUIRED COMPONENTS filesystem)

# Conan2-generated find_package calls + ${..._LIB} vars (from conanfile init()).
include(ConanLibImports.cmake)

# Pull the honuware components from the shared repo, pinned to a SHA. Placed AFTER
# Conan so honuware's component/link declarations resolve ${CROW_LIB}/${PQXX_LIB}/
# ${GTEST_LIB}/... from OUR ConanLibImports (our conanfile is a superset of
# honuware's). honuware's dual-mode top-level (PROJECT_IS_TOP_LEVEL==OFF) declares
# honuware_foundation/_data/_services/_square/_scheduler/_platform/_testing/_tests +
# their edges + include roots, and sets HONUWARE_TESTS_TARGET=honuware_tests in its
# own scope. Must run BEFORE add_subdirectory(src)/add_subdirectory(test).
#
# Local cross-repo co-development: point at a working checkout instead of the pinned
# SHA with -DFETCHCONTENT_SOURCE_DIR_HONUWARE=<path-to-server_components>.
include(FetchContent)
FetchContent_Declare(
    honuware
    GIT_REPOSITORY https://github.com/honuware/server_components.git
    GIT_TAG        17a818d928cbdfa0b7f1f07648c8f137adc67813
)
FetchContent_MakeAvailable(honuware)

target_include_directories(communityfinder_test_cases PUBLIC ${CMAKE_CURRENT_LIST_DIR})

# Our app tree (src) + the app test exe (test) — both link the honuware_* targets
# FetchContent created above.
add_subdirectory(src)
add_subdirectory(test)

# Now that every component's link edges are declared, enforce the layer DAG against
# the fully-resolved link graph.
honuware_validate_layering()

set(
    SOURCE_FILES
    src/main.cpp
)

add_executable(communityfinder_server ${SOURCE_FILES})

if(WIN32)
    set_target_properties(communityfinder_server PROPERTIES LINK_FLAGS "/PDB:communityfinder_server.pdb")
endif()

target_link_libraries(communityfinder_server PUBLIC communityfinder_core)

# Copy the CA certificate bundle to the build output for SSL verification (the HTTP
# client resolves it at the CWD-relative certs/cacert.pem).
add_custom_command(TARGET communityfinder_server POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/certs"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/certs/cacert.pem"
        "${CMAKE_BINARY_DIR}/certs/cacert.pem"
    COMMENT "Copying CA certificate bundle to build output"
)
