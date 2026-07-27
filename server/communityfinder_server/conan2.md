# Building the CommunityFinder server with Conan 2 + Visual Studio 2022

The dependencies (Boost, Abseil, Crow, libpqxx, OpenSSL, GoogleTest, ftxui, replxx,
…) come from Conan 2, wired into CMake by `conan_provider.cmake` (the cmake-conan
dependency provider). On Linux the docker client (`server/docker/`) runs
`conan install` explicitly; on Windows/VS the provider runs Conan automatically at
configure time — but only if VS passes `-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=conan_provider.cmake`,
which lives in **`CMakeSettings.json`**.

## Prerequisites

1. **Conan 2.0** — on Windows with VS 2022, install from https://conan.io/downloads
   and choose "install local for the user". `CMakeSettings.json` looks for
   `conan.exe` at `${env:LOCALAPPDATA}/Conan/conan/conan.exe`; if you installed it
   elsewhere (e.g. via pip), privately edit the `CONAN_CMD` value in your local
   `CMakeSettings.json` (don't commit that change).
2. **Visual Studio 2022** with the "Desktop development with C++" workload and the
   "C++ CMake tools for Windows" individual component.
3. **CMake version:** if a `--recreate`-style configure fails inside libpqxx with a
   `cmake_configure` error on a very new CMake, install CMake 3.29.9
   (https://github.com/Kitware/CMake/releases/tag/v3.29.9) and point VS at it. (The
   Linux docker build uses 3.25 and is unaffected; try your bundled CMake first.)

## Open + build in Visual Studio 2022

1. **Open Folder** → `server/communityfinder_server` (the folder containing
   `CMakeLists.txt` + `CMakeSettings.json`).
2. **Disable CMakePresets.json (REQUIRED).** Tools → Options → CMake → General →
   **uncheck "Use CMakePresets.json"**. This project is driven by `CMakeSettings.json`;
   if presets stay enabled, VS does NOT pass
   `-DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=conan_provider.cmake`, Conan never runs, and
   `find_package(Boost … REQUIRED)` in `CMakeLists.txt` fails with
   "Could NOT find Boost". Re-open the folder after unchecking so VS re-configures.
3. First configure runs `conan install` for every dependency — **~30 min the first
   time** (no prebuilt binaries for this toolchain), then cached. Watch the CMake
   output window; it ends with "Configuring done / Generating done".
4. Pick a target in the toolbar and build/debug like any VS project:
   - `communityfinder_tests.exe` — the unit + component test suite.
   - `communityfinder_database_helper.exe` — `--recreate_database` / `--migrate`.
   - `communityfinder_server.exe` — the web server (port 18081).
   - `communityfinder_test_helper.exe` — the dev REPL/dashboard.

## honuware source (pinned SHA vs local co-dev)

`CMakeLists.txt` pulls honuware via FetchContent pinned to a SHA. The committed
`CMakeSettings.json` sets **no** `FETCHCONTENT_SOURCE_DIR_HONUWARE`, so VS git-clones
that pinned commit at configure time (needs network + git). That is the
contributor-safe default.

To co-develop against a **local** honuware working tree instead (no clone; your
honuware edits are picked up immediately), add this variable to your **local**
`CMakeSettings.json` and do NOT commit it (it hard-codes your machine's path):

```json
{
  "name": "FETCHCONTENT_SOURCE_DIR_HONUWARE",
  "value": "C:/Users/<you>/source/repos/server_components",
  "type": "PATH"
}
```

## Building outside Visual Studio (command line)

```sh
cd server/communityfinder_server
cmake -S . -B out/build/cli -DCMAKE_PROJECT_TOP_LEVEL_INCLUDES=conan_provider.cmake -DCMAKE_BUILD_TYPE=Debug
cmake --build out/build/cli
```

(The Linux docker client — `server/docker/build_and_test.sh` — is the authoritative
CI-style build and runs Conan itself; see `server/docker/README.md`.)
