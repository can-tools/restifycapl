# Plan (v11): Build the CAPL REST DLL (restifycapl) from zero

Revision of v10. Stage 4 is now complete: `.gitignore` revised, `lib/README` template re-specified, the `vcpkg.json` manifest adopted, and the tracked binaries un-committed. The bootstrap script's migration remains unverified by a real run.

---

## 1. Goal

Build the CAPL REST DLL (`restifycapl`) from the cloned repository to a working, dual-architecture (x86 + x64) native CANoe plugin: synchronous and asynchronous REST/HTTP for CAPL scripts plus JSON flattening and typed path accessors. Struct mapping and CAPL-side request building stay deferred until a demonstrated need. The CANoe ABI is proven on one trivial operation before any REST logic exists. All business logic is unit-tested outside CANoe. The export table stays a thin, append-only glue layer. Every version number derives from the Git tag.

---

## 2. Dependency architecture — the governing principle

**Compiled binary artifacts are never shared between environments.**

A developer machine and a CI runner are independent environments. Each provisions its own dependency binaries, built by its own toolchain, from a **shared source-level pin** — a version number in a manifest file, not a committed `.lib`.

| Shared (committed) | Not shared (provisioned per environment) |
|---|---|
| `vcpkg.json` — the authoritative pin | `lib/x86/*.lib`, `lib/x64/*.lib` (libcurl, zlib) |
| `include/vendor/json.hpp` — pinned source header, SHA-256 verified | `lib/gtest/x86/*.lib`, `lib/gtest/x64/*.lib` |
| `include/vendor/capl-dll-sdk/` — source headers, no package-manager path exists | `include/vendor/gtest/` — headers ship with the gtest libs and must stay version-matched to them |

**Why this matters more than the ABI question it replaces.** v8 and v9 committed compiled `.lib` files and proposed managing the resulting toolchain-drift risk — cataloguing the failure mode, noting it surfaces loudly as `LNK2038`, and pre-designing an escape hatch. That was risk *management* where risk *elimination* was available at no cost. Under this architecture CI never links a binary that some laptop produced, so MSVC's cross-toolset compatibility guarantee is never put under test in the first place. A structural fix beats a monitored one.

**Scope of the reversal.** This reversed the "commit the vendored binaries" position for **libcurl and zlib as well as GoogleTest**. The earlier justification distinguished them on ABI fragility (C boundary versus C++ boundary), but that is an argument about how likely one is to *get away with* sharing an artifact, not about whether sharing is sound.

**What does not change:** vendored *source* stays committed. `json.hpp` is a single pinned, checksummed source file — no compilation, no ABI, no environment coupling. The Vector SDK headers are source for which no package-manager path exists, and the decision to commit them to a public repository stands on its own separately-accepted grounds.

**The pin is `vcpkg.json`, and only the pin.** Note that the manifest's `"version-string": "0.0.0"` is required manifest boilerplate describing *this project as a vcpkg package*. It feeds nothing: not `version.rc`, not the Makefile's `VER_*`, not `/VERSION:`. The product version remains derived solely from the Git tag. This is worth stating explicitly because a hardcoded `0.0.0` sitting in a tracked file looks exactly like the second version source that `msvc-build-conventions` forbids. See BPE-16.

---

## 3. Current state

**Stage 1 — complete and verified.**

**Stage 2 — complete, but the manifest migration is unverified by a real run.** `scripts/setup-dev-env.ps1` last produced 19 OK / 0 WARN / 0 FAIL, but that result predates BPE-15's migration to manifest mode. **A prior clean result does not transfer to modified code** — this is the project's most consistently validated lesson, and it applies to the script that proved it.

**Stage 3 — UNCONFIRMED, and the critical path.** Neither HUM-10 (install CANoe) nor HUM-11 (build and load the official Vector sample) is observable from the repository. Stage 3 hard-blocks Stage 5.

**Stage 4 — COMPLETE.** All of BPE-2 through BPE-7 plus BPE-15 and TEST-1 are done and independently verified:

