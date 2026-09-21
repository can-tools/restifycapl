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
8.21.0#1, the version this project's `builtin-baseline` resolves to as of
BPE-25 -- see "Pinning the vcpkg tool itself" below) has no feature named
`schannel` — running
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

## Manifest-mode triplet installs need separate install roots

**The bug, exactly as first observed (HUM-19's first real re-run,
2026-09-18).** A real run of `setup-dev-env.ps1` produced 16 OK and 3 FAIL,
all three on the x86 side: `lib copy (x86-windows-static)`,
`gtest lib copy (x86-windows-static)`, and `gtest headers` all failed with
"Expected lib/header directory not found", even though the
`vcpkg install (x86-windows-static)` step immediately before each of them
had reported `[OK]`. x64 succeeded completely, including a real
`lib copy (x64-windows-static)` producing `gmock.lib`, `gtest.lib`,
`libcurl.lib`, `zs.lib` (see the next section for why `gmock.lib` showing up
there was itself a separate, real bug).

**Root cause, found by running `vcpkg install` directly, not by reading
code.** The script calls `vcpkg install --triplet=x86-windows-static` and
then, immediately after, `vcpkg install --triplet=x64-windows-static` --
both against one shared `$RepoRoot\vcpkg_installed\` directory (the
default install root vcpkg picks when cwd is at the manifest root).
Running the exact same two commands directly, outside the script, in the
same order the script itself uses -- x86 first, x64 second -- showed the
*second* call's (x64) own console output announcing:

```
The following packages will be removed:
    curl:x86-windows-static
    gtest:x86-windows-static
    zlib:x86-windows-static
```

when installing **x64-windows-static** -- i.e. installing the second
triplet deleted the *first* triplet's already-installed packages,
including removing `vcpkg_installed\x86-windows-static\` from disk
entirely. Manifest mode treats the installed tree at a given root as a
closure that must exactly match what the currently-requested `--triplet`
resolves to; anything belonging to a different triplet under that same
root is treated as extraneous and pruned. The on-disk layout
(`installed/<triplet>/...`) looks like it's designed for multiple triplets
to coexist side by side under one root, and does not warn you otherwise --
it isn't, at least not across separate `vcpkg install` invocations against
a shared root, for the vcpkg build in use here (`vcpkg version` =
2026-07-27-...). This is exactly why the FAIL always landed on x86: the
script calls x86 first, then x64; x64's install silently deleted what
x86's install had just produced, and the copy step for x86 ran (and
correctly failed) only after that.

**The fix.** Give each triplet its own install root via vcpkg's
`--x-install-root=<path>` flag (marked "(experimental)" in
`vcpkg install --help`, but this project's target is 100% scripted,
version-pinned local/CI environments, not arbitrary end-user vcpkg usage,
so relying on it is an acceptable tradeoff -- re-check if a future vcpkg
upgrade renames or removes the flag; see
[Pinning the vcpkg tool itself](#pinning-the-vcpkg-tool-itself) below for
how the *other* half of that risk -- a future vcpkg release silently
changing the flag's behavior instead of removing it -- is mitigated):
`vcpkg_installed-x86\` for `x86-windows-static`, `vcpkg_installed-x64\` for
`x64-windows-static`. Verified directly: running both installs with
separate `--x-install-root` values left both triplet directories intact
afterward, with no removal messages for either. `vcpkg list` needs the
matching `--x-install-root` too, for the same reason (it otherwise
resolves against its own default root, which no longer matches once
installs use separate roots).

**Why the previous "[OK]" was a real gap, not just bad luck.** The
`vcpkg install` step's success was reported purely from `$LASTEXITCODE`.
That exit code was, in isolation, accurate for the one command that
produced it -- the real defect was that a *later*, seemingly unrelated step
silently invalidated it. `Install-ManifestTriplet` now also verifies the
triplet's `lib/` directory genuinely exists immediately after installing,
before reporting `[OK]`, as defence in depth against this class of gap
recurring for a different reason in the future.

## gmock.lib: real copy-noise, not a link-time leak

The x64 run above copied `gmock.lib` (and `gtest.lib`) into the
product-linked `lib/x64/`, alongside the intended `libcurl.lib`/`zs.lib`.
Confirmed by reading `Copy-TripletLibs`'s previous implementation: it
copied everything matching `*.lib` in the triplet's `lib/` directory with
no name filter, and under manifest mode curl, zlib, gtest and gmock (gtest
pulls in gmock as part of the same googletest source tree -- true of the
`1.18.0` port resolved at the time of this incident, and equally true of
the `1.17.0#3` port this project's `builtin-baseline` resolves to as of
BPE-25, see "Pinning the vcpkg tool itself" below) all land together in
one shared `<triplet>/lib/` -- unlike classic mode, where each `vcpkg
install <port>:<triplet>` call touched a narrower set.

