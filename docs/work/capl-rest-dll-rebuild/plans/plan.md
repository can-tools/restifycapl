# Plan (v5): Build the CAPL REST DLL (restifycapl) from zero

Revision of v4. Changes in this revision: consistent phase/stage numbering, per-agent task breakdowns, corrected configuration scope, and CI moved earlier.

---

## 1. Goal

Build the CAPL REST DLL (`restifycapl`) from the cloned repository to a working, dual-architecture (x86 + x64) native CANoe plugin: synchronous and asynchronous REST/HTTP for CAPL scripts plus JSON flattening and typed path accessors. Struct mapping and CAPL-side request building stay deferred until a demonstrated need. Environment setup is scripted wherever possible. The CANoe ABI is proven on one trivial operation before any REST logic exists. All business logic is unit-tested outside CANoe. The export table stays a thin, append-only glue layer. Every version number derives from the Git tag.

---

## 2. Current state, verified

**Completed** — six configuration files already reflect the target state: `CLAUDE.md`, `.claude/skills/msvc-build-conventions/SKILL.md`, `.claude/skills/capl-export-contract/SKILL.md`, `.claude/agents/code-reviewer.md`, `.claude/agents/cpp-implementer.md`, `.claude/settings.json`.

**Still stale — four items v4 missed or under-specified:**

- `.claude/agents/build-pipeline-engineer.md` — still says `build32`/`build64` in three places (frontmatter description, responsibilities bullet 1, hard rule 4) and references `version.rc` without its path. v4 listed this file under "source files" but never wrote an edit spec for it.
- `.claude/agents/test-engineer.md` — still names previous-iteration modules (`json-path-resolver`, `type-converters`, `json-helpers`, `request-builder`, `sync-rest-operations`, `async-rest-operations`). Never mentioned in v4.
- `.claude/skills/cpp-testing-conventions/SKILL.md` — same stale module names. Never mentioned in v4.
- `docs/.locals/04-FLOW-AND-DEPENDENCIES.md` — §6.2 still concludes "a dedicated script to bootstrap the whole project isn't needed here"; §6.1 and §6.3 still say install full Visual Studio. Both contradict decisions already made.

**Also outstanding:** the three Vector SDK headers are still flat at `include/vendor/cdll.h`, `VIA.h`, `VIA_CDLL.h` and need to move into `include/vendor/capl-dll-sdk/`.

**Not started:** no `src/`, `lib/`, `tests/`, `scripts/`, `examples/`, `Makefile`, or `.github/` exist.

---

## 3. Numbering scheme

v4 used `P → 0A → 0B → 1…11` with conditional `9`/`10` — inconsistent, and requiring the reader to remember why a stage was `0A` rather than `1`. Replaced by **six phases containing sixteen sequentially numbered stages**, no letter suffixes, no gaps:

| Phase | Stages | Theme |
|---|---|---|
| **Phase 1 — Foundation & Environment** | 1–4 | Config correctness, scripted bootstrap, manual CANoe setup, repo skeleton |
| **Phase 2 — ABI Proof & Continuous Verification** | 5–6 | Hello DLL in CANoe, then CI running on every push |
| **Phase 3 — Business Logic & CAPL Surface** | 7–12 | Core logic, HTTP, async, flattening, accessors |
| **Phase 4 — Release Pipeline** | 13 | Tag-driven versioning, approval gate, publish |
| **Phase 5 — Hardening** | 14 | Cleanup and consistency |
| **Phase 6 — Conditional Extensions** | 15–16 | Only on demonstrated need |

Mapping from v4: `P1→2`, `P2→3`, `0A→1`, `0B→4`, `1→5`, `2→7`, `3→8`, `4→9`, `5→10`, `6→11`, `7→12`, `8→6 and 13` (split), `11→14`, `9→15`, `10→16`.

**One deliberate change beyond renumbering: CI is split and moved earlier.** v4 had a single CI stage after all logic was written. v5 splits it — a **CI baseline** (build both architectures, run tests, on every push) lands at Stage 6, right after the ABI is proven, so every subsequent stage is continuously verified on a clean machine rather than only on the developer's box. The **release pipeline** (tag extraction, approval gate, publish) stays late at Stage 13, because it only matters once there is something worth releasing. This directly serves the project's stated aim of catching problems at the cheapest possible point.

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
- **Agents cannot `git push` or `git tag`** — both denied by design. Pushes and release tags are human actions.

