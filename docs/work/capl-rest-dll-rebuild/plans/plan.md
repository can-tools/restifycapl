# Plan (v8): Build the CAPL REST DLL (restifycapl) from zero

Revision of v7. Changes: Stage 2 complete and empirically proven; documentation infrastructure recorded; Stage 3/4 now active.

---

## 1. Goal

Build the CAPL REST DLL (`restifycapl`) from the cloned repository to a working, dual-architecture (x86 + x64) native CANoe plugin: synchronous and asynchronous REST/HTTP for CAPL scripts plus JSON flattening and typed path accessors. Struct mapping and CAPL-side request building stay deferred until a demonstrated need. Environment setup is scripted wherever possible. The CANoe ABI is proven on one trivial operation before any REST logic exists. All business logic is unit-tested outside CANoe. The export table stays a thin, append-only glue layer. Every version number derives from the Git tag.

---

## 2. Current state

**Stage 1 — complete and verified.** Nine HUM items applied; all operational config files (`CLAUDE.md`, `.claude/**`) clean under REV-1's corrected scope.

**Stage 2 — complete and empirically proven.** `scripts/setup-dev-env.ps1` written, debugged across three real runs, and independently reviewed at each round. Final run: **14 OK / 0 WARN / 0 FAIL**. Evidence on disk, independently confirmed:

- `lib/x86/libcurl.lib`, `lib/x86/zs.lib`, `lib/x64/libcurl.lib`, `lib/x64/zs.lib`
- `include/vendor/json.hpp`
- `lib/README` with libcurl `8.22.0#1` recorded and a script-written timestamp (`Last refreshed: 2026-09-17 21:41:47`)
- The pinned SHA-256 at script line 94 is **64 hex characters** and matches the official nlohmann/json v3.11.3 release asset — the original 63-character truncation is fixed.
- The `VSCMD_ARG_TGT_ARCH` delayed-expansion fix is present (lines 294–309): `cmd.exe /v:on` with `!VAR!`, plus a guard that reports `(unset)` when expansion fails while still failing correctly on a genuine architecture mismatch.
- The stale `curl[schannel]` feature is gone; `lib/README` records the current reality (default features pull SChannel automatically).

**Documentation infrastructure — established** (see §5a). `project-docs` skill, `CHANGELOG.md`, `README.md`, `docs/development-environment.md`.

**Not yet started:** no `src/`, `tests/`, `examples/`, `build/`, `Makefile`, or `.github/`. **Stages 3 and 4 are the next work, and may proceed in parallel.**

---

## 3. Numbering scheme

Six phases, sixteen sequentially numbered stages, no letter suffixes, no gaps:

| Phase | Stages | Theme |
|---|---|---|
| **Phase 1 — Foundation & Environment** | 1–4 | Config correctness, scripted bootstrap, manual CANoe setup, repo skeleton |
| **Phase 2 — ABI Proof & Continuous Verification** | 5–6 | Hello DLL in CANoe, then CI running on every push |
| **Phase 3 — Business Logic & CAPL Surface** | 7–12 | Core logic, HTTP, async, flattening, accessors |
| **Phase 4 — Release Pipeline** | 13 | Tag-driven versioning, approval gate, publish |
| **Phase 5 — Hardening** | 14 | Cleanup and consistency |
| **Phase 6 — Conditional Extensions** | 15–16 | Only on demonstrated need |

Mapping from v4: `P1→2`, `P2→3`, `0A→1`, `0B→4`, `1→5`, `2→7`, `3→8`, `4→9`, `5→10`, `6→11`, `7→12`, `8→6 and 13` (split), `11→14`, `9→15`, `10→16`.

**CI is split and moved earlier.** A **CI baseline** (build both architectures, run tests, on every push) lands at Stage 6, right after the ABI is proven, so every subsequent stage is continuously verified on a clean machine. The **release pipeline** stays at Stage 13.

Documentation work is deliberately **not** a numbered stage — see §5a for why.

---

## 4. Standing constraints

Repeated here so executing agents need no access to `docs/.locals/`:

