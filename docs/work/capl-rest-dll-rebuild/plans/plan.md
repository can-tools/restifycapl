# Plan (v14): Build the CAPL REST DLL (restifycapl) from zero

Revision of v13. **The branching strategy is folded in as a new Stage 7**, merged from the former standalone `docs/work/branching-strategy/plans/plan.md` (now a superseded stub) and extended with a settled branch/PR automation layer. **All former Stages 7–16 are renumbered 8–17.** Task IDs are unchanged — they were never stage-encoded. Stage 5's code and Stage 6's CI pipeline remain written and reviewed clean but unexecuted; Stage 3 remains the blocker on the ABI proof only.

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

**Stage 7 — NEW, DESIGNED AND SETTLED, NOT STARTED.** The branching and automation strategy (below) is fully designed with every open question answered except HUM-21's exact `settings.json` wording. Nothing is implemented: no branch, no `auto-pr.yml`, no skill, no permission change.

**Next:** commit and push (HUM-12) — that single act is what first executes CI and produces the project's first independent build evidence, including the x86 test run that Stage 4 could not evidence. Then confirm Stage 3 and close the Stage 5 gate.

---

## 4. Numbering scheme

Six phases, seventeen sequentially numbered stages, no letter suffixes, no gaps:

| Phase | Stages | Theme |
|---|---|---|
| **Phase 1 — Foundation & Environment** | 1–4 | Config correctness, scripted bootstrap, manual CANoe setup, repo skeleton |
| **Phase 2 — ABI Proof, Continuous Verification & Workflow** | 5–7 | Hello DLL in CANoe, CI running on every push, then branching + PR automation |
| **Phase 3 — Business Logic & CAPL Surface** | 8–13 | Core logic, HTTP, async, flattening, accessors |
| **Phase 4 — Release Pipeline** | 14 | Tag-driven versioning, approval gate, publish |
| **Phase 5 — Hardening** | 15 | Cleanup and consistency |
| **Phase 6 — Conditional Extensions** | 16–17 | Only on demonstrated need |

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
- **CAPL naming convention — permanently fixed at Stage 5: `restify<VerbNoun>`** (e.g. `restifyGetVersion`), documented inline at the head of `src/module/exports.cpp`. Because entries can never be renamed, every operation added in Stages 10–13 and 16–17 must follow this form. Downstream agents do not need to read the plan to find this — it is written in the file they will be editing — but it is repeated here so it is never re-litigated.
- **Table entries use `CAPLPASCAL` (`__stdcall`)** calling convention for the function pointers, matching the SDK's documented field order and stack discipline. This is load-bearing on x86, where a convention mismatch corrupts the stack rather than failing to link.
- **Never return a raw text pointer** from a CAPL-exposed operation — always write into a caller-supplied buffer with its size. `restifyGetVersion` is the reference implementation of this shape.
- **1-byte packing must cover the entire export table** through and including the terminating pointer.
- **Dependency direction.** `src/core/` imports nothing from `src/http/`, `src/registry/`, `src/mapping/`. Only `src/module/` includes the CAPL SDK headers — currently `exports.cpp` is the only `.cpp` under `src/` at all, and it is the only file including those headers. `make test` compiles `src/core`, `src/http`, `src/registry`, `src/mapping` and deliberately excludes `src/module`.
- **`lib/<arch>/` is product-linked; `lib/gtest/<arch>/` is test-only** and must never enter the DLL link line.
- **Tests run outside CANoe.** The CAPL export glue is the documented exception (`cpp-testing-conventions`) — it can only be verified inside a real CANoe instance, which is why `src/module` is excluded from the test compile and why HUM-13 is irreplaceable.
- **No version number is ever typed by hand.** `vcpkg.json`'s `version-string` is manifest boilerplate and is not an exception — it feeds nothing. `restifyGetVersion` reads the DLL's *own* version resource at runtime (`GetModuleHandleExA` / `GetFileVersionInfoA`), so even the version string CAPL sees derives from the Git tag through `version.rc` rather than from a literal.
- **Every export-table append gets a `CHANGELOG.md` `[Unreleased]` entry in the same change.** BPE-18 exists because the adjacent rule — *build and CI changes also get an entry* — was the half that slipped.
- **No stage work is committed to `main` directly.** From Stage 7 onward all work happens on a branch and reaches `main` only through a PR — see Stage 7 for topology, naming, merge criteria and automation.
- **Agents cannot `git tag`, and cannot push to `main`.** Stage 7 narrows the blanket `git push` deny to the working-branch prefixes only (pending HUM-21); `main` and `git tag` stay denied.

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

**Scope of the block, restated precisely.** Stage 3 blocks HUM-13 and therefore the Stage 5 hard gate. It does **not** block Stage 6 going green, nor the x86 test evidence that CI produces, nor Stage 8's pure-logic work in principle. Treating it as a universal blocker would now idle work that is genuinely unblocked.

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

**What `make test` currently proves.** All of `src/core`, `src/http`, `src/registry`, `src/mapping` still contain only `.gitkeep` — `exports.cpp` is the only `.cpp` under `src/`, and it is deliberately excluded from the test compile. So the suite still compiles exactly one file. Green proves **the harness** — flags, include paths, GoogleTest linkage, subsystem, runner — and no project logic. Stage 8 is the first stage where `make test` proves anything about the product.

**x86 evidence gap — now addressable without CANoe.** Only `build/test/x64/` artifacts exist locally. `ci.yml` runs `make test ARCH=x86` on its x86 matrix leg, so the first green CI run closes this gap permanently. That makes pushing (HUM-12) worth more than routine hygiene.

**REV-2 — CLEAN, CONFIRMED.** The full review passed; its one Must-fix (still-tracked `.lib` files) was closed; the follow-up findings (transcript inversion in `docs/development-environment.md`, missing tool pin) were closed and independently re-verified, including an independent AST parse at 0 errors and upstream confirmation of the pinned tag. The `/MT` provenance check is a **per-environment** check: it validates local provisioning, and CI runs the equivalent against its own copies — which it now does.

**HUM-12 — Commit and push.** Now covers Stage 4, Stage 5 and Stage 6 work, all uncommitted. See §14.

---

## 6a. Documentation practice — standing, not a stage

**Why not a numbered stage.** Documentation here is a standing obligation attached to other work — a CHANGELOG entry whenever the export table gains an entry, README updates when the build story changes, release mechanics at Stage 14. Encoding it as a stage would imply it finishes.

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

**Optional follow-up from REV-3 (nice-to-have, not required):** `CopyOwnVersionString` mixes Win32 resource lookup with pure buffer/bounds logic. The pure sliver — empty-buffer check, truncation check, `memcpy` plus NUL — could be extracted to `src/core/` so the truncation branch becomes reachable from GoogleTest. The current exclusion is already well-justified inline, so this is a "could", not a "should". Tracked as CPP-16 / TEST-12 and naturally folds into Stage 8, when `src/core/` gains its first real files and the extraction costs almost nothing. Do not do it as standalone work now.

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