---

## 5. Phase 1 — Foundation & Environment

### Stage 1 — Finish configuration reconciliation

Six files were already brought to target state. Five items remain. **Ownership note:** `.claude/**` and `CLAUDE.md` edits are routed to the **main session under human supervision**, not to a subagent — these files govern agent behaviour, and having an agent rewrite its own operating instructions is a governance smell. `code-reviewer` verifies afterwards.

**HUM-1 — Move the SDK headers.** `include/vendor/cdll.h`, `VIA.h`, `VIA_CDLL.h` → `include/vendor/capl-dll-sdk/`. `json.hpp` lands in `include/vendor/` at Stage 2; keeping Vector's three licensed headers flat beside it destroys the "what's foreign, and whose" separation that justifies `vendor/` existing, and blurs the licensing boundary.

**HUM-2 — Fix `.claude/agents/build-pipeline-engineer.md`** (three stale spots plus additions):

- Frontmatter `description`: "Maintains the Makefile build targets (build32/build64)…" → "…the Makefile build targets (`all`, `build-x86`, `build-x64`, `test`, `clean`)…"
- Responsibilities bullet 1: replace with —

```markdown
- Maintain the `all`, `build-x86`, `build-x64`, `test` and `clean` Makefile
  targets. `build-x86` and `build-x64` must be thin wrappers over a single
  parameterized rule with the architecture passed as a Make variable — never
  two parallel recipes. MSVC flags (`/MT`, `/std:c++17`, `/EHsc`) are
  identical across architectures; only `/MACHINE:` and the `lib/` path differ.
```

- Hard rule 4: `version.rc`, `build32`, `build64` → `src/module/version.rc`, `build-x86`, `build-x64`.
- Add a responsibility: "Own `scripts/setup-dev-env.ps1`. It provisions the environment only — it must never become a second build system."

**HUM-3 — Fix `.claude/agents/test-engineer.md`.** Replace the first two responsibility bullets:

```markdown
- Write and maintain unit tests in `tests/`, mirroring the layered `src/`
  layout: `tests/core/`, `tests/http/`, `tests/mapping/` (and `tests/registry/`
  only if that module is ever built).
- Test targets, in build order: `type-conversion`, `json-path` (core);
  `http-client`, `sync-operations`, `async-operations` (http); `json-flatten`,
  `json-accessors` (mapping). `struct-registry` and `struct-mapping` are
  deferred and out of scope unless explicitly reactivated.
```

**HUM-4 — Fix `.claude/skills/cpp-testing-conventions/SKILL.md`.** Same substitution in "Framework" and "What can and cannot be tested here": the testable list becomes `type-conversion`, `json-path`, `json-flatten`, `json-accessors`, plus logic extracted from `sync-operations`/`async-operations`. Add: "Tests mirror the layered `src/` structure — `tests/core/`, `tests/http/`, `tests/mapping/`." Add a required-coverage bullet: "for the async layer specifically: ready-flag-cleared-after-read, and request-ID correlation across consecutive requests."

**HUM-5 — Update `docs/.locals/04-FLOW-AND-DEPENDENCIES.md`.** §6.2's conclusion ("a dedicated script to bootstrap the whole project isn't needed here") now contradicts a decided position; rewrite it to state that a local bootstrap script *is* used, and why the original reasoning no longer holds — category 3 shrank once the headers were committed, and a script is re-runnable on a second machine and verifiable in CI, which a prose checklist is not. In §6.1 and §6.3, replace "Visual Studio (Desktop development with C++)" with "Visual Studio **Build Tools** (`VCTools` workload)" and note that silent unattended install is supported, so it belongs in the script rather than in a manual checklist.

**REV-1 — Verification pass.** `code-reviewer` confirms no `build32`/`build64`, `lib-32b`/`lib-64b`, `build-32b`/`build-64b`, `capl-rest-dll.cpp`, `capl-rest-32b.dll`, or previous-iteration module names survive anywhere in `CLAUDE.md`, `.claude/**`, or `docs/.locals/**`. A single grep sweep; this is exactly the drift that v4 missed twice.

**Human approval gate: YES.** These files direct every later stage.

### Stage 2 — Scripted development-environment bootstrap

**BPE-1 — Write `scripts/setup-dev-env.ps1`.** Idempotent, re-runnable, safe to run twice, prints a pass/fail summary (modelled on 04-FLOW §6.9) rather than aborting at the first problem. Tasks:

1. Probe for an existing MSVC toolchain via `vswhere.exe` at `%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe` before attempting any install.
2. If absent, silently install **VS Build Tools**: `vs_BuildTools.exe --quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended`. Detect non-elevated sessions and print the exact command to run elevated rather than failing obscurely. Treat exit code `3010` as success-pending-reboot, not failure.
3. Resolve and report `vcvarsall.bat`; confirm `cl`, `rc`, `link`, `dumpbin` resolve for **both** architectures. Refuse to continue silently in the wrong Native Tools environment — running an x64 build from an x86 prompt is 04-FLOW §6.3's named cause of misleading linker errors.
4. Detect `make`; install via MSYS2/Chocolatey/Scoop if absent, or report the exact command when elevation is required.
5. Detect or bootstrap vcpkg; run `vcpkg install curl[schannel]:x86-windows-static` and `curl[schannel]:x64-windows-static`.
6. Copy the resulting `.lib` files into `lib/x86/` and `lib/x64/` — **separate per-architecture folders**, never a shared vcpkg tree; the whole point is making a wrong-architecture link impossible.
7. Download `json.hpp` from the pinned v3.11.3 Releases URL (amalgamated single file, not `git clone`) and **verify its SHA-256** before placing it in `include/vendor/`. A pin without a checksum is a pin in name only.
8. Create the directory skeleton and write `lib/README` recording exact libcurl and json.hpp versions.
9. **Verify, never fetch,** that `include/vendor/capl-dll-sdk/` holds the three SDK headers, and fail with a clear message pointing at Stage 3 if not.

**Human approval gate: YES** — approve before first run; it installs software and touches global state.

### Stage 3 — Manual toolchain setup (human only)

No automation path exists for either item — a licensing constraint, not a technical one.

**HUM-6 — Install Vector CANoe/CANalyzer.** Licensed, GUI-installed, machine-bound.

**HUM-7 — Build and load the official "Example of a Windows DLL for CAPL" sample, unchanged, in CANoe.** The step people skip and shouldn't: it proves the toolchain + CANoe pairing works before any project code can be blamed for a failure. Build Tools ships MSBuild, so the sample compiles without the IDE; loading it into CANoe is manual.

Stage 3 runs in parallel with Stages 1–2 and 4. It only hard-blocks Stage 5.

### Stage 4 — Repo skeleton, Makefile, versioning

**BPE-2 — Finalize `.gitignore`.** Delete the dead `build-*/` (a leftover from the abandoned `build-32b/` naming; `build/` already covers output). Annotate the two commented lines (`#*.lib`, `# Makefile`) with *why* they are disabled — without a note, a future tidy-up of the "CMake generated files" and "Compiled Static libraries" blocks will silently untrack the Makefile and every vendored `.lib`.

**BPE-3 — Create the directory skeleton** per `CLAUDE.md`'s layout: `src/{core,http,registry,mapping,module}/`, `lib/{x86,x64}/`, `build/{x86,x64}/`, `tests/{core,http,mapping}/`, `examples/`, `scripts/`.

**BPE-4 — Write the `Makefile`.** Targets `all` (default, both architectures), `build-x86`, `build-x64`, `test`, `clean`. The two architecture targets are thin wrappers over **one parameterized rule** with architecture as a Make variable — this makes flag drift between x86 and x64 structurally impossible rather than merely forbidden, which materially reduces the plan's second-largest risk. `clean` removes **all** intermediates, not a selected few.

**BPE-5 — Wire the versioning mechanism in from day one.** `src/module/version.rc` parameterized via `rc.exe /D VER_MAJOR/VER_MINOR/VER_BUILD/VER_REV` with `#ifndef` fallbacks; `/VERSION:X.Y` on the linker; `git describe --tags --always --dirty` into `StringFileInfo`; `git rev-list --count <tag>..HEAD` for local build/revision. Feed only small integers to `FILEVERSION`/`PRODUCTVERSION` — four 16-bit fields capped at 65535 that **wrap silently**. Test it deliberately now, in a repo that currently has **no tags at all** — `--tags --always` carries both flags precisely for this case.

**BPE-6 — Write `lib/README`** recording exact libcurl and json.hpp versions.

**BPE-7 — Integrate GoogleTest built `/MT`** and make `make test` functional.

