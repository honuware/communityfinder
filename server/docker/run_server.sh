#!/usr/bin/env bash
#
# Build and run communityfinder_server on Linux, inside the container from
# docker/load_container.cmd.
#
#   ./docker/run_server.sh          # build, then serve on $PORT (default 18081)
#   ./docker/run_server.sh --build-only
#
# load_container.cmd publishes -p 18081:18081 and sets PORT=18081, so once this is
# running the API is reachable from Windows at http://localhost:18081/api/... .
#
# It serves the REAL `communityfinder` database (App::kDatabaseName), not a test
# one. Nothing here creates or migrates it; use communityfinder_database_helper.
#
# Overridable:
#   SRC_DIR / BUILD_DIR / HONUWARE_SRC_DIR   see build_common.sh
#   PORT                                     listen port (default 18081)

set -euo pipefail

BUILD_ONLY=0
case "${1:-}" in
    -h|--help)
        awk 'NR == 1 { next } /^#/ { sub(/^# ?/, ""); print; next } { exit }' "$0"
        exit 0
        ;;
    --build-only)
        BUILD_ONLY=1
        ;;
esac

SCRIPT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
# shellcheck source=build_common.sh
. "$SCRIPT_DIR/build_common.sh"

cf_build communityfinder_server

SERVER_BIN="$BUILD_DIR/communityfinder_server"
if [ ! -x "$SERVER_BIN" ]; then
    SERVER_BIN=$(find "$BUILD_DIR" -name communityfinder_server -type f -perm -u+x | head -n 1)
fi
if [ -z "$SERVER_BIN" ] || [ ! -x "$SERVER_BIN" ]; then
    echo "ERROR: communityfinder_server binary not found under $BUILD_DIR" >&2
    exit 1
fi

if [ "$BUILD_ONLY" -eq 1 ]; then
    echo "[communityfinder] built: $SERVER_BIN (--build-only, not starting)"
    exit 0
fi

# Run from the build dir: the build copies certs/cacert.pem to
# ${CMAKE_BINARY_DIR}/certs and the HTTP client resolves it at the CWD-relative
# "certs/cacert.pem".
cd "$BUILD_DIR"

echo "[communityfinder] starting $SERVER_BIN on port ${PORT:-18081}"
echo "[communityfinder] reachable from Windows at http://localhost:${PORT:-18081}/"
echo "[communityfinder] Ctrl-C to stop."
exec "$SERVER_BIN"
