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

# cmake      : the build system. bookworm ships 3.25 -- which is exactly why the root
#              CMakeLists needs its if(POLICY CMP0167) guard (that policy is 3.30+,
#              and setting an unknown policy is a hard ERROR).
# git        : the root CMakeLists pulls honuware via FetchContent at configure time.
# python3-pip: to install Conan.
# libkrb5-dev: NOT optional. The static libpq that libpqxx pulls in transitively
#              auto-detects GSSAPI, and the root CMakeLists appends -lgssapi_krb5 on
#              non-Windows to resolve it.
RUN apt-get update -qq && apt-get install -y -qq --no-install-recommends \
        cmake \
        git \
        python3-pip \
        libkrb5-dev \
    && rm -rf /var/lib/apt/lists/*

# Conan 2.x is required: the 1.x index is missing recipes we depend on.
# --break-system-packages is required on bookworm (PEP 668).
RUN pip3 install --break-system-packages --quiet 'conan>=2.0,<3.0' \
    && conan profile detect --force

ENV CONAN_HOME=/root/.conan2

WORKDIR /src
CMD ["bash"]