**This was not an actual `/MT` or link-time contamination.** The
Makefile's `LIBS` variable names `libcurl.lib zs.lib` explicitly (plus the
seven system libs) -- `link.exe` never pulls in `gmock.lib`/`gtest.lib`
just because they happen to sit in a directory named on `/LIBPATH:`, only
if a `.lib` is actually named on the link line or referenced via
`#pragma comment(lib, ...)` in a compiled object, neither of which
applies here. So `gmock.lib` never reached the shipped DLL. It was still a
real correctness bug in the script, though: the product-linked directory
is documented project-wide (see `msvc-build-conventions`) as containing
only the product's own link dependencies, and unfiltered noise there
defeats that contract for the next person reading it expecting it to be
authoritative.

`gmock.lib` specifically has no legitimate destination in this project at
all, product or test-only -- `TEST_LIBS` in the Makefile only ever
references `gtest.lib gtest_main.lib`, never `gmock.lib` or
`gmock_main.lib`. `Copy-TripletLibs` now allow-lists exactly
`libcurl.lib`, `zs.lib` by name instead of copying every `*.lib` it finds,
which removes `gmock.lib` and `gtest.lib` from `lib/<arch>/` entirely
rather than relocating them anywhere.

## Pinning the vcpkg tool itself

**Two separate pins, easy to conflate.** `vcpkg.json`'s `builtin-baseline`
(plus its `curl`/`gtest` dependency entries) pins WHICH port versions
`vcpkg install` resolves to -- the registry *content*. It says nothing
about WHICH `vcpkg.exe` build does that resolving, or how that build
interprets its own command-line flags. Those are two independent axes,
and only the first one was pinned until now.

**Why the second one matters here specifically.** The triplet-isolation
fix above depends on `--x-install-root`, a flag vcpkg's own `--help`
output marks `(experimental)`. `Install-VcpkgIfMissing` previously
bootstrapped vcpkg with a plain
`git clone https://github.com/microsoft/vcpkg.git $Root` and no follow-up
`checkout` -- so a fresh bootstrap always tracked the moving default
branch tip. If a future vcpkg release weakens or changes
`--x-install-root`'s semantics without renaming or removing it (which
would at least fail loudly with "unknown option"), a contributor
bootstrapping fresh months from now could silently reproduce the original
triplet-collision bug (see above) with no script change to blame -- the
exact same command line, against a different tool build, behaving
differently.

**The fix.** `Set-VcpkgPinnedVersion` (called from
`Install-VcpkgIfMissing`) checks out a specific, known-good vcpkg release
tag -- `$VcpkgPinnedTag = '2026.07.29'` -- after cloning (fresh-checkout
path) or immediately after `Repair-ShallowVcpkgClone` (existing-checkout
path; a shallow clone may not have the pinned tag's commit reachable
until unshallowed, the same failure mode manifest installs hit, see
`Repair-ShallowVcpkgClone`'s doc comment). `2026.07.29` was confirmed via
`git ls-remote --tags https://github.com/microsoft/vcpkg.git` on
2026-09-18 to be the latest real, tagged upstream release at that time
(annotated tag object `c76c06644034521fb761a39f8f52d8e87d1103d5`,
dereferencing to commit `9e593bb18ea69cc5095e012465dcd675a822ed0d`) -- not
invented, and not the newest possible choice for its own sake: it's also
the same release generation already proven working end-to-end by this
project's own successful HUM-19 run (`vcpkg version` there reported
`2026-07-27-...`, a commit from two days earlier), so adopting the pin
changes nothing behaviorally on the machine that already passed. Re-derive
the tag the same way, and update the provenance comment next to
`$VcpkgPinnedTag` (not just the value), if this pin is ever bumped.

