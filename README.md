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

## Build & test

The server builds with CMake + Conan against a pinned honuware SHA. The Linux
docker client `server/docker/build_and_test.sh` is the authoritative build+test
gate (with a test-count floor). See **`CLAUDE.md`** for conventions, the layering
rules, and the honuware co-development workflow.

## Developer machine setup (Windows)

These steps take a bare Windows machine to a running server and UI. Every problem
we hit while setting up a second machine is recorded in the steps, and again under
*Troubleshooting* at the end, keyed by the error message you'll see.

### 1. Install the prerequisites

| Tool | Version | Notes |
|---|---|---|
| Visual Studio 2026 | Community or higher | Install the **Desktop development with C++** workload. |
| Git for Windows | current | Make sure `git --version` works in a plain terminal. CMake needs it to fetch honuware. |
| CMake | **4.4.3** | <https://cmake.org/download/> |
| Conan | **2.31.2** | <https://conan.io/downloads>. It must be 2.x. |
| Python | 3.13 | Run `winget install Python.Python.3.13 --scope machine` from an **elevated** prompt. **Don't** install it from the Microsoft Store. |
| Docker Desktop | current | Virtualization must be enabled in the BIOS. |
| Node.js | 22 LTS (22.12+) | Node includes npm. The UI is pinned to npm 11.1.0: `npm install -g npm@11.1.0`. |

Python is required even though nothing in this repo is Python. `libpq` builds with
Meson, which is a Python application. The Microsoft Store version installs a
`python.exe` stub that exits silently, and Meson then fails with an error that
never mentions Python. If you already have the Store version, turn off the aliases
under *Settings → Apps → Advanced app settings → App execution aliases*.

You don't need to install the Angular CLI globally. The project's own copy runs
through `npm start` / `npx ng`.

### 2. Clone both repositories side by side

```powershell
cd $HOME\source\repos
git clone https://github.com/honuware/server_components.git
git clone https://github.com/honuware/communityfinder.git
```

The build pulls honuware in automatically. You still need the `server_components`
checkout because it owns the shared PostgreSQL container's scripts (step 3).

### 3. Start the shared PostgreSQL container

CommunityFinder shares a single PostgreSQL container with the other honuware apps
(see `database_server/README.md`). Start Docker Desktop, then run:

```powershell
cd $HOME\source\repos\server_components\database_server
.\create_network.cmd     # once per machine; creates the knotty-net network
.\load_container.cmd     # starts knotty-postgres-docker on port 5432
```

`docker ps` should list `knotty-postgres-docker` (postgres:13.1) publishing
`5432`. Nothing else may listen on 5432: if you have a native Windows PostgreSQL
service (`Get-Service *postgres*`), stop it or move it to another port.

### 4. Configure Visual Studio

Go to **Tools → Options → CMake** and set *Prefer using CMake Presets* to **use
CMake Presets if available**. The server is driven by `CMakePresets.json`.

