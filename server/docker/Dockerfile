# Linux build/test/run client for the CommunityFinder server.
#
# Purpose: build and run communityfinder_server and communityfinder_tests on Linux
# from a Windows dev box, against the PostgreSQL already running in Docker. Windows/VS
# (Debug/MSVC) cannot see Linux-only or Release-only defects; this container is where
# they show up (see docker/README.md).
#
# This image holds the TOOLCHAIN ONLY. The source is bind-mounted at run time (see
# load_container.cmd), so editing code never requires rebuilding the image.
#
# Build:  docker\build_container.cmd
# Run:    docker\load_container.cmd
#
# Same builder image as honuware + knottyyoga, so this container, CI, and the deploy
# build all agree on compiler, libstdc++ and glibc. Do not bump without checking the
# builder/runtime glibc pairing (the release image runs on ubuntu:22.04 / glibc 2.35;
# you may not build against a newer glibc than you run on).
FROM gcc:14.2.0

# curl       : to fetch the pinned CMake tarball below.
# git        : the root CMakeLists pulls honuware via FetchContent at configure time.
# python3-pip: to install Conan.
# libkrb5-dev: NOT optional. The static libpq that libpqxx pulls in transitively
#              auto-detects GSSAPI, and the root CMakeLists appends -lgssapi_krb5 on
#              non-Windows to resolve it.
#
# NOTE cmake is deliberately NOT in this list any more (honuware Phase 12.2).
RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
        curl \
        ca-certificates \
        git \
        python3-pip \
        libkrb5-dev \
    && rm -rf /var/lib/apt/lists/*

# CMake from the OFFICIAL BINARY TARBALL, pinned by version AND SHA-256
# (honuware Phase 12.2).
#
# bookworm apt shipped 3.25, which is the whole problem: the dev boxes run 4.x,
# so every Linux gate verified a build nobody performs. Worse, 3.25 is below the
# 3.30 floor where CMP0167 exists, so the root CMakeLists' if(POLICY CMP0167)
# guard was always FALSE here — Linux silently used FindBoost module mode while
# Windows used config mode. Raising this flips that branch on.
#
# Why a tarball rather than apt or pip: Kitware's APT repo reintroduces
# "whatever is current" instead of a fixed build, and `pip install cmake` drags a
# Python dependency into an image that otherwise needs none. The checksum makes
# the image reproducible in a way `apt-get install cmake` never was.
#
# ALL FIVE build surfaces (three gate images, the release image, CI) pin the SAME
# version. A partial raise means several CMake versions across the gates, which
# is worse than none.
ARG CMAKE_VERSION=4.4.3
ARG CMAKE_SHA256=d6c83076c575bc00b823522ac974bda66d0af05d6ddc30e739c12385cf32c6cc
RUN set -eux; \
    curl -fsSL -o /tmp/cmake.tar.gz \
        "https://github.com/Kitware/CMake/releases/download/v${CMAKE_VERSION}/cmake-${CMAKE_VERSION}-linux-x86_64.tar.gz"; \
    echo "${CMAKE_SHA256}  /tmp/cmake.tar.gz" | sha256sum -c -; \
    mkdir -p /opt/cmake; \
    tar -xzf /tmp/cmake.tar.gz -C /opt/cmake --strip-components=1; \
    rm /tmp/cmake.tar.gz; \
    ln -sf /opt/cmake/bin/cmake /usr/local/bin/cmake; \
    ln -sf /opt/cmake/bin/ctest /usr/local/bin/ctest; \
    ln -sf /opt/cmake/bin/cpack /usr/local/bin/cpack; \
    cmake --version

# Conan 2.x is required: the 1.x index is missing recipes we depend on.
# --break-system-packages is required on bookworm (PEP 668).
RUN pip3 install --break-system-packages --quiet 'conan>=2.0,<3.0' \
    && conan profile detect --force

ENV CONAN_HOME=/root/.conan2

WORKDIR /src
CMD ["bash"]