- `vcpkg.json` exists with a real `builtin-baseline` (`386d7c47…`, confirmed against the actual local vcpkg checkout as a genuine dated commit and an exact `HEAD` match) and a `curl` override of `8.22.0` matching the real port file at that baseline.
- `.gitignore` carries `*.lib` plus the four scoped ignores (`lib/x86/`, `lib/x64/`, `lib/gtest/`, `include/vendor/gtest/`), with `lib/README` correctly left tracked.
- The `lib/README` template inside the script (~lines 986–1062) contains every required element: provisioned/uncommitted framing, exact filenames including the unguessable `zs.lib`, the product-linked versus test-only distinction, the `manual-link` split rationale, and "the authoritative pin is `vcpkg.json`". It also distinguishes `json.hpp` as committed *source* rather than a provisioned binary — beyond what was specified, and correct.
- The previously-tracked `lib/x86/*.lib` and `lib/x64/*.lib` are removed from the index while remaining on disk. A stray untracked `nul` file (a shell-redirection artifact) was deleted.

**Next:** re-run the script for real (BPE-15 verification); confirm Stage 3; then Stage 5.

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
- **`/MT` static CRT** for the DLL and every static dependency. Never mix `/MT` and `/MD` in one link. Verify with `dumpbin /directives` — expect `/DEFAULTLIB:LIBCMT`, never `MSVCRT`.
- **Bitness parity.** x86 and x64 must both build and behave identically — same sources, same flags (`/std:c++17`, `/EHsc`, `/MT`), same export table. Only `/MACHINE:` and library path differ; the Makefile enforces this structurally via one parameterized rule.
- **Export contract.** The `CAPL_DLL_INFO_LIST`/`CAPL_DLL_INFO4` table in `src/module/exports.cpp` is the real API. Append-only; never rename, reorder, or remove. Reserved first entry (`CDLL_VERSION_NAME`/`CDLL_VERSION`) always present. Exported functions `extern "C"`.
- **`exports.def` contains `EXPORTS` and nothing else** — no `LIBRARY` line; it is shared by both architecture builds, which emit differently-named DLLs.
- **Never return a raw text pointer** from a CAPL-exposed operation — always write into a caller-supplied buffer with its size.
- **1-byte packing must cover the entire export table** through and including the terminating pointer.
- **Dependency direction.** `src/core/` imports nothing from `src/http/`, `src/registry/`, `src/mapping/`. Only `src/module/` includes the CAPL SDK headers. `make test` compiles `src/core`, `src/http`, `src/registry`, `src/mapping` and deliberately excludes `src/module`.
- **`lib/<arch>/` is product-linked; `lib/gtest/<arch>/` is test-only** and must never enter the DLL link line.
- **Tests run outside CANoe.**
- **No version number is ever typed by hand.** `vcpkg.json`'s `version-string` is manifest boilerplate and is not an exception — it feeds nothing.
- **Every export-table append gets a `CHANGELOG.md` `[Unreleased]` entry in the same change.**
- **Agents cannot `git push` or `git tag`.**

---

## 6. Phase 1 — Foundation & Environment

### Stage 1 — Configuration reconciliation — COMPLETE

All nine items (HUM-1 … HUM-9) applied and verified. **REV-1 — SATISFIED**, scope corrected to `CLAUDE.md` and `.claude/**` only.

