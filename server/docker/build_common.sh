# Shared Conan + CMake configure/build for the CommunityFinder Linux container.
#
# Sourced by build_and_test.sh and run_server.sh -- not run directly. Keeping the
# configure in one place stops the two entry points from drifting apart.
#
# Overridable:
#   SRC_DIR            CMake source root  (default /src/communityfinder_server)
#   BUILD_DIR          build tree         (default /build)
#   HONUWARE_SRC_DIR   local honuware checkout to use instead of the pinned SHA
#                      (set by load_container.cmd; unset => use the pinned SHA)
#
# NOTE on the layout: load_container.cmd mounts the repo's `server/` directory at
# /src, NOT `server/communityfinder_server`. docker/ is a SIBLING of
# communityfinder_server/, so mounting only the CMake source root would leave these
# scripts outside the container. Hence /src/docker (the scripts) and
# /src/communityfinder_server (the CMake source root) below.

SRC_DIR=${SRC_DIR:-/src/communityfinder_server}
BUILD_DIR=${BUILD_DIR:-/build}

# cf_build <target...>
cf_build() {
    local targets=("$@")

    echo "[communityfinder] source : $SRC_DIR"
    echo "[communityfinder] build  : $BUILD_DIR"

    # --- Conan ---------------------------------------------------------------
    # cppstd=17 governs only the DEPENDENCY builds; the app itself compiles as
    # C++20 via CMAKE_CXX_STANDARD -- the same combination the release build ships.
    # --output-folder side-steps the recipe's Windows-oriented vs_layout.
    echo "[communityfinder] conan install ..."
    conan install "$SRC_DIR" \
        --output-folder="$BUILD_DIR" \
        --build=missing \
        -s build_type=Release \
        -s compiler.cppstd=17

    # Conan 2.28+ nests generators under conan/; older versions put the toolchain at
    # the output-folder root. Probe rather than assume.
    local toolchain=""
    local candidate
    for candidate in \
        "$BUILD_DIR/conan_toolchain.cmake" \
        "$BUILD_DIR/conan/conan_toolchain.cmake" \
        "$BUILD_DIR/build/conan_toolchain.cmake"; do
        if [ -f "$candidate" ]; then
            toolchain="$candidate"
            break
        fi
    done
    if [ -z "$toolchain" ]; then
        echo "ERROR: conan_toolchain.cmake not found under $BUILD_DIR" >&2
        exit 1
    fi
    echo "[communityfinder] toolchain: $toolchain"

    # --- Configure -----------------------------------------------------------
    # FETCHCONTENT_SOURCE_DIR_HONUWARE points FetchContent at a local honuware
    # checkout instead of the SHA pinned in the root CMakeLists (the documented
    # cross-repo co-development override). Without it, configure git-clones the
    # pinned commit.
    local honuware_arg=()
    if [ -n "${HONUWARE_SRC_DIR:-}" ]; then
        if [ ! -f "$HONUWARE_SRC_DIR/CMakeLists.txt" ]; then
            echo "ERROR: HONUWARE_SRC_DIR=$HONUWARE_SRC_DIR has no CMakeLists.txt" >&2
            exit 1
        fi
        honuware_arg=("-DFETCHCONTENT_SOURCE_DIR_HONUWARE=$HONUWARE_SRC_DIR")
        echo "[communityfinder] honuware : $HONUWARE_SRC_DIR (local override)"
    else
        echo "[communityfinder] honuware : pinned SHA (FetchContent will clone it)"
    fi

    echo "[communityfinder] cmake configure ..."
    cmake -S "$SRC_DIR" -B "$BUILD_DIR" \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_TOOLCHAIN_FILE="$toolchain" \
        -DCMAKE_POLICY_DEFAULT_CMP0091=NEW \
        "${honuware_arg[@]}"

    # --- Build ---------------------------------------------------------------
    echo "[communityfinder] cmake build (${targets[*]}) ..."
    local target_args=()
    local t
    for t in "${targets[@]}"; do
        target_args+=(--target "$t")
    done
    cmake --build "$BUILD_DIR" -j"$(nproc)" "${target_args[@]}"
}