**TEST-1 — Create the `tests/` skeleton with one trivial passing test** so `make test` is green from the very first commit. 04-FLOW §3.3 is explicit that `tests/` existing from day one — not as a "nice to have" — is what forces the reflex of writing a test before moving on.

**REV-2 — Review** Makefile parameterization, versioning derivation, `.gitignore` semantics, and confirm `dumpbin /directives` on every vendored `.lib` shows `/DEFAULTLIB:LIBCMT`.

**HUM-8 — Commit and push** (agents are denied `git push`).

**Human approval gate: YES** — a `/MD` library slipping through here poisons everything downstream.

---

## 6. Phase 2 — ABI Proof & Continuous Verification

### Stage 5 — "Hello DLL": one operation, verified in CANoe — HARD GATE

**CPP-1 — Write `src/module/exports.cpp` and `exports.def`.** Exactly **one** trivial operation (e.g. return a fixed version string into a caller-supplied buffer). `exports.def` contains `EXPORTS` and nothing else. Apply both post-mortem rules from section 4: output-buffer pattern, and 1-byte packing across the whole table including the terminating pointer. **This stage permanently fixes the CAPL-visible naming convention** for every operation the project will ever expose — the never-rename rule means it cannot be revised later without a major-version break. Decide it deliberately and write it down.

**BPE-8 — Link and resource wiring** for both architectures, producing `build/x86/restifycapl-x86.dll` and `build/x64/restifycapl-x64.dll`; verify the export list with `dumpbin /exports`.

**REV-3 — Export-contract review. The single most important review in this plan.** Naming convention, table layout, reserved version entry, `extern "C"`, packing, absence of a `LIBRARY` line.

**HUM-9 — Load and call the operation from a real `.can` script in CANoe.** No agent can do this.

**Human approval gate: YES — the most important gate in this plan.** Do not start Stage 6 until the operation is confirmed callable from CAPL. Everything downstream assumes the ABI is proven. This is exactly why 04-FLOW moves CANoe integration to its Step 1 rather than leaving it until dozens of operations exist — a return-value bug at that point hid every operation at once and cost disproportionate time to find.

### Stage 6 — CI baseline: build both architectures and run tests on every push

**BPE-9 — Write `.github/workflows/ci.yml`.** `windows-latest`, matrix over x86/x64, MSVC environment activated (`vcvarsall.bat` or an equivalent action) for the matching architecture before invoking Make. **Calls the same `build-x86`/`build-x64` Make targets used locally — never duplicates `cl.exe`/`rc.exe` invocations in YAML.** Runs `make test`. Uploads both DLLs as workflow artifacts on every push so a reviewable binary always exists. Because the SDK headers are committed, CI compiles `src/module/` and links the DLLs itself — no split-responsibility design is needed.

**REV-4 — Review** that CI reuses Make targets rather than reimplementing the build, and that the matrix covers both architectures symmetrically.

**Human approval gate: YES** — this defines what gets verified from here on.

---

## 7. Phase 3 — Business Logic & CAPL Surface

Stages 9–12 each append to the export table. Every append is a contract change requiring `code-reviewer` and a human gate.

### Stage 7 — Core pure logic (level 0)

**CPP-2** — `src/core/type-conversion.*` (JSON ↔ C++ value conversions).
**CPP-3** — `src/core/json-path.*` (path resolution inside a JSON tree).
Both depend on `json.hpp` only — zero I/O, zero CANoe knowledge, not yet exported.
**TEST-2 / TEST-3** — `tests/core/` coverage for each: valid input, malformed/missing JSON fields, type mismatches.
**Human approval: no.**

### Stage 8 — HTTP layer and synchronous operations (logic only)

**CPP-4** — `src/http/http-client.*` wrapping libcurl.
**CPP-5** — `src/http/sync-operations.*` (blocking request/response).
**BPE-10** — Link libcurl + zlib + all Windows system libs (crypt32, bcrypt, secur32, ws2_32, normaliz, wldap32, advapi32 — link all, they are transitively required).
**TEST-4** — Build the libcurl fake/mock boundary. No real network calls in the suite.
**TEST-5** — Coverage including **simulated timeouts and error responses**, not just the happy path.
Verify as a standalone console program against httpbin.org, per 04-FLOW Step 2.
**Human approval: no.**

### Stage 9 — Expose synchronous REST to CAPL (first contract append)