Three permanent rules stand: only `CLAUDE.md` and `.claude/**` are operationally loaded and must be swept; `docs/.locals/**` is exempt as historical record (the v1–v3 drafts carry the user's inline answers, intelligible only alongside the questions they answered); the sweep is self-matching and excludes this plan file. `Grep` honours `.gitignore` and produces the correct scope naturally; Bash `grep -r` / `rg --no-ignore` do not.

### Stage 2 — Local development-environment bootstrap — COMPLETE, RE-VERIFICATION OUTSTANDING

**BPE-1 — DONE, but its proof is now stale.** The script provisions toolchain, dependencies and headers on the developer's machine, built by the developer's own MSVC. Its 19 OK / 0 WARN / 0 FAIL run predates BPE-15's manifest-mode migration.

Two of its four original bugs were invisible to static review — a Polish-localized `cl.exe` banner defeating an English-only architecture check, and a `curl[schannel]` vcpkg feature name that no longer exists. The `VSCMD_ARG_TGT_ARCH` delayed-expansion fix was additionally verified to still fail correctly on a genuinely wrong architecture. The GoogleTest step surfaced a non-obvious vcpkg layout fact found by reading the port files: the port's own patch relocates `gtest_main.lib` to `lib/manual-link/`, separate from `gtest.lib` in plain `lib/`, so the copy logic handles both source directories.

**Outstanding: a real re-run.** The migration passes static review (fresh AST parse, 0 errors; a full grep for classic-mode residue — `Install-CurlTriplet`, `Install-GTestTriplet`, `VcpkgRootPath`, old `installed\$Triplet` paths — found only comments contrasting old against new for future readers). None of that is execution. Until a human runs it, BPE-15 is unverified.

**Known precondition for that run.** The local vcpkg checkout at `%LOCALAPPDATA%\vcpkg` is **confirmed shallow right now** — not a hypothetical. Manifest mode with a `builtin-baseline` needs the registry history at that commit, so the run must either exercise the script's `Repair-ShallowVcpkgClone` self-heal (present and correctly placed) or be preceded by a manual `git fetch --unshallow`. This makes the re-run a genuine test of the self-heal rather than a formality.

**Clarification of the script's role.** `setup-dev-env.ps1` is the **local** provisioning path only. CI must never run it — it installs VS Build Tools, `make` and vcpkg itself. The two paths share the *pin*, never the *mechanism* and never the *output*.

### Stage 3 — Manual toolchain setup (human only) — UNCONFIRMED, GATES STAGE 5

No automation path exists for either item — a licensing constraint, not a technical one, and neither is observable from the repository.

**HUM-10 — Install Vector CANoe/CANalyzer.**

**HUM-11 — Build and load the official "Example of a Windows DLL for CAPL" sample, unchanged, in CANoe.** Worth doing before writing a line of `exports.cpp`: if the official sample does not load, that is an environment problem, and discovering it while simultaneously debugging a first hand-written export table is the compounded-unknowns situation 04-FLOW warns against.

**This is the critical path.** Stage 4 is finished, so Stage 3 is what stands between the project and the Stage 5 hard gate.

### Stage 4 — Repo skeleton, Makefile, versioning, dependency pinning — COMPLETE

**BPE-2 — DONE, verified.** `.gitignore` now ignores `*.lib` on purpose, with scoped exclusions for `lib/x86/`, `lib/x64/`, `lib/gtest/` and `include/vendor/gtest/`. Scoping to subdirectories rather than `lib/` keeps `lib/README` tracked. The `# Makefile` annotation is untouched; that reasoning was always independent.

**BPE-3 — DONE.** Skeleton present.

**BPE-4 — DONE, verified with the real GNU Make 3.81 binary.** `build-x86`/`build-x64` are thin recursive wrappers over one `_build` rule; the per-ARCH block is the only place the architectures differ; `clean` removes all of `build/`.

**BPE-5 — DONE, decisively settled.** Everything derives: `GIT_DESCRIBE`, `LAST_TAG` with zero-tag fallback, `VER_*` declared with `?=` so CI can override from the tag, `rc.exe /D` passing all five, `/VERSION:` on the link line. `version.rc` carries `#ifndef` fallbacks with a comment stating they are not a second home for version numbers.

The `VER_STRING` quoting was contested across two reviews — one reproduction produced RC2237 and looked like a real defect. It was settled by running the actual Make binary against the actual Makefile rather than an approximated shell invocation, then decoding the compiled `.res`'s UTF-16LE `FileVersion`/`ProductVersion` fields: both correct and unmangled. **The RC2237 was an artifact of the reproduction, not the build.** When a hand-reconstructed invocation and the real build disagree, the build wins.

**BPE-6 — DONE, verified.** The `lib/README` template inside the script (~lines 986–1062) carries the provisioned/uncommitted framing, the exact filenames (`libcurl.lib`, `zs.lib`, `gtest.lib`, `gtest_main.lib`, with an explicit note that `zs.lib` is not the name anyone would guess), the product-linked versus test-only distinction, the `manual-link` split rationale, per-environment `/MT` verification, and "the authoritative pin is `vcpkg.json`". It additionally distinguishes `json.hpp` as committed source rather than a provisioned binary — beyond specification and correct.

Editing the template rather than the generated file was the right call: `lib/README` is script-generated and a hand edit would have silently reverted on the next run.

**BPE-7 — DONE.** GoogleTest via vcpkg into `lib/gtest/<arch>/`. The separation is real in the link line, not just in folder naming: the Makefile's `LIBS` (product) contains only `libcurl.lib zs.lib` plus the seven system libs, while `TEST_LIBS` adds `gtest.lib gtest_main.lib` with a separate `/LIBPATH:$(GTESTDIR)` used solely by the test recipe.

**BPE-15 — DONE (code), UNVERIFIED (execution).** `vcpkg.json` is the single authoritative pin:

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

The baseline resolves to a genuine dated commit in the real vcpkg registry and is an exact `HEAD` match; the `curl` override matches the port file at that exact baseline. The script is migrated to manifest mode with a `Repair-ShallowVcpkgClone` self-heal. **Execution verification is outstanding — see Stage 2.**

**Un-commit — DONE.** `lib/x86/*.lib` and `lib/x64/*.lib` removed from the index, files retained on disk (confirmed present). `lib/gtest/**/*.lib` and `include/vendor/gtest/` were never staged. Blobs remain in history; no rewrite proposed.

**BPE-16 — NEW, small. Preempt the `version-string` confusion.** Add one line stating that `vcpkg.json`'s `"version-string": "0.0.0"` is manifest boilerplate and is not a product version source. Best home is `msvc-build-conventions`' Versioning section, which is where someone checks "is this a version source?" and which currently says no version is ever hand-edited — a hardcoded `0.0.0` in a tracked file reads as a counterexample. A line in the `lib/README` template is a reasonable second home, since it already discusses `vcpkg.json`. Non-blocking.

**TEST-1 — DONE.** `tests/core/sanity-test.cpp` plus a real linked `build/test/x64/restifycapl-tests.exe`.

This surfaced a real latent bug that could not manifest until a real `main()`-providing static library existed to link against: **`link.exe` infers the subsystem and entry point only from `main`/`WinMain` in `.obj` files passed directly on the command line, never transitively from a `.lib`.** With `main()` coming solely from `gtest_main.lib`, the link failed `LNK1561`. Root-caused with `/VERBOSE` and `dumpbin /symbols`, fixed with `/SUBSYSTEM:CONSOLE` on the test recipe only, and independently re-verified by a separate reviewer running its own `dumpbin` and `make test`.

**What `make test` currently proves.** All of `src/core`, `src/http`, `src/registry`, `src/mapping` contain only `.gitkeep`, so the suite compiles exactly one file. Green proves **the harness** — flags, include paths, GoogleTest linkage, subsystem, runner — and no project logic. That is what TEST-1 was for.

**x86 evidence gap.** Only `build/test/x64/` artifacts exist. The Makefile fully parameterizes the test tree, so `make test ARCH=x86` is supported by construction, but an x86 run is not evidenced. Stage 6's CI should run tests on both architectures so this stops depending on anyone remembering.

**REV-2 — CLEAN, pending the re-run.** The full review passed with one Must-fix — the still-tracked `.lib` files — which has since been closed. The `/MT` provenance check (`dumpbin /directives`, expecting `/DEFAULTLIB:LIBCMT`) is now a **per-environment** check: it validates local provisioning, and CI must run the equivalent against its own copies.

**HUM-12 — Commit and push.**

---

## 6a. Documentation practice — standing, not a stage

**Why not a numbered stage.** Documentation here is a standing obligation attached to other work — a CHANGELOG entry whenever the export table gains an entry, README updates when the build story changes, release mechanics at Stage 13. Encoding it as a stage would imply it finishes.

**What exists:** `.claude/skills/project-docs/SKILL.md` (three-documents-three-audiences model, wired into `build-pipeline-engineer` and `cpp-implementer`); `CHANGELOG.md`; `README.md`; `docs/development-environment.md`.

**README impact of §2.** The Development setup section must state plainly that provisioning is **required**, not a convenience: a fresh clone cannot build or test until `scripts/setup-dev-env.ps1` has run, because no compiled dependency is committed.

**Drift watch.** Rationale living in `docs/development-environment.md` rather than inline keeps the script readable but creates two artifacts that must move together. The Makefile deliberately takes the opposite approach for its two hardest-won findings — the `/SUBSYSTEM:CONSOLE` requirement and the `make -n` quirk are long inline comments, because both are traps a future reader hits *while editing that exact recipe*. Rationale belongs where it will be read at the moment it is needed; that is a judgement per case, not a uniform rule.

---

## 7. Phase 2 — ABI Proof & Continuous Verification

### Stage 5 — "Hello DLL": one operation, verified in CANoe — HARD GATE

**Blocked on Stage 3.**

**CPP-1 — Write `src/module/exports.cpp` and `exports.def`.** Exactly **one** trivial operation (e.g. return a fixed version string into a caller-supplied buffer). `exports.def` contains `EXPORTS` and nothing else. Apply both post-mortem rules from §5. **This stage permanently fixes the CAPL-visible naming convention** for every operation the project will ever expose — the never-rename rule means it cannot be revised later without a major-version break. Decide it deliberately and write it down.

The Makefile already references `src/module/exports.def` as a prerequisite of the DLL target, so `make build-x64` currently fails on the missing file. Expected and correct: the build is wired and waiting for this stage's deliverable.

**BPE-8 — Link and resource wiring** for both architectures, producing `build/x86/restifycapl-x86.dll` and `build/x64/restifycapl-x64.dll`; verify with `dumpbin /exports`.

**REV-3 — Export-contract review. The single most important review in this plan.**

**HUM-13 — Load and call the operation from a real `.can` script in CANoe.** No agent can do this.

**Human approval gate: YES — the most important gate in this plan.**

### Stage 6 — CI baseline: independent provisioning, build, and test on every push

**BPE-9 — Write `.github/workflows/ci.yml`.** `windows-latest`, matrix over x86/x64, MSVC environment activated for the matching architecture before invoking Make. **Calls the same `build-x86`/`build-x64` Make targets used locally** — never duplicates `cl.exe`/`rc.exe` invocations in YAML. Runs `make test` for **both** architectures, making bitness parity continuously enforced rather than spot-checked. Uploads both DLLs as workflow artifacts.

**CI provisioning step — the structural core of §2.** Before Make, CI runs its own vcpkg install from `vcpkg.json`, on the runner, producing binaries built by the runner's own toolchain:

- CI **must not** run `scripts/setup-dev-env.ps1`.
- The provisioning step places libs where the Makefile expects them (`lib/<arch>/`, `lib/gtest/<arch>/`), so the Makefile needs no CI-specific branch.
- CI runs its own `dumpbin /directives` `/MT` check — the CI-side equivalent of REV-2.

**Carry-over trap for whoever writes this — identified by REV-4's readiness pre-check.** The shallow-clone failure is a property of **how vcpkg gets bootstrapped**, not of the manifest. A naive shallow clone of the vcpkg registry on a runner hits the identical failure the local machine is hitting right now. BPE-9 must therefore either do a full clone or replicate the unshallow self-heal. This is the clearest example of the §13 provisioning-drift risk: two mechanisms honouring one pin can still fail differently at the bootstrap layer beneath it.

**Caching — purely a performance concern, never a correctness one.** Because no committed binary is involved, caching can be added, tuned or removed without affecting whether the build is correct. Recommended: `actions/cache` over the vcpkg binary cache or `vcpkg_installed/`, keyed on a hash of `vcpkg.json` **plus the triplet plus the runner image version**. Including the image version is the important detail: when GitHub bumps the runner's toolset, the key changes, the cache misses, and dependencies rebuild against the new compiler automatically. The cache stays correct by construction rather than by anyone remembering to invalidate it.

GitHub evicts cache entries after 7 days without access and this project pushes in bursts, so cold builds will be common — a cost measured in minutes, not a correctness risk.

**REV-4 — NOT STARTED.** Only a readiness pre-check exists, run before `.github/workflows/` exists. It found the design sound — the manifest is self-sufficient as a CI-readable pin, and nothing in the script's local-machine-only structure blocks CI from having its own simpler provisioning step — and produced the shallow-clone carry-over above. The real REV-4 waits on BPE-9.

**Human approval gate: YES.**

---

## 8. Phase 3 — Business Logic & CAPL Surface

Stages 9–12 each append to the export table. Every append requires `code-reviewer`, a human gate, and a `CHANGELOG.md` `[Unreleased]` entry in the same change.

### Stage 7 — Core pure logic (level 0)
**CPP-2** — `src/core/type-conversion.*`. **CPP-3** — `src/core/json-path.*`. Both depend on `json.hpp` only — zero I/O, zero CANoe knowledge, not yet exported.
**TEST-2 / TEST-3** — `tests/core/` coverage: valid input, malformed/missing JSON fields, type mismatches.
These are the first files to land in `src/`; the Makefile's `$(wildcard …)` picks them up automatically. **Human approval: no.**

### Stage 8 — HTTP layer and synchronous operations (logic only)
**CPP-4** — `src/http/http-client.*` wrapping libcurl. **CPP-5** — `src/http/sync-operations.*`.
**BPE-10** — Link `libcurl.lib` and `zs.lib` from the matching `lib/<arch>/`, plus all seven Windows system libs.
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

**BPE-11 — Write `.github/workflows/release.yml`.** Triggered by a `vX.Y.Z` tag push. Extracts `X.Y.Z` from `github.ref_name`, overriding `VER_MAJOR`/`VER_MINOR`/`VER_BUILD`/`VER_REV` — the Makefile declares these with `?=` specifically so CI can override without edits. Uses the same independent provisioning step as Stage 6, including the shallow-clone precaution. Builds both architectures, runs tests, then **halts at a manual approval gate** (a GitHub Environment with required reviewers), publishing only after approval.

**BPE-12 — Generate the exposed-operation list from the export table at build time.**
**BPE-14 — Convert `CHANGELOG.md`'s `[Unreleased]` into a released section.**
**REV-9 — Review** the release workflow: no hardcoded version anywhere, approval gate genuinely blocks.
**HUM-17 — Create the release tag.** **HUM-18 — Verify the CI-built artifact in CANoe, then approve the publish.**
**Human approval gate: YES.**

---

## 10. Phase 5 — Hardening

### Stage 14 — Cleanup and consistency pass
**BPE-13** — One `Makefile`, no historical variants; `clean` removes every intermediate; no build artifacts tracked; version and operation list each maintained in exactly one place. Confirm no compiled dependency has crept back into tracking.
**TEST-9** — Coverage audit across `src/`.
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

### `build-pipeline-engineer` — 16 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| BPE-1 | 2 | `scripts/setup-dev-env.ps1` — local provisioning | **DONE — proof stale after BPE-15** |
| BPE-2 | 4 | `.gitignore` — excludes provisioned binaries, keeps `lib/README` | **DONE** |
| BPE-3 | 4 | Directory skeleton | **DONE** |
| BPE-4 | 4 | `Makefile`, one parameterized rule | **DONE — run with real GNU Make** |
| BPE-5 | 4 | Git-tag versioning wiring | **DONE — `.res` fields decoded and verified** |
| BPE-6 | 4 | `lib/README` template — provisioned framing + exact filenames | **DONE** |
| BPE-7 | 4 | GoogleTest via vcpkg into `lib/gtest/<arch>/` | **DONE** |
| BPE-15 | 4 | `vcpkg.json` manifest + script migration to manifest mode | **DONE (code) — real re-run OUTSTANDING** |
| BPE-16 | 4 | One-line note that `vcpkg.json`'s `version-string` is not a version source | **NEW, non-blocking** |
| BPE-8 | 5 | Link/resource wiring for both DLLs; `dumpbin /exports` | |
| BPE-9 | 6 | CI: independent provisioning (full clone or unshallow self-heal) + build + `make test` both arches + artifacts + caching | |
| BPE-10 | 8 | Link `libcurl.lib`, `zs.lib` and the seven system libs | |
| BPE-11 | 13 | Release workflow — provisioning, tag extraction, approval gate, publish | |
| BPE-12 | 13 | Generate the operation list from the export table | |
| BPE-13 | 14 | Build-system cleanup pass | |
| BPE-14 | 13 | Cut `CHANGELOG.md` `[Unreleased]` into a released section | |

### `cpp-implementer` — 15 tasks

| ID | Stage | Task |
|---|---|---|
| CPP-1 | 5 | `exports.cpp` + `exports.def` — one operation; fixes the CAPL naming convention permanently |
| CPP-2 | 7 | `src/core/type-conversion.*` |
| CPP-3 | 7 | `src/core/json-path.*` |
| CPP-4 | 8 | `src/http/http-client.*` — must expose an injectable seam (see Stage 8) |
| CPP-5 | 8 | `src/http/sync-operations.*` |
| CPP-6 | 9 | Append sync operations + CHANGELOG entry |
| CPP-7 | 10 | `src/http/async-operations.*` + response-state semantics |
| CPP-8 | 10 | Append async operations + CHANGELOG entry |
| CPP-9 | 11 | `src/mapping/json-flatten.*` |
| CPP-10 | 11 | Append flattening operations + CHANGELOG entry |
| CPP-11 | 11 | `examples/*.can` — only after HUM-16 |
| CPP-12 | 12 | `src/mapping/json-accessors.*` |
| CPP-13 | 12 | Append accessor operations + CHANGELOG entry |
| CPP-14 | 15 | Struct registry + mapping (conditional) |
| CPP-15 | 16 | CAPL-side request building (conditional) |

### `test-engineer` — 11 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| TEST-1 | 4 | `tests/` skeleton + one passing test | **DONE — exe built and run** |
| TEST-2 | 7 | `tests/core/` — type-conversion | |
| TEST-3 | 7 | `tests/core/` — json-path | |
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
| REV-2 | 4 | Makefile, versioning, `/MT` provenance, tracking hygiene | **CLEAN — its one Must-fix closed; re-run still owed** |
| REV-3 | 5 | **Export-contract genesis — the most important review in the plan** | |
| REV-4 | 6 | CI reuses Make targets; matrix symmetry; provisions independently | **NOT STARTED — readiness pre-check only** |
| REV-5 | 9 | Contract append — sync | |
| REV-6 | 10 | Contract append — async | |
| REV-7 | 11 | Contract append — flattening | |
| REV-8 | 12 | Contract append — accessors | |
| REV-9 | 13 | Release workflow; no hardcoded versions; approval gate blocks | |
| REV-10 | 14 | Final consistency review incl. `project-docs` agreement | |
| REV-11 | 15 | Contract append — struct mapping (conditional) | |
| REV-12 | 16 | Contract append — request building (conditional) | |

### Human — 18 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| HUM-1 … HUM-9 | 1 | Configuration reconciliation (nine items) | **ALL DONE** |
| HUM-10 | 3 | Install Vector CANoe/CANalyzer | **UNCONFIRMED — gates Stage 5** |
| HUM-11 | 3 | Build + load the official Vector sample unchanged in CANoe | **UNCONFIRMED — gates Stage 5** |
| HUM-12 | 4 | Commit and push | |
| HUM-19 | 2 | Re-run `setup-dev-env.ps1` for real to verify BPE-15 | **OUTSTANDING** |
| HUM-13 | 5 | Load and call the Hello DLL operation from a real `.can` script | |
| HUM-14 | 9 | Verify sync operations in CANoe | |
| HUM-15 | 10 | Verify async operations in CANoe | |
| HUM-16 | 11 | Verify CAPL associative-field syntax against the official CANoe help | |
| HUM-17 | 13 | Create the release tag | |
| HUM-18 | 13 | Verify the CI artifact in CANoe, then approve the publish | |

---

## 13. Risks

**Static review does not substitute for execution — five instances, the project's most reliable predictor of defects.** (1) Polish-localized `cl.exe` banner defeating an English-only architecture check; (2) a `curl[schannel]` vcpkg feature name that no longer exists; (3) `%VSCMD_ARG_TGT_ARCH%` returning its own literal text; (4) `LNK1561` from `link.exe` refusing to infer an entry point from a `.lib`; (5) the `make -n` `CreateProcess` artifact. Item 4 was unpredictable in principle — it could not manifest until a real `main()`-providing `.lib` existed. The RC2237 scare showed the inverse failure: a hand-reconstructed invocation produced a defect the real build did not have.

**This risk is live right now.** BPE-15 has passed a thorough static review — AST parse, residue grep, baseline SHA resolved against the real registry — and none of that is execution. The migration is unverified until HUM-19 runs it, and the known-shallow local vcpkg clone means the first run genuinely exercises the self-heal rather than skipping past it.

**Sharing compiled artifacts between environments — the anti-pattern this architecture removes.** v8 and v9 committed `.lib` files, making CI link binaries produced on a developer's machine, and proposed monitoring for toolchain drift with a pre-planned escape hatch. That was risk management substituting for an available structural fix. The general lesson: when a failure class can be *designed out* at comparable cost, designing it out beats detecting it — and a proposal that ships with a pre-planned fallback to a *different architecture* is a signal the fallback may be the right primary.

**Provisioning drift between local and CI — and beneath the pin.** Two mechanisms (`setup-dev-env.ps1` and the CI step) read one manifest, and can still diverge in *where they place* libraries or *which triplet* they select. The shallow-clone trap is the sharper form: it sits **below** the manifest, in how vcpkg itself is obtained, where the shared pin gives no protection at all. A runner doing a naive shallow clone fails exactly as the local machine currently does. REV-4 checks placement and triplet; BPE-9 must handle the bootstrap layer explicitly.

**Export contract.** Stage 5 is irreversible in practice: the naming convention and version-entry layout chosen there bind every later append. Stages 9–12 and 15–16 each append.

**ABI failure modes that hide *all* operations, not just the new one.** Raw text pointer instead of the caller-supplied buffer; incorrect 1-byte alignment coverage through the terminating pointer.

**`/MT` contamination.** Now a per-environment check rather than a property of a committed artifact. If a dependency is only available as `/MD`, **stop and ask**.

**Bitness parity — structurally enforced for the build, not yet continuously verified for tests.** The single parameterized Make rule makes flag drift impossible by construction, but only x64 test artifacts are evidenced. Stage 6's CI should run tests on both architectures.

**Apparent second version sources.** `vcpkg.json`'s `version-string` feeds nothing, but reads as a counterexample to "no version number is ever hand-edited". BPE-16 preempts it. The general shape — a tracked file containing a literal version that is not *the* version — will recur; label them where they appear.

**Generated files silently reverting hand edits.** `lib/README` is produced by `setup-dev-env.ps1`; editing the artifact instead of the template appears to work and vanishes on the next run. BPE-6 was handled correctly on exactly this basis.

**Documentation drift between the script and `docs/development-environment.md`.** Two artifacts that must move together.

**Configuration drift — closed, with a scoped standing check.** REV-1's corrected scope excludes historical drafts and its own definition.

**Agent routing metadata is easy to miss.** Body and frontmatter `description` are separate surfaces.

**Vector SDK headers remain committed and remain an accepted risk.** Public repository, explicit user decision. Unaffected by §2: source, not compiled output, and no package-manager path exists.

**Versioning.** `FILEVERSION`/`PRODUCTVERSION` are four 16-bit fields that **wrap silently** above 65535; `/VERSION:` accepts major.minor only. Verified end to end on a zero-tag repository; the untested path is the **release** path where CI overrides `VER_*` from a real tag, first exercised at Stage 13.

**Steps no agent can verify.** The ten remaining HUM tasks, of which HUM-19 (script re-run) and HUM-10/HUM-11 (CANoe) are the near-term ones.

**Scope creep toward struct mapping.** Stages 11 and 12 must ship before Stage 15 is reconsidered.

---

## 14. Operational loose ends

1. **The README execution-policy/switches expansion** has still not been through `code-reviewer`. Prose, not executable logic; let it ride along with the next review.
2. **Commit hygiene.** Confirm nothing is half-staged before committing.

---

## 15. Execution order

**1 (done) → 2 (done, HUM-19 re-run outstanding) → 4 (done) → 3 (UNCONFIRMED — critical path) → 5 (hard gate) → 6 → 7 → 8 → 9 → 10 → 11 → 12 → 13 → 14**, with Stages 15–16 only on demonstrated need.

Immediate, none blocking each other: **HUM-19** — re-run the script for real, resolving the shallow vcpkg clone; **HUM-10 / HUM-11** — confirm Stage 3, the gate on everything downstream; **BPE-16** — the one-line `version-string` note.

**Status:** v11. Stage 4 complete. Stage 2 complete in code, unverified in execution. Stage 3 remains the gate on everything downstream.