**Why a `git checkout <tag>` step instead of alternatives.** Per-arch
manifest files were explicitly considered and rejected: this project
already flags "a second pin source" as an antipattern elsewhere (see
`msvc-build-conventions` on `vcpkg.json` being *the* authoritative
dependency pin), and a second manifest file would reintroduce exactly
that. A plain `git checkout` after the existing clone/unshallow logic
adds one pin without adding a second mechanism.

**Interaction with `Repair-ShallowVcpkgClone`, explicitly.** Both the
fresh-clone and existing-checkout paths in `Install-VcpkgIfMissing` now
run the pin:

- **Fresh clone.** Clone (full, not `--depth 1`) happens first, then
  `Set-VcpkgPinnedVersion`, then `Invoke-VcpkgBootstrap` builds
  `vcpkg.exe` from the pinned commit directly -- there is no pre-existing
  `vcpkg.exe` to worry about being stale.
- **Existing checkout.** `Repair-ShallowVcpkgClone` runs first (unshallows
  if needed, otherwise a fast no-op), then `Set-VcpkgPinnedVersion`. If
  the existing checkout was already on the pinned commit, this is a fast
  `git rev-parse` no-op and the pre-existing `vcpkg.exe` is reused
  as-is. If it moves HEAD (a checkout that predates this pin, or one
  manually updated to a newer commit), `Install-VcpkgIfMissing` reruns
  `Invoke-VcpkgBootstrap` to rebuild `vcpkg.exe` from the newly
  checked-out commit's own `toolsrc/` sources, rather than leaving a
  `vcpkg.exe` built from a different commit than what's now checked out
  in place.

Tag resolution failures (no network, tag renamed/removed upstream) and a
`git checkout` that doesn't land on the expected commit (e.g. local
modifications in the checkout blocking it) are both treated as `WARN`,
not `FAIL`: the pin is a defence-in-depth measure, not something that
should block an otherwise-working provisioning run over a transient
network hiccup. The existing, already-resolvable `vcpkg.exe` (if any) is
still used in that case.

**The invariant BPE-25 revealed: the two pins must name the same commit.**
`builtin-baseline` and `$VcpkgPinnedTag` were treated as independent for
too long -- `builtin-baseline` was originally recorded from an unpinned
clone's moving `HEAD` (commit `386d7c478221b7ee0c97bfe6ea61dcf65121d564`),
before this tag pin existed, and the two were never reconciled after the
tag pin was added. The result: `builtin-baseline` pointed at a *newer*
registry commit than `$VcpkgPinnedTag`'s checkout, so `vcpkg install`
resolved baseline versions (curl 8.22.0, gtest 1.18.0) that don't exist in
the older, checked-out version database at all -- "no version database
entry for curl at 8.22.0" / "... gtest at 1.18.0". `vcpkg.json` also
carried an explicit `curl` version override pinning 8.22.0, which looked
like the likely cause at first -- but `gtest` failed identically with no
override at all, which is what proved the override was a red herring: the
newer version was coming from the baseline commit itself, not from the
override, so removing only the override would not have fixed the gtest
failure. **`builtin-baseline`
must be the exact commit the pinned `VCPKG_PINNED_TAG` dereferences to --
the two pins are two independent axes that must still name the same
point, or version resolution breaks in exactly this way.** The fix:
`builtin-baseline` is now `9e593bb18ea69cc5095e012465dcd675a822ed0d` --
re-derived and confirmed (`git ls-remote --tags
https://github.com/microsoft/vcpkg.git 2026.07.29`) to be exactly the
commit `2026.07.29` (the existing `$VcpkgPinnedTag`) dereferences to, not
a new tag chosen to chase newer dependency versions. This downgrades the
resolved versions to curl 8.21.0#1 and gtest 1.17.0#3 (both confirmed
present in the version database at that same commit) -- a deliberate,
accepted tradeoff, not a bug to "fix" by bumping `VCPKG_PINNED_TAG`
forward instead. Both CI (`.github/workflows/ci.yml`) and this script
(`Assert-VcpkgBaselinePin`, called right after the tool pin is applied)
now assert this invariant structurally and fail loudly if it ever
diverges again, rather than relying on this paragraph alone.

## Human approval gate

The script installs software and touches global machine state (VS Build
Tools, `make`, vcpkg packages). Per the project plan it must be approved
before its first run on a given machine.
