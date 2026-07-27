#!/usr/bin/env bash
#
# Build and run communityfinder_tests on Linux, inside the container from
# docker/load_container.cmd.
#
#   ./docker/build_and_test.sh                        # full suite
#   ./docker/build_and_test.sh --gtest_filter=Event*   # args go to the runner
#
# The exe links BOTH test bags, so this runs the app's own tests AND the honuware
# component tests composed against the app's schema:
#   communityfinder_test_cases  app *_test.cpp
#   honuware_tests              component *_test.cpp
#
# It drives the `test_communityfinder` database, which it DROPs and CREATEs at
# startup. That is distinct from knottyyoga's `test_knottyyoga` and honuware's
# `honuware_test`, so all three suites coexist on one server -- but never run the
# SAME suite from Windows and Linux at once.
#
# Overridable:
#   SRC_DIR / BUILD_DIR / HONUWARE_SRC_DIR   see build_common.sh
#   MIN_EXPECTED_TESTS                       floor for the count assertion

set -euo pipefail

case "${1:-}" in
    -h|--help)
        awk 'NR == 1 { next } /^#/ { sub(/^# ?/, ""); print; next } { exit }' "$0"
        exit 0
        ;;
esac

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=build_common.sh
. "$SCRIPT_DIR/build_common.sh"

# The honuware component suite alone clears ~1000 even before CommunityFinder grows
# its own tests, so this floor is safe from the first build. BUMP it as app tests
# accumulate; never lower it to make a broken endpoint anchor pass -- a silent drop
# in the count (routes/tests vanishing at -O2 while the exit code stays 0) is
# exactly what it guards.
MIN_EXPECTED_TESTS=${MIN_EXPECTED_TESTS:-1000}

cf_build communityfinder_tests

TEST_BIN="$BUILD_DIR/test/communityfinder_tests"
if [ ! -x "$TEST_BIN" ]; then
    # Locate it rather than hardcode a path a CMake reorg could invalidate.
    TEST_BIN=$(find "$BUILD_DIR" -name communityfinder_tests -type f -perm -u+x | head -n 1)
fi
if [ -z "$TEST_BIN" ] || [ ! -x "$TEST_BIN" ]; then
    echo "ERROR: communityfinder_tests binary not found under $BUILD_DIR" >&2
    exit 1
fi

# Run from the build dir: the build copies certs/cacert.pem to
# ${CMAKE_BINARY_DIR}/certs and the real HTTP client resolves it at the
# CWD-relative "certs/cacert.pem".
cd "$BUILD_DIR"

echo "[communityfinder] running tests: $TEST_BIN"
"$TEST_BIN" "$@" 2>&1 | tee test-output.txt

if [ "$#" -ne 0 ]; then
    echo "[communityfinder] filtered run -- skipping the test-count floor."
    exit 0
fi

count=$(sed -n 's/^\[==========\] Running \([0-9][0-9]*\) test.*/\1/p' \
        test-output.txt | head -n 1)

if [ -z "$count" ]; then
    echo "ERROR: could not parse a test count from the runner output." >&2
    exit 1
fi

echo "[communityfinder] tests executed: $count (floor: $MIN_EXPECTED_TESTS)"

if [ "$count" -lt "$MIN_EXPECTED_TESTS" ]; then
    echo "ERROR: only $count tests ran, expected >= $MIN_EXPECTED_TESTS." >&2
    echo "       The suite did not fully link -- see this script's comment." >&2
    exit 1
fi

echo "[communityfinder] OK"
