# Plan (v13): Build the CAPL REST DLL (restifycapl) from zero

Revision of v12. **Stage 5's code and Stage 6's CI pipeline are both written and have passed a clean `code-reviewer` pass with zero Must-fix items.** Neither is *verified by execution* yet: Stage 5's hard gate still requires CANoe (Stage 3), and the CI workflow has never run because `.github/` is untracked. Stage 3 remains the blocker on the ABI proof, but it no longer blocks a green CI signal.

---

## 1. Goal

Build the CAPL REST DLL (`restifycapl`) from the cloned repository to a working, dual-architecture (x86 + x64) native CANoe plugin: synchronous and asynchronous REST/HTTP for CAPL scripts plus JSON flattening and typed path accessors. Struct mapping and CAPL-side request building stay deferred until a demonstrated need. The CANoe ABI is proven on one trivial operation before any REST logic exists. All business logic is unit-tested outside CANoe. The export table stays a thin, append-only glue layer. Every version number derives from the Git tag.

---

## 2. Dependency architecture — the governing principle

**Compiled binary artifacts are never shared between environments.**

A developer machine and a CI runner are independent environments. Each provisions its own dependency binaries, built by its own toolchain, from a **shared source-level pin** — a version number in a manifest file, not a committed `.lib`.

| Shared (committed) | Not shared (provisioned per environment) |
|---|---|
| `vcpkg.json` — the authoritative content pin | `lib/x86/*.lib`, `lib/x64/*.lib` (libcurl, zlib) |
| `include/vendor/json.hpp` — pinned source header, SHA-256 verified | `lib/gtest/x86/*.lib`, `lib/gtest/x64/*.lib` |
| `include/vendor/capl-dll-sdk/` — source headers, no package-manager path exists | `include/vendor/gtest/` — headers ship with the gtest libs and must stay version-matched to them |

**Why this matters more than the ABI question it replaces.** v8 and v9 committed compiled `.lib` files and proposed managing the resulting toolchain-drift risk — cataloguing the failure mode, noting it surfaces loudly as `LNK2038`, and pre-designing an escape hatch. That was risk *management* where risk *elimination* was available at no cost. Under this architecture CI never links a binary that some laptop produced, so MSVC's cross-toolset compatibility guarantee is never put under test in the first place.

**This is now implemented on both sides, not just designed.** Stage 2 provisions locally from `vcpkg.json`; Stage 6's `.github/workflows/ci.yml` provisions independently on the runner from the same manifest, with its own `/MT` provenance check. The two mechanisms share the pin and nothing else — no mechanism, no output.

**Two pins, not one.** `vcpkg.json`'s `builtin-baseline` pins the registry *content* (which port versions). It does not pin the vcpkg *tool*. Since triplet isolation now depends on `--x-install-root`, a flag vcpkg's own `--help` marks `(experimental)`, the tool binary is pinned too — see §5 and BPE-15. CI honours the same pin (`VCPKG_PINNED_TAG: '2026.07.29'`, commit `9e593bb18ea69cc5095e012465dcd675a822ed0d`), declared as a workflow env var with a comment pointing at `setup-dev-env.ps1` as its twin.

**The pin is `vcpkg.json`, and only the pin.** The manifest's `"version-string": "0.0.0"` is required boilerplate describing *this project as a vcpkg package*. It feeds nothing: not `version.rc`, not the Makefile's `VER_*`, not `/VERSION:`. The product version remains derived solely from the Git tag. Worth stating explicitly because a hardcoded `0.0.0` in a tracked file looks exactly like the second version source that `msvc-build-conventions` forbids. See BPE-16.

**What does not change:** vendored *source* stays committed. `json.hpp` is a single pinned, checksummed source file. The Vector SDK headers are source for which no package-manager path exists, and the decision to commit them to a public repository stands on separately-accepted grounds.

---

## 3. Current state

**Stage 1 — complete and verified.**

**Stage 2 — COMPLETE AND EXECUTION-VERIFIED.** `scripts/setup-dev-env.ps1` ran end to end at **19 OK / 0 WARN / 0 FAIL on both architectures**, after two real bugs were found and fixed (see BPE-15). This closes HUM-19.

**Stage 3 — UNCONFIRMED.** Neither HUM-10 (install CANoe) nor HUM-11 (build and load the official Vector sample) is observable from the repository. **It is no longer the sole blocker on everything** — it blocks only the *verification* half of Stage 5, not Stage 6.

**Stage 4 — COMPLETE.**

**Stage 5 — CODE COMPLETE AND REVIEWED CLEAN; HARD GATE STILL OPEN.** `src/module/exports.cpp` and `src/module/exports.def` now exist (untracked). The export table is populated with the reserved `CDLL_VERSION_NAME`/`CDLL_VERSION` sentinel row plus exactly one real operation, `restifyGetVersion`. `code-reviewer` passed it with **zero Must-fix items**. What is *not* yet evidenced: a real `make build-x86` / `make build-x64` producing both DLLs with `dumpbin /exports` confirming the export surface (BPE-8), and HUM-13 loading it in CANoe. **Until HUM-13 passes, the ABI is designed, reviewed and plausible — not proven.**

**Stage 6 — WRITTEN AND REVIEWED CLEAN; NEVER EXECUTED.** `.github/workflows/ci.yml` exists (untracked) and satisfies every BPE-9 requirement including all three carry-over traps. `code-reviewer` passed it with zero Must-fix items, and an earlier pass (REV-4) already caught and closed two real defects. But **`.github/` is untracked, so this workflow has never run a single time.** A CI pipeline that has never executed sits squarely in this project's most reliable defect category (§13).

**Three follow-ups outstanding, all non-blocking:** BPE-17 (stale `lib/x64/` residue), BPE-16 (`version-string` note), and the new BPE-18 (missing CHANGELOG entries, plus a stale count in an existing entry).

**Next:** commit and push (HUM-12) — that single act is what first executes CI and produces the project's first independent build evidence, including the x86 test run that Stage 4 could not evidence. Then confirm Stage 3 and close the Stage 5 gate.

---

## 4. Numbering scheme

Six phases, sixteen sequentially numbered stages, no letter suffixes, no gaps:

| Phase | Stages | Theme |
|---|---|---|
| **Phase 1 — Foundation & Environment** | 1–4 | Config correctness, scripted bootstrap, manual CANoe setup, repo skeleton |
| **Phase 2 — ABI Proof & Continuous Verification** | 5–6 | Hello DLL in CANoe, then CI running on every push |
| **Phase 3 — Business Logic & CAPL Surface** | 7–12 | Core logic, HTTP, async, flattening, accessors |
| **Phase 4 — Release Pipeline** | 13 | Tag-driven versioning, approval gate, publish |
| **Phase 5 — Hardening** | 14 | Cleanup and consistency |
| **Phase 6 — Conditional Extensions** | 15–16 | Only on demonstrated need |

Documentation practice is deliberately not a numbered stage — see §6a.

---

## 5. Standing constraints

