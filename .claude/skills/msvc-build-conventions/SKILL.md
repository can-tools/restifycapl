---
name: msvc-build-conventions
description: MSVC build rules for the CAPL REST DLL — runtime library, architecture targets, dependency and versioning conventions. Load this before touching the Makefile, build scripts, GitHub Actions workflow, version.rc, or adding a dependency.
---

# MSVC build conventions

## Runtime library: /MT is mandatory

- The project is built with `/MT` (static, multithreaded CRT), not `/MD`.
- Every statically linked dependency (libcurl, zlib, GoogleTest, and any
  future dependency) must also be built with `/MT`. Mixing `/MT` and `/MD`
  objects in the same link produces `LNK4098` warnings at best, and can
  result in the DLL holding a separate CRT heap/state from the CANoe host
  process at worst.
- If a new dependency is only available as a prebuilt `/MD` library, do not
  link it in silently — flag the mismatch and ask before proceeding.

## Architecture targets

- `build-x86` compiles with `/MACHINE:X86`, links against `lib/x86/`,
  produces `build/x86/restifycapl-x86.dll`.
- `build-x64` compiles with `/MACHINE:X64`, links against `lib/x64/`,
  produces `build/x64/restifycapl-x64.dll`.
- `all` builds both; bare `make` runs `help` (the default goal) and builds
  nothing; `test` and `clean` follow GNU target conventions.
- Both architecture targets must be thin wrappers over a **single
  parameterized rule**, with the architecture passed as a Make variable.
  Do not write two parallel recipes — flag drift between x86 and x64 is a
  top project risk, and one shared rule makes it structurally impossible.
- The two targets stay behaviorally identical: same source files, same
  flags (`/std:c++17`, `/EHsc`, `/MT`), same export table — only the
  architecture flag and library path differ.

## Directory conventions

```
lib/x86/, lib/x64/       static dependencies (.lib), built with /MT —
                         linked into the product DLL
lib/gtest/x86/, x64/     GoogleTest (.lib), built with /MT — build-time-only,
                         linked into the test executable, NEVER the DLL
build/x86/, build/x64/   build output (.dll, .lib, .exp, .res) — gitignored
include/vendor/          third-party headers (json.hpp, capl-dll-sdk/,
                         gtest/ — the latter is test-only, same rule as
                         lib/gtest/ above)
scripts/                 setup-dev-env.ps1 — environment bootstrap only,
                         never a second build system
```

**`.gitkeep` lifecycle.** A `.gitkeep` is removed in the same change that
adds the first real tracked file to its directory. It is never swept
separately, never left behind "to clean up later," and never removed from
a directory that is still empty. `lib/gtest/x86/`, `lib/gtest/x64/`
(gitignored, matching `lib/x86/`/`lib/x64/`) never need a `.gitkeep` at
all — `setup-dev-env.ps1` creates all four directories on every run.

## Versioning — single source of truth: the Git tag

No version number is ever hand-edited in any file. The mechanism:

`vcpkg.json`'s `"version-string": "0.0.0"` is manifest boilerplate required by
vcpkg's own package format, not a version source — it feeds nothing here (not
`version.rc`, not the Makefile's `VER_*`, not `/VERSION:`); the product
version is derived solely from the Git tag below.

**Release builds (GitHub Actions, triggered by pushing a tag).**
The tag format is `vX.Y.Z` (e.g. `v2.5.0`) — this is the dominant
convention across the Git/GitHub ecosystem, not specific to C++. The
workflow extracts `X.Y.Z` from the tag (`github.ref_name`, stripping the
`v` prefix) and passes it into the build as environment variables/Make
variables. This value is used for both:
- the linker `/VERSION:X.Y` flag (only accepts major.minor — two numbers),
- the `FILEVERSION`/`PRODUCTVERSION` fields in `version.rc` (four 16-bit
  numbers, `major,minor,build,revision`, each capped at 65535 — values
  wrap silently if exceeded, so never feed anything but small integers
  here).
On a clean tag, the numeric fields and the human-readable string fields
end up identical — there's no divergence to manage for release builds.

**Local/development builds (no tag context).**
- Numeric fields (`/VERSION:`, `FILEVERSION`/`PRODUCTVERSION`) get a
  placeholder major.minor (e.g. `0.0`), with `build`/`revision` filled by
  the number of commits since the last tag
  (`git rev-list --count <last-tag>..HEAD`, or the count segment from
  `git describe --tags --long`). This keeps the numeric fields both valid
  and meaningfully incrementing, without depending on GitHub Actions
  context (so it works identically locally and in CI).