### Stage 7 — Development workflow: branching, auto-PR, and scoped push permissions

Folded in from the former standalone `docs/work/branching-strategy/plans/plan.md`, which is superseded and now carries only a pointer here. This stage is process, not product: it produces one workflow file, one skill, one permission change and one round of manual GitHub configuration. It is **the last thing that happens on `main` directly** — every stage after it goes through a branch.

Stages 1–6, including `exports.cpp` and `ci.yml`, were committed straight to `main` (33acd51) and stay there. This strategy is not retroactive.

**Every design question in this stage is settled except HUM-21's exact `settings.json` wording.** The automation shape is fixed: bot-side auto-PR using the default `GITHUB_TOKEN`, `ci.yml` keeping a `push` trigger that covers branch work, draft PRs, and a hand-written `gh` script rather than a marketplace action.

#### 7.1 The flow, end to end

```
  agent/skill: git switch -c stage/08-core-pure-logic
               ... work, commit ...
               git push -u origin stage/08-core-pure-logic     <- needs HUM-21
                        |
                        +--> auto-pr.yml   opens draft PR (if absent, if ahead of main)
                        |
                        +--> ci.yml (push)     build + test, x86 and x64, artifacts
                                 |
                             PR now open; each later push runs ci.yml twice
                             (push + pull_request) -- free, public repo
                                 |
                        code-reviewer on git diff main...HEAD
                        human gate in CANoe against the PR's artifact (Stages 10-13)
                        human marks "Ready for review"
                                 |
                        HUMAN MERGES (--no-ff for stage/*)     <- no agent, ever
                                 |
                             ci.yml (push to main)
```

Steps 1 and 4 are the human/agent boundary; steps 2 and 3 are entirely GitHub-side.

#### 7.2 Topology — per-stage branches off `main`, no long-lived integration branch

**Chosen model: GitHub Flow, one branch per unit of work, PR into `main`.**

**Explicitly rejected: a long-lived `develop` branch.** It creates a second integration point where "CI green" means less than it does on `main`, duplicates every merge, and still forces release tags onto `main` after a release merge — ceremony with no payoff at one developer. A `develop` branch mainly substitutes for a missing per-unit "done" signal; this project already has one in the HUM gates.

**Unit of work = one plan stage,** with three deliberate exceptions:

- **Stages 8 and 9 may share one branch or take one each.** Neither touches the export contract, neither has a human gate, and Stage 9's design constraint (the injectable libcurl seam, CPP-4) is best settled with Stage 8's `src/core/` files visible in the same tree. One branch, `stage/08-09-core-and-http`, is acceptable and probably preferable.
- **Stages 10–13 get one branch each, strictly serialized.** Each appends to `CAPL_DLL_INFO_LIST4` and each has its own CANoe gate. See 7.9 for why parallel export-table branches are the sharpest hazard in this strategy.
- **Non-stage follow-ups (BPE-16/17/18/19, doc fixes, CI fixes) get their own small branches** and do not wait on a stage.

**Stacking.** When a stage is blocked on its predecessor's *unmerged* code — e.g. Stage 11's async work needs Stage 10's exports in spirit while HUM-14 is still pending in CANoe — branch Stage 11 **off Stage 10's branch**, not off `main`, and target its PR at Stage 10's branch. GitHub retargets the child PR at `main` automatically when the parent lands. Do not cherry-pick between stage branches: it duplicates commits and breaks the "this exact reviewed SHA is what merged" property.

**Refresh, don't rebase.** Long-lived branches (a CANoe gate can take days) stay current by **merging `main` into the branch**, never by rebasing. Rebasing rewrites SHAs, destroying the claim "`code-reviewer` passed *this* commit" — a claim this project leans on heavily (REV-3, REV-4).

#### 7.3 Naming convention

| Prefix | Form | Example |
|---|---|---|
| Stage work | `stage/<nn>-<kebab-slug>` | `stage/08-core-pure-logic` |
| Combined stages | `stage/<nn>-<nn>-<slug>` | `stage/08-09-core-and-http` |
| Non-stage follow-up owned by a task ID | `chore/<task-id>-<slug>` | `chore/bpe-18-changelog-entries` |
| Bug fix against merged work | `fix/<slug>` | `fix/stage5-abi-calling-convention` |
| Docs-only | `docs/<slug>` | `docs/plan-v15` |

Rules: two-digit zero-padded stage numbers so `git branch` sorts correctly (`stage/08` before `stage/10`); lowercase kebab-case throughout; no personal-name or date prefixes (one developer — the plan ID is the identity); branches deleted after merge via GitHub's "automatically delete head branches".

**These four prefixes are load-bearing in three separate places** — `auto-pr.yml`'s branch filter (7.5), the `settings.json` allowlist (7.4), and `ci.yml`'s `push` filter if BPE-20 is ever taken (7.6). A branch named outside this convention silently gets no PR and a permission prompt. Deliberately loud, but know it in advance.

`release/*` is deliberately absent. There are no release branches; see 7.7.

#### 7.4 Step 1 — branch creation and push (BPE-22, HUM-21)

**BPE-22 — write `.claude/skills/stage-branch/SKILL.md`.** Owner: `build-pipeline-engineer`. Deliverable: a short skill encoding the branch-naming convention, the create-and-push procedure below, and the guardrails. **Blocked on HUM-21** — the skill is unusable until agents can push to the working-branch prefixes. No human approval gate of its own; it is documentation.

**Owner: a new lightweight skill, `.claude/skills/stage-branch/SKILL.md` (BPE-22)**, invoked by whichever agent owns the stage's first task — `cpp-implementer` for Stage 8, `build-pipeline-engineer` for a `chore/` branch, `test-engineer` for a test-only branch. `build-pipeline-engineer` owns the skill's content as a file.

**Rejected: bolting this onto `build-pipeline-engineer` as an agent responsibility.** The usual argument for it — concentrate push capability in one agent for a smaller blast radius — **does not hold here**, because `CLAUDE.md` states Claude Code's permission rules are global, not per-subagent. Restricting the agent grants zero containment; every agent in the session inherits the same allow/deny set regardless. With the security benefit illusory, the ergonomic argument wins uncontested: three different agents need the same six lines, and routing every stage start through an agent with no other work in that stage is pure ceremony.

The procedure the skill encodes:

```
git switch main
git pull --ff-only
git switch -c stage/<nn>-<slug>
# ... work, git add, git commit ...
git push -u origin stage/<nn>-<slug>
```

**The skill's scope stops at the push. It does not open the PR** — that is the bot's job (7.5).

**HUM-21 — OPEN, requires explicit sign-off before any edit.** `Bash(git push *)` is currently a blanket deny, so no agent can push anything today. This flow requires narrowing it. Proposed wording:

