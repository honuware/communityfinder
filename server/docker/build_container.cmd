@echo off
REM Build the CommunityFinder Linux build/test/run client image.
REM
REM Run this once, and again whenever docker\Dockerfile changes. The image holds
REM only the toolchain -- the source is bind-mounted at run time, so editing code
REM does NOT require rebuilding this image.
REM
REM Usage: build_container.cmd

REM The trailing dot in "%~dp0." keeps a valid path (%~dp0 ends in a backslash, and
REM "...\" would escape the closing quote).
docker build -t communityfinder_build:latest -f "%~dp0Dockerfile" "%~dp0."
if errorlevel 1 (
    echo.
    echo ERROR: image build failed.
    exit /b 1
)

echo.
echo Built image communityfinder_build:latest
echo Next: load_container.cmd
