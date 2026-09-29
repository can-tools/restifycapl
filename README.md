# restifycapl

Native Windows DLL plugin for Vector CANoe, exposing REST/HTTP operations to
CAPL scripts running inside CANoe.

## Build

Built with MSVC via a Makefile (no CMake), targeting both x86 and x64. See
[`CLAUDE.md`](CLAUDE.md#build) for the exact commands (`make build-x86`,
`make build-x64`, `make test`) and toolchain requirements.

Synchronous HTTP is available from CAPL today: `restifyGetSync`,
`restifyPostSync`, `restifyPutSync`, `restifyPatchSync`, `restifyDeleteSync`,
and the general-purpose `restifyRequestSync` (see
[`docs/capl-sync-surface.md`](docs/capl-sync-surface.md) for the full
signature table).

Asynchronous HTTP is also available, for use from a Simulation Setup node
without blocking the realtime thread: `restifyGetAsync`, `restifyPostAsync`,
`restifyPutAsync`, `restifyPatchAsync`, `restifyDeleteAsync`, and the
general-purpose `restifyRequestAsync`, paired with `restifyPollResponse`,
`restifyAwaitResponse`, `restifyReadResponse`, `restifyDiscardResponse`, and
`restifyDiscardAllResponses` to track and retrieve each dispatched request
(see [`docs/capl-async-surface.md`](docs/capl-async-surface.md) for the full
signature table).

## Using the DLL from CAPL

Pick the DLL matching your CANoe installation's bitness: the x86 build
(`restifycapl-x86.dll`) for a 32-bit CANoe, the x64 build
(`restifycapl-x64.dll`) for a 64-bit CANoe. CANoe refuses to load a DLL
built for the wrong bitness.

Reference the DLL from a CAPL program's includes section with
`#pragma library`, then call an exported operation like any other CAPL
function:

```capl
#pragma library("restifycapl-x64.dll")

variables
{
  char gVersion[64];
}

on start
{
  restifyReadVersion(gVersion, elcount(gVersion));
  write("restifycapl version: %s", gVersion);
}
```

CANoe 13 and later resolve the `#pragma library` path via a module
description file rather than a direct DLL path; consult your CANoe
version's own CAPL DLL documentation for that file's format. Earlier CANoe
versions accept a direct path to the DLL, as shown above. A DLL can
instead be registered globally for all CAPL programs via the Options
dialog rather than `#pragma library` — see CANoe's own CAPL DLL
documentation for both mechanisms.

Both architectures build and are tested through CI on every push. CANoe
load/recognition of the exported functions above has so far been confirmed
against a real CANoe instance for the x64 build only.

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