- **`/MT` static CRT** for the DLL and every static dependency. Never mix `/MT` and `/MD` in one link. Verify with `dumpbin /directives` — expect `/DEFAULTLIB:LIBCMT`, never `MSVCRT`.
- **Bitness parity.** x86 and x64 must both build and behave identically — same sources, same flags (`/std:c++17`, `/EHsc`, `/MT`), same export table. Only `/MACHINE:` and library path differ.
- **Export contract.** The `CAPL_DLL_INFO_LIST`/`CAPL_DLL_INFO4` table in `src/module/exports.cpp` is the real API. Append-only; never rename, reorder, or remove. Reserved first entry (`CDLL_VERSION_NAME`/`CDLL_VERSION`) always present. Exported functions `extern "C"`.
- **`exports.def` contains `EXPORTS` and nothing else** — no `LIBRARY` line. It is shared by both architecture builds, which emit differently-named DLLs; a `LIBRARY` line can only ever be right for one of them.
- **Never return a raw text pointer** from a CAPL-exposed operation — always write into a caller-supplied buffer with its size. One violation of this hid *every* operation in CANoe last time, not just the new one.
- **1-byte packing must cover the entire export table** through and including the terminating pointer; pragma ordering is critical.
- **Dependency direction.** `src/core/` imports nothing from `src/http/`, `src/registry/`, `src/mapping/`. Only `src/module/` includes the CAPL SDK headers.
- **Tests run outside CANoe.** Logic must be reachable without the export glue.
- **No version number is ever typed by hand.** Git tag `vX.Y.Z` is the single source of truth.
- **Every export-table append gets a `CHANGELOG.md` `[Unreleased]` entry in the same change** — see §5a.
- **Agents cannot `git push` or `git tag`** — both denied by design. Pushes and release tags are human actions.

---

## 5. Phase 1 — Foundation & Environment

### Stage 1 — Configuration reconciliation — COMPLETE

All nine items (HUM-1 … HUM-9) applied and independently verified on disk: SDK headers relocated to `include/vendor/capl-dll-sdk/`; `build-pipeline-engineer.md`, `test-engineer.md`, `cpp-implementer.md`, `code-reviewer.md` and `cpp-testing-conventions/SKILL.md` corrected in both body and frontmatter; 04-FLOW §6.1/§6.2/§6.3 updated; supersession banner on the design log.

**REV-1 — SATISFIED.** Scope, as corrected in v6 and reconfirmed after HUM-6…HUM-9: the stale-identifier sweep covers **`CLAUDE.md` and `.claude/**` only**. Zero hits.

Three findings stand as permanent rules, recorded so they are not re-litigated:

1. **Only `CLAUDE.md` and `.claude/**` are operationally loaded** — skills and agent definitions auto-load into agent context, `CLAUDE.md` auto-loads into every session. Those are what the sweep must cover.
2. **`docs/.locals/**` is exempt as historical record.** Git-ignored, never shipped, planner-facing scratch. The v1–v3 drafts carry the user's inline answers, which are only intelligible alongside the question they answered — deleting `build32` from a file where the user wrote "so the target names should all be changed" destroys the evidence of *why* the current naming exists. The one file needing disambiguation (`capl-rest-dll-design-log.md`, not version-named and cited here as authoritative) carries HUM-9's banner.
3. **The sweep is self-matching and excludes this plan file**, which necessarily names the identifiers it searches for.

**Tooling note.** The `Grep` tool honours `.gitignore`, so it does not see `docs/.locals/` and naturally produces the correct scope. Bash `grep -r` / `rg --no-ignore` do not, and will re-raise these false positives.

**Non-blocking, deliberately not an item:** `CLAUDE.md`'s opening line names "struct mapping", which is deferred to conditional Stage 15. Unlike agent frontmatter this is not routing metadata — nothing selects on it — and `## Scope` states the deferral fifteen lines below. Fold into any future `CLAUDE.md` edit made for another reason.

### Stage 2 — Scripted development-environment bootstrap — COMPLETE, PROVEN BY REAL RUN

**BPE-1 — DONE.** `scripts/setup-dev-env.ps1` provisions the toolchain, dependencies and headers. All nine specified behaviours implemented: `vswhere` probe before install; silent VS Build Tools install with `3010` treated as success-pending-reboot; `vcvarsall.bat` resolution and per-architecture Native Tools verification; `make` detection; vcpkg bootstrap and both static triplets; per-architecture `.lib` copy into `lib/x86/` and `lib/x64/`; SHA-256-verified `json.hpp`; `lib/README` with recorded versions; SDK-header verification that checks but never fetches.

**This is the first item in the plan verified by execution rather than review.** It went through three real runs, not just static review, and the distinction mattered — two of the four bugs found were invisible to static analysis:

