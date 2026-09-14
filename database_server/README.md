# Dev database — shared PostgreSQL (no new compose project)

CommunityFinder does **not** run its own database container (decision Q6). It reuses
the single shared PostgreSQL that knottyyoga already runs in Docker; the two apps'
databases coexist on it, keyed by name.

## The shared container

Owned by **server_components'** `database_server/docker-compose.yml` — it moved
there from knottyyoga in migration plan Phase 14.2, because all three repos depend
on it and it did not belong inside one of the applications:

| Property | Value |
|---|---|
| compose service (network alias) | `postgresql` |
| container name | `knotty-postgres-docker` |
| image | `postgres:13.1` |
| network | `knotty-net` (external bridge) |
| host port | `5432` |
| user / password | `docker` / `docker` |

The service's alias on `knotty-net` is literally `postgresql`, which is exactly the
framework's default Linux DB host — so a client that simply joins `knotty-net` needs
**no** connection configuration (host `postgresql`, port `5432`, user/pass
`docker`/`docker`, and `HONUWARE_DB_SSLMODE=disable` are all defaults). Override any
of them with the `HONUWARE_DB_*` env vars if needed.

## Bringing it up

If the container isn't already running (check `docker ps`):

```
<server_components>\database_server\create_network.cmd    REM once, if knotty-net is missing
<server_components>\database_server\load_container.cmd     REM starts knotty-postgres-docker
```

`<server_components>` is the sibling `server_components` checkout — the framework
this repo already consumes via FetchContent, so it is present on any machine set up
to build CommunityFinder. See `server_components/database_server/README.md` for the
data directory, the database inventory, and `postgres_shell.cmd`.

Do **not** copy the compose project here. Two compose projects claiming the
container name `knotty-postgres-docker` would fight over one cluster; sharing the
single container is the decision (Q6), not a fallback.

## CommunityFinder's databases

Created on this shared server by the server binaries, alongside knottyyoga's
(`knottyyoga`, `test_knottyyoga_*`) and honuware's (`honuware_test_*`):

| Database | Created by | Purpose |
|---|---|---|
| `communityfinder` | `communityfinder_database_helper --recreate_database` | dev/real data |
| `test_communityfinder_windows` | `communityfinder_tests` on Windows, at startup (DROP + CREATE) | the test suite |
| `test_communityfinder_linux` | `communityfinder_tests` in the Linux gate, at startup (DROP + CREATE) | the test suite |

Both arrive in **Phase 2** — Phase 2.5 wires `--recreate_database`, and Phase 2.6's
test main drives the test database. Because they are distinct database *names* on
the same server, they coexist with the existing databases without collision.

**Test databases are platform-qualified** (honuware Phase 10.2). The app's test
main supplies the base name `test_communityfinder`; the harness appends a token
derived at **compile time** — `_windows` under `_WIN32`, `_linux` otherwise. So a
Linux gate and a Windows run of this repo drive different physical databases and
**can run concurrently**, which is the whole point: three repos × two platforms =
six test databases, all able to run at once against one PostgreSQL.

Two consequences worth knowing:

- **Two Linux gates from two checkouts of the same repo still collide** — they
  compile to the same suffix. Accepted deliberately; the suffix is compile-time
  rather than an environment variable because the harness DROPs and CREATEs
  whatever name it is handed, and an externally-supplied string inside a
  destructive operation would need validation it does not currently have.
- **The old unsuffixed databases are now orphaned**: `honuware_test`,
  `test_knottyyoga`, `test_communityfinder`. They are dropped-and-recreated
  scratch databases with nothing to preserve, so drop them by hand once and they
  will not come back.