**CPP-6** — Append sync operations to the export table; rebuild both architectures.
**REV-5** — Export-contract review (mandatory).
**HUM-10** — Verify in CANoe.
**Human approval gate: YES.**

### Stage 10 — Asynchronous layer with response state designed correctly up front

**CPP-7** — `src/http/async-operations.*`: background dispatch, readiness check, wait-for-result. Response-state semantics settled **now, not retrofitted** — the ready flag is cleared once read, and a request ID ties a response to the call that produced it. 04-FLOW §5 item 3 records that the previous iteration identified this early but never confirmed it was implemented. Shared state is global within the DLL with per-module synchronization; **one active response at a time** by deliberate design, not a concurrent request pool.
**TEST-6** — `tests/http/` coverage specifically for ready-flag-cleared-after-read and request-ID correlation across consecutive requests.
**CPP-8** — Append async operations to the export table.
**REV-6** — Export-contract review.
**HUM-11** — Verify in CANoe.
**Human approval gate: YES** — contract append, and the one-active-response simplification bounds what the DLL can ever do.

### Stage 11 — JSON flattening (highest user value — ship before struct mapping)

**CPP-9** — `src/mapping/json-flatten.*`: flatten the response into a dot-notation key/value map, plus key count, key-by-index, value-by-key.
**TEST-7** — `tests/mapping/` coverage including deeply nested objects, arrays, and empty/malformed documents.
**HUM-12 — Mandatory before any `.can` example is written:** verify associative-field syntax against the official CANoe help (`Help → CAPL → General → Associative Fields`). The correct form has **no extra keyword before the type** — `char[30] name[char[]];`. An invented keyword was copied across many docs and example files last time. Check the product help; do not trust generated snippets.
**CPP-10** — Append flattening operations to the export table.
**CPP-11** — Write `examples/*.can`, only after HUM-12 confirms the syntax.
**REV-7** — Export-contract review.
**Human approval gate: YES** — contract append; confirm HUM-12 actually happened.

### Stage 12 — Typed JSON accessors

**CPP-12** — `src/mapping/json-accessors.*`: typed point reads at a JSON path (integer/float/bool/string) without flattening, array helpers (length, element-by-index), optional cache for recent queries.
**TEST-8** — Coverage including type mismatches at each accessor type and cache invalidation between responses.
**CPP-13** — Append accessor operations to the export table.
**REV-8** — Export-contract review.
**Human approval gate: YES** — contract append.

---

## 8. Phase 4 — Release Pipeline

### Stage 13 — Tag-driven release with an approval gate

**BPE-11 — Write `.github/workflows/release.yml`.** Triggered by a `vX.Y.Z` tag push. Extracts `X.Y.Z` from `github.ref_name` (stripping `v`), feeds `X.Y` to `/VERSION:` and `major,minor,build,revision` to `FILEVERSION`/`PRODUCTVERSION` via `rc.exe /D`. Builds both architectures and runs tests, then **halts at a manual approval gate** (a GitHub Environment with required reviewers is the natural mechanism), and only publishes to GitHub Releases after approval.

**BPE-12 — Generate the exposed-operation list from the export table at build time** and publish it with the release, so documentation cannot drift from code. The previous iteration had different operation counts stated in different README files depending on when each was last edited.

**REV-9 — Review** the release workflow, confirming no hardcoded version anywhere and that the approval gate genuinely blocks publication.

**HUM-13 — Create the release tag** (agents are denied `git tag`).
**HUM-14 — Verify the CI-built artifact manually in CANoe, then approve the publish.** This ordering is what makes manual CANoe verification a real precondition of release rather than an aspiration.

**Human approval gate: YES** — this determines what ships.

---

## 9. Phase 5 — Hardening

### Stage 14 — Cleanup and consistency pass

**BPE-13** — One `Makefile`, no historical variants; `clean` removes every intermediate; no build artifacts tracked; version and operation list each maintained in exactly one place.
**TEST-9** — Coverage audit: every module in `src/` that can be tested has a corresponding test file in the mirrored `tests/` location.
**REV-10** — Final review. Anything removed as dead code must be removed **in full** (export-table entry + implementation + documentation) in a single commit — last time part of the JSON-building operations were removed while a simpler variant stayed exposed, contradicting docs that claimed the whole module was gone.
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

### `build-pipeline-engineer` — 13 tasks