- A **Polish-localized `cl.exe` banner** defeated an English-only architecture check. No amount of code reading finds this; it requires running on a non-English Windows.
- A **stale `curl[schannel]` vcpkg feature name** that no longer exists in the current port (SChannel is now automatic via the default `ssl` feature). Only a real `vcpkg install` surfaces it.
- A **truncated SHA-256** (63 of 64 hex chars) and a broken exit-code gate in `Test-NativeToolsArch` — both caught by static review before the first run.
- `%VSCMD_ARG_TGT_ARCH%` returning the literal unexpanded string, a `cmd.exe` parse-time-versus-runtime expansion issue in a `&&`-chained command line. Fixed with `cmd /v:on` + `!VAR!` delayed expansion, and verified to still fail correctly on a genuinely wrong architecture — a fix that suppressed the symptom by disabling the check would have been worse than the bug.

Every round had its own `code-reviewer` confirmation pass. No outstanding Must-fix or Should-fix items on the script.

**Scope note — BPE-1 step 8 versus BPE-3.** The script creates the directories it needs (`lib/x86`, `lib/x64`, `include/vendor`). It does **not** create `src/`, `tests/`, `examples/` or `build/` — those remain **BPE-3**'s job in Stage 4. The v5 wording of step 8 ("create the directory skeleton") was ambiguous between the two; this is the settled reading.

**Open follow-up for Stage 8 — record the exact `.lib` filenames.** `lib/README` currently says zlib is "bundled transitively […] see installed/<triplet>/lib for the exact file". The artifact on disk is named **`zs.lib`**, which is not a name anyone would guess. **BPE-10** (Stage 8) has to write the linker line naming exact libraries, so the names should be recorded in `lib/README` now, while they are known and verified, rather than rediscovered later. Low effort, prevents a needless stall.

### Stage 3 — Manual toolchain setup (human only) — NEXT

No automation path exists for either item — a licensing constraint, not a technical one.

**HUM-10 — Install Vector CANoe/CANalyzer.** Licensed, GUI-installed, machine-bound.

**HUM-11 — Build and load the official "Example of a Windows DLL for CAPL" sample, unchanged, in CANoe.** The step people skip and shouldn't: it proves the toolchain + CANoe pairing works before any project code can be blamed for a failure. Build Tools ships MSBuild, so the sample compiles without the IDE; loading it into CANoe is manual.

Stage 3 runs in parallel with Stage 4. It hard-blocks Stage 5.

### Stage 4 — Repo skeleton, Makefile, versioning — NEXT

**BPE-2 — Finalize `.gitignore`.** Delete the dead `build-*/` (a leftover from the abandoned per-architecture-suffix naming; `build/` already covers output). Annotate the two commented lines (`#*.lib`, `# Makefile`) with *why* they are disabled — without a note, a future tidy-up of the "CMake generated files" and "Compiled Static libraries" blocks will silently untrack the Makefile and every vendored `.lib`.
**Precondition:** an unexplained modification to `.gitignore` is currently staged from another source. Reconcile it against this task before editing, so BPE-2 does not duplicate or revert it blindly.

**BPE-3 — Create the directory skeleton**: `src/{core,http,registry,mapping,module}/`, `build/{x86,x64}/`, `tests/{core,http,mapping}/`, `examples/`. (`lib/{x86,x64}/` and `include/vendor/` already exist, created by BPE-1.)

**BPE-4 — Write the `Makefile`.** Targets `all` (default, both architectures), `build-x86`, `build-x64`, `test`, `clean`. The two architecture targets are thin wrappers over **one parameterized rule** with architecture as a Make variable — this makes flag drift between x86 and x64 structurally impossible rather than merely forbidden. `clean` removes **all** intermediates, not a selected few.

**BPE-5 — Wire the versioning mechanism in from day one.** `src/module/version.rc` parameterized via `rc.exe /D VER_MAJOR/VER_MINOR/VER_BUILD/VER_REV` with `#ifndef` fallbacks; `/VERSION:X.Y` on the linker; `git describe --tags --always --dirty` into `StringFileInfo`; `git rev-list --count <tag>..HEAD` for local build/revision. Feed only small integers to `FILEVERSION`/`PRODUCTVERSION` — four 16-bit fields capped at 65535 that **wrap silently**. Test deliberately in a repo that still has **no tags at all** — `--tags --always` carries both flags precisely for this case.

**BPE-6 — Record exact dependency versions and `.lib` filenames in `lib/README`** (see the Stage 2 follow-up above; the version block already exists, the filenames do not).

**BPE-7 — Integrate GoogleTest built `/MT`** and make `make test` functional.

**TEST-1 — Create the `tests/` skeleton with one trivial passing test** so `make test` is green from the very first commit. 04-FLOW §3.3 is explicit that `tests/` existing from day one — not as a "nice to have" — is what forces the reflex of writing a test before moving on.

