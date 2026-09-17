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
- `all` builds both; `test` and `clean` follow GNU target conventions.
- Both architecture targets must be thin wrappers over a **single
  parameterized rule**, with the architecture passed as a Make variable.
  Do not write two parallel recipes — flag drift between x86 and x64 is a
  top project risk, and one shared rule makes it structurally impossible.
- The two targets stay behaviorally identical: same source files, same
  flags (`/std:c++17`, `/EHsc`, `/MT`), same export table — only the
  architecture flag and library path differ.

## Directory conventions

```
lib/x86/, lib/x64/       static dependencies (.lib), built with /MT
build/x86/, build/x64/   build output (.dll, .lib, .exp, .res) — gitignored
include/vendor/          third-party headers (json.hpp, capl-dll-sdk/)
scripts/                 setup-dev-env.ps1 — environment bootstrap only,
                         never a second build system
```

## Versioning — single source of truth: the Git tag

No version number is ever hand-edited in any file. The mechanism:

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

- libcurl: vcpkg, `curl[schannel]:x86-windows-static` and
  `curl[schannel]:x64-windows-static`. Static triplets are `/MT` by
  default — verify with `dumpbin /directives`, expecting
  `/DEFAULTLIB:LIBCMT` and never `MSVCRT`.
- zlib arrives transitively with libcurl.
- nlohmann/json: `json.hpp` pinned to v3.11.3, taken from the Releases page
  (amalgamated single file) and SHA-256 verified. Not `git clone`.
- Windows system libs, always link all of them: crypt32, bcrypt, secur32,
  ws2_32, normaliz, wldap32, advapi32.
- Record exact versions in `lib/README`.
- Toolchain: Visual Studio Build Tools with the `VCTools` workload; the
  full VS IDE is not required.