Open **`server\communityfinder_server\`** as a folder (**File → Open → Folder**).
Don't open the repo root: Visual Studio would then use a different `.vs` folder,
and the launch settings from step 6 wouldn't apply.

### 5. First configure and build

Let CMake generation finish with the `x64-Debug` preset. Conan runs automatically
and installs the pinned versions from the committed `conan.lock`. The first run
compiles every dependency (Boost, libpq, OpenSSL, …) from source and takes a long
time. Later runs reuse the Conan cache. Then **Build → Build All**.

To build from a terminal instead, use the *Developer PowerShell for VS 2026*. A
plain shell fails with `Cannot open include file: 'algorithm'`.

```powershell
cmake --preset x64-Debug
cmake --build --preset x64-Debug
```

### 6. Generate the debug launch settings

Visual Studio keeps each target's command-line `args` and `env` in
`.vs/launch.vs.json`. That file is gitignored and disposable, so **don't edit it
by hand**. Keep the values in `launch_defaults.local.json` instead (also
gitignored, because it holds credentials) and let honuware's
`sync_launch_targets.ps1` write them into `launch.vs.json`. Without this step every
debug target launches bare: the database helper gets no `--recreate_database`, and
nothing gets the database connection variables.

After the first configure, FetchContent has placed the script and an example
defaults file in the build tree. From `server\communityfinder_server\`:

```powershell
copy out\build\x64-Debug\_deps\honuware-src\tools\launch_defaults.example.json launch_defaults.local.json
# edit launch_defaults.local.json (see below), then:
out\build\x64-Debug\_deps\honuware-src\tools\sync_launch_targets.ps1 -RepoPath . -Config x64-Debug
```

If PowerShell says *running scripts is disabled on this system*, you are on
Windows' default execution policy. Either run the script once with the policy
bypassed:

```powershell
powershell -ExecutionPolicy Bypass -File out\build\x64-Debug\_deps\honuware-src\tools\sync_launch_targets.ps1 -RepoPath . -Config x64-Debug
```

or allow local scripts for your account, a one-time setting:
`Set-ExecutionPolicy -Scope CurrentUser RemoteSigned`.

The script reads every executable target from the CMake file API and stamps the
defaults onto each entry. Entries under `all` apply to every target. Keys under
`targets` match the target label as a wildcard. Edit your copy to look like this:

```json
{
  "all": {
    "env": {
      "HONUWARE_DB_HOST": "localhost",
      "HONUWARE_DB_PORT": "5432",
      "HONUWARE_DB_USER": "docker",
      "HONUWARE_DB_PASSWORD": "docker",
      "HONUWARE_DB_SSLMODE": "disable",
      "SCHEDULER_SERVICE_ACCOUNT_PASSWORD": "dev-scheduler-password"
    }
  },
  "targets": {
    "*database_helper.exe*": {
      "args": ["--recreate_database"],
      "env": {
        "HONUWARE_ALLOW_DESTRUCTIVE": "1",
        "HONUWARE_MAIL_APP_PASSWORD": "<app password for community.finder.seattle@gmail.com>"
      }
    },
    "*tests.exe*": {
      "args": ["--gtest_filter=*"]
    }
  }
}
```

- `HONUWARE_ALLOW_DESTRUCTIVE` must be exactly `"1"`, otherwise
  `--recreate_database` refuses to run.
- `HONUWARE_MAIL_APP_PASSWORD` is **required**. It is read only at seed time
  (`--recreate_database`) and stored in `config_secrets`. If it's unset, the seed
  still succeeds, but the server then refuses to start (`MakeMailHelper` throws
  `config_secrets.mail_app_password is empty - cannot send mail`). The value must
  be a Gmail app password for the sender account,
  `community.finder.seattle@gmail.com`, because mail is sent by logging in as that
  address. Get it privately from a maintainer, and never commit it or copy
  someone else's `launch_defaults.local.json`. Google displays it as four groups
  of four; the spaces are display only, so enter the 16 characters without them.
- `SCHEDULER_SERVICE_ACCOUNT_PASSWORD` can be any non-empty value locally. The
  scheduler service account isn't seeded in CommunityFinder yet; the value is
  kept so the file already works when it is.

To change a value, edit `launch_defaults.local.json` and **re-run the script**.
Also re-run it after a new executable target appears or after `.vs/` has been
cleared. Values in the defaults file override what's in `launch.vs.json`. Pass
`-WhatIf` to preview the result without writing it. If Visual Studio doesn't pick
up the change, close and reopen the folder.

### 7. Create and seed the dev database

Select `communityfinder_database_helper.exe` as the startup item and run it. It
drops and recreates the `communityfinder` database, creates the tables, and seeds
them. A successful run prints:

```
[database_helper] event=recreate_database_starting
Database connection: host=localhost port=5432 dbname=communityfinder user=docker sslmode=disable sslrootcert=<unset>
[database_helper] event=recreate_database_done
```

and exits with code 0. **Passing tests don't prove this step worked.** The test
suite uses its own database (`test_communityfinder_windows`) and builds its tables
itself, so it passes even when `communityfinder` is empty or missing.

To check the result, run
`docker exec -it knotty-postgres-docker psql -U docker -d communityfinder -c "\dt"`.
The list should include `config_secrets`.

The seed creates admin accounts for the maintainers with the dev password
`changeme` (see `PopulatePeople` in
`server/communityfinder_server/src/database_helper/create_database.cpp`).

### 8. Run the server

Select `communityfinder_server.exe` and run it. It listens on
**<http://localhost:18081>**; `http://localhost:18081/api/health` answers once it
is up. The full list of environment variables is in `CLAUDE.md` under
*Environment variables*.

### 9. Run the UI

From `ui\`:

```powershell
npm ci
npx ng serve -c development    # real backend: proxies /api to the server on 18081
npm start                      # offline: in-memory mock data, no server needed
```

Both serve on **<http://localhost:4201>**. `npm ci` installs exactly what
`package-lock.json` pins, including `@honuware/ui` from the public npm registry.
Use `npm ci`, not `npm install`, so the lock file doesn't change. `npm start` uses
the `local` configuration, which never talks to the server, so use
`-c development` whenever you need real data or login.

The UI can also be opened as its own folder in a second Visual Studio instance.

### Troubleshooting

| What you see | Cause and fix |
|---|---|
| `ERROR: Lockfile doesn't exist: …\conan.lock`, followed by `Could not find a package configuration file provided by "Boost"` | Your checkout predates the committed `conan.lock`. Pull, then **Project → Delete Cache and Reconfigure**. The Boost error goes away with the Conan one. |
| `sync_launch_targets.ps1 cannot be loaded because running scripts is disabled on this system` | PowerShell's default execution policy. See step 6. |
| The database helper exits immediately with `Specify exactly one of --recreate_database or --migrate`, or the server can't reach the database | The launch settings weren't generated, or were cleared along with `.vs/`. Redo step 6. |
| The server fails with `ERROR: relation "config_secrets" does not exist` | The `communityfinder` database exists but is empty: the helper never ran successfully. Redo step 7 and check its output. |
| The server throws `config_secrets.mail_app_password is empty - cannot send mail` (in `Mail::MakeMailHelper`) | The database was seeded without `HONUWARE_MAIL_APP_PASSWORD`. Add it (step 6), re-run the sync script, then re-run the database helper. |
| The server crashes and the console window is empty | On Windows, the server's log output is buffered and lost when the process crashes. Run it under the debugger (F5) and read the **Call Stack** window when it stops. Or add `"HONUWARE_LOG_DEST": "C:\\temp\\communityfinder.log"` to the server target's `env`, which logs to a file that is flushed after every line. |
| `fatal error C1083: Cannot open include file: 'algorithm'` when building from a terminal | The shell isn't a developer prompt. Use *Developer PowerShell for VS 2026*. |
| The Conan build of `libpq` fails in Meson with an error that doesn't mention Python | The Microsoft Store `python.exe` stub. See step 1. |
| The settings in `launch_defaults.local.json` have no effect | Visual Studio has the repo root open instead of `server\communityfinder_server\`. See step 4. |
| `ng serve` loads, but pages show mock data or login does nothing | You ran `npm start` (the offline `local` configuration). Use `npx ng serve -c development` with the server running. |

## Contributing

Contributions from project collaborators are welcome. The project is licensed
under Apache-2.0 (see `LICENSE` and `NOTICE`); by submitting a contribution you
agree it is provided under those terms.

## License

Apache-2.0 — see [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE).