**REV-2 — Review** Makefile parameterization, versioning derivation, `.gitignore` semantics, and confirm `dumpbin /directives` on `libcurl.lib` and `zs.lib` in both architectures shows `/DEFAULTLIB:LIBCMT`. This is the first opportunity to actually verify `/MT` provenance on the real artifacts — they now exist.

**HUM-12 — Commit and push** (agents are denied `git push`).

**Human approval gate: YES** — a `/MD` library slipping through here poisons everything downstream.

---

## 5a. Documentation practice — established, standing, not a stage

Established during Stage 2 at the user's request, outside the original plan. Recorded here because the plan should not stay silent about work that happened.

**Why this is not a numbered stage.** Documentation here is not a one-time deliverable — it is a standing obligation attached to other work: a CHANGELOG entry every time the export table gains an entry, README updates when the build or install story changes, and release mechanics at Stage 13. Encoding it as a stage would imply it finishes. It is encoded as a skill instead, wired into the agents that do the work.

**What exists:**

- **`.claude/skills/project-docs/SKILL.md`** — conventions for `README.md`, `examples/*.can` and `CHANGELOG.md` under a three-documents-three-audiences model, explicitly designed to prevent the duplication drift this project has already suffered once (see §12). Wired into `build-pipeline-engineer` and `cpp-implementer`'s `skills:` lists with matching responsibility and workflow bullets.
- **`CHANGELOG.md`** — Keep a Changelog format, one `[Unreleased]` entry for the bootstrap script.
- **`README.md`** — project description plus a Development setup section: what the script does, the elevation requirement, execution-policy bypass two ways, an idempotency note, and a table of all six script switches.
- **`docs/development-environment.md`** — engineering rationale that had been accumulating as long inline comments in the script: json.hpp hash provenance, the `curl[schannel]` story, the `VSCMD_ARG_TGT_ARCH` delayed-expansion saga, the `where`-chain bare-`&` rationale, and make-installer ordering. The script was then trimmed to short comments pointing at this document's anchors.

Moving rationale out of the script and into a linked document is the right split: the script stays readable as code, and the reasoning stays discoverable without being re-derived. The risk is the two drifting apart — the anchors were hand-checked against the current script at review time, and any future script change that invalidates a documented rationale must update the document in the same change.

**Standing obligation, cross-referenced from the stages that trigger it:** Stages 9, 10, 11 and 12 each append to the export table, and each therefore requires a `CHANGELOG.md` `[Unreleased]` entry in the same change. Stage 13 converts `[Unreleased]` into a released section as part of cutting the tag.

---

## 6. Phase 2 — ABI Proof & Continuous Verification

### Stage 5 — "Hello DLL": one operation, verified in CANoe — HARD GATE

**CPP-1 — Write `src/module/exports.cpp` and `exports.def`.** Exactly **one** trivial operation (e.g. return a fixed version string into a caller-supplied buffer). `exports.def` contains `EXPORTS` and nothing else. Apply both post-mortem rules from §4: output-buffer pattern, and 1-byte packing across the whole table including the terminating pointer. **This stage permanently fixes the CAPL-visible naming convention** for every operation the project will ever expose — the never-rename rule means it cannot be revised later without a major-version break. Decide it deliberately and write it down.

**BPE-8 — Link and resource wiring** for both architectures, producing `build/x86/restifycapl-x86.dll` and `build/x64/restifycapl-x64.dll`; verify the export list with `dumpbin /exports`.

**REV-3 — Export-contract review. The single most important review in this plan.** Naming convention, table layout, reserved version entry, `extern "C"`, packing, absence of a `LIBRARY` line.

**HUM-13 — Load and call the operation from a real `.can` script in CANoe.** No agent can do this.

**Human approval gate: YES — the most important gate in this plan.** Do not start Stage 6 until the operation is confirmed callable from CAPL. Stage 2 has just demonstrated the general principle concretely: two of its four bugs were invisible to static review and only a real run found them. The same asymmetry applies here, with far higher stakes.

### Stage 6 — CI baseline: build both architectures and run tests on every push

**BPE-9 — Write `.github/workflows/ci.yml`.** `windows-latest`, matrix over x86/x64, MSVC environment activated (`vcvarsall.bat` or an equivalent action) for the matching architecture before invoking Make. **Calls the same `build-x86`/`build-x64` Make targets used locally — never duplicates `cl.exe`/`rc.exe` invocations in YAML.** Runs `make test`. Uploads both DLLs as workflow artifacts on every push so a reviewable binary always exists. Because the SDK headers are committed, CI compiles `src/module/` and links the DLLs itself.