Add to `allow`:
```
"Bash(git switch -c stage/*)", "Bash(git switch -c chore/*)",
"Bash(git switch -c fix/*)",   "Bash(git switch -c docs/*)",
"Bash(git switch main)",       "Bash(git pull --ff-only)",
"Bash(git push -u origin stage/*)", "Bash(git push -u origin chore/*)",
"Bash(git push -u origin fix/*)",   "Bash(git push -u origin docs/*)",
"Bash(git push origin stage/*)",    "Bash(git push origin chore/*)",
"Bash(git push origin fix/*)",      "Bash(git push origin docs/*)"
```

Keep and extend `deny`:
```
"Bash(git push)",
"Bash(git push origin main*)", "Bash(git push * main*)", "Bash(git push * HEAD:main*)",
"Bash(git push --force*)", "Bash(git push * --force*)", "Bash(git push * -f*)",
"Bash(git push * --tags*)", "Bash(git push * --delete*)", "Bash(git push * :*)",
"Bash(git tag *)"
```

**Two caveats that must not be softened:**

1. **`Bash(git push *)` does not clearly match a bare `git push` with no arguments** — which is precisely the dangerous form, since `push.default` can send the current branch, and after a `git switch main` that branch is `main`. `Bash(git push)` must be denied explicitly.
2. **Pattern matching on a command as expressive as `git push` cannot be made airtight.** `git -C . push ...`, `git push origin HEAD:main`, and chained commands all exist. This allowlist raises the bar; it does not close the door.

**Therefore the structural guarantee is GitHub branch protection (7.8), not `settings.json`** — and this reverses one of the original strategy's recommendations. It suggested leaving "Do not allow bypassing (include administrators)" **OFF**, reasoning that "agents cannot push regardless, so this setting is about the human only." **That premise expires the moment HUM-21 lands.** Recommend ON.

**The human edits `settings.json` by hand — not an agent.** An agent editing the file that grants that agent permissions is a self-escalation shape and should not be normalised here to save one edit. HUM-21's deliverable is the edit, not merely the approval of wording.

#### 7.5 Step 2 — `auto-pr.yml` (BPE-21)

**Decision: Option A — the bot authenticates with the default `GITHUB_TOKEN`, and `ci.yml` keeps a `push` trigger covering branch work.**

**Requirements for BPE-21 to satisfy.** The literal workflow file is `build-pipeline-engineer`'s deliverable, not this document's — what follows is the contract it must meet, not its source:

- **Trigger:** `push` to `stage/**`, `chore/**`, `fix/**`, `docs/**`. Never `main`.
- **Runner:** `ubuntu-latest`.
- **No `actions/checkout`** — the job needs no working tree.
- **Permissions:** `contents: read`, `pull-requests: write`, declared explicitly.
- **Auth:** the default `GITHUB_TOKEN`, per the Option A decision above.
- **Guard 1 — ahead-by check.** If the branch has no commits ahead of `main`, log and exit 0. `gh pr create` would otherwise fail with "No commits between main and <branch>", turning a harmless situation into a red run. Determine this via the GitHub API rather than git, so no checkout is needed.
- **Guard 2 — idempotency.** If an open PR already exists for the branch (`gh pr list --head`), log and exit 0. This workflow fires on every push, forever; only the first one should create anything.
- **Action:** `gh pr create --draft --base main`, title derived from the branch name, body set to the fixed checklist scaffold described below.
- **No `|| true` anywhere.** Past both guards, any failure is a genuine one and must surface as a red run.

**Design notes, each one a trap avoided:**

- **`ubuntu-latest`, not `windows-latest`.** `gh` ships on every GitHub-hosted runner including Windows, so it would work there — but this job builds nothing and Linux boots substantially faster. On a public repo that is latency, not cost. `ci.yml` stays on `windows-latest`.
- **No `actions/checkout`.** `GH_REPO` gives `gh` its repo context, and both guards use the API rather than git. A checkout would be pure waste.
- **Explicit `permissions:` block.** Default `GITHUB_TOKEN` permissions may be repo-default read-only, in which case `gh pr create` fails without it.
- **Two guards, then create — and no `|| true` anywhere.** After both guards, the only remaining failure is a genuine one. A red run here should mean something is actually wrong, almost certainly the repo setting below.
- **Draft PRs, by decision.** Drafts still run `pull_request` workflows, and under Option A CI arrives from the push trigger anyway, so draft state is doubly irrelevant to whether tests run. What it buys is an accurate signal: the bot opens the PR at the *first commit*, which is by definition unfinished. The one cost is clicking "Ready for review" before merging — and since the merge is deliberately manual, that click is a free explicit "I consider this done" moment rather than friction. Note this is now a fixed global policy baked into the YAML, not a per-branch judgement call.
- **The bot creates a scaffold, not a finished PR body.** It cannot infer plan semantics. The body is a fixed checklist — CI green both legs / `code-reviewer` clean / human gate / CHANGELOG entry / branch up to date — plus a `TODO: stage and task IDs` line the human or implementing agent fills in, so the PR ends up self-describing ("Stage 10 — CPP-6, REV-5, HUM-14") without anyone reading this plan.
- **A run fires on every push forever**, no-oping via guard 2 after the first. Seconds on ubuntu, free; not worth suppressing.
- **No recursion risk.** The workflow pushes nothing, and `main` is not in the branch filter.

**Required repo setting, and the most likely first-run failure:** Settings → Actions → General → **"Allow GitHub Actions to create and approve pull requests" → ON**. Without it `gh pr create` fails with *"GitHub Actions is not permitted to create or approve pull requests."* **This requirement exists *because* Option A was chosen** — the setting governs `GITHUB_TOKEN` specifically and would not have applied to a PAT or GitHub App. Workflow permissions must also not be repo-default read-only. Both belong to HUM-20.

**Behavioural consequence, stated plainly:** the PR appears at the **first push carrying a commit**, not at branch creation. The original strategy's draft-PR-first rule assumed an empty branch and empty PR were acceptable; that is not achievable automatically, and it loses nothing — an empty branch has nothing to build or review.

**Why a hand-written script rather than a marketplace action — decided, not defaulted.** `peter-evans/create-pull-request`, the obvious candidate, does a different job: it commits *uncommitted workspace changes* to a new branch and opens a PR, which is the inverse of our situation (the branch already exists and was pushed by an agent). Using it here would be off-label and would require adding `actions/checkout`, which this design deliberately avoids. Actions closer to the mark exist (`repo-sync/pull-request`, `thomaseizinger/create-pull-request`) but none is known to handle the ahead-by guard, which exists specifically to prevent a false-red run in this project's flow. The custom logic is about twelve lines; an action's `uses:`/`with:` block is about eight — a net saving of roughly four lines in exchange for a dependency, a likely added checkout step, and debugging someone else's code on first run. **The script has no dependencies at all: `gh` is preinstalled on every GitHub-hosted runner.** Revisit only if the bot grows real features (auto-labelling, reviewer assignment, templated bodies from files); twelve lines with two guards is nowhere near that threshold.

