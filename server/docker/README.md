# Linux build / test / run client

Build and run **communityfinder_server** and **communityfinder_tests** on Linux,
from a Windows dev box, against the shared PostgreSQL running in Docker.

Windows/VS builds Debug/MSVC. That combination cannot see Linux-only or
Release-only defects — and those are not hypothetical. When the honuware components
were first built this way, four latent bugs surfaced, every one of which would have
hit production:

| | Why Windows never saw it |
|---|---|
| `cmake_policy(SET CMP0167 OLD)` unguarded | Hard error below CMake 3.30; VS bundles a newer one, Debian ships 3.25 |
| `find_package(openssl)` | Conan emits `OpenSSLConfig.cmake`; lookup is case-sensitive on Linux only |
| `dbname=` swallowing `sslmode` | `sslMode` is `""` in Debug, `"prefer"` under `NDEBUG` — Release only |
| Endpoint anchors dead-stripped | `-O2` deletes the unused file-scope variables; `-O0` keeps them |

That last one would make a Release server start, bind its port, and **404 every
route**. Treat this container as a required check before a release, not a curiosity.

## Prerequisites

The shared PostgreSQL, on the `knotty-net` network. CommunityFinder does **not**
run its own database container (Q6) — it reuses the one owned by knottyyoga's
`database_server/` compose project (see `../../database_server/README.md`):

```
docker\create_network.cmd                          REM once (shared with knottyyoga)
<knottyyoga>\database_server\load_container.cmd     REM starts knotty-postgres-docker
```

No database configuration is needed: the framework's default Linux DB host is
literally `postgresql`, which is the alias that compose service gets on
`knotty-net`. Port 5432 and user/password `docker`/`docker` are defaults too.

## Use

```
docker\build_container.cmd       REM once, and after Dockerfile changes
docker\load_container.cmd        REM opens a Linux shell
```

Then inside the shell:

```
./docker/build_and_test.sh                        # build + run communityfinder_tests
./docker/build_and_test.sh --gtest_filter=Event*   # args go to the runner
./docker/run_server.sh                            # build + serve on :18081
./docker/run_server.sh --build-only               # compile check only
```

With the server running, the API is reachable **from Windows** at
`http://localhost:18081/` — `load_container.cmd` publishes `-p 18081:18081`.

> Until Phase 2 lands the CMake source root (`server/communityfinder_server/`),
> there is nothing to build; these scripts are the ready-to-go co-dev harness.

## What runs, and against which database

`communityfinder_tests` links **both** test bags:

| Bag | Contents |
|---|---|
| `communityfinder_test_cases` | app `*_test.cpp` |
| `honuware_tests` | component `*_test.cpp`, composed against the app schema |

| Binary | Database |
|---|---|
| `communityfinder_tests` | `test_communityfinder` — **dropped and recreated** at startup |
| `communityfinder_server` | `communityfinder` — real data. Nothing here creates or migrates it; use `communityfinder_database_helper`. |

The honuware suite owns `honuware_test` and knottyyoga owns `test_knottyyoga`, so
all three coexist on one server. **Never run the same suite from Windows and Linux
at once** — each DROPs and CREATEs its database at startup.

## The honuware source override

The root `CMakeLists.txt` pulls honuware via FetchContent pinned to a SHA.
`load_container.cmd` mounts your local `server_components` checkout at `/honuware`
and passes `-DFETCHCONTENT_SOURCE_DIR_HONUWARE=/honuware` — the documented
cross-repo co-development override (see the root `CLAUDE.md`). So this build uses
**your honuware working tree, not the pinned commit**, which is what you want while
both repos change together.

To build against the **pinned SHA** — what CI and the release build use — set
`HONUWARE_SRC=none` before running `load_container.cmd`. Point at a different
checkout with `HONUWARE_SRC=<path>`.

## Volumes

| Volume | Why |
|---|---|
| `honuware-conan2` | The Conan package cache, **shared with the honuware and knottyyoga clients**. CommunityFinder's conanfile overlaps honuware's heavily, so sharing avoids recompiling boost/openssl/etc. ConanCenter publishes **no** prebuilt gcc-14 binaries, so that reuse is worth a lot. |
| `communityfinder-linux-build` | The build tree, kept on the container filesystem rather than under `/src`: Docker Desktop bind mounts are very slow for the many small files a C++ build emits, and it keeps the Linux build tree from colliding with the `out\`/`build\` directory Visual Studio uses. A named volume, so rebuilds stay incremental across `--rm` containers. |

Nuke either with `docker volume rm <name>`.

## Troubleshooting

**`could not translate host name "postgresql"`** — the container is not on the same
network as the database. Check `docker network inspect knotty-net`.

**Configure fails cloning honuware** — you are on the pinned-SHA path
(`HONUWARE_SRC=none`) and need git + network to GitHub, or the pinned SHA predates a
fix you need. Drop the override to use your local tree.

**A stale or confused build tree** — `docker volume rm communityfinder-linux-build`.

**`bad interpreter` / `$'\r'`** on the `.sh` files — line endings got rewritten to
CRLF. `.gitattributes` pins these to LF; run `git add --renormalize .` if needed.
