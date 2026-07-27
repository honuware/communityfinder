@echo off
REM Remove the CommunityFinder client image (does not touch the shared PostgreSQL
REM container or the knotty-net network).
docker image rm -f communityfinder_build:latest
