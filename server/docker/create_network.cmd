@echo off
REM CommunityFinder shares the `knotty-net` bridge network with knottyyoga and the
REM shared PostgreSQL container, so this usually already exists (knottyyoga created
REM it). Creating it again errors harmlessly -- ignore "network already exists".
REM
REM https://docs.docker.com/network/network-tutorial-standalone/
docker network create --driver bridge knotty-net
