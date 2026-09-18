# Plan (v12): Build the CAPL REST DLL (restifycapl) from zero

Revision of v11. **Stage 4 is fully complete and execution-verified.** The bootstrap script's manifest migration was re-run for real at 19 OK / 0 WARN / 0 FAIL on both architectures after two genuine bugs were found and fixed. Stage 3 is now the sole remaining blocker before Stage 5.

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

**Two pins, not one.** `vcpkg.json`'s `builtin-baseline` pins the registry *content* (which port versions). It does not pin the vcpkg *tool*. Since triplet isolation now depends on `--x-install-root`, a flag vcpkg's own `--help` marks `(experimental)`, the tool binary is pinned too — see §5 and BPE-15.

**The pin is `vcpkg.json`, and only the pin.** The manifest's `"version-string": "0.0.0"` is required boilerplate describing *this project as a vcpkg package*. It feeds nothing: not `version.rc`, not the Makefile's `VER_*`, not `/VERSION:`. The product version remains derived solely from the Git tag. Worth stating explicitly because a hardcoded `0.0.0` in a tracked file looks exactly like the second version source that `msvc-build-conventions` forbids. See BPE-16.

**What does not change:** vendored *source* stays committed. `json.hpp` is a single pinned, checksummed source file. The Vector SDK headers are source for which no package-manager path exists, and the decision to commit them to a public repository stands on separately-accepted grounds.

---

## 3. Current state

**Stage 1 — complete and verified.**

**Stage 2 — COMPLETE AND EXECUTION-VERIFIED.** `scripts/setup-dev-env.ps1` ran end to end at **19 OK / 0 WARN / 0 FAIL on both architectures**, after two real bugs were found and fixed (see BPE-15). This closes HUM-19.

**Stage 3 — UNCONFIRMED. The sole remaining blocker before Stage 5.** Neither HUM-10 (install CANoe) nor HUM-11 (build and load the official Vector sample) is observable from the repository. Nothing in the dependency-architecture work blocks anything any more.

**Stage 4 — COMPLETE.**

**One local-hygiene item outstanding (BPE-17).** `lib/x64/` currently holds four `.lib` files — `libcurl.lib`, `zs.lib`, plus stale `gmock.lib` and `gtest.lib` left over from a pre-fix run. `lib/x86/` holds exactly the correct two. This is residue, not a regression: `Copy-TripletLibs` is additive and never prunes its destination. Details and consequences in BPE-17.

**Next:** confirm Stage 3, then Stage 5.

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
- **Both the vcpkg registry content and the vcpkg tool binary are pinned.** `builtin-baseline` pins port versions; `$VcpkgPinnedTag` (currently `2026.07.29`, commit `9e593bb18ea69cc5095e012465dcd675a822ed0d`) pins the tool. The tool pin exists because triplet isolation depends on `--x-install-root`, which vcpkg marks `(experimental)`. Pin-enforcement failure is **WARN, not FAIL** — it is defence in depth over an already-solid content pin, and aborting an otherwise-working provisioning run over a speculative future flag change would be the wrong trade. WARN is not silent here, because the project's success bar is 0 WARN.
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

**Clarification of the script's role.** `setup-dev-env.ps1` is the **local** provisioning path only. CI must never run it — it installs VS Build Tools, `make` and vcpkg itself. The two paths share the *pin*, never the *mechanism* and never the *output*.

### Stage 3 — Manual toolchain setup (human only) — UNCONFIRMED, SOLE REMAINING BLOCKER

No automation path exists for either item — a licensing constraint, not a technical one, and neither is observable from the repository.

**HUM-10 — Install Vector CANoe/CANalyzer.**

**HUM-11 — Build and load the official "Example of a Windows DLL for CAPL" sample, unchanged, in CANoe.** Worth doing before writing a line of `exports.cpp`: if the official sample does not load, that is an environment problem, and discovering it while simultaneously debugging a first hand-written export table is the compounded-unknowns situation 04-FLOW warns against.

