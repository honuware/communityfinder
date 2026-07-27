# CLAUDE.md — CommunityFinder

Guidance for Claude Code in this repository. CommunityFinder is a community events
site built as the **second consumer** of the honuware server components
(github.com/honuware/server_components) and the `@honuware/ui` Angular library. The
backend is C++ (Crow + PostgreSQL); the client is Angular.

## Planning documents live in the vault, not here

The plan is `C:\Users\mason\Documents\Obsidian\CommunityFinder\Claude\Setting up the project.md`
(and siblings in that folder). Do **not** create or edit plans under `.claude/plans`
or in this repo — update that vault document. Read it for context; put open
questions in its Open Questions section rather than blocking on them.

## Division of labor (co-development with honuware)

- **Claude** runs all C++ builds/tests in the Linux docker client
  (`server/docker/build_and_test.sh`) as the per-change gate, and does read-only
  git only. When honuware changes in the same slice, build against a local
  honuware tree via `-DFETCHCONTENT_SOURCE_DIR_HONUWARE` (env `HONUWARE_SRC_DIR`).
- **Mason** does Windows/VS spot-checks and **all git writes** (commits, pushes,
  the honuware re-pin).
- honuware is consumed at a pinned SHA in
  `server/communityfinder_server/CMakeLists.txt` (`FetchContent … GIT_TAG`). At a
  cross-repo bump-point: Mason pushes honuware → Claude re-pins + verifies the
  pinned build (fresh clone, no override) → Mason commits.

## Architecture — layering is law

App runtime layering, high→low: **endpoints → business_logic → table_helpers →
db_schema**, all resting on the honuware component DAG (**foundation → data →
services → platform**, with `square` / `scheduler` as side branches). App targets
may link honuware targets; honuware never links app code. An app-superset
`honuware_layering.cmake` enforces it at configure time — an upward/sideways edge
fails the build.

- **Endpoints are thin**: validate + authorize + delegate to business_logic. No
  SQL in endpoints. Pass data across boundaries as `KeyValueTable` / `Json::Value`.
- **db_schema** = table DDL + column-name constants; **table_helpers** = typed CRUD
  wrappers; **business_logic** = the domain layer.

## The volatile endpoint-anchor convention (READ THIS)

Endpoints self-register at file scope via `RoutingBase`. In a static library an
object file is linked only to resolve an undefined symbol, so at `-O2` a
self-registering translation unit is **dead-stripped and every route 404s in
Release** unless it is anchored. Anchor each app endpoint TU in
`RegisterAllEndpoints()` (`endpoints/web_app.cpp`) through a
`static AnchorFunc volatile anchor = reinterpret_cast<AnchorFunc>(&Endpoints::X)`.
A plain (non-`volatile`) unused pointer is NOT enough — the optimizer removes it.
The docker gate's test-count floor exists to catch a broken anchor (routes/tests
silently vanishing while the exit code stays 0).

## Testing conventions (honuware harness)

- **No test fixtures.** Each test is self-contained; set up dependencies at the top
  of the test function. No `class XxxTest : public ::testing::Test`.
- **Tables are pre-created** by `GlobalDatabaseTestSupport` at startup from the
  composed `DatabaseInfo`; each test runs in a transaction aborted at the end.
  Tests do NOT create tables. **The harness does NOT run `create_database`'s seed**
  (it builds the schema from `MakeDatabaseInfo`) — verify seed changes with a live
  `--recreate_database`, not the suite.
- **`ThreadPool::Shutdown()` before the next DB read.** After `handle_full()`
  returns, endpoints' async writes re-enter the test's libpqxx connection, which is
  not thread-safe. Call `Shutdown()` before the first DB-touching assertion.
- **Don't assume collection order** — find rows by a unique identifier, not index.
- **Add a test when you add a method.** App test sources are self-registering
  `TEST()`s in the `communityfinder_test_cases` bag, beside their sources.

## Windows / Crow gotcha

Use the **PascalCase** `crow::HTTPMethod` aliases — `Post`, `Get`, `Put`, `Patch`,
`Delete`, `Head`, `Options` — never SCREAMING_CASE. `winnt.h` `#define DELETE`
makes Crow drop the entire SCREAMING_CASE half of the enum once `<windows.h>` is
pulled in (libpqxx does so transitively), so `POST`/`GET`/… all vanish too.

## Common C++ API pitfalls (honuware)

- `Json::Value` (`util/json_value.h`) wraps crow::json: read with
  `value.Get<int64_t>()` / `Get<std::string>()` / `Get<bool>()`, arrays via
  `.GetArray()`. Build objects with `Json::Value(Json::JsonObject{{"k", v}})`.
  There is no `GetInt64()`, `ArraySize()`, or `Json::Value::Object(...)`.
- `ErrorResponse` (`util/error_response.h`): `BadRequest` (400),
  `NotAuthenticated` (401), `NotAuthorized` (403), `NotFound` (404),
  `ValidationError` (400), `InternalError` (500). There is no `ServerError`.
- Read the actual honuware header before using an API; don't assume naming.

## String / email formatting

- Use `FormatString` with `{placeholder}` constants (`util/types.h`), not
  line-by-line stream building. Wrap generated email HTML with `NormalizeCrLf(...)`
  — mailio requires `\r\n` and rejects the bare `\n` in raw string literals.
- Resolve a mail From-address via `::Mail::LoadSenderAddress(secrets, txn)` (leading
  `::Mail::` — nested namespaces can shadow it). Its brand values come from this
  app's secret defaults (`app_secret_values.*`), not honuware.

## Environment variables

- `HONUWARE_DB_HOST` / `_PORT` / `_USER` / `_PASSWORD` / `_SSLMODE` — Postgres
  connection (defaults host `postgresql`, user/pass `docker`/`docker`, port 5432 on
  Linux; the shared dev container lives on the `knotty-net` docker network, host
  port 5432).
- `HONUWARE_SECRET_KEY` — at-rest `config_secrets` key (non-prod falls back to a
  fixed dev key).
- `HONUWARE_ALLOW_DESTRUCTIVE=1` — gates `--recreate_database`.
- `SCHEDULER_SERVICE_ACCOUNT_PASSWORD` — the scheduler service account password.
- `HONUWARE_SRC_DIR` → `-DFETCHCONTENT_SOURCE_DIR_HONUWARE` for local co-dev.
- App databases: `communityfinder` (prod/dev), `test_communityfinder` (tests).
- App ports: server **18081**, `ng serve` **4201**.

## Naming

Full descriptive names, not abbreviations. `snake_case` files, `camelCase`
variables, `PascalCase` classes/functions. File names match the primary class they
contain.