**REV-4 — Review** that CI reuses Make targets rather than reimplementing the build, and that the matrix covers both architectures symmetrically.

**Human approval gate: YES** — this defines what gets verified from here on.

---

## 7. Phase 3 — Business Logic & CAPL Surface

Stages 9–12 each append to the export table. Every append is a contract change requiring `code-reviewer`, a human gate, and a `CHANGELOG.md` `[Unreleased]` entry in the same change (§5a).

### Stage 7 — Core pure logic (level 0)

**CPP-2** — `src/core/type-conversion.*` (JSON ↔ C++ value conversions).
**CPP-3** — `src/core/json-path.*` (path resolution inside a JSON tree).
Both depend on `json.hpp` only — zero I/O, zero CANoe knowledge, not yet exported.
**TEST-2 / TEST-3** — `tests/core/` coverage for each: valid input, malformed/missing JSON fields, type mismatches.
**Human approval: no.**

### Stage 8 — HTTP layer and synchronous operations (logic only)

**CPP-4** — `src/http/http-client.*` wrapping libcurl.
**CPP-5** — `src/http/sync-operations.*` (blocking request/response).
**BPE-10** — Link `libcurl.lib` and `zs.lib` from the matching `lib/<arch>/`, plus all Windows system libs (crypt32, bcrypt, secur32, ws2_32, normaliz, wldap32, advapi32 — link all, they are transitively required).
**TEST-4** — Build the libcurl fake/mock boundary. No real network calls in the suite.
**TEST-5** — Coverage including **simulated timeouts and error responses**, not just the happy path.
Verify as a standalone console program against httpbin.org, per 04-FLOW Step 2.
**Human approval: no.**

### Stage 9 — Expose synchronous REST to CAPL (first contract append)

**CPP-6** — Append sync operations to the export table; rebuild both architectures; add the `CHANGELOG.md` entry.
**REV-5** — Export-contract review (mandatory).
**HUM-14** — Verify in CANoe.
**Human approval gate: YES.**

### Stage 10 — Asynchronous layer with response state designed correctly up front

**CPP-7** — `src/http/async-operations.*`: background dispatch, readiness check, wait-for-result. Response-state semantics settled **now, not retrofitted** — the ready flag is cleared once read, and a request ID ties a response to the call that produced it. 04-FLOW §5 item 3 records that the previous iteration identified this early but never confirmed it was implemented. Shared state is global within the DLL with per-module synchronization; **one active response at a time** by deliberate design, not a concurrent request pool.
**TEST-6** — `tests/http/` coverage specifically for ready-flag-cleared-after-read and request-ID correlation across consecutive requests.
**CPP-8** — Append async operations to the export table; add the `CHANGELOG.md` entry.
**REV-6** — Export-contract review.
**HUM-15** — Verify in CANoe.
**Human approval gate: YES** — contract append, and the one-active-response simplification bounds what the DLL can ever do.

### Stage 11 — JSON flattening (highest user value — ship before struct mapping)

**CPP-9** — `src/mapping/json-flatten.*`: flatten the response into a dot-notation key/value map, plus key count, key-by-index, value-by-key.
**TEST-7** — `tests/mapping/` coverage including deeply nested objects, arrays, and empty/malformed documents.
**HUM-16 — Mandatory before any `.can` example is written:** verify associative-field syntax against the official CANoe help (`Help → CAPL → General → Associative Fields`). The correct form has **no extra keyword before the type** — `char[30] name[char[]];`. An invented keyword was copied across many docs and example files last time. Check the product help; do not trust generated snippets.
**CPP-10** — Append flattening operations to the export table; add the `CHANGELOG.md` entry.
**CPP-11** — Write `examples/*.can` per the `project-docs` skill, only after HUM-16 confirms the syntax.
**REV-7** — Export-contract review.
**Human approval gate: YES** — contract append; confirm HUM-16 actually happened.

### Stage 12 — Typed JSON accessors

**CPP-12** — `src/mapping/json-accessors.*`: typed point reads at a JSON path (integer/float/bool/string) without flattening, array helpers (length, element-by-index), optional cache for recent queries.
**TEST-8** — Coverage including type mismatches at each accessor type and cache invalidation between responses.
**CPP-13** — Append accessor operations to the export table; add the `CHANGELOG.md` entry.
**REV-8** — Export-contract review.
**Human approval gate: YES** — contract append.

---

## 8. Phase 4 — Release Pipeline

### Stage 13 — Tag-driven release with an approval gate