- **Compiled dependency binaries are never committed and never shared between environments** (§2). Each environment provisions its own from `vcpkg.json`.
- **Both the vcpkg registry content and the vcpkg tool binary are pinned.** `builtin-baseline` pins port versions; `$VcpkgPinnedTag` (currently `2026.07.29`, commit `9e593bb18ea69cc5095e012465dcd675a822ed0d`) pins the tool, in both `setup-dev-env.ps1` and `ci.yml`. Pin-enforcement failure is **WARN, not FAIL** — it is defence in depth over an already-solid content pin, and aborting an otherwise-working provisioning run over a speculative future flag change would be the wrong trade. WARN is not silent here, because the project's success bar is 0 WARN.
- **`/MT` static CRT** for the DLL and every static dependency. Never mix `/MT` and `/MD` in one link. Verify with `dumpbin /directives` — expect `/DEFAULTLIB:LIBCMT`, never `MSVCRT`. **The check must require the release `LIBCMT` token *and* the absence of the debug `LIBCMTD` token**: a bare substring match on `/DEFAULTLIB:LIBCMT` also matches `LIBCMTD` and would pass a debug-CRT lib. This was a live defect caught by REV-4 and is now correct in `ci.yml`; replicate the two-part form anywhere else this check is written.
- **Bitness parity.** x86 and x64 must both build and behave identically — same sources, same flags (`/std:c++17`, `/EHsc`, `/MT`), same export table. Only `/MACHINE:` and library path differ; the Makefile enforces this structurally via one parameterized rule, and CI now runs both legs of a matrix over the same targets.
- **Export contract.** The `CAPL_DLL_INFO_LIST4` / `CAPL_DLL_INFO4` table in `src/module/exports.cpp` is the real API. Append-only; never rename, reorder, or remove. Reserved first entry (`CDLL_VERSION_NAME`/`CDLL_VERSION`) always present. Exported functions `extern "C"`.
- **The export-table baseline, as of Stage 5 — this is what every later stage appends to:**
  - row 0: `CDLL_VERSION_NAME` / `(CAPL_FARCALL)CDLL_VERSION` — reserved sentinel, never a real function pointer;
  - row 1: `restifyGetVersion(char buffer[], dword bufferSize) : long` — returns 0 on success, `-1` empty buffer, `-2` truncation;
  - terminator row, covered by the `#pragma pack(push, 1)` / `pack(pop)` pair through and including the terminating pointer;
  - the **sole** real Windows DLL export is `caplDllGetTable4`; `exports.def` contains `EXPORTS` and that one name, nothing else.
- **CAPL naming convention — permanently fixed at Stage 5: `restify<VerbNoun>`** (e.g. `restifyGetVersion`), documented inline at the head of `src/module/exports.cpp`. Because entries can never be renamed, every operation added in Stages 9–12 and 15–16 must follow this form. Downstream agents do not need to read the plan to find this — it is written in the file they will be editing — but it is repeated here so it is never re-litigated.
- **Table entries use `CAPLPASCAL` (`__stdcall`)** calling convention for the function pointers, matching the SDK's documented field order and stack discipline. This is load-bearing on x86, where a convention mismatch corrupts the stack rather than failing to link.
- **Never return a raw text pointer** from a CAPL-exposed operation — always write into a caller-supplied buffer with its size. `restifyGetVersion` is the reference implementation of this shape.
- **1-byte packing must cover the entire export table** through and including the terminating pointer.
- **Dependency direction.** `src/core/` imports nothing from `src/http/`, `src/registry/`, `src/mapping/`. Only `src/module/` includes the CAPL SDK headers — currently `exports.cpp` is the only `.cpp` under `src/` at all, and it is the only file including those headers. `make test` compiles `src/core`, `src/http`, `src/registry`, `src/mapping` and deliberately excludes `src/module`.
- **`lib/<arch>/` is product-linked; `lib/gtest/<arch>/` is test-only** and must never enter the DLL link line.
- **Tests run outside CANoe.** The CAPL export glue is the documented exception (`cpp-testing-conventions`) — it can only be verified inside a real CANoe instance, which is why `src/module` is excluded from the test compile and why HUM-13 is irreplaceable.
- **No version number is ever typed by hand.** `vcpkg.json`'s `version-string` is manifest boilerplate and is not an exception — it feeds nothing. `restifyGetVersion` reads the DLL's *own* version resource at runtime (`GetModuleHandleExA` / `GetFileVersionInfoA`), so even the version string CAPL sees derives from the Git tag through `version.rc` rather than from a literal.
- **Every export-table append gets a `CHANGELOG.md` `[Unreleased]` entry in the same change.** BPE-18 exists because the adjacent rule — *build and CI changes also get an entry* — was the half that slipped.
- **Agents cannot `git push` or `git tag`.**

---

## 6. Phase 1 — Foundation & Environment

### Stage 1 — Configuration reconciliation — COMPLETE

All nine items (HUM-1 … HUM-9) applied and verified. **REV-1 — SATISFIED**, scope corrected to `CLAUDE.md` and `.claude/**` only.