**Note on precedent:** this is *not* the project's first third-party-action decision. `ci.yml` already uses `ilammy/msvc-dev-cmd@v1`, in a far more sensitive position — it configures the compiler environment for both build legs. That dependency earns its place because no official action activates an MSVC developer environment and hand-rolling `vcvarsall` propagation is genuinely hard. An auto-PR action would buy four lines. The existence of one well-justified third-party dependency does not lower the bar for a gratuitous one.

**Small hardening follow-up, unrelated to this decision:** `ilammy/msvc-dev-cmd@v1` is pinned to a *mutable tag*, not a commit SHA, so the maintainer can move `v1` and CI would silently pick up different code. Pinning to a full SHA with the version in a trailing comment costs nothing. Not urgent; fits Stage 15's cleanup pass or whoever next touches `ci.yml`.

#### 7.6 Step 3 — `ci.yml`, and BPE-20's demoted status

**No changes required.** `ci.yml`'s current unfiltered `push:` / `pull_request:` already fires on every branch, running `make build-<arch>` and `make test ARCH=<arch>` across both x86 and x64 and uploading both DLLs as artifacts. "Tests and trial builds on the branch" maps 1:1 onto what already exists. **Under Option A, CI arrives from the developer's own push event, so GitHub's `GITHUB_TOKEN` recursion rule never comes into play.**

**The constraint that shaped this decision, recorded so it is not rediscovered.** Events created using the repository's default `GITHUB_TOKEN` do not start new workflow runs — GitHub's recursion guard. A PR opened by the bot therefore emits a `pull_request: opened` event that no workflow sees. **This only bites if the PR is created by `GITHUB_TOKEN` *and* CI is expected to fire from the PR event.** Option A makes the second condition false, so the constraint is sidestepped rather than worked around. The rejected alternative was a PAT or GitHub App token (which *does* trigger workflows), giving exactly one run per push at the cost of a managed credential — and, more importantly, making CI depend on the bot succeeding. Option A degrades gracefully: PR creation and CI are independent, and losing one does not cost the other.

Once a PR is open, each push runs the matrix twice — once for `push`, once for `pull_request`. **Free on this public repo**; the cost is two same-named entries in the Checks tab.

**BPE-20 is now OPTIONAL AND DEFERRED, and its motivation has shifted.** The former standalone strategy framed it as "do before Stage 7 work begins", justified mainly by wasted runner minutes. That justification is near-zero here, because GitHub Actions minutes are free on public repositories. What survives is the **tag collision**: an unfiltered `push:` also fires on tag pushes, so Stage 14's `v1.0.0` tag would trigger both `ci.yml` and `release.yml` with no answer to which is authoritative. Adding a `branches:` filter to `push` fixes that, because a branch filter means tag pushes stop matching. **Best decided at Stage 14 alongside BPE-11, not a prerequisite for anything in this stage.** If taken:

```yaml
on:
  push:
    branches: [main, 'stage/**', 'chore/**', 'fix/**', 'docs/**']
  pull_request:
    branches: [main]
```

**Implementation trap if BPE-20 is ever taken:** do **not** unify the push and `pull_request` runs into one concurrency group to dedupe them. With `cancel-in-progress: true` they would cancel each other, and **a cancelled run is not a successful required check** — branch protection would block the merge. Keep the existing ref-based group; the two events carry different refs and never collide.

**One thing to verify rather than assume:** when two runs report the same check name against the same commit, branch protection should evaluate the latest status for that context. That is believed correct, but it is exactly the runtime behaviour this project keeps being surprised by (§13). Confirm it on the first real PR.

#### 7.7 Step 4 — merge, tags and releases

**Merge is human-only and stays that way.** No agent merges, and no automation merges. HUM-21 does not relax the `main` deny. The merge commit lands on `main` and `ci.yml` runs there.

**"Only after CI succeeds" is convention until branch protection makes it mechanical** — *Require status checks to pass*, with `build + test (x86)` and `build + test (x64)` selected (7.8).

**One expectation corrected while it is cheap:** the post-merge run on `main` is the *same* workflow as the PR run, not a heavier one. The `pull_request` run already builds the **merge result** (`refs/pull/N/merge`), not just the branch tip, so it is already testing the merged state. The `main` run is confirmation and a record for `main`'s history, not additional coverage. The genuinely heavier pipeline is the tag-driven release workflow, which does not exist yet (Stage 14).

- **Release tags are cut from `main` only, on the merge commit, after `main`'s own CI run is green.** Never from a stage branch, never from a PR head. `release.yml` (BPE-11, Stage 14) builds and publishes from whatever commit the tag points at; a tag on an unmerged branch would publish code that is not on `main`, under a real release version, with no way to un-ship it.
- **`main` is always the release source** — the concrete reason `develop` was rejected in 7.2.
- **Tagging is a human action (HUM-17).** `Bash(git tag *)` stays denied and HUM-21 explicitly does not relax it; `git push --tags` is denied too.
- **`fetch-depth: 0` must be in `release.yml` too.** `ci.yml` has it; `release.yml` does not exist yet. Without it `git describe` degrades silently rather than failing loudly — flag for BPE-11.
- **Stage 14's approval gate is a GitHub *Environment* with required reviewers**, under Settings → Environments. Different from branch protection and configured separately, also by hand.

#### 7.8 What "done enough to land on `main`" means, and the manual GitHub configuration (HUM-20)

`main` is **gate-passed, CI-green work only.** A stage branch merges when all of the following hold, in this order:

1. **Both CI matrix legs green on the PR** — `build + test (x86)` and `build + test (x64)`. Non-negotiable; mechanically enforced by branch protection.
2. **`code-reviewer` clean on the branch**, reviewed against `git diff main...HEAD` (three-dot — the PR diff, i.e. what this branch introduces, excluding what `main` gained meanwhile), zero Must-fix. Should-fix may be deferred to a `chore/` branch if explicitly recorded; nice-to-haves need no ceremony.
3. **The stage's human gate has passed, where it has one.** For Stages 10/11/12 (HUM-14/15/16) and any future export-table append, the gate is verified **against the branch's CI artifact, before merge** — the human downloads `restifycapl-x86`/`restifycapl-x64` from the PR's workflow run and loads it in CANoe. Verifying after merge would put unverified export-table rows on `main`, which is the exact thing this strategy exists to prevent.
4. **`CHANGELOG.md` `[Unreleased]` entry present in the branch**, per §5 — both halves of the rule (export-table appends *and* user-visible build/CI/packaging changes; BPE-18 exists because the second half was missed).
5. **Branch is up to date with `main`** (enforced by protection).

Stages without a human gate (8, 9, 15) merge on 1 + 2 + 4 + 5.

