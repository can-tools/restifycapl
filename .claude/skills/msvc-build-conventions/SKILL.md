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

- `build32` compiles with `/MACHINE:X86`, linking against `lib-32b/`.
- `build64` compiles with `/MACHINE:X64`, linking against `lib-64b/`.
- The two targets must stay behaviorally identical: same source files, same
  compiler flags (`/std:c++17`, `/EHsc`, `/MT`), same export table — only
  the architecture-specific flags and library paths differ.
- Any change made to one target's flags or source list must be mirrored in
  the other, unless the change is intentionally architecture-specific.

## Directory conventions

```
lib-32b/, lib-64b/     static dependencies (.lib), built with /MT
build-32b/, build-64b/ build output (.dll, .lib, .exp, .res)
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

- The GitHub Actions workflow must invoke the same `build32`/`build64`
  Make targets used locally — do not duplicate the `cl.exe`/`rc.exe`
  invocation directly in YAML.
- CI runs on `windows-latest` and must activate the MSVC developer
  environment (`vcvarsall.bat` or an equivalent action) for the matching
  architecture before invoking Make.
- Do not introduce CMake or replace the Makefile-based build unless
  explicitly asked to.
