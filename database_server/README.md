# Dev database — shared PostgreSQL (no new compose project)

CommunityFinder does **not** run its own database container (decision Q6). It reuses
the single shared PostgreSQL that knottyyoga already runs in Docker; the two apps'
databases coexist on it, keyed by name.

## The shared container

Owned by **knottyyoga's** `database_server/docker-compose.yml`:

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
server\docker\create_network.cmd                    REM once, if knotty-net is missing
<knottyyoga-checkout>\database_server\load_container.cmd   REM starts knotty-postgres-docker
```

`<knottyyoga-checkout>` is the sibling `knottyyoga` repo. (If knottyyoga is not
available, copy its `database_server/` compose project here and run it — but the
default is to share the one container.)

## CommunityFinder's databases

Created on this shared server by the server binaries, alongside knottyyoga's
(`knottyyoga`, `test_knottyyoga`) and honuware's (`honuware_test`):

| Database | Created by | Purpose |
|---|---|---|
| `communityfinder` | `communityfinder_database_helper --recreate_database` | dev/real data |
| `test_communityfinder` | `communityfinder_tests` at startup (DROP + CREATE) | the test suite |

Both arrive in **Phase 2** — Phase 2.5 wires `--recreate_database`, and Phase 2.6's
test main drives `test_communityfinder`. Because they are distinct database *names*
on the same server, they coexist with the existing databases without collision.