**Merge method.**
- `stage/*` → **merge commit (`--no-ff`)**. Preserves the implement → review → fix cycle, which this project treats as evidence, and preserves the reviewed SHA. It inflates `git describe --tags --long`'s commit count slightly versus squashing; harmless, since the count only needs to be valid and monotonically increasing per `msvc-build-conventions`.
- `chore/*`, `fix/*`, `docs/*` → **squash.** Small, single-purpose, no review history worth keeping.
- **Never rebase-merge anything that touched `src/module/exports.cpp`.** Rewritten SHAs sever the link between the reviewed commit and the merged one, on the one file where that link matters most.

**Review happens once, on the branch, before merge — not again after.** Nothing changes on merge but the merge commit. **One exception:** if the merge required non-trivial conflict resolution — above all in `src/module/exports.cpp` — re-run `code-reviewer` on the resolved result before pushing the merge. A hand-resolved export table has never been reviewed by anyone.

**HUM-20 — manual GitHub configuration. No agent can do any of this.** There is no GitHub admin access in this session at all.

Settings → Branches → ruleset targeting `main`:

| Setting | Recommendation | Note |
|---|---|---|
| Require a pull request before merging | **ON**, required approvals **0** | The real review is `code-reviewer`, not GitHub's approval UI. Solo developer — requiring 1 would self-block. |
| Require status checks to pass | **ON** | |
| — required checks | `build + test (x86)`, `build + test (x64)` | These are the **job** `name:` values from `ci.yml`, not the workflow name `CI`. |
| Require branches to be up to date before merging | **ON** | Directly mitigates the export-table hazard in 7.9. |
| Require conversation resolution | Optional | Low value at one developer. |
| Do not allow force pushes to `main` | **ON** | |
| Do not allow deletions | **ON** | |
| Do not allow bypassing (include administrators) | **ON — reversed from the original strategy** | It said OFF because "agents cannot push regardless." **HUM-21 ends that.** With agents able to push, this is the only control that cannot be talked around. |

Also required:
- Settings → Actions → General → **"Allow GitHub Actions to create and approve pull requests" → ON.** The auto-PR bot does not work at all until this is set. Required under Option A specifically.
- Settings → Actions → General → **Workflow permissions** must not be read-only, or `auto-pr.yml`'s `permissions:` block cannot grant `pull-requests: write`.
- Settings → General → Pull Requests → **"Automatically delete head branches" → ON.**

**Sequencing gotcha:** GitHub only offers a status check in the required-checks picker **after that check has reported at least once.** `build + test (x86)`/`(x64)` cannot be selected until HUM-12's first CI run has completed. Order: HUM-12 push → observe first CI run → *then* configure protection → *then* create the first stage branch.

#### 7.9 Agent workflow under branching

| Step | Actor | Notes |
|---|---|---|
| Create branch + push | Stage's lead agent, via the `stage-branch` skill | Requires HUM-21. Before HUM-21: human. |
| Implement | `cpp-implementer` / `build-pipeline-engineer` / `test-engineer` | `git add`/`git commit` locally on the checked-out branch; both already allow-listed. |
| Open draft PR | **`auto-pr.yml` bot** | Idempotent; scaffold body only; fires on the first push carrying a commit. |
| Fill in stage/task IDs in the PR body | Agent or human | The bot cannot infer these. |
| CI green on both legs | `ci.yml` | Fix cycles route back to `build-pipeline-engineer`. |
| Review | `code-reviewer` | **On the branch, before merge. Once, not twice.** Against `git diff main...HEAD`. |
| Human gate (where applicable) | **Human** | CANoe, against the PR's uploaded artifact. |
| Mark ready for review | **Human** | |
| Merge | **Human only** | `--no-ff` for `stage/*`, squash for the rest. No agent, no automation. |
| Tag a release | **Human only** | Still denied to agents. |

`build-pipeline-engineer` retains sole ownership of `ci.yml`, `auto-pr.yml`, `release.yml`, the `stage-branch` skill file, the Makefile and everything version-related.

#### 7.10 Stage 7 risks

**The export table is the sharpest merge hazard in this project, and it is a *semantic* risk, not a textual one.** Two stage branches both appending to `CAPL_DLL_INFO_LIST4` is the failure to design out. Git will merge two appends happily; the resulting **row order depends on merge resolution order.** While both rows are new and unreleased, any order is valid. The danger is asymmetric: once branch A has merged and its rows have shipped or been CANoe-verified, a stale branch B based on pre-merge `main` can, on merge, place its rows **before** A's already-shipped rows — silently **reordering an existing entry**, which `capl-export-contract` forbids absolutely, which no compiler catches, and which breaks CAPL scripts only at runtime.

Two mitigations, both required:
- **Serialize export-table work. Never have two open PRs that both touch `src/module/exports.cpp`.** Stages 10 → 11 → 12 → 13 run one at a time. If Stage 11's logic must start early, stack it off Stage 10 (7.2) and keep its export-table append as the last commit.
- **"Require branches to be up to date before merging"** (7.8), so a stale base cannot reach `main` at all.

**Merging `main` into a stage branch can reintroduce this.** When refreshing a long-lived branch, if `main` has gained export-table rows, the conflict must be resolved so `main`'s rows stay **before** the branch's — always. Resolving "mine first" is the exact reordering defect above, wearing a conflict marker.

**Narrowing the push deny is the largest single expansion of agent capability in this project's history.** Until now the guarantee was absolute — no agent could push anything. It becomes conditional, enforced by string patterns on a command that has many equivalent spellings. Three mitigations, in descending order of strength: GitHub branch protection with bypass disabled (structural, server-side, the only real one); the explicit deny of bare `Bash(git push)` and the `main`/force/tag forms; and the convention that merges are human. **Do not record this as "agents still cannot touch `main`" — record it as "the server refuses, and the client discourages."**

**`auto-pr.yml` will not have run when it is reviewed.** It joins Stage 5's DLLs and Stage 6's workflow in §13's standing category — seven recorded instances of static review failing to substitute for execution. The two most likely first-run failures are the "Allow GitHub Actions to create and approve pull requests" setting being off, and workflow permissions being repo-default read-only. Budget a fix cycle.

**A branch named outside the four prefixes gets no PR and a permission prompt.** Three systems key off the same naming convention. Loud enough to catch quickly, but know it in advance.

**Bitness parity is unaffected structurally** — enforced by the Makefile's single parameterized rule, which branching does not touch. But *continuous verification* of parity comes from CI running both legs, so an unverified branch is also an unverified-parity branch.

**Long-lived branches blocked on CANoe gates will drift.** Stages 10–13 each wait on a human with CANoe; days or weeks of `main` movement accumulate. Mitigated by regular merge-from-`main` refreshes — which is also why 7.2 forbids rebase as the refresh mechanism.

**`plan.md` becomes a conflict hotspot if every stage branch edits it.** Keep plan revisions on their own `docs/plan-vNN` branch. Stage branches should not edit `plan.md`; record stage status in the PR body and fold it into the next plan revision.

**This strategy does not retroactively fix anything already on `main`.** If HUM-13 fails, the fix is a `fix/` branch like any other — meaning the first real exercise of this workflow could be an export-contract fix, the highest-stakes possible debut. That is why BPE-21/22 deliberately go first on a `chore/` branch: exercise the workflow where the stakes are a YAML file.