Three permanent rules stand: only `CLAUDE.md` and `.claude/**` are operationally loaded and must be swept; `docs/.locals/**` is exempt as historical record (the v1–v3 drafts carry the user's inline answers, intelligible only alongside the questions they answered); the sweep is self-matching and excludes this plan file. `Grep` honours `.gitignore` and produces the correct scope naturally; Bash `grep -r` / `rg --no-ignore` do not.

### Stage 2 — Local development-environment bootstrap — COMPLETE, EXECUTION-VERIFIED

**BPE-1 / BPE-15 / HUM-19 — DONE.** `scripts/setup-dev-env.ps1` provisions the toolchain, dependencies and headers on the developer's machine from `vcpkg.json`, built by the developer's own MSVC. **Final real run: 19 OK / 0 WARN / 0 FAIL on both architectures.**

This is direct evidence, not testimony — the same standard applied to BPE-5's `.res` decode.

**Two real bugs found during that verification, neither visible to static review:**

**1. Shared install root destroying the other triplet.** Manifest mode defaults both triplets into one `vcpkg_installed/`. Installing x86 then x64 caused vcpkg to *remove* x86's already-installed packages — its own console output announced this — so x86's lib directory was gone by the time its copy step ran. The symptom was three x86 steps failing with "Expected lib directory not found" **while the preceding `vcpkg install (x86-windows-static)` step reported `[OK]`**, which is exactly the kind of misleading adjacency that makes a bug expensive: the failing step and the causing step were different steps. Fixed with separate `--x-install-root` values per triplet (`vcpkg_installed-x86/`, `vcpkg_installed-x64/`). Reproduced and re-verified against real vcpkg.

**2. Unfiltered lib copy.** `Copy-TripletLibs` did a blanket `*.lib` copy into the product-linked `lib/<arch>/`. Because manifest mode places every package for a triplet in one shared `lib/`, this dragged `gmock.lib` and `gtest.lib` in alongside `libcurl.lib` and `zs.lib`. Checked against the Makefile (`LIBS := libcurl.lib zs.lib $(SYSLIBS)` — explicit names, no glob) and confirmed to be **copy noise violating the documented product-versus-test-only separation, never link-time contamination** and never a `/MT`/ABI issue. Fixed by allow-listing exact filenames.

**`code-reviewer` independently reproduced both** the root cause and the fix, running its own `vcpkg install` calls with fresh install roots rather than inspecting the fixing agent's leftovers.

**3. vcpkg tool pin added** — a finding from that review, not from the fixing agent. `Install-VcpkgIfMissing` did a plain `git clone` with no checkout, so only registry *content* was pinned. Since triplet isolation now rests on `--x-install-root`, flagged `(experimental)` by vcpkg itself, a silent future semantic change would have had no pin protecting against it. Fixed with a `Set-VcpkgPinnedVersion` / `Invoke-VcpkgBootstrap` pair wired after clone-or-repair on both code paths, pinning tag `2026.07.29`. The tag was independently verified against the real upstream via `git ls-remote --tags` as genuinely current, not fabricated. Severity is WARN rather than FAIL by deliberate choice (see §5).

The reviewer explicitly **rejected** per-architecture manifest files as the alternative to `--x-install-root`, on the grounds that they would reintroduce the second-pin-source antipattern this project has already flagged elsewhere. Recording the rejected option matters as much as the chosen one.

**Earlier bugs, for the record:** a Polish-localized `cl.exe` banner defeating an English-only architecture check; a `curl[schannel]` vcpkg feature name that no longer exists; `%VSCMD_ARG_TGT_ARCH%` returning its own literal text. The `VSCMD_ARG_TGT_ARCH` fix was verified to still fail correctly on a genuinely wrong architecture. The GoogleTest step surfaced a non-obvious vcpkg layout fact found by reading the port files: the port's own patch relocates `gtest_main.lib` to `lib/manual-link/`, separate from `gtest.lib` in plain `lib/`.

**Clarification of the script's role.** `setup-dev-env.ps1` is the **local** provisioning path only. CI must never run it — it installs VS Build Tools, `make` and vcpkg itself. The two paths share the *pin*, never the *mechanism* and never the *output*. `ci.yml` honours this: it has its own inline provisioning steps and never invokes the script.

### Stage 3 — Manual toolchain setup (human only) — UNCONFIRMED, BLOCKS THE STAGE 5 GATE ONLY

No automation path exists for either item — a licensing constraint, not a technical one, and neither is observable from the repository.

**HUM-10 — Install Vector CANoe/CANalyzer.**

**HUM-11 — Build and load the official "Example of a Windows DLL for CAPL" sample, unchanged, in CANoe.** Still worth doing before trusting the hand-written table: if the official sample does not load, that is an environment problem, and discovering it while simultaneously debugging `exports.cpp` is the compounded-unknowns situation 04-FLOW warns against. Note that `exports.cpp` has now been written ahead of this — the sequencing advice was not followed, which slightly raises the cost of a Stage 3 surprise but does not change what HUM-11 is for.

**Scope of the block, restated precisely.** Stage 3 blocks HUM-13 and therefore the Stage 5 hard gate. It does **not** block Stage 6 going green, nor the x86 test evidence that CI produces, nor Stage 7's pure-logic work in principle. Treating it as a universal blocker would now idle work that is genuinely unblocked.

### Stage 4 — Repo skeleton, Makefile, versioning, dependency pinning — COMPLETE

**BPE-2 — DONE.** `.gitignore` ignores `*.lib` on purpose, with scoped exclusions for `lib/x86/`, `lib/x64/`, `lib/gtest/` and `include/vendor/gtest/`. Scoping to subdirectories rather than `lib/` keeps `lib/README` tracked.

**BPE-3 — DONE.** Skeleton present.

**BPE-4 — DONE, verified with the real GNU Make 3.81 binary.** `build-x86`/`build-x64` are thin recursive wrappers over one `_build` rule; the per-ARCH block is the only place the architectures differ; `clean` removes all of `build/`.

**BPE-5 — DONE, decisively settled.** Everything derives: `GIT_DESCRIBE`, `LAST_TAG` with zero-tag fallback, `VER_*` declared with `?=` so CI can override from the tag, `rc.exe /D` passing all five, `/VERSION:` on the link line. `version.rc` carries `#ifndef` fallbacks with a comment stating they are not a second home for version numbers.

The `VER_STRING` quoting was contested across two reviews — one reproduction produced RC2237 and looked like a real defect. It was settled by running the actual Make binary against the actual Makefile rather than an approximated shell invocation, then decoding the compiled `.res`'s UTF-16LE `FileVersion`/`ProductVersion` fields: both correct and unmangled. **The RC2237 was an artifact of the reproduction, not the build.** When a hand-reconstructed invocation and the real build disagree, the build wins.

**BPE-6 — DONE.** The `lib/README` template inside the script carries the provisioned/uncommitted framing, exact filenames (including an explicit note that `zs.lib` is not the name anyone would guess), the product-linked versus test-only distinction, the `manual-link` split rationale, per-environment `/MT` verification, and "the authoritative pin is `vcpkg.json`". It additionally distinguishes `json.hpp` as committed source rather than a provisioned binary — beyond specification and correct.

Editing the template rather than the generated file was the right call: `lib/README` is script-generated and a hand edit would have silently reverted.

**BPE-7 — DONE.** GoogleTest via vcpkg into `lib/gtest/<arch>/`. The separation is real in the link line, not just in folder naming: the Makefile's `LIBS` contains only `libcurl.lib zs.lib` plus the system libs, while `TEST_LIBS` adds `gtest.lib gtest_main.lib` with a separate `/LIBPATH:$(GTESTDIR)` used solely by the test recipe.

**BPE-15 — DONE AND VERIFIED.** `vcpkg.json` is the authoritative content pin:

```json
{
  "name": "restifycapl",
  "version-string": "0.0.0",
  "builtin-baseline": "386d7c478221b7ee0c97bfe6ea61dcf65121d564",
  "dependencies": [ "curl", "gtest" ],
  "overrides": [
    { "name": "curl", "version": "8.22.0" }
  ]
}
```

The baseline resolves to a genuine dated commit in the real registry and is an exact `HEAD` match; the `curl` override matches the port file at that baseline. The script runs in manifest mode with per-triplet `--x-install-root`, an allow-listed lib copy, a `Repair-ShallowVcpkgClone` self-heal, and a pinned tool binary. Verified by a real 19/0/0 run on both architectures.

**BPE-16 — Preempt the `version-string` confusion.** Add one line stating that `vcpkg.json`'s `"version-string": "0.0.0"` is manifest boilerplate, not a product version source. Best home is `msvc-build-conventions`' Versioning section — where someone checks "is this a version source?", and which currently says no version is ever hand-edited, making a tracked literal `0.0.0` read as a counterexample. The `lib/README` template is a reasonable second home. Non-blocking.

**BPE-17 — Make the lib copy synchronising, and clear the existing residue.**

`lib/x64/` contains **four** `.lib` files: `libcurl.lib`, `zs.lib`, and stale `gmock.lib` and `gtest.lib`. `lib/x86/` contains exactly the correct two.

This is **not a regression** — the allow-list fix works, demonstrably so, and the run's "Copied 2 .lib file(s)" message is accurate. `Copy-TripletLibs` is purely additive: it filters what it copies but never prunes its destination. The two stale files predate the fix and simply were never removed.

The asymmetry has a neat cause: **the very bug that broke x86 also protected it.** Because the shared-install-root bug destroyed x86's source directory, x86's copy step failed rather than running, so `lib/x86/` was never contaminated in the first place. x64 succeeded throughout and accumulated the residue.

Consequences today: **none for the build** — the Makefile names product libs explicitly, so there is no link-time contamination — and **none for the repository**, since `lib/x64/` is gitignored. What it does violate is the documented product-versus-test-only separation as it actually exists on disk, which is the kind of gap that misleads the next person to inspect that directory.

Two options: have `Copy-TripletLibs` remove non-allow-listed `.lib` files from the destination before copying (making the step synchronising rather than additive, and self-healing on any future change to the allow-list), or simply delete the two files by hand. The first is better — it makes the invariant hold for anyone who ever ran an older version of the script — but the second is sufficient for this machine. Non-blocking either way.

**TEST-1 — DONE.** `tests/core/sanity-test.cpp` plus a real linked `build/test/x64/restifycapl-tests.exe`.

This surfaced a real latent bug that could not manifest until a real `main()`-providing static library existed to link against: **`link.exe` infers the subsystem and entry point only from `main`/`WinMain` in `.obj` files passed directly on the command line, never transitively from a `.lib`.** With `main()` coming solely from `gtest_main.lib`, the link failed `LNK1561`. Root-caused with `/VERBOSE` and `dumpbin /symbols`, fixed with `/SUBSYSTEM:CONSOLE` on the test recipe only, and independently re-verified by a separate reviewer running its own `dumpbin` and `make test`.

**What `make test` currently proves.** All of `src/core`, `src/http`, `src/registry`, `src/mapping` still contain only `.gitkeep` — `exports.cpp` is the only `.cpp` under `src/`, and it is deliberately excluded from the test compile. So the suite still compiles exactly one file. Green proves **the harness** — flags, include paths, GoogleTest linkage, subsystem, runner — and no project logic. Stage 7 is the first stage where `make test` proves anything about the product.

**x86 evidence gap — now addressable without CANoe.** Only `build/test/x64/` artifacts exist locally. `ci.yml` runs `make test ARCH=x86` on its x86 matrix leg, so the first green CI run closes this gap permanently. That makes pushing (HUM-12) worth more than routine hygiene.

**REV-2 — CLEAN, CONFIRMED.** The full review passed; its one Must-fix (still-tracked `.lib` files) was closed; the follow-up findings (transcript inversion in `docs/development-environment.md`, missing tool pin) were closed and independently re-verified, including an independent AST parse at 0 errors and upstream confirmation of the pinned tag. The `/MT` provenance check is a **per-environment** check: it validates local provisioning, and CI runs the equivalent against its own copies — which it now does.

**HUM-12 — Commit and push.** Now covers Stage 4, Stage 5 and Stage 6 work, all uncommitted. See §14.

---

## 6a. Documentation practice — standing, not a stage

**Why not a numbered stage.** Documentation here is a standing obligation attached to other work — a CHANGELOG entry whenever the export table gains an entry, README updates when the build story changes, release mechanics at Stage 13. Encoding it as a stage would imply it finishes.

**What exists:** `.claude/skills/project-docs/SKILL.md` (three-documents-three-audiences model, wired into `build-pipeline-engineer` and `cpp-implementer`); `CHANGELOG.md`; `README.md`; `docs/development-environment.md`.

**README impact of §2.** The Development setup section must state plainly that provisioning is **required**, not a convenience: a fresh clone cannot build or test until `scripts/setup-dev-env.ps1` has run, because no compiled dependency is committed.

**Drift watch — now three demonstrated instances.** Rationale living in `docs/development-environment.md` rather than inline keeps the script readable but creates two artifacts that must move together. (1) The worked bug-reproduction transcript in that document had the wrong triplet's packages being removed in the wrong order, and was corrected during Stage 4. (2) `CHANGELOG.md`'s `setup-dev-env.ps1` entry still claims "14 OK, 0 WARN, 0 FAIL" while the verified final run was **19 OK / 0 WARN / 0 FAIL** — a stale number from an earlier run, folded into BPE-18. (3) The CHANGELOG has no entry at all for the project's first CI pipeline or the `version.lib` link change — also BPE-18. A transcript or a count that plausibly resembles the truth is worse than none, because it will be trusted.

**The obligation, stated in the form that would have caught BPE-18.** "Every export-table append gets a CHANGELOG entry" was followed — `restifyGetVersion` is documented correctly. What slipped is the *adjacent* rule: user-visible build, CI and packaging changes need an entry too. A rule scoped to one file type does not generalise itself.

The Makefile deliberately takes the opposite approach for its hardest-won findings — the `/SUBSYSTEM:CONSOLE` requirement and the `make -n` quirk are long inline comments, because both are traps a future reader hits *while editing that exact recipe*. Rationale belongs where it will be read at the moment it is needed; that is a judgement per case, not a uniform rule. BPE-19 is the counterweight: the same instinct applied without restraint produces comment bloat.

---

## 7. Phase 2 — ABI Proof & Continuous Verification

### Stage 5 — "Hello DLL": one operation — CODE COMPLETE, REVIEWED CLEAN; HARD GATE OPEN

**Status split deliberately into two halves, because they have different blockers.**

**Half A — written and reviewed. DONE.**

**CPP-1 — DONE.** `src/module/exports.cpp` and `src/module/exports.def` written.

- `exports.cpp` implements `CAPL_DLL_INFO_LIST4` with the reserved `CDLL_VERSION_NAME` / `CDLL_VERSION` sentinel row followed by exactly one real entry, `restifyGetVersion`. Field order and calling convention match the SDK's documented `CAPL_DLL_INFO4` layout, with `CAPLPASCAL` (`__stdcall`) on the exposed function — critical on x86, where a mismatch corrupts the stack rather than failing to link. The `#pragma pack(push, 1)` / `pack(pop)` pair wraps the table definition through and including the terminating pointer, satisfying the §5 rule.
- `restifyGetVersion` wraps `CopyOwnVersionString`, which reads the DLL's *own* version resource at runtime via `GetModuleHandleExA` / `GetFileVersionInfoA` and copies it into a caller-supplied buffer. Returns 0 on success, `-1` for an empty buffer, `-2` for truncation. This satisfies both the never-return-a-raw-pointer rule and the no-hand-typed-version rule in one design: the string CAPL sees traces back to the Git tag through `version.rc`.
- `exports.def` is `EXPORTS` plus `caplDllGetTable4` and nothing else — no `LIBRARY` line, per the shared-`.def` rule. `caplDllGetTable4` is the sole real DLL export.
- **The CAPL naming convention is now permanently fixed as `restify<VerbNoun>`,** documented at the head of `exports.cpp`. Stage 5's irreversibility warning is now discharged: the decision is made and written down. Every later append inherits it.

**REV-3 — CLEAN, ZERO MUST-FIX.** The single most important review in this plan passed. Confirmed: no export-contract break, no `/MT` violation, no bitness mismatch, no hardcoded version number, dependency direction intact (`exports.cpp` is the only file under `src/` including CAPL SDK headers; `core`/`http`/`registry`/`mapping` have no `.cpp` yet), and the `src/module` test-coverage exclusion correctly justified against `cpp-testing-conventions`.

**Half B — not yet evidenced. OPEN.**

**BPE-8 — Link and resource wiring — NEEDS EXECUTION EVIDENCE.** The Makefile has always referenced `exports.def` as a DLL prerequisite, so `make build-x86` / `make build-x64` should now both succeed where they previously failed on the missing file. **No run of either target is evidenced, and no `dumpbin /exports` output exists.** Close this by actually running both targets and confirming the export surface is exactly `caplDllGetTable4` on both DLLs. Given item (4) in §13 — `LNK1561` was found only by linking for real — "the code is correct" is not a substitute here. Owner: `build-pipeline-engineer`. Cheap, local, unblocked by Stage 3. If CI (Stage 6) runs first, its build legs supply this evidence for both architectures at once, though not the `dumpbin /exports` confirmation unless a step is added.

**HUM-13 — Load and call `restifyGetVersion` from a real `.can` script in CANoe. BLOCKED on Stage 3.** No agent can do this. This is the only thing that converts a reviewed table into a proven ABI.

**Human approval gate: YES — still open.** The gate is HUM-13, not REV-3. A clean review of an ABI is evidence about intent; only CANoe loading the DLL is evidence about reality.

**Optional follow-up from REV-3 (nice-to-have, not required):** `CopyOwnVersionString` mixes Win32 resource lookup with pure buffer/bounds logic. The pure sliver — empty-buffer check, truncation check, `memcpy` plus NUL — could be extracted to `src/core/` so the truncation branch becomes reachable from GoogleTest. The current exclusion is already well-justified inline, so this is a "could", not a "should". Tracked as CPP-16 / TEST-12 and naturally folds into Stage 7, when `src/core/` gains its first real files and the extraction costs almost nothing. Do not do it as standalone work now.

### Stage 6 — CI baseline — WRITTEN AND REVIEWED CLEAN; NEVER EXECUTED

**BPE-9 — WRITTEN. `.github/workflows/ci.yml` exists and meets every stated requirement:**

- `windows-latest`, matrix over x86/x64, MSVC developer environment activated for the matching architecture on each leg.
- **Calls the same Make targets used locally** — `make build-<arch>` and `make test ARCH=<arch>`. No `cl.exe`/`rc.exe`/`link.exe` invocation is duplicated in YAML, which is the rule that keeps CI from silently becoming a second build system.
- Runs tests on **both** architectures, making bitness parity continuously enforced rather than spot-checked, and closing Stage 4's x86 evidence gap on the first green run.
- `fetch-depth: 0` so `git describe` sees tag history — without this the versioning wiring degrades silently rather than failing loudly.
- Independent provisioning from `vcpkg.json` on the runner. Does **not** run `scripts/setup-dev-env.ps1`.
- All three carry-over traps handled: full clone (not shallow), distinct `--x-install-root` per triplet with separate cache keys, and the same pinned tool tag as the local script.
- Its own `dumpbin /directives` `/MT` provenance check — the CI-side equivalent of REV-2 — requiring `/DEFAULTLIB:LIBCMT`, excluding the `LIBCMTD` debug false positive, and rejecting `MSVCRT`.
- `actions/cache` over the vcpkg tool checkout and per-triplet install roots, plus build-concurrency cancellation.
- Uploads both DLLs as workflow artifacts.

**Makefile change shipped alongside it:** `version.lib` added to `SYSLIBS`, required by `CopyOwnVersionString`'s `GetFileVersionInfo` calls. `SYSLIBS` is the shared variable feeding the single parameterized rule, so it applies identically to both architectures by construction — no drift risk, which is exactly why that variable exists.

**REV-4 — CLEAN, ZERO MUST-FIX.** Two genuine defects were caught and fixed before this pass: the `/MT` substring-matching bug that would have passed a debug-CRT lib, and a missing clarification on the vcpkg-tool-checkout cache key not being triplet-scoped. Both are applied and were re-verified clean in the latest full pass.

**What is still missing, and it is the important part: the workflow has never run.** `.github/` is untracked. Every claim above is a claim about YAML that GitHub has never parsed, on a runner that has never existed. This project's single most reliable defect predictor is "static review does not substitute for execution", with seven recorded instances — and CI is dense with exactly the failure modes that only appear at runtime: action version resolution, PATH and environment inheritance between steps, cache key behaviour, `vcvarsall` propagation into later steps, and `make` availability on the runner image.

**Therefore Stage 6 is not done when the file is written. It is done when both matrix legs are green.** Concretely: HUM-12 pushes it, the first run is observed, and any failures are routed back to `build-pipeline-engineer`. Budget for a fix cycle rather than treating a red first run as a setback — a first CI run that passes untouched would be the surprise.

**Caching — purely a performance concern, never a correctness one.** Because no committed binary is involved, caching can be added, tuned or removed without affecting correctness. Keys include the triplet and the runner image version, so when GitHub bumps the toolset the key changes, the cache misses, and dependencies rebuild against the new compiler — correct by construction rather than by anyone remembering to invalidate. GitHub evicts entries after 7 days without access and this project pushes in bursts, so cold builds will be common: minutes, not a correctness risk.

**BPE-18 — NEW, open Should-fix from the latest review.** `CHANGELOG.md` has no `[Unreleased]` entry for the new CI workflow (`.github/workflows/ci.yml` — the project's *first* CI pipeline, plainly user-visible) or for the `version.lib` `SYSLIBS` addition. A one-line `### Added` bullet each closes it. While in the file, correct the stale "14 OK, 0 WARN, 0 FAIL" in the existing `setup-dev-env.ps1` entry to **19 OK / 0 WARN / 0 FAIL**, matching the verified run recorded in Stage 2. Owner: `build-pipeline-engineer`. Non-blocking, but it should land **in the same commit as the CI workflow** — a changelog entry added after the fact is a different and lesser artifact than one that shipped with its change.

**BPE-19 — NEW, optional/stylistic.** The `version.lib` rationale is explained twice at length, in both `exports.cpp` and the Makefile, at more words than the fact warrants. Trim toward one substantive explanation plus a pointer. Explicitly a nice-to-have from REV-4 and not a defect; §6a's "rationale belongs where it will be read" principle is right, but it has a ceiling.

**Human approval gate: YES — reframed.** Approve the workflow *and* the push, then treat the gate as satisfied only once both matrix legs are observed green. Approving unexecuted YAML is approving an intention.

---

## 8. Phase 3 — Business Logic & CAPL Surface

Stages 9–12 each append to the export table. Every append requires `code-reviewer`, a human gate, and a `CHANGELOG.md` `[Unreleased]` entry in the same change. Every appended operation follows the `restify<VerbNoun>` convention fixed at Stage 5.

### Stage 7 — Core pure logic (level 0)
**CPP-2** — `src/core/type-conversion.*`. **CPP-3** — `src/core/json-path.*`. Both depend on `json.hpp` only — zero I/O, zero CANoe knowledge, not yet exported.
**TEST-2 / TEST-3** — `tests/core/` coverage: valid input, malformed/missing JSON fields, type mismatches.
**CPP-16 / TEST-12 (optional, folded in here)** — if the `CopyOwnVersionString` buffer/bounds extraction from Stage 5 is taken up, this is where it belongs: `src/core/` is being populated anyway, and the truncation branch becomes testable at near-zero marginal cost. Skip without ceremony if it does not fit cleanly.
These are the first real files to land in `src/` outside `src/module/`; the Makefile's `$(wildcard …)` picks them up automatically, and `make test` starts proving product logic rather than only the harness. **Human approval: no.**

### Stage 8 — HTTP layer and synchronous operations (logic only)
**CPP-4** — `src/http/http-client.*` wrapping libcurl. **CPP-5** — `src/http/sync-operations.*`.
**BPE-10** — Link `libcurl.lib` and `zs.lib` from the matching `lib/<arch>/`, plus all Windows system libs in `SYSLIBS` (now including `version.lib`).
**TEST-4** — Build the libcurl fake/mock boundary. No real network calls in the suite.

**Design constraint from the Makefile.** `TEST_LIBS` is `gtest.lib gtest_main.lib $(LIBS)`, so the test executable links the **real** libcurl. Harmless today, but it means **the mock cannot be a link-time substitution** — a fake `libcurl.lib` cannot simply be swapped in. The seam must be a C++ abstraction inside `http-client.*` that tests inject through. Settle this in **CPP-4's design**, before TEST-4 tries to test around a shape that does not admit a fake.

**TEST-5** — Coverage including **simulated timeouts and error responses**. Verify as a standalone console program against httpbin.org. **Human approval: no.**

### Stage 9 — Expose synchronous REST to CAPL (first contract append)
**CPP-6** — Append sync operations; rebuild both architectures; add the `CHANGELOG.md` entry. **REV-5**. **HUM-14** — Verify in CANoe. **Human approval gate: YES.**

### Stage 10 — Asynchronous layer with response state designed correctly up front
**CPP-7** — `src/http/async-operations.*`: background dispatch, readiness check, wait-for-result. Response-state semantics settled **now, not retrofitted** — ready flag cleared once read, request ID tying a response to the call that produced it. 04-FLOW §5 item 3 records the previous iteration identified this early but never confirmed implementation. Shared state is global within the DLL with per-module synchronization; **one active response at a time** by deliberate design.
**TEST-6** — coverage for ready-flag-cleared-after-read and request-ID correlation across consecutive requests.
**CPP-8** — Append async operations; CHANGELOG entry. **REV-6**. **HUM-15** — Verify in CANoe. **Human approval gate: YES.**

### Stage 11 — JSON flattening (highest user value — ship before struct mapping)
**CPP-9** — `src/mapping/json-flatten.*`: dot-notation key/value map, key count, key-by-index, value-by-key.
**TEST-7** — coverage including deeply nested objects, arrays, empty/malformed documents.
**HUM-16 — Mandatory before any `.can` example is written:** verify associative-field syntax against the official CANoe help (`Help → CAPL → General → Associative Fields`). The correct form has **no extra keyword before the type** — `char[30] name[char[]];`. An invented keyword was copied across many docs and example files last time.
**CPP-10** — Append flattening operations; CHANGELOG entry. **CPP-11** — `examples/*.can`, only after HUM-16. **REV-7**. **Human approval gate: YES.**

### Stage 12 — Typed JSON accessors
**CPP-12** — `src/mapping/json-accessors.*`: typed point reads, array helpers, optional cache.
**TEST-8** — coverage including type mismatches per accessor and cache invalidation between responses.
**CPP-13** — Append accessor operations; CHANGELOG entry. **REV-8**. **Human approval gate: YES.**

---

## 9. Phase 4 — Release Pipeline

### Stage 13 — Tag-driven release with an approval gate

**BPE-11 — Write `.github/workflows/release.yml`.** Triggered by a `vX.Y.Z` tag push. Extracts `X.Y.Z` from `github.ref_name`, overriding `VER_MAJOR`/`VER_MINOR`/`VER_BUILD`/`VER_REV` — the Makefile declares these with `?=` specifically so CI can override without edits. **Reuse `ci.yml`'s provisioning steps rather than rewriting them** — it already encodes all three carry-over traps, the two-part `/MT` check and the cache keying, and a hand-rewritten second copy is exactly how those hard-won fixes get lost. Builds both architectures, runs tests, then **halts at a manual approval gate** (a GitHub Environment with required reviewers), publishing only after approval.

**BPE-12 — Generate the exposed-operation list from the export table at build time.**
**BPE-14 — Convert `CHANGELOG.md`'s `[Unreleased]` into a released section.**
**REV-9 — Review** the release workflow: no hardcoded version anywhere, approval gate genuinely blocks.
**HUM-17 — Create the release tag.** **HUM-18 — Verify the CI-built artifact in CANoe, then approve the publish.**

**First real exercise of the release versioning path.** Everything up to here has run against a zero-tag repository. Stage 13 is the first time `git describe` sees a real tag and CI overrides `VER_*` from it — and `restifyGetVersion` makes that path observable from CAPL for the first time, since it reads the resource the tag produced. A good early check: call `restifyGetVersion` against a tagged CI artifact and confirm the string matches the tag.

**Human approval gate: YES.**

---

## 10. Phase 5 — Hardening

### Stage 14 — Cleanup and consistency pass
**BPE-13** — One `Makefile`, no historical variants; `clean` removes every intermediate; no build artifacts tracked; version and operation list each maintained in exactly one place. Confirm no compiled dependency has crept back into tracking, and that `lib/<arch>/` contains only product libs (BPE-17's invariant, verified against disk rather than against the copy step's own report). Sweep for accumulated comment bloat (BPE-19's category).
**TEST-9** — Coverage audit across `src/`, including an explicit statement of what remains deliberately untested and why (`src/module`, per `cpp-testing-conventions`).
**REV-10** — Final review including a `project-docs` consistency check. Anything removed as dead code must be removed **in full** (export-table entry + implementation + documentation) in a single commit.
**Human approval: no**, unless it touches the export table.

---

## 11. Phase 6 — Conditional Extensions

### Stage 15 (CONDITIONAL) — Struct registry + JSON→struct mapping
Trigger: Stage 12's typed accessors prove insufficient for a concrete use case. **CPP-14** / **TEST-10** / **REV-11**. **Human approval gate: YES.**

### Stage 16 (CONDITIONAL) — CAPL-side request-body building
Trigger: hand-assembling JSON in CAPL proves genuinely cumbersome. Dead code last time. **CPP-15** / **TEST-11** / **REV-12**. **Human approval gate: YES.**

---

## 12. Task index by agent

### `build-pipeline-engineer` — 19 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| BPE-1 | 2 | `scripts/setup-dev-env.ps1` — local provisioning | **DONE — 19/0/0 both arches** |
| BPE-2 | 4 | `.gitignore` — excludes provisioned binaries, keeps `lib/README` | **DONE** |
| BPE-3 | 4 | Directory skeleton | **DONE** |
| BPE-4 | 4 | `Makefile`, one parameterized rule | **DONE — run with real GNU Make** |
| BPE-5 | 4 | Git-tag versioning wiring | **DONE — `.res` fields decoded and verified** |
| BPE-6 | 4 | `lib/README` template — provisioned framing + exact filenames | **DONE** |
| BPE-7 | 4 | GoogleTest via vcpkg into `lib/gtest/<arch>/` | **DONE** |
| BPE-15 | 4 | `vcpkg.json` manifest, per-triplet install roots, lib allow-list, tool pin | **DONE — EXECUTION-VERIFIED** |
| BPE-16 | 4 | One-line note that `vcpkg.json`'s `version-string` is not a version source | **Non-blocking** |
| BPE-17 | 4 | Make `Copy-TripletLibs` synchronising; clear stale `lib/x64/` residue | **Non-blocking** |
| BPE-8 | 5 | Link/resource wiring for both DLLs; `dumpbin /exports` | **NEEDS EXECUTION EVIDENCE — no build run evidenced** |
| BPE-9 | 6 | `ci.yml` — independent provisioning + build + `make test` both arches + artifacts + caching; `version.lib` in `SYSLIBS` | **WRITTEN, REVIEWED CLEAN — NEVER EXECUTED** |
| BPE-18 | 6 | CHANGELOG entries for `ci.yml` and `version.lib`; correct stale 14→19 OK count | **NEW — open Should-fix, non-blocking** |
| BPE-19 | 6 | Trim duplicated `version.lib` rationale in `exports.cpp` + Makefile | **NEW — optional/stylistic** |
| BPE-10 | 8 | Link `libcurl.lib`, `zs.lib` and the system libs | |
| BPE-11 | 13 | Release workflow — reuse `ci.yml` provisioning, tag extraction, approval gate, publish | |
| BPE-12 | 13 | Generate the operation list from the export table | |
| BPE-13 | 14 | Build-system cleanup pass | |
| BPE-14 | 13 | Cut `CHANGELOG.md` `[Unreleased]` into a released section | |

### `cpp-implementer` — 16 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| CPP-1 | 5 | `exports.cpp` + `exports.def` — one operation; fixes the CAPL naming convention permanently | **DONE — REVIEWED CLEAN** |
| CPP-2 | 7 | `src/core/type-conversion.*` | |
| CPP-3 | 7 | `src/core/json-path.*` | |
| CPP-16 | 7 | *(optional)* extract `CopyOwnVersionString`'s pure buffer/bounds sliver to `src/core/` | **Nice-to-have** |
| CPP-4 | 8 | `src/http/http-client.*` — must expose an injectable seam (see Stage 8) | |
| CPP-5 | 8 | `src/http/sync-operations.*` | |
| CPP-6 | 9 | Append sync operations + CHANGELOG entry | |
| CPP-7 | 10 | `src/http/async-operations.*` + response-state semantics | |
| CPP-8 | 10 | Append async operations + CHANGELOG entry | |
| CPP-9 | 11 | `src/mapping/json-flatten.*` | |
| CPP-10 | 11 | Append flattening operations + CHANGELOG entry | |
| CPP-11 | 11 | `examples/*.can` — only after HUM-16 | |
| CPP-12 | 12 | `src/mapping/json-accessors.*` | |
| CPP-13 | 12 | Append accessor operations + CHANGELOG entry | |
| CPP-14 | 15 | Struct registry + mapping (conditional) | |
| CPP-15 | 16 | CAPL-side request building (conditional) | |

### `test-engineer` — 12 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| TEST-1 | 4 | `tests/` skeleton + one passing test | **DONE — exe built and run (x64 only)** |
| TEST-2 | 7 | `tests/core/` — type-conversion | |
| TEST-3 | 7 | `tests/core/` — json-path | |
| TEST-12 | 7 | *(optional)* truncation-branch coverage for the extracted buffer sliver | **Nice-to-have, paired with CPP-16** |
| TEST-4 | 8 | libcurl fake/mock boundary — source-level seam, not link substitution | |
| TEST-5 | 8 | `tests/http/` — http-client + sync-operations, incl. timeouts and errors | |
| TEST-6 | 10 | `tests/http/` — async: ready-flag-cleared-after-read, request-ID correlation | |
| TEST-7 | 11 | `tests/mapping/` — json-flatten | |
| TEST-8 | 12 | `tests/mapping/` — json-accessors | |
| TEST-9 | 14 | Coverage audit across all of `src/` | |
| TEST-10 | 15 | Struct mapping tests (conditional) | |
| TEST-11 | 16 | Request-builder tests (conditional) | |

### `code-reviewer` — 12 tasks

| ID | Stage | Focus | Status |
|---|---|---|---|
| REV-1 | 1 | Stale-identifier sweep — `CLAUDE.md` and `.claude/**` only | **SATISFIED** |
| REV-2 | 4 | Makefile, versioning, `/MT` provenance, tracking hygiene, provisioning | **CLEAN — CONFIRMED** |
| REV-3 | 5 | **Export-contract genesis — the most important review in the plan** | **CLEAN — ZERO MUST-FIX** |
| REV-4 | 6 | CI reuses Make targets; matrix symmetry; provisions independently | **CLEAN — ZERO MUST-FIX; 1 Should-fix open (BPE-18), 2 nice-to-haves** |
| REV-5 | 9 | Contract append — sync | |
| REV-6 | 10 | Contract append — async | |
| REV-7 | 11 | Contract append — flattening | |
| REV-8 | 12 | Contract append — accessors | |
| REV-9 | 13 | Release workflow; no hardcoded versions; approval gate blocks | |
| REV-10 | 14 | Final consistency review incl. `project-docs` agreement | |
| REV-11 | 15 | Contract append — struct mapping (conditional) | |
| REV-12 | 16 | Contract append — request building (conditional) | |

### Human — 19 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| HUM-1 … HUM-9 | 1 | Configuration reconciliation (nine items) | **ALL DONE** |
| HUM-19 | 2 | Re-run `setup-dev-env.ps1` for real to verify BPE-15 | **DONE — 19/0/0 both arches** |
| HUM-10 | 3 | Install Vector CANoe/CANalyzer | **UNCONFIRMED — blocks HUM-13 only** |
| HUM-11 | 3 | Build + load the official Vector sample unchanged in CANoe | **UNCONFIRMED — blocks HUM-13 only** |
| HUM-12 | 4/5/6 | Commit and push Stage 4 + 5 + 6 work — **this is what first executes CI** | **NEXT ACTION** |
| HUM-13 | 5 | Load and call `restifyGetVersion` from a real `.can` script | **BLOCKED on Stage 3 — the Stage 5 gate** |
| HUM-14 | 9 | Verify sync operations in CANoe | |
| HUM-15 | 10 | Verify async operations in CANoe | |
| HUM-16 | 11 | Verify CAPL associative-field syntax against the official CANoe help | |
| HUM-17 | 13 | Create the release tag | |
| HUM-18 | 13 | Verify the CI artifact in CANoe, then approve the publish | |

---

## 13. Risks

**Static review does not substitute for execution — seven recorded instances, and two fresh candidates now standing unexecuted.** Recorded: (1) Polish-localized `cl.exe` banner defeating an English-only architecture check; (2) a `curl[schannel]` vcpkg feature name that no longer exists; (3) `%VSCMD_ARG_TGT_ARCH%` returning its own literal text; (4) `LNK1561` from `link.exe` refusing to infer an entry point from a `.lib`; (5) the `make -n` `CreateProcess` artifact; (6) the shared-install-root triplet collision; (7) the unfiltered lib copy. Items 6 and 7 both passed a clean static review and were found only by running the script.

**The two fresh candidates are Stage 5's unbuilt DLLs (BPE-8) and Stage 6's never-run workflow (BPE-9).** Both have passed clean reviews. On this project's record, that is weak evidence. CI is the more exposed of the two: action resolution, PATH and environment inheritance across steps, `vcvarsall` propagation, cache behaviour and `make` availability on the runner image are all runtime properties that no amount of YAML reading settles. **Plan for a fix cycle on the first CI run rather than treating red as a setback.**

Item 6 carries an extra lesson: **the failing step and the causing step were different steps.** Three x86 steps reported "Expected lib directory not found" while the `vcpkg install (x86-windows-static)` step immediately above them said `[OK]`. Both reports were accurate. Diagnosing it required running vcpkg directly and reading its own output about removing packages.

The RC2237 scare showed the inverse failure: a hand-reconstructed invocation produced a defect the real build did not have.

**Verifying the operation is not verifying the resulting state.** BPE-17's residue is the clean example: the copy step correctly reports "Copied 2 .lib file(s)", every review confirmed the code and the run output, and `lib/x64/` still contains four. A step that is additive rather than synchronising can be perfectly correct about what it did and still leave a directory that violates the invariant. Check destination state, not just the operation's own report.

**Reviewed is not proven — the Stage 5 form of the same error.** REV-3 was clean, and REV-3 was a review of *source code against a specification*. It cannot detect a `CAPL_DLL_INFO4` field-order mismatch against the SDK build actually installed, a calling-convention error that only manifests as x86 stack corruption at call time, or a packing assumption that differs in the real compiler. Those surface when CANoe loads the DLL and calls the function, and nowhere earlier. **Do not let a clean export-contract review be recorded as ABI proof.**

**Pins beneath the pin.** `vcpkg.json` pins registry content; it does not pin how vcpkg is obtained or invoked. All three carry-over traps — shallow clone, shared install root, unpinned tool — live in that layer. `ci.yml` now handles all three in writing; the first green run is what confirms it.

**Experimental flags as load-bearing structure.** Triplet isolation depends on `--x-install-root`, which vcpkg's own `--help` marks `(experimental)`. The tool pin makes this safe *for now* by freezing the tool alongside the content — in both the local script and CI. If that flag is ever stabilised or renamed, the pin buys time to migrate deliberately. Note the pin is now duplicated in two files that must move together; a drift here is silent until a runner and a laptop resolve different vcpkg behaviour.

**Substring matching in safety checks.** The `/MT` check would have accepted a debug-CRT lib because `LIBCMTD` contains `LIBCMT`. A check that can only fail loudly is worth more than one that can pass quietly for the wrong reason — and this one had *both* failure directions available, since it also had to reject `MSVCRT`. Any future provenance check gets the same two-part treatment.

**Sharing compiled artifacts between environments — the anti-pattern this architecture removes.** v8 and v9 committed `.lib` files, making CI link binaries produced on a developer's machine. That was risk management substituting for an available structural fix. When a failure class can be *designed out* at comparable cost, designing it out beats detecting it.

**Provisioning drift between local and CI.** Two mechanisms read one manifest and can still diverge in placement, triplet selection, or bootstrap. Both are now written; neither has been diffed against the other in a running state. The first green CI run is also the first evidence they agree.

**Export contract — now frozen in its first form.** Stage 5 is irreversible in practice: the `restify<VerbNoun>` naming convention, the sentinel row layout, and the caller-supplied-buffer return shape now bind every later append. Stages 9–12 and 15–16 each append to a table whose first real entry is `restifyGetVersion`.

**ABI failure modes that hide *all* operations, not just the new one.** Raw text pointer instead of the caller-supplied buffer; incorrect 1-byte alignment coverage through the terminating pointer; wrong calling convention on x86. All three are addressed in the current `exports.cpp` by design and by review — and all three remain unproven until HUM-13.

**`/MT` contamination.** A per-environment check rather than a property of a committed artifact, now performed on both sides. If a dependency is only available as `/MD`, **stop and ask**.

**Bitness parity — structurally enforced for the build, continuously verified only once CI runs.** Only x64 test artifacts exist locally. `ci.yml` closes this on its first green run; until then the gap is real.

**Documentation that plausibly resembles the truth.** Three instances now: the inverted transcript (corrected), the stale "14 OK" count, and the missing CI/`version.lib` entries. The first two are actively misleading; the third is merely absent, which is the better failure. BPE-18 closes all three.

**A changelog rule scoped to one file type does not generalise itself.** The export-table CHANGELOG rule was followed precisely. Build and CI changes — equally user-visible — were not covered by it and were missed. §5 now states both halves.

**Apparent second version sources.** `vcpkg.json`'s `version-string` feeds nothing but reads as a counterexample to "no version number is ever hand-edited". BPE-16 preempts it. The shape will recur; label them where they appear.

**Generated files silently reverting hand edits.** `lib/README` is produced by `setup-dev-env.ps1`; editing the artifact instead of the template appears to work and vanishes on the next run.

**Configuration drift — closed, with a scoped standing check.** REV-1's corrected scope excludes historical drafts and its own definition.

**Agent routing metadata is easy to miss.** Body and frontmatter `description` are separate surfaces.

**Vector SDK headers remain committed and remain an accepted risk.** Public repository, explicit user decision. Source, not compiled output, and no package-manager path exists.

**Versioning.** `FILEVERSION`/`PRODUCTVERSION` are four 16-bit fields that **wrap silently** above 65535; `/VERSION:` accepts major.minor only. Verified end to end on a zero-tag repository; the untested path is the **release** path where CI overrides `VER_*` from a real tag, first exercised at Stage 13. `restifyGetVersion` now makes that path observable from CAPL, which is a genuine improvement in testability.

**Steps no agent can verify.** The remaining HUM tasks. HUM-12 is immediately actionable; HUM-10, HUM-11 and HUM-13 are the near-term CANoe cluster.

**Scope creep toward struct mapping.** Stages 11 and 12 must ship before Stage 15 is reconsidered.

---

## 14. Operational loose ends

1. **Three stages' worth of work is uncommitted.** `src/module/exports.cpp`, `src/module/exports.def` and `.github/` are untracked; `Makefile` and `CHANGELOG.md` are modified. Because nothing is committed, reviews still cannot prove "only these changes" via a clean diff boundary and must verify by content-reading instead. Committing restores that boundary — and pushing is what first executes CI.
2. **BPE-18 should land in the same commit as the CI workflow,** not after it. A changelog entry that ships with its change is a different artifact from one added later.
3. **`lib/x64/` residue** — see BPE-17. Gitignored, so it will not enter the commit.
4. **BPE-8 has no execution evidence.** Running `make build-x86` and `make build-x64` locally is cheap, unblocked, and would have caught `LNK1561`-class problems before CI does.
5. **The README execution-policy/switches expansion** has still not been through `code-reviewer`. Prose, not executable logic; let it ride along with the next review.

---

## 15. Execution order

**1 (done) → 2 (done, verified) → 4 (done) → 5 Half A (done, reviewed clean) → 6 written (reviewed clean) → HUM-12 commit + push → 6 green → 3 (unconfirmed) → 5 gate (HUM-13) → 7 → 8 → 9 → 10 → 11 → 12 → 13 → 14**, with Stages 15–16 only on demonstrated need.

**The critical path has forked, and that is the main change in v13.** Stage 3 no longer gates everything. Two tracks now run in parallel:

- **Track A (no CANoe needed, actionable today):** commit and push → observe the first CI run → fix whatever it surfaces → Stage 6 green. This also closes the x86 test evidence gap and supplies BPE-8's build evidence.
- **Track B (human, CANoe):** HUM-10 → HUM-11 → HUM-13 → Stage 5 gate closed.

Stage 7 can begin once Track A is green; it does not need Track B. But **no export-table append (Stage 9 onward) may proceed until HUM-13 has passed** — appending to a table whose base layout has never been loaded by CANoe would multiply the unknowns in exactly the way Stage 5 exists to prevent.

BPE-16, BPE-17, BPE-18 and BPE-19 are non-blocking and can happen at any time; BPE-18 is best folded into the HUM-12 commit.

**Status:** v13. Stages 1, 2 and 4 complete and execution-verified. Stage 5 code complete and reviewed clean, hard gate open on Stage 3. Stage 6 written and reviewed clean, never executed. Next action: HUM-12.