**With Stage 4 closed, this is the only thing standing between the project and the Stage 5 hard gate.**

### Stage 4 — Repo skeleton, Makefile, versioning, dependency pinning — COMPLETE

**BPE-2 — DONE.** `.gitignore` ignores `*.lib` on purpose, with scoped exclusions for `lib/x86/`, `lib/x64/`, `lib/gtest/` and `include/vendor/gtest/`. Scoping to subdirectories rather than `lib/` keeps `lib/README` tracked.

**BPE-3 — DONE.** Skeleton present.

**BPE-4 — DONE, verified with the real GNU Make 3.81 binary.** `build-x86`/`build-x64` are thin recursive wrappers over one `_build` rule; the per-ARCH block is the only place the architectures differ; `clean` removes all of `build/`.

**BPE-5 — DONE, decisively settled.** Everything derives: `GIT_DESCRIBE`, `LAST_TAG` with zero-tag fallback, `VER_*` declared with `?=` so CI can override from the tag, `rc.exe /D` passing all five, `/VERSION:` on the link line. `version.rc` carries `#ifndef` fallbacks with a comment stating they are not a second home for version numbers.

The `VER_STRING` quoting was contested across two reviews — one reproduction produced RC2237 and looked like a real defect. It was settled by running the actual Make binary against the actual Makefile rather than an approximated shell invocation, then decoding the compiled `.res`'s UTF-16LE `FileVersion`/`ProductVersion` fields: both correct and unmangled. **The RC2237 was an artifact of the reproduction, not the build.** When a hand-reconstructed invocation and the real build disagree, the build wins.

**BPE-6 — DONE.** The `lib/README` template inside the script carries the provisioned/uncommitted framing, exact filenames (including an explicit note that `zs.lib` is not the name anyone would guess), the product-linked versus test-only distinction, the `manual-link` split rationale, per-environment `/MT` verification, and "the authoritative pin is `vcpkg.json`". It additionally distinguishes `json.hpp` as committed source rather than a provisioned binary — beyond specification and correct.

Editing the template rather than the generated file was the right call: `lib/README` is script-generated and a hand edit would have silently reverted.

**BPE-7 — DONE.** GoogleTest via vcpkg into `lib/gtest/<arch>/`. The separation is real in the link line, not just in folder naming: the Makefile's `LIBS` contains only `libcurl.lib zs.lib` plus the seven system libs, while `TEST_LIBS` adds `gtest.lib gtest_main.lib` with a separate `/LIBPATH:$(GTESTDIR)` used solely by the test recipe.

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

**BPE-17 — NEW. Make the lib copy synchronising, and clear the existing residue.**

`lib/x64/` currently contains **four** `.lib` files: `libcurl.lib`, `zs.lib`, and stale `gmock.lib` and `gtest.lib`. `lib/x86/` contains exactly the correct two.

This is **not a regression** — the allow-list fix works, demonstrably so, and the run's "Copied 2 .lib file(s)" message is accurate. `Copy-TripletLibs` is purely additive: it filters what it copies but never prunes its destination. The two stale files predate the fix and simply were never removed.

The asymmetry has a neat cause: **the very bug that broke x86 also protected it.** Because the shared-install-root bug destroyed x86's source directory, x86's copy step failed rather than running, so `lib/x86/` was never contaminated in the first place. x64 succeeded throughout and accumulated the residue.

Consequences today: **none for the build** — the Makefile names product libs explicitly, so there is no link-time contamination — and **none for the repository**, since `lib/x64/` is gitignored. What it does violate is the documented product-versus-test-only separation as it actually exists on disk, which is the kind of gap that misleads the next person to inspect that directory.

Two options: have `Copy-TripletLibs` remove non-allow-listed `.lib` files from the destination before copying (making the step synchronising rather than additive, and self-healing on any future change to the allow-list), or simply delete the two files by hand. The first is better — it makes the invariant hold for anyone who ever ran an older version of the script — but the second is sufficient for this machine. Non-blocking either way.