**BPE-11 — Write `.github/workflows/release.yml`.** Triggered by a `vX.Y.Z` tag push. Extracts `X.Y.Z` from `github.ref_name` (stripping `v`), feeds `X.Y` to `/VERSION:` and `major,minor,build,revision` to `FILEVERSION`/`PRODUCTVERSION` via `rc.exe /D`. Builds both architectures and runs tests, then **halts at a manual approval gate** (a GitHub Environment with required reviewers is the natural mechanism), and only publishes to GitHub Releases after approval.

**BPE-12 — Generate the exposed-operation list from the export table at build time** and publish it with the release, so documentation cannot drift from code. The previous iteration had different operation counts stated in different README files depending on when each was last edited.

**BPE-14 — Convert `CHANGELOG.md`'s `[Unreleased]` section into a released section** as part of cutting the tag, per the `project-docs` skill.

**REV-9 — Review** the release workflow, confirming no hardcoded version anywhere and that the approval gate genuinely blocks publication.

**HUM-17 — Create the release tag** (agents are denied `git tag`).
**HUM-18 — Verify the CI-built artifact manually in CANoe, then approve the publish.** This ordering is what makes manual CANoe verification a real precondition of release rather than an aspiration.

**Human approval gate: YES** — this determines what ships.

---

## 9. Phase 5 — Hardening

### Stage 14 — Cleanup and consistency pass

**BPE-13** — One `Makefile`, no historical variants; `clean` removes every intermediate; no build artifacts tracked; version and operation list each maintained in exactly one place.
**TEST-9** — Coverage audit: every module in `src/` that can be tested has a corresponding test file in the mirrored `tests/` location.
**REV-10** — Final review, including a `project-docs` consistency check: README, CHANGELOG and `examples/` agree with the code and with each other. Anything removed as dead code must be removed **in full** (export-table entry + implementation + documentation) in a single commit — last time part of the JSON-building operations were removed while a simpler variant stayed exposed, contradicting docs that claimed the whole module was gone.
**Human approval: no**, unless it touches the export table.

---

## 10. Phase 6 — Conditional Extensions

Build **only** on demonstrated need. 04-FLOW is emphatic that the previous iteration built these first and largest while the highest-value feature (flattening) waited.

### Stage 15 (CONDITIONAL) — Struct registry + JSON→struct mapping

Trigger: Stage 12's typed accessors prove insufficient for a concrete use case (large, stable response schemas).
**CPP-14** — `src/registry/struct-registry.*`, `src/mapping/struct-mapping.*`. **TEST-10** — coverage. **REV-11** — contract review. **Human approval gate: YES.**

### Stage 16 (CONDITIONAL) — CAPL-side request-body building

Trigger: hand-assembling JSON in CAPL proves genuinely cumbersome in practice. This was dead code last time — real flows built bodies by hand despite a ready-made API existing.
**CPP-15** / **TEST-11** / **REV-12**. **Human approval gate: YES.**

---

## 11. Task index by agent

### `build-pipeline-engineer` — 14 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| BPE-1 | 2 | `scripts/setup-dev-env.ps1` | **DONE — proven, 14 OK / 0 WARN / 0 FAIL** |
| BPE-2 | 4 | Finalize `.gitignore` (reconcile the staged change first) | |
| BPE-3 | 4 | Create `src/`, `build/`, `tests/`, `examples/` skeleton | |
| BPE-4 | 4 | `Makefile` — `all`/`build-x86`/`build-x64`/`test`/`clean` over one parameterized rule | |
| BPE-5 | 4 | Git-tag versioning (`version.rc` + `rc.exe /D` + `/VERSION:` + `git describe`) | |
| BPE-6 | 4 | Record exact `.lib` filenames in `lib/README` (versions already recorded) | |
| BPE-7 | 4 | GoogleTest built `/MT`; `make test` functional | |
| BPE-8 | 5 | Link/resource wiring for both DLLs; `dumpbin /exports` verification | |
| BPE-9 | 6 | CI baseline workflow — build both arches + tests on every push, artifact upload | |
| BPE-10 | 8 | Link `libcurl.lib`, `zs.lib` and all seven Windows system libs | |
| BPE-11 | 13 | Release workflow — tag extraction, approval gate, publish | |
| BPE-12 | 13 | Generate the operation list from the export table at build time | |
| BPE-13 | 14 | Build-system cleanup pass | |
| BPE-14 | 13 | Cut `CHANGELOG.md` `[Unreleased]` into a released section | |

### `cpp-implementer` — 15 tasks

