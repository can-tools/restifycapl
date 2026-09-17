# restifycapl

Native Windows DLL plugin for Vector CANoe, exposing REST/HTTP operations to
CAPL scripts.

## Development setup

`scripts/setup-dev-env.ps1` provisions the local toolchain needed to build
and test this repository (MSVC Build Tools, `make`, vcpkg-built libcurl for
both architectures, and the pinned `json.hpp`). It installs software and
touches global machine state, so it should be reviewed and approved before
its first run on a given machine.

Run it from the repository root, in a PowerShell prompt **with administrator
privileges** (needed to install MSVC Build Tools if it isn't already
present — without elevation the script detects this and prints the exact
command to re-run elevated instead of silently doing nothing):

```powershell
.\scripts\setup-dev-env.ps1
```

If PowerShell refuses to run the script ("running scripts is disabled on
this system"), either scope the bypass to the current session only:

```powershell
Set-ExecutionPolicy -Scope Process -ExecutionPolicy Bypass
.\scripts\setup-dev-env.ps1
```

or bypass it for a single invocation without changing the session's policy:

```powershell
powershell -ExecutionPolicy Bypass -File .\scripts\setup-dev-env.ps1
```

The script is idempotent — safe to re-run. Switches for partial/repeat runs:

| Switch | Effect |
|---|---|
| `-SkipVsBuildTools` | Skip probing/installing MSVC Build Tools |
| `-SkipMake` | Skip probing/installing `make` |
| `-SkipVcpkg` | Skip vcpkg bootstrap / curl install / `.lib` copy |
| `-SkipJson` | Skip the `json.hpp` download/verify step |
| `-VcpkgRoot <path>` | Use an explicit vcpkg checkout instead of the default (`$env:VCPKG_ROOT`, or `%LOCALAPPDATA%\vcpkg`) |
| `-Force` | Re-download `json.hpp` even if a verified copy is already present |

See [`docs/development-environment.md`](docs/development-environment.md) for
the detailed rationale behind the script's design decisions.