**TEST-1 — DONE.** `tests/core/sanity-test.cpp` plus a real linked `build/test/x64/restifycapl-tests.exe`.

This surfaced a real latent bug that could not manifest until a real `main()`-providing static library existed to link against: **`link.exe` infers the subsystem and entry point only from `main`/`WinMain` in `.obj` files passed directly on the command line, never transitively from a `.lib`.** With `main()` coming solely from `gtest_main.lib`, the link failed `LNK1561`. Root-caused with `/VERBOSE` and `dumpbin /symbols`, fixed with `/SUBSYSTEM:CONSOLE` on the test recipe only, and independently re-verified by a separate reviewer running its own `dumpbin` and `make test`.

**What `make test` currently proves.** All of `src/core`, `src/http`, `src/registry`, `src/mapping` contain only `.gitkeep`, so the suite compiles exactly one file. Green proves **the harness** — flags, include paths, GoogleTest linkage, subsystem, runner — and no project logic.

**x86 evidence gap.** Only `build/test/x64/` artifacts exist. The Makefile fully parameterizes the test tree, so `make test ARCH=x86` is supported by construction, but an x86 run is not evidenced. Stage 6's CI should run tests on both architectures so this stops depending on anyone remembering.

**REV-2 — CLEAN, CONFIRMED.** The full review passed; its one Must-fix (still-tracked `.lib` files) was closed; the follow-up findings (transcript inversion in `docs/development-environment.md`, missing tool pin) were closed and independently re-verified, including an independent AST parse at 0 errors and upstream confirmation of the pinned tag. The `/MT` provenance check is a **per-environment** check: it validates local provisioning, and CI must run the equivalent against its own copies.

**HUM-12 — Commit and push.** The Stage 4 work is complete and uncommitted.

---

## 6a. Documentation practice — standing, not a stage

**Why not a numbered stage.** Documentation here is a standing obligation attached to other work — a CHANGELOG entry whenever the export table gains an entry, README updates when the build story changes, release mechanics at Stage 13. Encoding it as a stage would imply it finishes.

**What exists:** `.claude/skills/project-docs/SKILL.md` (three-documents-three-audiences model, wired into `build-pipeline-engineer` and `cpp-implementer`); `CHANGELOG.md`; `README.md`; `docs/development-environment.md`.

**README impact of §2.** The Development setup section must state plainly that provisioning is **required**, not a convenience: a fresh clone cannot build or test until `scripts/setup-dev-env.ps1` has run, because no compiled dependency is committed.

**Drift watch — and a demonstrated instance.** Rationale living in `docs/development-environment.md` rather than inline keeps the script readable but creates two artifacts that must move together. This risk materialised during Stage 4: the worked bug-reproduction transcript in that document had the wrong triplet's packages being removed in the wrong order, and was corrected to match reality. A transcript that plausibly resembles the truth is worse than no transcript, because it will be trusted. Any script change invalidating a documented rationale must update the document in the same change.

The Makefile deliberately takes the opposite approach for its two hardest-won findings — the `/SUBSYSTEM:CONSOLE` requirement and the `make -n` quirk are long inline comments, because both are traps a future reader hits *while editing that exact recipe*. Rationale belongs where it will be read at the moment it is needed; that is a judgement per case, not a uniform rule.

---

## 7. Phase 2 — ABI Proof & Continuous Verification

### Stage 5 — "Hello DLL": one operation, verified in CANoe — HARD GATE

**Blocked on Stage 3 only.**

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
- The provisioning step places libs where the Makefile expects them (`lib/<arch>/`, `lib/gtest/<arch>/`).
- CI runs its own `dumpbin /directives` `/MT` check — the CI-side equivalent of REV-2.

**Three carry-over traps for whoever writes this, all learned the hard way locally:**