| ID | Stage | Task |
|---|---|---|
| BPE-1 | 2 | Write `scripts/setup-dev-env.ps1` (Build Tools probe/install, make, vcpkg curl[schannel] both triplets, json.hpp v3.11.3 + SHA-256, skeleton, `lib/README`, SDK-header verification) |
| BPE-2 | 4 | Finalize `.gitignore` — remove dead `build-*/`, annotate the two commented lines |
| BPE-3 | 4 | Create the directory skeleton |
| BPE-4 | 4 | Write the `Makefile` — `all`/`build-x86`/`build-x64`/`test`/`clean` over one parameterized rule |
| BPE-5 | 4 | Wire the git-tag versioning mechanism (`version.rc` + `rc.exe /D` + `/VERSION:` + `git describe`) |
| BPE-6 | 4 | Write `lib/README` with exact dependency versions |
| BPE-7 | 4 | Integrate GoogleTest built `/MT`; make `make test` functional |
| BPE-8 | 5 | Link/resource wiring for both DLLs; `dumpbin /exports` verification |
| BPE-9 | 6 | CI baseline workflow — build both arches + tests on every push, artifact upload |
| BPE-10 | 8 | Link libcurl, zlib and all seven Windows system libs |
| BPE-11 | 13 | Release workflow — tag extraction, approval gate, publish |
| BPE-12 | 13 | Generate the operation list from the export table at build time |
| BPE-13 | 14 | Build-system cleanup pass |

### `cpp-implementer` — 15 tasks

| ID | Stage | Task |
|---|---|---|
| CPP-1 | 5 | `exports.cpp` + `exports.def` — one operation; fixes the CAPL naming convention permanently |
| CPP-2 | 7 | `src/core/type-conversion.*` |
| CPP-3 | 7 | `src/core/json-path.*` |
| CPP-4 | 8 | `src/http/http-client.*` (libcurl wrapper) |
| CPP-5 | 8 | `src/http/sync-operations.*` |
| CPP-6 | 9 | Append sync operations to the export table |
| CPP-7 | 10 | `src/http/async-operations.*` + response-state semantics |
| CPP-8 | 10 | Append async operations |
| CPP-9 | 11 | `src/mapping/json-flatten.*` |
| CPP-10 | 11 | Append flattening operations |
| CPP-11 | 11 | `examples/*.can` — only after HUM-12 |
| CPP-12 | 12 | `src/mapping/json-accessors.*` |
| CPP-13 | 12 | Append accessor operations |
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

| ID | Stage | Focus |
|---|---|---|
| REV-1 | 1 | Verify configuration reconciliation is complete — grep sweep for every stale identifier |
| REV-2 | 4 | Makefile parameterization, versioning derivation, `/MT` provenance of vendored libs |
| REV-3 | 5 | **Export-contract genesis — the most important review in the plan** |
| REV-4 | 6 | CI reuses Make targets; matrix symmetry |
| REV-5 | 9 | Contract append — sync |
| REV-6 | 10 | Contract append — async |
| REV-7 | 11 | Contract append — flattening |
| REV-8 | 12 | Contract append — accessors |
| REV-9 | 13 | Release workflow; no hardcoded versions; approval gate actually blocks |
| REV-10 | 14 | Final consistency review |
| REV-11 | 15 | Contract append — struct mapping (conditional) |
| REV-12 | 16 | Contract append — request building (conditional) |

### Human — 14 tasks

| ID | Stage | Task |
|---|---|---|
| HUM-1 | 1 | Move SDK headers into `include/vendor/capl-dll-sdk/` |
| HUM-2 | 1 | Fix `.claude/agents/build-pipeline-engineer.md` |
| HUM-3 | 1 | Fix `.claude/agents/test-engineer.md` |
| HUM-4 | 1 | Fix `.claude/skills/cpp-testing-conventions/SKILL.md` |
| HUM-5 | 1 | Update 04-FLOW §6.1, §6.2, §6.3 |
| HUM-6 | 3 | Install Vector CANoe/CANalyzer |
| HUM-7 | 3 | Build + load the official Vector sample unchanged in CANoe |
| HUM-8 | 4 | Commit and push the skeleton |
| HUM-9 | 5 | Load and call the Hello DLL operation from a real `.can` script |
| HUM-10 | 9 | Verify sync operations in CANoe |
| HUM-11 | 10 | Verify async operations in CANoe |
| HUM-12 | 11 | Verify CAPL associative-field syntax against the official CANoe help |
| HUM-13 | 13 | Create the release tag |
| HUM-14 | 13 | Verify the CI artifact in CANoe, then approve the publish |