- The free-text version string in `version.rc`'s `StringFileInfo` block is
  NOT constrained to numbers — use the full output of
  `git describe --tags --always --dirty` there (e.g.
  `v2.4-3-gabc1234-dirty`) for an unambiguous, human-readable build
  identifier visible in the file's Windows properties dialog.
- Use `--tags` (not just annotated tags) so lightweight release tags are
  picked up, and `--always` so the command never fails if no tag exists yet
  in the repo's history.

**Why commit-count-since-tag over CI run numbers.** `github.run_number` is
simpler but only exists inside GitHub Actions — using it would require a
second, different way of computing a build identifier for local builds.
Commit count from Git works identically in both contexts with one code
path, which is why it's the standard choice here.

**version.rc parameterization.** `FILEVERSION`/`PRODUCTVERSION` cannot
contain a git-describe string directly (numeric only). Pass numeric parts
into `rc.exe` via `/D` preprocessor defines
(e.g. `rc.exe /D VER_MAJOR=2 /D VER_MINOR=5 /D VER_BUILD=0 /D VER_REV=3`),
with `version.rc` providing safe defaults (`#ifndef VER_MAJOR #define
VER_MAJOR 0 #endif`, etc.) for the rare case it's invoked without them.

**Ownership.** `build-pipeline-engineer` owns this mechanism end to end.
No other agent edits `version.rc`, the `/VERSION:` flags, or anything
related to version numbers. `code-reviewer` flags any hardcoded version
number found anywhere as a bug.

## Local builds — the only sanctioned invocation

- Building and testing locally is only ever done via `make <target>`
  (`make build-x86`, `make build-x64`, `make test`, `make clean`). No agent
  other than `build-pipeline-engineer`, doing actual build-system
  maintenance, may author, edit, or invoke a standalone build script, batch
  file, or direct `cl.exe`/`link.exe`/`rc.exe` invocation.
  `build-pipeline-engineer` owns the build mechanism end to end; every
  other agent only ever reaches it through `make`.
- Both the activated MSVC developer environment and the working directory
  are process state: neither survives from one shell invocation to the
  next, and each Bash tool call starts a fresh process from a reset cwd.
  Activating the environment in one call and running `make` in a separate
  later call fails with `'cl' is not recognized as an internal or external
  command` — that failure means the activation from the earlier call never
  carried over to this one, **not** that the build itself is broken.
  Environment activation and the `make` invocation must always happen in
  one call.
- Verified working form: feed a short script to `cmd.exe` over stdin in a
  single call. Piping into `cmd.exe` (rather than passing the whole thing
  as a `cmd /c "..."` string) sidesteps a real, reproduced failure mode —
  nested double quotes inside a `cmd /c` argument get mangled by the
  calling shell before `cmd.exe` ever sees them, breaking `for /f`'s own
  quoting.

  x86:
  ```
  cmd.exe <<'EOF'
  @echo off
  for /f "tokens=*" %i in ('"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath') do set VSINSTALL=%i
  call "%VSINSTALL%\VC\Auxiliary\Build\vcvarsall.bat" x86
  cd /d C:\Workspace\restifycapl
  make build-x86
  EOF
  ```

  x64: identical, with `x86` replaced by `x64` in the `vcvarsall.bat`
  argument, and the final line replaced by the target actually wanted
  (`make build-x64`, `make test ARCH=x64`, `make clean`, etc.).

  `vswhere.exe` locates the Visual Studio install rather than hardcoding
  `VC\Auxiliary\Build\vcvarsall.bat`'s full path, so this keeps working
  across VS installs/updates. Both `make build-x86` and `make build-x64`
  (and `make test ARCH=x64`) have been run end-to-end through this exact
  form and produced a working DLL / a passing test suite.
- `cd /d` to the repo root explicitly inside the same block — it cannot be
  assumed from the calling agent's prior state, for the same reason the
  environment can't be.
- A line reading `'vswhere.exe' is not recognized as an internal or
  external command` printed immediately after the `call vcvarsall.bat`
  line is harmless noise from `vcvarsall.bat`'s own internal probing, not
  a sign this incantation failed — confirmed by checking `where cl`
  immediately afterward, which still resolves to the correct
  architecture's `cl.exe`. Do not treat that line as a build failure.
- Once `scripts/setup-dev-env.ps1` has stored `RESTIFY_MSVC_X64_*` /
  `RESTIFY_MSVC_X86_*` as user environment variables, the Makefile sets
  `PATH`/`INCLUDE`/`LIB`/`LIBPATH` from the set matching `ARCH`, so
  `make <target>` works from any shell opened afterwards. Without them (CI,
  a machine where the script has not run) the shell's own activated
  environment is used; the cmd.exe activation form above stays valid. The
  Makefile itself never calls `vcvarsall.bat` and checks nothing.

