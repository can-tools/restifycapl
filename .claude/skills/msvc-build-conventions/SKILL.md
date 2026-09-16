---
name: msvc-build-conventions
description: MSVC build rules for the CAPL REST DLL — runtime library, architecture targets, dependency and versioning conventions. Load this before touching the Makefile, build scripts, GitHub Actions workflow, or adding a dependency.
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

## Versioning

- The version number currently appears in three places: `version.rc`, and
  the linker `/VERSION:` flag in both `build32` and `build64`. These must be
  kept in sync manually until/unless the project decides to consolidate
  them into a single source of truth — do not consolidate this without
  explicit approval, since it changes the build script structure.

## CI/CD

- The GitHub Actions workflow must invoke the same `build32`/`build64`
  Make targets used locally — do not duplicate the `cl.exe`/`rc.exe`
  invocation directly in YAML.
- CI runs on `windows-latest` and must activate the MSVC developer
  environment (`vcvarsall.bat` or an equivalent action) for the matching
  architecture before invoking Make.
- Do not introduce CMake or replace the Makefile-based build unless
  explicitly asked to.