**Process discipline is the whole enforcement mechanism for what GitHub cannot check.** Protection enforces CI-green and PR-before-merge. It cannot enforce "`code-reviewer` ran", "the CANoe gate passed", "the CHANGELOG entry is there", or "only one export-table PR is open". Those four are convention. The PR body naming its stage and task IDs is the cheapest available counterweight.

#### 7.11 Tasks and sequence

| ID | Task | Owner | Gate |
|---|---|---|---|
| **HUM-21** | Approve the exact `settings.json` allow/deny wording, **and make the edit by hand** | Human only | **OPEN — blocks BPE-22 and all agent-driven branching** |
| **HUM-20** | GitHub config: branch protection, required checks, "Allow Actions to create PRs" ON, workflow permissions, auto-delete branches, bypass ON | Human only | Partly blocked until the first CI run |
| **BPE-21** | `.github/workflows/auto-pr.yml` as specified in 7.5 | `build-pipeline-engineer` | **Human approval: YES** |
| **BPE-22** | `.claude/skills/stage-branch/SKILL.md` | `build-pipeline-engineer` | No; blocked on HUM-21 |
| **BPE-20** | `ci.yml` `branches:` filter | `build-pipeline-engineer` | **Optional/deferred — revisit at Stage 14 for the tag collision** |
| **REV-13** | Review BPE-21 + BPE-22 together, plus the applied `settings.json` diff | `code-reviewer` | — |

