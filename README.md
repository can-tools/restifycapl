# restifycapl

[![CI](https://github.com/can-tools/restifycapl/actions/workflows/ci.yml/badge.svg?branch=main)](https://github.com/can-tools/restifycapl/actions/workflows/ci.yml)
<!-- TODO: version badge once tagged releases are published; license badge once a license is chosen -->

Native Windows DLL for Vector CANoe that gives CAPL scripts synchronous and asynchronous REST/HTTP calls (libcurl + Schannel), built for x86 and x64

## Project status

> [!NOTE]
> This project is pre-1.0 and has no tagged release — the only way to get the DLL today is to build it from source (see [Building the DLL](#building-the-dll)).
>
> The CAPL compiler recognises every exported function in a real CANoe instance; this has been confirmed for the x64 build. No request has yet been run against a live server from a running measurement.
>
> JSON parsing/flattening is not available yet.

## Features

- Synchronous GET, POST, PUT, PATCH and DELETE, plus a general-purpose request function with explicit connect/total timeouts and a response-size cap.
- Asynchronous dispatch/poll/read/discard, the realtime-safe way to reach REST from a Simulation Setup node.
- TLS through Windows' own Schannel — no OpenSSL dependency.
- A single DLL: the C++ runtime (`/MT`) and libcurl are statically linked into it.
- Both x86 and x64 builds.
- TLS certificate verification is always on; it cannot be turned off from CAPL.

## Contents

- [Project status](#project-status)
- [Features](#features)
- [Why this exists](#why-this-exists)
- [Requirements](#requirements)
- [Building the DLL](#building-the-dll)
- [Usage from CAPL](#usage-from-capl)
- [Operations](#operations)
- [Development and testing](#development-and-testing)
- [Roadmap](#roadmap)
- [Changelog](#changelog)
- [Support](#support)
- [Contributing](#contributing)
- [License](#license)

## Why this exists

restifycapl gives CAPL scripts direct REST/HTTP access: blocking calls for Measurement Setup and test nodes, and a non-blocking dispatch/poll/read/discard surface for Simulation Setup nodes, where a blocking call is not allowed at all — this split is the whole reason the async surface exists (see `docs/capl-async-surface.md`'s "Realtime-safety summary"). TLS goes through Windows' own Schannel rather than bundling a separate TLS library, so the DLL stays statically linked (`/MT`) with no extra runtime dependency to ship.

## Requirements

- Windows.
- A CANoe installation whose bitness matches the DLL you load: 32-bit CANoe needs the x86 build, 64-bit CANoe needs the x64 build. CANoe refuses to load a DLL of the wrong bitness.
- To build it yourself: whatever [`scripts/setup-dev-env.ps1`](scripts/setup-dev-env.ps1) provisions — MSVC Build Tools, `make`, vcpkg-built libcurl for both architectures, and the pinned `json.hpp`.

## Building the DLL

There is no tagged release yet, so building from source is the only way to get the DLL. Running the setup script first is required on a fresh clone — no compiled dependency is committed to this repository.

Run it once, from an elevated PowerShell prompt at the repository root:

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

Then, from a matching MSVC developer command prompt, build one architecture at a time:

```shell
make build-x64
make build-x86
```

Each must be run from an MSVC developer shell for that architecture; do not run `make all` from a single shell, since it fails at the other architecture's link step. The outputs are `build/x64/restifycapl-x64.dll` and `build/x86/restifycapl-x86.dll`.

See [`docs/development-environment.md`](docs/development-environment.md) for the script's switches and the engineering rationale behind its design.

TODO: download instructions once tagged releases are published on GitHub Releases.

## Usage from CAPL

1. Choose the DLL that matches your CANoe installation's bitness.
2. Reference it from your CAPL program's includes section with `#pragma library("<path to the DLL>")`, or register it for all CAPL programs via CANoe's Options dialog. How CANoe resolves the path given to `#pragma library` can differ between CANoe versions; consult the CAPL DLL documentation for your version.

The example below calls `restifyGetSync` from an `on key` handler in a Measurement Setup or test node. It is accepted by the CAPL compiler in CANoe; it has not yet been run against a live server in a measurement.

```capl
variables
{
  char gUrl[32] = "https://example.com/";
  char gNoHeaders[1] = "";
  char gResponseBody[2048];
  long gHttpStatusCode;
  dword gResponseBodyLength;
}

on key 'a'
{
  long result;

  result = restifyGetSync(gUrl, elcount(gUrl),
                           gNoHeaders, elcount(gNoHeaders),
                           gResponseBody, elcount(gResponseBody),
                           gHttpStatusCode, gResponseBodyLength);

  if (result == 0)
  {
    write("restifyGetSync ok: HTTP %ld, %lu bytes", gHttpStatusCode, gResponseBodyLength);
  }
  else
  {
    write("restifyGetSync failed: status %ld", result);
  }
}
```

TODO: expected output once verified in a running measurement.

`https://example.com/` is a reserved example domain here, not a project endpoint. Every text argument is a declared `char[]` variable passed with `elcount()` as its size — string literals cannot be passed directly as `char[]` arguments in CAPL. An empty header block is passed as a one-element array holding an empty string (`gNoHeaders` above). The full argument rules, including the header-block grammar, are in [`docs/capl-sync-surface.md`](docs/capl-sync-surface.md); the async surface follows the same rules.

> [!IMPORTANT]
> **Realtime caveat.** Synchronous calls, `restifyAwaitResponse`, and `restifyDiscardAllResponses` block or free memory on the calling thread. Use them only from Measurement Setup or test nodes — never from a Simulation Setup node. From a Simulation Setup node, dispatch asynchronously (e.g. `restifyGetAsync`), poll with `restifyPollResponse` from an `on timer` handler, then read with `restifyReadResponse`.

<details>
<summary>Async outline (dispatch, poll, read, discard)</summary>

1. Dispatch a request (`restifyGetAsync`, `restifyPostAsync`, ...) to get back a `requestId`.
2. Poll it from an `on timer` handler with `restifyPollResponse` until it reports complete.
3. Read the result with `restifyReadResponse`.
4. Release its slot with `restifyDiscardResponse`, or release every slot at once with `restifyDiscardAllResponses` from `on stopMeasurement` or a test node — it frees memory on the calling thread, so it is not realtime-safe.

See [`docs/capl-async-surface.md`](docs/capl-async-surface.md) for the full signature table, state machine and status codes.

</details>

TODO: link to runnable examples/ once they exist.

## Operations

**Common**

| Operation | Purpose |
|---|---|
| `restifyReadVersion` | Returns this DLL's build-version string. |

**Sync** (blocking — Measurement Setup or test nodes only)

| Operation | Purpose |
|---|---|
| `restifyGetSync` | Blocking HTTP GET. |
| `restifyDeleteSync` | Blocking HTTP DELETE. |
| `restifyPostSync` | Blocking HTTP POST with a request body. |
| `restifyPutSync` | Blocking HTTP PUT with a request body. |
| `restifyPatchSync` | Blocking HTTP PATCH with a request body. |
| `restifyRequestSync` | Blocking HTTP request for any method, with explicit timeouts and a response-size cap. |

**Async** (non-blocking dispatch, safe from Simulation Setup unless noted)

| Operation | Purpose |
|---|---|
| `restifyGetAsync` | Dispatches a non-blocking HTTP GET. |
| `restifyDeleteAsync` | Dispatches a non-blocking HTTP DELETE. |
| `restifyPostAsync` | Dispatches a non-blocking HTTP POST with a request body. |
| `restifyPutAsync` | Dispatches a non-blocking HTTP PUT with a request body. |
| `restifyPatchAsync` | Dispatches a non-blocking HTTP PATCH with a request body. |
| `restifyRequestAsync` | Dispatches a non-blocking HTTP request for any method, with explicit timeouts and a response-size cap. |
| `restifyPollResponse` | Checks, without blocking, whether a dispatched request is still running or has completed. |
| `restifyAwaitResponse` | Blocks until a dispatched request completes or a timeout elapses (not realtime-safe). |
| `restifyReadResponse` | Copies a completed response's body into the caller's buffer, without blocking. |
| `restifyDiscardResponse` | Releases a single dispatched request's slot, without blocking. |
| `restifyDiscardAllResponses` | Releases every dispatched request's slot at once; frees memory on the caller's thread (not realtime-safe). |

Full signature tables and status codes: [`docs/capl-sync-surface.md`](docs/capl-sync-surface.md), [`docs/capl-async-surface.md`](docs/capl-async-surface.md), [`docs/status-codes.md`](docs/status-codes.md).

TODO: JSON operations are not available yet.

## Development and testing

`make test` builds and runs the GoogleTest suite outside CANoe (defaults to the x64 architecture). CI builds and tests both x86 and x64 on every push.

See [`CLAUDE.md`](CLAUDE.md) for the full directory layout and project conventions.

## Roadmap

Planned, non-conditional work:

- JSON flattening — turn an HTTP response body into a flat, CAPL-addressable representation.
- Typed JSON path accessors — read individual values out of a flattened JSON response by path, with an explicit type.
- Tagged releases — publish built DLLs (x86 and x64) as downloadable GitHub Releases assets, instead of build-from-source being the only way to get the DLL.

## Changelog

See [`CHANGELOG.md`](CHANGELOG.md) for a dated, tag-linked history of changes.

## Support

Use [GitHub Issues](https://github.com/can-tools/restifycapl/issues) for this repository.

## Contributing

Currently maintained by a single maintainer; pull requests are welcome and will be reviewed. See [`CLAUDE.md`](CLAUDE.md) for project conventions. Please note that no license has been chosen yet (see [License](#license)).

## License

> No license has been chosen yet — `LICENSE` is a placeholder. Until one is added, no permission to use, modify or redistribute this code is granted beyond what GitHub's Terms of Service allow for public repositories. TODO: choose a license.

This DLL bundles or statically links third-party components, each under its own upstream license: libcurl, nlohmann/json, and the Vector CAPL DLL SDK headers.