| ID | Stage | Task |
|---|---|---|
| CPP-1 | 5 | `exports.cpp` + `exports.def` — one operation; fixes the CAPL naming convention permanently |
| CPP-2 | 7 | `src/core/type-conversion.*` |
| CPP-3 | 7 | `src/core/json-path.*` |
| CPP-4 | 8 | `src/http/http-client.*` (libcurl wrapper) |
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

| ID | Stage | Task |
|---|---|---|
| TEST-1 | 4 | `tests/` skeleton + one passing test so `make test` is green from the first commit |
| TEST-2 | 7 | `tests/core/` — type-conversion |
| TEST-3 | 7 | `tests/core/` — json-path |
| TEST-4 | 8 | libcurl fake/mock boundary (no real network in the suite) |
| TEST-5 | 8 | `tests/http/` — http-client + sync-operations, incl. timeouts and error responses |
| TEST-6 | 10 | `tests/http/` — async: ready-flag-cleared-after-read, request-ID correlation |
| TEST-7 | 11 | `tests/mapping/` — json-flatten (nesting, arrays, empty/malformed) |
| TEST-8 | 12 | `tests/mapping/` — json-accessors (type mismatches, cache invalidation) |
| TEST-9 | 14 | Coverage audit across all of `src/` |
| TEST-10 | 15 | Struct mapping tests (conditional) |
| TEST-11 | 16 | Request-builder tests (conditional) |

### `code-reviewer` — 12 tasks

| ID | Stage | Focus | Status |
|---|---|---|---|
| REV-1 | 1 | Stale-identifier sweep — scope: `CLAUDE.md` and `.claude/**` only | **SATISFIED** |
| REV-2 | 4 | Makefile parameterization, versioning derivation, `/MT` provenance of the real `.lib` artifacts | |
| REV-3 | 5 | **Export-contract genesis — the most important review in the plan** | |
| REV-4 | 6 | CI reuses Make targets; matrix symmetry | |
| REV-5 | 9 | Contract append — sync | |
| REV-6 | 10 | Contract append — async | |
| REV-7 | 11 | Contract append — flattening | |
| REV-8 | 12 | Contract append — accessors | |
| REV-9 | 13 | Release workflow; no hardcoded versions; approval gate actually blocks | |
| REV-10 | 14 | Final consistency review incl. `project-docs` agreement | |
| REV-11 | 15 | Contract append — struct mapping (conditional) | |
| REV-12 | 16 | Contract append — request building (conditional) | |

### Human — 18 tasks

| ID | Stage | Task | Status |
|---|---|---|---|
| HUM-1 … HUM-9 | 1 | Configuration reconciliation (nine items) | **ALL DONE** |
| HUM-10 | 3 | Install Vector CANoe/CANalyzer | |
| HUM-11 | 3 | Build + load the official Vector sample unchanged in CANoe | |
| HUM-12 | 4 | Commit and push the skeleton | |
| HUM-13 | 5 | Load and call the Hello DLL operation from a real `.can` script | |
| HUM-14 | 9 | Verify sync operations in CANoe | |
| HUM-15 | 10 | Verify async operations in CANoe | |
| HUM-16 | 11 | Verify CAPL associative-field syntax against the official CANoe help | |
| HUM-17 | 13 | Create the release tag | |
| HUM-18 | 13 | Verify the CI artifact in CANoe, then approve the publish | |

---

## 12. Risks

**Export contract.** Stage 5 is irreversible in practice: the naming convention and version-entry layout chosen there bind every later append. Stages 9, 10, 11, 12, 15, 16 each append — each requires `code-reviewer` and a human gate.

**ABI failure modes that hide *all* operations, not just the new one.** Raw text pointer instead of the caller-supplied buffer; incorrect 1-byte alignment coverage through the terminating pointer. 04-FLOW §7 states plainly that this project's historical difficulty was never business logic — it was ABI compatibility with CANoe. Stage 5 is a hard gate for exactly this reason.

**Static review does not substitute for execution — now demonstrated, not theorised.** Stage 2's script passed a clean static review and still failed on first contact with a real machine: a Polish-localized compiler banner and a vcpkg feature name that no longer exists were both invisible to code reading. Treat "reviewed clean" as necessary and not sufficient for anything that touches an external tool, a locale, or a network. This raises rather than lowers confidence in the Stage 5 hard gate.

**`/MT` contamination.** A `/MD` libcurl/zlib/GoogleTest yields `LNK4098` at best and a second CRT heap inside the CANoe host process at worst. The artifacts now exist, so REV-2 can verify `dumpbin /directives` on `libcurl.lib` and `zs.lib` in both architectures for real rather than on trust. If a dependency is only available as `/MD`, **stop and ask**.