**Sequence:** HUM-12 (push, the plan's current next action) → observe first CI run → HUM-20 → HUM-21 → BPE-21 + BPE-22 on `chore/bpe-21-auto-pr` — **the workflow's first exercise of itself, deliberately while the stakes are a YAML file and not an export table** → REV-13 → human merge → Stage 8 on `stage/08-core-pure-logic`.

**Human approval gate: YES.** It touches CI, `.claude/settings.json`, and what gets shipped. Treat the gate as satisfied only once the bot has actually opened a PR and its CI run has been observed green — per §13, written-and-reviewed is not executed.

---

## 8. Phase 3 — Business Logic & CAPL Surface

Stages 10–13 each append to the export table. Every append requires `code-reviewer`, a human gate, and a `CHANGELOG.md` `[Unreleased]` entry in the same change. Every appended operation follows the `restify<VerbNoun>` convention fixed at Stage 5.

### Stage 8 — Core pure logic (level 0)
**CPP-2** — `src/core/type-conversion.*`. **CPP-3** — `src/core/json-path.*`. Both depend on `json.hpp` only — zero I/O, zero CANoe knowledge, not yet exported.
**TEST-2 / TEST-3** — `tests/core/` coverage: valid input, malformed/missing JSON fields, type mismatches.
**CPP-16 / TEST-12 (optional, folded in here)** — if the `CopyOwnVersionString` buffer/bounds extraction from Stage 5 is taken up, this is where it belongs: `src/core/` is being populated anyway, and the truncation branch becomes testable at near-zero marginal cost. Skip without ceremony if it does not fit cleanly.
These are the first real files to land in `src/` outside `src/module/`; the Makefile's `$(wildcard …)` picks them up automatically, and `make test` starts proving product logic rather than only the harness. **Human approval: no.**

### Stage 9 — HTTP layer and synchronous operations (logic only)
**CPP-4** — `src/http/http-client.*` wrapping libcurl. **CPP-5** — `src/http/sync-operations.*`.
**BPE-10** — Link `libcurl.lib` and `zs.lib` from the matching `lib/<arch>/`, plus all Windows system libs in `SYSLIBS` (now including `version.lib`).
**TEST-4** — Build the libcurl fake/mock boundary. No real network calls in the suite.

**Design constraint from the Makefile.** `TEST_LIBS` is `gtest.lib gtest_main.lib $(LIBS)`, so the test executable links the **real** libcurl. Harmless today, but it means **the mock cannot be a link-time substitution** — a fake `libcurl.lib` cannot simply be swapped in. The seam must be a C++ abstraction inside `http-client.*` that tests inject through. Settle this in **CPP-4's design**, before TEST-4 tries to test around a shape that does not admit a fake.

**TEST-5** — Coverage including **simulated timeouts and error responses**. Verify as a standalone console program against httpbin.org. **Human approval: no.**

### Stage 10 — Expose synchronous REST to CAPL (first contract append)
**CPP-6** — Append sync operations; rebuild both architectures; add the `CHANGELOG.md` entry. **REV-5**. **HUM-14** — Verify in CANoe. **Human approval gate: YES.**

### Stage 11 — Asynchronous layer with response state designed correctly up front
**CPP-7** — `src/http/async-operations.*`: background dispatch, readiness check, wait-for-result. Response-state semantics settled **now, not retrofitted** — ready flag cleared once read, request ID tying a response to the call that produced it. 04-FLOW §5 item 3 records the previous iteration identified this early but never confirmed implementation. Shared state is global within the DLL with per-module synchronization; **one active response at a time** by deliberate design.
**TEST-6** — coverage for ready-flag-cleared-after-read and request-ID correlation across consecutive requests.
**CPP-8** — Append async operations; CHANGELOG entry. **REV-6**. **HUM-15** — Verify in CANoe. **Human approval gate: YES.**

### Stage 12 — JSON flattening (highest user value — ship before struct mapping)
**CPP-9** — `src/mapping/json-flatten.*`: dot-notation key/value map, key count, key-by-index, value-by-key.
**TEST-7** — coverage including deeply nested objects, arrays, empty/malformed documents.
**HUM-16 — Mandatory before any `.can` example is written:** verify associative-field syntax against the official CANoe help (`Help → CAPL → General → Associative Fields`). The correct form has **no extra keyword before the type** — `char[30] name[char[]];`. An invented keyword was copied across many docs and example files last time.
**CPP-10** — Append flattening operations; CHANGELOG entry. **CPP-11** — `examples/*.can`, only after HUM-16. **REV-7**. **Human approval gate: YES.**

### Stage 13 — Typed JSON accessors
**CPP-12** — `src/mapping/json-accessors.*`: typed point reads, array helpers, optional cache.
**TEST-8** — coverage including type mismatches per accessor and cache invalidation between responses.
**CPP-13** — Append accessor operations; CHANGELOG entry. **REV-8**. **Human approval gate: YES.**

---

## 9. Phase 4 — Release Pipeline

### Stage 14 — Tag-driven release with an approval gate

**BPE-11 — Write `.github/workflows/release.yml`.** Triggered by a `vX.Y.Z` tag push. Extracts `X.Y.Z` from `github.ref_name`, overriding `VER_MAJOR`/`VER_MINOR`/`VER_BUILD`/`VER_REV` — the Makefile declares these with `?=` specifically so CI can override without edits. **Reuse `ci.yml`'s provisioning steps rather than rewriting them** — it already encodes all three carry-over traps, the two-part `/MT` check and the cache keying, and a hand-rewritten second copy is exactly how those hard-won fixes get lost. Builds both architectures, runs tests, then **halts at a manual approval gate** (a GitHub Environment with required reviewers), publishing only after approval.

**BPE-12 — Generate the exposed-operation list from the export table at build time.**
**BPE-14 — Convert `CHANGELOG.md`'s `[Unreleased]` into a released section.**
**REV-9 — Review** the release workflow: no hardcoded version anywhere, approval gate genuinely blocks.
**HUM-17 — Create the release tag.** **HUM-18 — Verify the CI-built artifact in CANoe, then approve the publish.**

**First real exercise of the release versioning path.** Everything up to here has run against a zero-tag repository. Stage 14 is the first time `git describe` sees a real tag and CI overrides `VER_*` from it — and `restifyGetVersion` makes that path observable from CAPL for the first time, since it reads the resource the tag produced. A good early check: call `restifyGetVersion` against a tagged CI artifact and confirm the string matches the tag.

**Human approval gate: YES.**

---

## 10. Phase 5 — Hardening

### Stage 15 — Cleanup and consistency pass
**BPE-13** — One `Makefile`, no historical variants; `clean` removes every intermediate; no build artifacts tracked; version and operation list each maintained in exactly one place. Confirm no compiled dependency has crept back into tracking, and that `lib/<arch>/` contains only product libs (BPE-17's invariant, verified against disk rather than against the copy step's own report). Sweep for accumulated comment bloat (BPE-19's category).
**TEST-9** — Coverage audit across `src/`, including an explicit statement of what remains deliberately untested and why (`src/module`, per `cpp-testing-conventions`).
**REV-10** — Final review including a `project-docs` consistency check. Anything removed as dead code must be removed **in full** (export-table entry + implementation + documentation) in a single commit.
**Human approval: no**, unless it touches the export table.

---

## 11. Phase 6 — Conditional Extensions

### Stage 16 (CONDITIONAL) — Struct registry + JSON→struct mapping
Trigger: Stage 13's typed accessors prove insufficient for a concrete use case. **CPP-14** / **TEST-10** / **REV-11**. **Human approval gate: YES.**

### Stage 17 (CONDITIONAL) — CAPL-side request-body building
Trigger: hand-assembling JSON in CAPL proves genuinely cumbersome. Dead code last time. **CPP-15** / **TEST-11** / **REV-12**. **Human approval gate: YES.**

---

## 12. Task index by agent

### `build-pipeline-engineer` — 22 tasks

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
| BPE-10 | 9 | Link `libcurl.lib`, `zs.lib` and the system libs | |
| BPE-11 | 14 | Release workflow — reuse `ci.yml` provisioning, tag extraction, approval gate, publish | |
| BPE-12 | 14 | Generate the operation list from the export table | |
| BPE-13 | 15 | Build-system cleanup pass | |
| BPE-14 | 14 | Cut `CHANGELOG.md` `[Unreleased]` into a released section | |
| BPE-21 | 7 | `auto-pr.yml` — bot-side draft PR, GITHUB_TOKEN, ahead-by + idempotency guards | **NEW — designed, not written** |
| BPE-22 | 7 | `.claude/skills/stage-branch/SKILL.md` — branch creation + push procedure | **NEW — blocked on HUM-21** |
| BPE-20 | 7 | `ci.yml` `branches:` filter — tag-collision fix | **NEW — optional/deferred to Stage 14** |

### `cpp-implementer` — 16 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| CPP-1 | 5 | `exports.cpp` + `exports.def` — one operation; fixes the CAPL naming convention permanently | **DONE — REVIEWED CLEAN** |
| CPP-2 | 8 | `src/core/type-conversion.*` | |
| CPP-3 | 8 | `src/core/json-path.*` | |
| CPP-16 | 8 | *(optional)* extract `CopyOwnVersionString`'s pure buffer/bounds sliver to `src/core/` | **Nice-to-have** |
| CPP-4 | 9 | `src/http/http-client.*` — must expose an injectable seam (see Stage 9) | |
| CPP-5 | 9 | `src/http/sync-operations.*` | |
| CPP-6 | 10 | Append sync operations + CHANGELOG entry | |
| CPP-7 | 11 | `src/http/async-operations.*` + response-state semantics | |
| CPP-8 | 11 | Append async operations + CHANGELOG entry | |
| CPP-9 | 12 | `src/mapping/json-flatten.*` | |
| CPP-10 | 12 | Append flattening operations + CHANGELOG entry | |
| CPP-11 | 12 | `examples/*.can` — only after HUM-16 | |
| CPP-12 | 13 | `src/mapping/json-accessors.*` | |
| CPP-13 | 13 | Append accessor operations + CHANGELOG entry | |
| CPP-14 | 16 | Struct registry + mapping (conditional) | |
| CPP-15 | 17 | CAPL-side request building (conditional) | |

### `test-engineer` — 12 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| TEST-1 | 4 | `tests/` skeleton + one passing test | **DONE — exe built and run (x64 only)** |
| TEST-2 | 8 | `tests/core/` — type-conversion | |
| TEST-3 | 8 | `tests/core/` — json-path | |
| TEST-12 | 8 | *(optional)* truncation-branch coverage for the extracted buffer sliver | **Nice-to-have, paired with CPP-16** |
| TEST-4 | 9 | libcurl fake/mock boundary — source-level seam, not link substitution | |
| TEST-5 | 9 | `tests/http/` — http-client + sync-operations, incl. timeouts and errors | |
| TEST-6 | 11 | `tests/http/` — async: ready-flag-cleared-after-read, request-ID correlation | |
| TEST-7 | 12 | `tests/mapping/` — json-flatten | |
| TEST-8 | 13 | `tests/mapping/` — json-accessors | |
| TEST-9 | 15 | Coverage audit across all of `src/` | |
| TEST-10 | 16 | Struct mapping tests (conditional) | |
| TEST-11 | 17 | Request-builder tests (conditional) | |

### `code-reviewer` — 13 tasks

| ID | Stage | Focus | Status |
|---|---|---|---|
| REV-1 | 1 | Stale-identifier sweep — `CLAUDE.md` and `.claude/**` only | **SATISFIED** |
| REV-2 | 4 | Makefile, versioning, `/MT` provenance, tracking hygiene, provisioning | **CLEAN — CONFIRMED** |
| REV-3 | 5 | **Export-contract genesis — the most important review in the plan** | **CLEAN — ZERO MUST-FIX** |
| REV-4 | 6 | CI reuses Make targets; matrix symmetry; provisions independently | **CLEAN — ZERO MUST-FIX; 1 Should-fix open (BPE-18), 2 nice-to-haves** |
| REV-5 | 10 | Contract append — sync | |
| REV-6 | 11 | Contract append — async | |
| REV-7 | 12 | Contract append — flattening | |
| REV-8 | 13 | Contract append — accessors | |
| REV-9 | 14 | Release workflow; no hardcoded versions; approval gate blocks | |
| REV-10 | 15 | Final consistency review incl. `project-docs` agreement | |
| REV-11 | 16 | Contract append — struct mapping (conditional) | |
| REV-12 | 17 | Contract append — request building (conditional) | |
| REV-13 | 7 | `auto-pr.yml` + `stage-branch` skill + the applied `settings.json` diff | **NEW** |

### Human — 21 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| HUM-1 … HUM-9 | 1 | Configuration reconciliation (nine items) | **ALL DONE** |
| HUM-19 | 2 | Re-run `setup-dev-env.ps1` for real to verify BPE-15 | **DONE — 19/0/0 both arches** |
| HUM-10 | 3 | Install Vector CANoe/CANalyzer | **UNCONFIRMED — blocks HUM-13 only** |
| HUM-11 | 3 | Build + load the official Vector sample unchanged in CANoe | **UNCONFIRMED — blocks HUM-13 only** |
| HUM-12 | 4/5/6 | Commit and push Stage 4 + 5 + 6 work — **this is what first executes CI** | **NEXT ACTION** |
| HUM-13 | 5 | Load and call `restifyGetVersion` from a real `.can` script | **BLOCKED on Stage 3 — the Stage 5 gate** |
| HUM-14 | 10 | Verify sync operations in CANoe | |
| HUM-15 | 11 | Verify async operations in CANoe | |
| HUM-16 | 12 | Verify CAPL associative-field syntax against the official CANoe help | |
| HUM-17 | 14 | Create the release tag | |
| HUM-18 | 14 | Verify the CI artifact in CANoe, then approve the publish | |
| HUM-20 | 7 | GitHub config: branch protection, required checks, Actions-can-create-PRs, auto-delete branches | **NEW** |
| HUM-21 | 7 | Approve exact `settings.json` scoped-push wording **and make the edit by hand** | **NEW — OPEN, blocks BPE-22** |

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

**Export contract — now frozen in its first form.** Stage 5 is irreversible in practice: the `restify<VerbNoun>` naming convention, the sentinel row layout, and the caller-supplied-buffer return shape now bind every later append. Stages 10–13 and 16–17 each append to a table whose first real entry is `restifyGetVersion`.

**Parallel export-table branches can silently reorder already-shipped rows — the sharpest hazard branching introduces.** Git merges two appends to `CAPL_DLL_INFO_LIST4` happily, and the resulting row order depends on merge resolution order. Once branch A has merged and its rows have shipped, a stale branch B based on pre-merge `main` can place its rows *before* A's — a reordering the `capl-export-contract` skill forbids absolutely, that no compiler catches, and that breaks CAPL scripts only at runtime. Mitigated by strictly serializing Stages 10–13 (never two open PRs touching `exports.cpp`) and by "require branches up to date before merging". See Stage 7.10.

**Narrowing the `git push` deny turns an absolute guarantee into a conditional one.** Until Stage 7, no agent could push anything. Afterwards the protection is string patterns on a command with many equivalent spellings (`git -C . push`, `git push origin HEAD:main`, chained commands, and bare `git push` after a `git switch main`). The structural control is GitHub branch protection with bypass disabled, not `.claude/settings.json`. Record it as "the server refuses, and the client discourages" — not as "agents still cannot touch `main`".

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

**Versioning.** `FILEVERSION`/`PRODUCTVERSION` are four 16-bit fields that **wrap silently** above 65535; `/VERSION:` accepts major.minor only. Verified end to end on a zero-tag repository; the untested path is the **release** path where CI overrides `VER_*` from a real tag, first exercised at Stage 14. `restifyGetVersion` now makes that path observable from CAPL, which is a genuine improvement in testability.

**Steps no agent can verify.** The remaining HUM tasks. HUM-12 is immediately actionable; HUM-10, HUM-11 and HUM-13 are the near-term CANoe cluster.

**Scope creep toward struct mapping.** Stages 12 and 13 must ship before Stage 16 is reconsidered.

---

## 14. Operational loose ends

1. **Three stages' worth of work is uncommitted.** `src/module/exports.cpp`, `src/module/exports.def` and `.github/` are untracked; `Makefile` and `CHANGELOG.md` are modified. Because nothing is committed, reviews still cannot prove "only these changes" via a clean diff boundary and must verify by content-reading instead. Committing restores that boundary — and pushing is what first executes CI.
2. **BPE-18 should land in the same commit as the CI workflow,** not after it. A changelog entry that ships with its change is a different artifact from one added later.
3. **`lib/x64/` residue** — see BPE-17. Gitignored, so it will not enter the commit.
4. **BPE-8 has no execution evidence.** Running `make build-x86` and `make build-x64` locally is cheap, unblocked, and would have caught `LNK1561`-class problems before CI does.
5. **The README execution-policy/switches expansion** has still not been through `code-reviewer`. Prose, not executable logic; let it ride along with the next review.
6. **`docs/work/branching-strategy/` is untracked and now superseded.** Its `plans/plan.md` has been reduced to a stub pointing at Stage 7. The folder may simply be deleted instead — nothing in history depends on it, since it was never committed.

---

## 15. Execution order

**1 (done) → 2 (done, verified) → 4 (done) → 5 Half A (done, reviewed clean) → 6 written (reviewed clean) → HUM-12 commit + push → 6 green → 7 (branching + automation) → 3 (unconfirmed) → 5 gate (HUM-13) → 8 → 9 → 10 → 11 → 12 → 13 → 14 → 15**, with Stages 16–17 only on demonstrated need.

**The critical path has forked, and that is the main change in v13.** Stage 3 no longer gates everything. Two tracks now run in parallel:

- **Track A (no CANoe needed, actionable today):** commit and push → observe the first CI run → fix whatever it surfaces → Stage 6 green. This also closes the x86 test evidence gap and supplies BPE-8's build evidence.
- **Track B (human, CANoe):** HUM-10 → HUM-11 → HUM-13 → Stage 5 gate closed.

Stage 7 can begin once Track A is green; it does not need Track B. Stage 8 begins once Stage 7 has landed, on a branch. But **no export-table append (Stage 10 onward) may proceed until HUM-13 has passed** — appending to a table whose base layout has never been loaded by CANoe would multiply the unknowns in exactly the way Stage 5 exists to prevent.

BPE-16, BPE-17, BPE-18 and BPE-19 are non-blocking and can happen at any time; BPE-18 is best folded into the HUM-12 commit.

**Status:** v14. Stages 1, 2 and 4 complete and execution-verified. Stage 5 code complete and reviewed clean, hard gate open on Stage 3. Stage 6 written and reviewed clean, never executed. Stage 7 designed and settled (Option A auto-PR, draft PRs, hand-written `gh` script, BPE-20 deferred), with HUM-21's exact `settings.json` wording the sole open decision. Next action: HUM-12.