1. **Shallow clone.** The failure is a property of *how vcpkg gets bootstrapped*, not of the manifest. A naive shallow clone of the registry on a runner hits the identical failure the local machine hit. Use a full clone or replicate the unshallow self-heal.
2. **Per-triplet install roots.** Both triplets sharing one `vcpkg_installed/` causes the second install to *remove* the first's packages. CI must pass distinct `--x-install-root` values exactly as the local script does.
3. **Tool pin.** CI should honour the same `$VcpkgPinnedTag`, for the same reason the local path does — `--x-install-root` is experimental, and an unpinned runner tool is an unpinned dependency.

All three sit **beneath** the manifest, in how vcpkg itself is obtained and invoked, where the shared content pin gives no protection at all.

**Caching — purely a performance concern, never a correctness one.** Because no committed binary is involved, caching can be added, tuned or removed without affecting correctness. Recommended: `actions/cache` over the vcpkg binary cache or the install roots, keyed on a hash of `vcpkg.json` **plus the triplet plus the runner image version**. Including the image version is the important detail: when GitHub bumps the runner's toolset, the key changes, the cache misses, and dependencies rebuild against the new compiler automatically — correct by construction rather than by anyone remembering to invalidate it.

GitHub evicts cache entries after 7 days without access and this project pushes in bursts, so cold builds will be common — a cost measured in minutes, not a correctness risk.

**REV-4 — NOT STARTED.** Only a readiness pre-check exists, run before `.github/workflows/` existed. It found the design sound — the manifest is self-sufficient as a CI-readable pin, and nothing in the script's local-machine-only structure blocks CI from having its own simpler provisioning step — and produced trap 1 above. Traps 2 and 3 were discovered subsequently during HUM-19.

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

**BPE-11 — Write `.github/workflows/release.yml`.** Triggered by a `vX.Y.Z` tag push. Extracts `X.Y.Z` from `github.ref_name`, overriding `VER_MAJOR`/`VER_MINOR`/`VER_BUILD`/`VER_REV` — the Makefile declares these with `?=` specifically so CI can override without edits. Uses the same independent provisioning step as Stage 6, including all three carry-over traps. Builds both architectures, runs tests, then **halts at a manual approval gate** (a GitHub Environment with required reviewers), publishing only after approval.

**BPE-12 — Generate the exposed-operation list from the export table at build time.**
**BPE-14 — Convert `CHANGELOG.md`'s `[Unreleased]` into a released section.**
**REV-9 — Review** the release workflow: no hardcoded version anywhere, approval gate genuinely blocks.
**HUM-17 — Create the release tag.** **HUM-18 — Verify the CI-built artifact in CANoe, then approve the publish.**
**Human approval gate: YES.**

---

## 10. Phase 5 — Hardening

### Stage 14 — Cleanup and consistency pass
**BPE-13** — One `Makefile`, no historical variants; `clean` removes every intermediate; no build artifacts tracked; version and operation list each maintained in exactly one place. Confirm no compiled dependency has crept back into tracking, and that `lib/<arch>/` contains only product libs.
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

### `build-pipeline-engineer` — 17 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| BPE-1 | 2 | `scripts/setup-dev-env.ps1` — local provisioning | **DONE — 19/0/0 both arches** |
| BPE-2 | 4 | `.gitignore` — excludes provisioned binaries, keeps `lib/README` | **DONE** |
| BPE-3 | 4 | Directory skeleton | **DONE** |
| BPE-4 | 4 | `Makefile`, one parameterized rule | **DONE — run with real GNU Make** |
| BPE-5 | 4 | Git-tag versioning wiring | **DONE — `.res` fields decoded and verified** |
| BPE-6 | 4 | `lib/README` template — provisioned framing + exact filenames | **DONE** |
| BPE-7 | 4 | GoogleTest via vcpkg into `lib/gtest/<arch>/` | **DONE** |
| BPE-15 | 4 | `vcpkg.json` manifest, manifest-mode migration, per-triplet install roots, lib allow-list, tool pin | **DONE — EXECUTION-VERIFIED** |
| BPE-16 | 4 | One-line note that `vcpkg.json`'s `version-string` is not a version source | **Non-blocking** |
| BPE-17 | 4 | Make `Copy-TripletLibs` synchronising; clear stale `lib/x64/` residue | **NEW, non-blocking** |
| BPE-8 | 5 | Link/resource wiring for both DLLs; `dumpbin /exports` | |
| BPE-9 | 6 | CI: independent provisioning (full clone, per-triplet roots, tool pin) + build + `make test` both arches + artifacts + caching | |
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
| REV-2 | 4 | Makefile, versioning, `/MT` provenance, tracking hygiene, provisioning correctness | **CLEAN — CONFIRMED** |
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