**Bitness parity.** CANoe loads only a matching-bitness DLL; a mismatch gives "Requested CAPL DLL is invalid". BPE-4's single parameterized rule makes flag drift structurally impossible rather than merely forbidden, and the no-`LIBRARY` rule neutralizes the shared-`.def` trap. Environment trap: building x64 from an x86 Native Tools prompt gives misleading linker errors — now guarded by the script's per-architecture check.

**Documentation drift between the script and `docs/development-environment.md`.** Moving rationale out of inline comments improves both, but creates two artifacts that must move together. Any script change invalidating a documented rationale must update the document in the same change; REV-10 checks this at Stage 14, but each change is responsible in the moment.

**Configuration drift — closed, with a scoped standing check.** v4 declared five stale config files; the real number was eight. All operational files are now clean and re-verified. REV-1's corrected scope excludes historical drafts and its own definition — as originally written it produced noise that obscured real drift, and a check that cries wolf gets ignored.

**Agent routing metadata is easy to miss.** Body and frontmatter `description` are separate surfaces; only the body is visible when reading a file as prose. Any future agent-scope change must check both.

**Vector SDK header redistribution — accepted risk.** The repository is public and the headers are committed; an explicit user decision, not re-litigated. Git history makes it effectively permanent.

**A "tidy-up" regression in `.gitignore`.** The `Makefile` and `*.lib` rules are commented out, not deleted, and sit inside blocks labelled "CMake generated files" and "Compiled Static libraries" where they look like they belong. Without BPE-2's annotation, someone will eventually restore them and silently untrack the Makefile and every vendored `.lib`.

**Bootstrap script scope creep.** `scripts/setup-dev-env.ps1` installs software and touches global state. It must stay idempotent and must never become a second build system — 04-FLOW §5 explicitly warns against "a growing set of helper scripts" in place of one Makefile. The script provisions the environment; `make` builds the project.

**Toolchain divergence between local Build Tools and the CI runner image.** Local setup installs whatever Build Tools version is current; `windows-latest` ships its own. Pin the toolset in both places if a reproducibility question arises, rather than debugging a version-skew symptom.

**Versioning is new, untested machinery.** `FILEVERSION`/`PRODUCTVERSION` are four 16-bit fields capped at 65535 that **wrap silently**; `/VERSION:` accepts major.minor only; `version.rc` needs working `#ifndef` fallbacks so a bare `rc.exe` doesn't fail. `git describe --tags --always` carries both flags precisely so it works in a repo with no tags — still this repo's state. BPE-5 must test this before a real release depends on it.

**Steps no agent can verify.** The nine remaining HUM tasks, particularly the CANoe verifications at Stages 5, 9, 10, 11 and 13. 04-FLOW §5's last checklist item names the previous iteration's habit of leaving these perpetually open: "status: ready to build" ≠ "status: tested and working".

**Scope creep toward struct mapping.** Stages 11 and 12 must ship before Stage 15 is reconsidered.

---

## 13. Operational loose ends (not plan content)

Tracked here so they are not lost; these belong to the session, not to a stage.

1. **The entire Stage 2 + documentation batch is uncommitted**, with mixed staged/unstaged state: `CHANGELOG.md`, `docs/development-environment.md`, `scripts/setup-dev-env.ps1` and `.gitignore` staged; `.claude/skills/project-docs/SKILL.md` and `README.md` unstaged. Commit as one coherent change or deliberately split — but do not leave it half-staged, since a partial commit of this batch would ship a script whose rationale document is missing.
2. **The staged `.gitignore` modification is unexplained** and is not from any plan task. Identify it before committing; BPE-2 (Stage 4) owns this file next and should not blindly duplicate or revert it.
3. **The final README expansion (execution-policy and switches) has not been through `code-reviewer`.** It is prose documentation, not executable logic, and the surrounding batch reviewed clean. Recommendation: let it ride along with the next review rather than spending a dedicated cycle — but note it, so "reviewed" is not claimed more broadly than it was.

---

## 14. Execution order

**1 (done) → 2 (done) → 3 ∥ 4 → 5 (hard gate) → 6 → 7 → 8 → 9 → 10 → 11 → 12 → 13 → 14**, with Stages 15–16 only on demonstrated need. Stage 3 and Stage 4 may proceed in parallel; Stage 3 must complete before Stage 5.

**Status:** v8. Stages 1 and 2 complete, Stage 2 proven by a real run. **Stage 3 (manual CANoe setup, human) and Stage 4 (skeleton + Makefile + versioning, `build-pipeline-engineer`) are the next work.**