---

## 12. Risks

**Export contract.** Stage 5 is irreversible in practice: the naming convention and version-entry layout chosen there bind every later append. Stages 9, 10, 11, 12, 15, 16 each append — each requires `code-reviewer` and a human gate.

**ABI failure modes that hide *all* operations, not just the new one.** Raw text pointer instead of the caller-supplied buffer; incorrect 1-byte alignment coverage through the terminating pointer. 04-FLOW §7 states plainly that this project's historical difficulty was never business logic — it was ABI compatibility with CANoe. Stage 5 exists as its own hard gate for exactly this reason.

**`/MT` contamination.** A `/MD` libcurl/zlib/GoogleTest yields `LNK4098` at best and a second CRT heap inside the CANoe host process at worst. Static triplets are `/MT` by default — verify with `dumpbin /directives` rather than trusting it. If a dependency is only available as `/MD`, **stop and ask**.

**Bitness parity.** CANoe loads only a matching-bitness DLL; a mismatch gives "Requested CAPL DLL is invalid". This risk drops materially because BPE-4 uses one parameterized rule instead of two near-duplicate recipes, and the related `.def` trap is neutralized by the no-`LIBRARY` rule. Environment trap: building x64 from an x86 Native Tools prompt gives misleading linker errors.

**Configuration drift — demonstrated twice, not hypothetical.** v4 declared five stale files; the real number was eight, and `build-pipeline-engineer.md`, `test-engineer.md` and `cpp-testing-conventions` were all still directing agents with previous-iteration names after the "completed" pass. Agents load skills automatically, so a stale skill silently misdirects work on the most safety-critical files in the project. REV-1 exists specifically to close this class of error with a mechanical sweep rather than another manual enumeration. Stage 1 must complete before Stage 5.

**Vector SDK header redistribution — accepted risk.** The repository is public and the headers are committed; this was an explicit user decision and is not re-litigated here. Practical note only: git history makes it effectively permanent — reversing the position later requires history rewriting, not a delete commit.

**A "tidy-up" regression in `.gitignore`.** The `Makefile` and `*.lib` rules are commented out, not deleted, and sit inside blocks labelled "CMake generated files" and "Compiled Static libraries" where they look like they belong. Without the annotation in BPE-2, someone — or some agent — will eventually restore them and silently untrack the Makefile and every vendored `.lib`.

**Bootstrap script scope creep.** `scripts/setup-dev-env.ps1` installs software and touches global state. It must stay idempotent and must never become a second build system — 04-FLOW §5 explicitly warns against "a growing set of helper scripts" in place of one Makefile. The script provisions the environment; `make` builds the project.

**Toolchain divergence between local Build Tools and the CI runner image.** Local setup installs whatever Build Tools version is current; `windows-latest` ships its own. Usually harmless, but pin the toolset in both places if a reproducibility question ever arises, rather than debugging a version-skew symptom.

**Versioning is new, untested machinery.** `FILEVERSION`/`PRODUCTVERSION` are four 16-bit fields capped at 65535 that **wrap silently**; `/VERSION:` accepts major.minor only; `version.rc` needs working `#ifndef` fallbacks so a bare `rc.exe` doesn't fail. `git describe --tags --always` carries both flags precisely so it works in a repo with no tags — this repo's current state. BPE-5 must test this before a real release depends on it.

**Steps no agent can verify.** All fourteen HUM tasks, particularly the CANoe verifications at Stages 5, 9, 10, 11 and 13. 04-FLOW §5's last checklist item names the previous iteration's habit of leaving these perpetually open: "status: ready to build" ≠ "status: tested and working".

**Scope creep toward struct mapping.** Stages 11 and 12 must ship before Stage 15 is reconsidered.

---

## 13. Execution order

**1 → 2 → 4 → 5 (hard gate) → 6 → 7 → 8 → 9 → 10 → 11 → 12 → 13 → 14**, with Stage 3 running in parallel with 1–2–4 but completing before Stage 5, and Stages 15–16 only on demonstrated need. Stage 1 must not run in parallel with Stage 5.

**Status:** v5 revision. Stage 1 partially complete (six of eleven items done); Stages 2 onward not started.