### Human — 19 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| HUM-1 … HUM-9 | 1 | Configuration reconciliation (nine items) | **ALL DONE** |
| HUM-19 | 2 | Re-run `setup-dev-env.ps1` for real to verify BPE-15 | **DONE — 19/0/0 both arches** |
| HUM-10 | 3 | Install Vector CANoe/CANalyzer | **UNCONFIRMED — sole blocker on Stage 5** |
| HUM-11 | 3 | Build + load the official Vector sample unchanged in CANoe | **UNCONFIRMED — sole blocker on Stage 5** |
| HUM-12 | 4 | Commit and push | |
| HUM-13 | 5 | Load and call the Hello DLL operation from a real `.can` script | |
| HUM-14 | 9 | Verify sync operations in CANoe | |
| HUM-15 | 10 | Verify async operations in CANoe | |
| HUM-16 | 11 | Verify CAPL associative-field syntax against the official CANoe help | |
| HUM-17 | 13 | Create the release tag | |
| HUM-18 | 13 | Verify the CI artifact in CANoe, then approve the publish | |

---

## 13. Risks

**Static review does not substitute for execution — now seven instances, the project's most reliable predictor of defects.** (1) Polish-localized `cl.exe` banner defeating an English-only architecture check; (2) a `curl[schannel]` vcpkg feature name that no longer exists; (3) `%VSCMD_ARG_TGT_ARCH%` returning its own literal text; (4) `LNK1561` from `link.exe` refusing to infer an entry point from a `.lib`; (5) the `make -n` `CreateProcess` artifact; (6) the shared-install-root triplet collision; (7) the unfiltered lib copy. Items 6 and 7 both passed a clean static review of the migrated script and were found only by running it.

Item 6 carries an extra lesson: **the failing step and the causing step were different steps.** Three x86 steps reported "Expected lib directory not found" while the `vcpkg install (x86-windows-static)` step immediately above them said `[OK]`. Both reports were accurate. Diagnosing it required running vcpkg directly and reading its own output about removing packages — the script's own log could not have revealed it.

The RC2237 scare showed the inverse failure: a hand-reconstructed invocation produced a defect the real build did not have.

**Verifying the operation is not verifying the resulting state.** BPE-17's residue is the clean example: the copy step correctly reports "Copied 2 .lib file(s)", every review confirmed the code and the run output, and `lib/x64/` still contains four. A step that is additive rather than synchronising can be perfectly correct about what it did and still leave a directory that violates the invariant. Check destination state, not just the operation's own report.

**Pins beneath the pin.** `vcpkg.json` pins registry content; it does not pin how vcpkg is obtained or invoked. All three of Stage 6's carry-over traps — shallow clone, shared install root, unpinned tool — live in that layer, where the shared content pin offers no protection. The tool pin (§5) closes the third; the other two must be handled explicitly in BPE-9.

**Experimental flags as load-bearing structure.** Triplet isolation depends on `--x-install-root`, which vcpkg's own `--help` marks `(experimental)`. The tool pin makes this safe *for now* by freezing the tool alongside the content. If that flag is ever stabilised or renamed, the pin buys time to migrate deliberately rather than discovering it through a broken run. Recorded so a future reader does not have to re-derive why the tool is pinned at all.