## Local vs CI verification

- Local verification defaults to x64 only: `make build-x64`, `make test`
  (which defaults to `ARCH=x64`). x86 coverage comes from CI's build/test
  matrix, which must be green on both legs for a given commit before any
  human sign-off gate (e.g. HUM-15). Building or testing x86 locally is the
  exception, not the default — do it only to reproduce an x86-only CI
  failure, or when the dispatch explicitly asks for it (e.g. the user
  doesn't want to push before a human gate). State which architecture(s)
  a given local result actually covers.
- Without the setup-stored variables, one shell only ever has one
  architecture's toolchain active — see "Local builds" above — so running
  both `make build-x86` and `make build-x64` requires two separately
  activated shells, and `make all` in a single shell fails at the
  mismatched architecture's link step. With the variables, both targets
  and `make all` work from one shell.
- Ad-hoc compiler/shell probes (a one-off `cl.exe` invocation to check a
  symbol, a syslib list, or similar — not a `make` target) must run from
  the session's scratch/temp directory, with explicit output paths
  (`/Fo`, `/Fe`) or redirection targets, never with the repo root as the
  working directory. A probe run from the repo root with implicit output
  paths leaves stray `.obj`/`.tmp` files behind that `make clean` doesn't
  know about and that don't match `build/`'s `/Fo"$@"` convention.

## CI/CD

- The GitHub Actions workflow must invoke the same `build-x86`/`build-x64`
  Make targets used locally — do not duplicate the `cl.exe`/`rc.exe`
  invocation directly in YAML.
- CI runs on `windows-latest` and must activate the MSVC developer
  environment (`vcvarsall.bat` or an equivalent action) for the matching
  architecture before invoking Make.
- Do not introduce CMake or replace the Makefile-based build unless
  explicitly asked to.

## Dependency acquisition

- libcurl: vcpkg, `curl:x86-windows-static` and `curl:x64-windows-static`.
  Do not add an explicit `[schannel]` feature — the current vcpkg `curl`
  port has no feature by that name (`vcpkg install curl[schannel]:...`
  fails with "curl has no feature named schannel"); SChannel is wired in
  automatically by the port's default `ssl` feature on Windows
  (non-UWP), which sets `-DCURL_USE_SCHANNEL=ON`, so plain `curl:<triplet>`
  already gets SChannel with no OpenSSL dependency. Static triplets are
  `/MT` by default — verify with `dumpbin /directives`, expecting
  `/DEFAULTLIB:LIBCMT` and never `MSVCRT`.
- zlib arrives transitively with libcurl. Its import library is named
  `zs.lib`, not `zlib.lib` — not guessable from the port name, and it is
  the name the Makefile's `LIBS` must use.
- nlohmann/json: `json.hpp` pinned to v3.11.3, taken from the Releases page
  (amalgamated single file) and SHA-256 verified. Not `git clone`.
- GoogleTest: vcpkg, `gtest:x86-windows-static` and `gtest:x64-windows-static`
  — `/MT` by default, same as libcurl. Build-time-only (the test executable
  links it; the product DLL never does), so it is copied into a separate
  `lib/gtest/x86/`, `lib/gtest/x64/` location, never mixed into `lib/x86/`,
  `lib/x64/`. The vcpkg `gtest` port's own patch relocates any `_main`
  target: `gtest.lib` lands in the triplet's plain `lib/`, but
  `gtest_main.lib` lands in a separate `lib/manual-link/` — the bootstrap
  script copies from both source directories, so both files end up
  together under `lib/gtest/<arch>/`.
- Windows system libs, always link all of them: crypt32, bcrypt, secur32,
  ws2_32, normaliz, wldap32, advapi32, iphlpapi (libcurl's IPv6 scope-ID
  handling needs iphlpapi; the linker error only surfaces once a real
  curl symbol is referenced, so confirm this list by linking, not by
  inspection, after any dependency refresh).
- `scripts/setup-dev-env.ps1` writes the resolved versions into `lib/README`
  on every run. That file is local, generated and deliberately untracked
  (see `.gitignore`) — per-environment output, not a repo document. Never
  commit it, never hand-edit it, and never treat it as a version source:
  the authoritative pin is `vcpkg.json`.
- Toolchain: Visual Studio Build Tools with the `VCTools` workload; the
  full VS IDE is not required.
