@echo off
setlocal
REM Open a Linux shell with the CommunityFinder toolchain, the server source mounted
REM at /src, the honuware components at /honuware, and the container joined to the
REM knotty-net network so it reaches the shared PostgreSQL.
REM
REM Usage:
REM   load_container.cmd                 (network defaults to knotty-net)
REM   load_container.cmd <network>
REM
REM Once inside:
REM   ./docker/build_and_test.sh    build + run communityfinder_tests
REM   ./docker/run_server.sh        build + run communityfinder_server
REM
REM No database environment variables are set on purpose. The framework's default
REM Linux DB host is literally "postgresql", and the shared docker-compose service
REM (knottyyoga\database_server\) gets that alias on knotty-net -- so joining the
REM network is the whole configuration. Port 5432 and user/password docker/docker
REM are defaults too.

set NETWORK=%~1
if "%NETWORK%"=="" set NETWORK=knotty-net

REM Resolve the source trees. This script lives in <repo>\server\docker\.
REM
REM Mount the whole `server\` directory at /src, NOT server\communityfinder_server:
REM docker\ is a SIBLING of communityfinder_server\, so mounting only the CMake
REM source root would leave these scripts outside the container. So inside:
REM   /src/docker                  these scripts
REM   /src/communityfinder_server  the CMake source root
for %%i in ("%~dp0..") do set MOUNT_DIR=%%~fi
for %%i in ("%~dp0..\communityfinder_server") do set SRC_DIR=%%~fi
REM The honuware component repo sits alongside this repo by default (..\..\..).
REM Override by setting HONUWARE_SRC before running.
if "%HONUWARE_SRC%"=="" (
    for %%i in ("%~dp0..\..\..\server_components") do set HONUWARE_SRC=%%~fi
)

if not exist "%SRC_DIR%\CMakeLists.txt" (
    echo ERROR: server source not found at "%SRC_DIR%".
    echo        Phase 2 adds the CMake source root ^(server\communityfinder_server^);
    echo        until then there is nothing to build here.
    exit /b 1
)

docker image inspect communityfinder_build:latest >nul 2>&1
if errorlevel 1 (
    echo ERROR: image communityfinder_build:latest not found. Run build_container.cmd first.
    exit /b 1
)

docker network inspect %NETWORK% >nul 2>&1
if errorlevel 1 (
    echo ERROR: Docker network "%NETWORK%" does not exist.
    echo        Create it with create_network.cmd, then start the shared PostgreSQL
    echo        with knottyyoga\database_server\load_container.cmd.
    exit /b 1
)

REM ---------------------------------------------------------------------------
REM The honuware mount + FETCHCONTENT_SOURCE_DIR_HONUWARE override.
REM
REM The root CMakeLists pulls honuware via FetchContent pinned to a SHA. Pointing
REM FetchContent at a local working tree instead (the documented cross-repo
REM co-development override -- see the root CLAUDE.md) means this build uses YOUR
REM honuware checkout rather than the pinned commit. That is what you want while the
REM two repos are being changed together: fixes land in honuware and are exercised
REM here immediately, with no push/re-pin round trip.
REM
REM To build against the PINNED SHA instead (what CI and the release build use), set
REM HONUWARE_SRC=none before running.
REM ---------------------------------------------------------------------------
set HONUWARE_MOUNT=
set HONUWARE_ENV=
if /I not "%HONUWARE_SRC%"=="none" (
    if not exist "%HONUWARE_SRC%\CMakeLists.txt" (
        echo ERROR: honuware source not found at "%HONUWARE_SRC%".
        echo        Set HONUWARE_SRC to your server_components checkout, or set
        echo        HONUWARE_SRC=none to build against the pinned SHA instead.
        exit /b 1
    )
    set HONUWARE_MOUNT=-v "%HONUWARE_SRC%:/honuware"
    set HONUWARE_ENV=-e HONUWARE_SRC_DIR=/honuware
    echo honuware : %HONUWARE_SRC%  -^>  /honuware  ^(local override^)
) else (
    echo honuware : pinned SHA from the root CMakeLists ^(cloned at configure time^)
)

echo source   : %MOUNT_DIR%  -^>  /src   ^(cmake root: /src/communityfinder_server^)
echo network  : %NETWORK%
echo.

REM Volumes:
REM   <server>:/src               edit in Windows, build in Linux (the repo's server\
REM                               dir, so /src/docker + /src/communityfinder_server
REM                               are both visible).
REM   honuware-conan2             SHARED with the honuware + knottyyoga clients' Conan
REM                               cache. CF's conanfile overlaps honuware's heavily, so
REM                               sharing avoids recompiling boost/openssl/etc (there
REM                               are NO prebuilt gcc-14 binaries on ConanCenter).
REM   communityfinder-linux-build the build tree, on the CONTAINER filesystem rather
REM                               than under /src (Docker Desktop bind mounts are slow
REM                               for the many small files a C++ build emits, and it
REM                               keeps the Linux tree from colliding with VS's out\).
REM                               Nuke with: docker volume rm communityfinder-linux-build

docker run --rm -it ^
    --network %NETWORK% ^
    -v "%MOUNT_DIR%:/src" ^
    %HONUWARE_MOUNT% ^
    -v honuware-conan2:/root/.conan2 ^
    -v communityfinder-linux-build:/build ^
    %HONUWARE_ENV% ^
    -e HONUWARE_DB_SSLMODE=disable ^
    -p 18081:18081 ^
    -e PORT=18081 ^
    -w /src ^
    communityfinder_build:latest bash

endlocal