**Sharing compiled artifacts between environments — the anti-pattern this architecture removes.** v8 and v9 committed `.lib` files, making CI link binaries produced on a developer's machine, and proposed monitoring for toolchain drift with a pre-planned escape hatch. That was risk management substituting for an available structural fix. When a failure class can be *designed out* at comparable cost, designing it out beats detecting it — and a proposal shipping with a pre-planned fallback to a *different architecture* is a signal the fallback may be the right primary.

**Provisioning drift between local and CI.** Two mechanisms read one manifest and can still diverge in placement, triplet selection, or bootstrap. REV-4 checks placement and triplet; BPE-9 must handle bootstrap explicitly.

**Export contract.** Stage 5 is irreversible in practice: the naming convention and version-entry layout chosen there bind every later append. Stages 9–12 and 15–16 each append.

**ABI failure modes that hide *all* operations, not just the new one.** Raw text pointer instead of the caller-supplied buffer; incorrect 1-byte alignment coverage through the terminating pointer.

**`/MT` contamination.** A per-environment check rather than a property of a committed artifact. If a dependency is only available as `/MD`, **stop and ask**.

**Bitness parity — structurally enforced for the build, not yet continuously verified for tests.** Only x64 test artifacts are evidenced. Stage 6's CI should run tests on both architectures.

**Documentation that plausibly resembles the truth.** The inverted transcript in `docs/development-environment.md` was corrected during Stage 4. A worked example that looks right and is wrong is worse than none, because it will be trusted rather than checked.

**Apparent second version sources.** `vcpkg.json`'s `version-string` feeds nothing but reads as a counterexample to "no version number is ever hand-edited". BPE-16 preempts it. The shape will recur; label them where they appear.

**Generated files silently reverting hand edits.** `lib/README` is produced by `setup-dev-env.ps1`; editing the artifact instead of the template appears to work and vanishes on the next run.

**Configuration drift — closed, with a scoped standing check.** REV-1's corrected scope excludes historical drafts and its own definition.

**Agent routing metadata is easy to miss.** Body and frontmatter `description` are separate surfaces.

**Vector SDK headers remain committed and remain an accepted risk.** Public repository, explicit user decision. Unaffected by §2: source, not compiled output, and no package-manager path exists.

**Versioning.** `FILEVERSION`/`PRODUCTVERSION` are four 16-bit fields that **wrap silently** above 65535; `/VERSION:` accepts major.minor only. Verified end to end on a zero-tag repository; the untested path is the **release** path where CI overrides `VER_*` from a real tag, first exercised at Stage 13.

**Steps no agent can verify.** The remaining HUM tasks, of which HUM-10 and HUM-11 are now the only near-term ones.

**Scope creep toward struct mapping.** Stages 11 and 12 must ship before Stage 15 is reconsidered.

---

## 14. Operational loose ends

1. **The Stage 4 work is uncommitted.** Because nothing is committed yet, the final review could not prove "only these two changes" via a clean diff boundary and verified by content-reading instead. Committing restores that boundary for future reviews.
2. **`lib/x64/` residue** — see BPE-17. Gitignored, so it will not enter the commit.
3. **The README execution-policy/switches expansion** has still not been through `code-reviewer`. Prose, not executable logic; let it ride along with the next review.

---

## 15. Execution order

**1 (done) → 2 (done, verified) → 4 (done) → 3 (UNCONFIRMED — sole blocker) → 5 (hard gate) → 6 → 7 → 8 → 9 → 10 → 11 → 12 → 13 → 14**, with Stages 15–16 only on demonstrated need.

**Stage 3 is now the only thing between the project and the Stage 5 hard gate.** HUM-10 and HUM-11 are human-only and unblocked. BPE-16 and BPE-17 are non-blocking cleanups that can happen at any time.

**Status:** v12. Stages 1, 2 and 4 complete, all execution-verified. Stage 3 unconfirmed and now the sole remaining blocker.
