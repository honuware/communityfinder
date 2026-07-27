# CommunityFinder

A community events site for the gay community — AI-scanned and manually curated
events with a calendar (Seattle first). Built as the **second consumer** of
[honuware](https://github.com/honuware/server_components) (reusable C++ web-server
components) and [`@honuware/ui`](https://github.com/honuware/web_components) (the
Angular component library).

## Layout (monorepo)

- `server/` — the C++ backend (Crow + PostgreSQL), consuming honuware via CMake
  FetchContent at a pinned SHA. CMake source root: `server/communityfinder_server/`.
- `ui/` — the Angular client, consuming `@honuware/ui`.
- `database_server/` — pointer to the shared dev PostgreSQL container (no new
  compose project).
- `server/docker/` — the Linux build/test client (mirrors honuware's `docker/`);
  it is the per-change test gate.

*(Directories fill in from Phase 2 onward — see the plan.)*

## Build & test

The server builds with CMake + Conan against a pinned honuware SHA. The Linux
docker client `server/docker/build_and_test.sh` is the authoritative build+test
gate (with a test-count floor). See **`CLAUDE.md`** for conventions, the layering
rules, and the honuware co-development workflow.

## Contributing

Contributions from project collaborators are welcome. The project is licensed
under Apache-2.0 (see `LICENSE` and `NOTICE`); by submitting a contribution you
agree it is provided under those terms.

## License

Apache-2.0 — see [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE).
