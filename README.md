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

## Running in Visual Studio (Windows)

Open `server/communityfinder_server/` as a CMake folder and let it configure with
the `x64-Debug` preset. Conan installs the dependencies on the first configure.
The targets connect to the shared dev PostgreSQL container, so start it first
(see `database_server/`).

Visual Studio keeps each target's command-line `args` and `env` in
`.vs/launch.vs.json`. That file is gitignored and disposable, so **don't edit it
by hand**. Instead, keep the values in `launch_defaults.local.json` (also
gitignored, because it holds credentials) and let honuware's
`sync_launch_targets.ps1` write them into `launch.vs.json`. Without this step
every debug target launches bare: `communityfinder_database_helper` gets no
`--recreate_database`, and nothing gets the database connection variables.

After the first configure, FetchContent has placed honuware's copy of the script
and an example defaults file in the build tree. You don't need a separate
server_components checkout. From `server/communityfinder_server/`:

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
`targets` match the target label as a wildcard. The example file is already a
working local setup:

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
        "HONUWARE_MAIL_APP_PASSWORD": "<your 16-character gmail app password>"
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
- `HONUWARE_MAIL_APP_PASSWORD` is optional locally. It is read only at seed time
  (`--recreate_database`). If it's unset, the seed still succeeds but outgoing
  mail fails. Use your own Gmail app password and never copy someone else's
  `launch_defaults.local.json`. honuware's README ("Getting a Gmail app password")
  explains how to get one.
- `SCHEDULER_SERVICE_ACCOUNT_PASSWORD` can be any non-empty value locally. It is
  set under `all` because the seed and the scheduler both read it and their values
  must match.

To change a value, edit `launch_defaults.local.json` and **re-run the script**.
Also re-run it after a new executable target appears or after `.vs/` has been
cleared. Values in the defaults file override what's in `launch.vs.json`. Pass
`-WhatIf` to preview the result without writing it. If Visual Studio doesn't pick
up the change, close and reopen the folder.

Run `communityfinder_database_helper` once to create and seed the `communityfinder`
database, then run the server (it listens on port **18081**). The full list of
environment variables is in `CLAUDE.md` under *Environment variables*.

## Contributing

Contributions from project collaborators are welcome. The project is licensed
under Apache-2.0 (see `LICENSE` and `NOTICE`); by submitting a contribution you
agree it is provided under those terms.

## License

Apache-2.0 — see [`LICENSE`](LICENSE) and [`NOTICE`](NOTICE).
