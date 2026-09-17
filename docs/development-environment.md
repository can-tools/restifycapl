# Development environment bootstrap

This document holds the detailed engineering rationale behind
`scripts/setup-dev-env.ps1`: non-obvious design decisions, what was tried
and rejected, and why. The script itself only carries short pointers back
to the sections below — keep this page as the single place the full story
lives, so the two never drift out of sync.

`setup-dev-env.ps1` provisions the environment only (MSVC Build Tools,
`make`, vcpkg-built libcurl for both architectures, the pinned
`json.hpp`). It never compiles project sources and must never grow into a
second build system — `make` owns the build, this script only makes sure
`make`'s dependencies are present. See
`docs/work/capl-rest-dll-rebuild/plans/plan.md` (Stage 2, task BPE-1) for
the authoritative spec this script implements.

## json.hpp SHA-256 verification

nlohmann/json v3.11.3's amalgamated `json.hpp` is pinned by SHA-256, not
taken on trust from any third-party listing. The hash in the script was
independently computed on 2026-09-17 from two sources that had to agree
before being trusted:

- the official GitHub Release asset:
  `https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp`
- the raw v3.11.3 tag content:
  `https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp`

Both downloads were byte-identical (919975 bytes), hashed to the same
value, and the file's own `NLOHMANN_JSON_VERSION_{MAJOR,MINOR,PATCH}`
macros read `3.11.3`.

A commonly-quoted checksum for this file
(`a22461d13119ac5c78f205d3df1db13403e58ce1bb1794469ccb779b737395bc`) does
**not** match either official download and must not be used. If this pin
is ever bumped to a new version, re-derive the hash from both sources
above and confirm they agree before changing the constant in the script.

## curl: no `[schannel]` vcpkg feature

The script installs `curl:$Triplet` with no explicit feature list. It
deliberately does not request `curl[schannel]:$Triplet`.

The current vcpkg `curl` port (checked against port version curl
8.22.0-1) has no feature named `schannel` — running
`vcpkg install curl[schannel]:...` fails outright with "curl has no
feature named schannel." SChannel is wired in automatically instead:
curl's default features include `ssl`, and on Windows (non-UWP),
`portfile.cmake` adds `-DCURL_USE_SCHANNEL=ON` whenever `ssl` is enabled
and `http3` is not. Plain `curl:$Triplet` therefore already builds
against SChannel with no OpenSSL dependency, matching the intent in the
`msvc-build-conventions` skill.

If a future vcpkg port revision reintroduces an explicit opt-in feature
for this, re-add it in `Install-CurlTriplet` and update
`msvc-build-conventions` accordingly.

## Native Tools architecture detection (`Test-NativeToolsArch`)

This function confirms that `cl`/`rc`/`link`/`dumpbin` resolve under each
architecture's Native Tools environment, and that the environment is
actually initialized for the architecture requested — not a stale or
wrong-architecture one.

**Why not parse the `cl.exe` banner text.** An earlier version matched
English substrings like `"for x86"`/`"for x64"` in `cl`'s version banner.
That banner is localized to the MSVC install's UI language — on a
Polish-language Build Tools install, `cl` prints `"...dla architektury
x86"` / `"...dla x64"`, neither of which contains the English substring
being matched. This was confirmed to false-FAIL on a real,
correctly-configured install. Matching localized text is inherently
locale-fragile and was rejected.

**Why `%VSCMD_ARG_TGT_ARCH%` instead.** `vcvarsall.bat` sets this
variable itself — it drives the rest of vcvarsall's own logic — so it is
never translated, and its value is exactly `x86` or `x64`: an
authoritative, locale-independent signal for which architecture the
environment was actually initialized for.

**Why delayed expansion (`!VAR!`) and `cmd.exe /v:on`, not plain
`%VAR%`.** `cmd.exe` expands every `%VAR%` in a compound command line
(commands chained with `&&`) once, at parse time, before any command in
that line has actually run. With plain `%VSCMD_ARG_TGT_ARCH%`, the
variable is substituted before `call vcvarsall.bat` has had a chance to
set it; since it's undefined at parse time, cmd.exe leaves the literal
text `%VSCMD_ARG_TGT_ARCH%` in place rather than an empty string. This
was confirmed empirically and is exactly the false-FAIL text the
function used to produce.

`/v:on` turns on delayed expansion for the whole invocation *before*
parsing begins, so `!VSCMD_ARG_TGT_ARCH!` is resolved lazily, at
execution time, after `vcvarsall.bat` has actually run.

**Why not `setlocal enabledelayedexpansion` mid-line instead.** The
compound line is still parsed as a whole before any command in it
executes, so turning on delayed expansion partway through the same line
does not retroactively change how the rest of that already-parsed line
resolves `!VAR!` references. This was confirmed empirically to still
return the literal `!VSCMD_ARG_TGT_ARCH!` text. `/v:on` on the `cmd.exe`
invocation itself is the only fix that works for this single-line
chained shape.

## `where` probes chained with bare `&`, not `&&`

`where cl & where rc & where link & where dumpbin` are joined with bare
`&`, not `&&`, so all four always run regardless of whether an earlier
one failed to resolve.

This means `$LASTEXITCODE` after the chain only reflects the *last*
command (`where dumpbin`), not a logical AND of all four — so on its own
it is **not** a meaningful "all four resolved" signal. That's fine
because it's never used alone: the loop right after the call
independently and authoritatively re-checks that all four tool names
appear in the captured output text, regardless of exit code.

Running every `where` unconditionally (rather than short-circuiting on
the first failure) is what makes that per-tool re-check possible: if an
early tool fails to resolve, the later ones still run and still get
checked, so a single pass reports every missing tool instead of only the
first one encountered.

The architecture check (`VSCMD_ARG_TGT_ARCH`, see above) is deliberately
a separate `cmd.exe` invocation from the `where` probes, so a
tool-resolution failure and an architecture mismatch are reported as
distinct, unambiguous reasons rather than conflated into one command's
exit code.

## `make` installer preference order

When `make` is not already on `PATH`, the script tries installers in
this order, a judgment call rather than something mandated by the
project plan:

1. **Scoop** — per-user installs, never needs elevation.
2. **an existing MSYS2 install** (via `pacman`) — common on dev machines
   already, no extra package manager to bootstrap.
3. **Chocolatey** — ubiquitous but installs to a machine-wide location
   and needs elevation.
4. Fall back to printing exact manual install commands for all three.

The rationale is minimizing both elevation prompts and the number of new
package managers introduced on a machine that doesn't already have one
for this purpose.

## Human approval gate

The script installs software and touches global machine state (VS Build
Tools, `make`, vcpkg packages). Per the project plan it must be approved
before its first run on a given machine.
