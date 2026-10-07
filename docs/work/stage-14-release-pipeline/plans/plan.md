Status: APPROVED by the user (draft v2 decisions G1, G7, G10, G11 recorded in section 9).

# Stage 14 plan: tag-driven automatic release

## 1. Goal

1. A tag `vX.Y.Z` pushed by you on `main` makes the release happen automatically, with no approval gate.
   - It builds and tests x86 and x64 from the tagged commit, taking the version only from the tag.
   - It generates the export table from `src/module/exports.cpp`.
   - It attests the build provenance of the DLLs.
   - It publishes `v0.1.0` as a normal GitHub release through draft, then verify, then make public.
2. `ci.yml` is restructured around a shared setup action and a reusable per-architecture workflow, without the concurrency block.
3. Housekeeping:
   - record the CANoe x64 compile of rows 26–29 at `8bd35f4` in the docs;
   - add the line-ending guard;
   - check that the five Stage 13 files no longer have LF line endings.

## 2. Settled decisions

**Release**
- First version `v0.1.0`, a normal release (OQ1, OQ2). **No approval gate.** Your tag push is the decision to publish.
- HUM-18 ("check in CANoe, then approve") is **dropped by your decision and recorded as not done**. The `restifyReadVersion` call stays in the CANoe-licence batch.
- **Release assets:** `restifycapl-x86.dll` and `restifycapl-x64.dll` (no version in the names, because `capl/includes/includes.cin` lines 5 and 7 load them by name) and `SHA256SUMS`. No debug-symbol files: the Makefile has no `/Zi`, `/DEBUG` or `/PDB`. `capl/` is not packaged; users take it from the same tag's source archive (OQ7a).
- **Release notes** = the `[vX.Y.Z]` CHANGELOG section without its `### Development` part, plus generated parts (see G11).
  - The section contains the verification statement, the Reference line and the contract sentence.
  - The generated parts are the export table, the sentence telling users to take `capl/` from the same tag, and the line explaining how to verify an attestation.
  - Notes must be non-empty after removing `### Development`, and at most 125,000 characters.
- **Draft, then verify, then make public** (OQ17), all automatic. Making the release public is the single last command.
- **Failed-release policy (OQ10):**
  - A, a passing (transient) failure: re-run the failed jobs.
  - B, a defect found before anything is public: delete the draft, delete the tag, fix through a `fix/` PR with its entry in the unpublished `[v0.1.0]` section (release-machinery fixes under `### Development`), re-tag. Not B′ (always bump the version).
  - C, the release is public: never move the tag; ship `v0.1.1`; mark the bad release "Withdrawn" and the CHANGELOG heading `[YANKED]`.
  - D, the release went public but the job reports failure: check the release page by hand.
  - The six workflow safeguards are listed in §4.
- No `environment:` in `publish` (OQ16a), no tag ruleset (OQ18a), no check of `main`'s CI by `validate` (OQ15b). "Green CI on `main`, then tag" stays a convention, listed on the cut-PR checklist.
- **Dry run** with `workflow_dispatch`: once on `main` after the merge, and again on the CHANGELOG-cut branch (OQ9). It never publishes.
- **Artifact attestation:** yes (§4).

**Workflows**
- A shared composite action `provision` holds today's setup steps 2–12 from `ci.yml`, unchanged.
- A reusable `arch-pipeline.yml` takes inputs `arch` and an optional `release-version`. It runs build ‖ test on Windows.
  - `dumpbin /exports` is a step in the build job (U2).
  - The release-version check is a step in the build job, on only when `release-version` is given (U6).
- `ci.yml`:
  - calls `arch-pipeline` once for x86 and once for x64;
  - has gate jobs `build + test (x86)` / `(x64)` that use `if: always()` and pass only if the pipeline succeeded, with check names confirmed on the first run (T2);
  - runs the `export-table` job, which the gates **do not** wait for (R18, mitigations M1–M3);
  - has **no concurrency block** (OQ13).
- No comparison of the two DLLs (OQ5). Light jobs (`validate`, `export-table`, `assemble`, `attest`, `publish`) run on Linux.

**Export-table script**
- `scripts/list-export-table.ps1`, run with `pwsh` 7. `setup-dev-env.ps1` installs `pwsh` and falls back to a warning (P2, P3).
- Reads the fields of each `CAPL_DLL_INFO_LIST4` row by position. Its output is grouped by `categoryName`, compared case-sensitively as whole strings, with row number, CAPL signature and full description.
- Checks 1–12, with 7 and 12 in the "as today" form. TEST-24 test fixtures in `tests/export-table/*.fixture` run only in the `export-table` job. No GoogleTest on the compiled table (OQ20: no).
- Text changes approved:
  - **`scripts/` folder rule** in `CLAUDE.md` and `msvc-build-conventions`: "`scripts/` — environment setup and small repo helpers (e.g. `list-export-table.ps1`); never a second build system."
  - **A1**, replacing lines 23–24 of `.claude/agents/build-pipeline-engineer.md`: "Own `scripts/` (`setup-dev-env.ps1`, `list-export-table.ps1`). `setup-dev-env.ps1` provisions the environment only; nothing in `scripts/` may become a second build system."
  - **`capl-export-contract`**, a new bullet after "Only append new entries…": "**Write every row out in full.** `parTypes`, `array` and `parNames` each give exactly `parCount` entries, as a brace list or a string, with no `""` dimension string and no omitted trailing dimensions or names. This is stricter than Vector's own examples, and is enforced by `scripts/list-export-table.ps1` and its fixtures in `tests/export-table/`."

**Line endings**
- `.gitattributes`:
  ```
  * text=auto eol=crlf
  include/vendor/json.hpp -text
  *.sh text eol=lf
  ```
- `.editorconfig` with `root = true` and `[*] end_of_line = crlf` only. No `charset` (the `.can`/`.cin` files are Windows-1250) and no `trim_trailing_whitespace`.
- Both go on the Stage 14 branch (OQ11). 0L is closed after a read-only check. The OQ12 method (delete, then `git restore`) is optional, done by you, for the six remaining LF files.

**CHANGELOG**
- A separate cut PR `chore/bpe-14-changelog-v0.1.0`, made after the dry run (OQ6A).
- Tidied by four tests (OQ6b):
  - audience: would a CAPL user or someone building from source notice it?
  - "ever shipped": leave out changes and fixes relative to states that never shipped;
  - internals: repo and CI internals that shipped in v0.1.0 go to `### Development`, not to the user sections (G1);
  - duplicates: don't list all 29 signatures, since the export table is in the notes.
- A `### Development` subsection for internal changes, removed from the notes (OQ19-i).
- No stage or task IDs anywhere in `CHANGELOG.md`, including `### Development` (G1).
- You approve the exact text. The cut-PR checklist is approved (§3, step 17).

## 3. Steps

Human-only throughout: push, merging `main` into the stage branch (step 0f), opening or marking the PR ready for review, merge, creating or deleting tags, deleting drafts, editing a public release, GitHub settings. Every verification reports **x86 and x64 separately**. No version number is typed anywhere.

**Before the stage**

| # | Step | Owner | Approval / human | Files |
|---|---|---|---|---|
| 0L | BPE-39: read-only check. `git status` is clean; `git ls-files --eol` shows the five Stage 13 files as `i/lf w/crlf`. Then 0L is closed and HUM-42 dropped. If not, stop and report. | `build-pipeline-engineer` | — | none |
| 0a | Create branch `docs/canoe-verified-rows-26-29` from `main` | `build-pipeline-engineer` | — | — |
| 0b | Update "Verified in CANoe": the x64 compile at `8bd35f4` recognizes rows 26–29, and `restify-json.cin` and `restify-verify-json.can` compile (HUM-36). Items 7 and 8 stay "not yet verified" (HUM-37). Report any README impact. | `docs-writer` | — | `docs/capl-framework.md` |
| 0c | Commit locally | `build-pipeline-engineer` | — | — |
| 0d | REV-24: light review (only that file changed; statements match HUM-36) | `code-reviewer` | — | — |
| 0e | Push, PR, squash-merge | **human** | — | — |
| 0f | Bring the stage branch up to date after step 0e. First `build-pipeline-engineer` commits this plan file, at your request (it was saved uncommitted on the branch and travels with it). Then you merge `main` into `stage/14-release-pipeline` (a merge, not a rebase, per the `stage-branch` skill) and push. This must happen before the fold-in (step 13) and the merge (step 14); otherwise branch protection's up-to-date rule blocks the merge. | **human** (merge, push); `build-pipeline-engineer` (plan-file commit, when you ask) | **human-only**; commit and push are human-triggered | `docs/work/stage-14-release-pipeline/plans/plan.md` |

**Stage 14, on `stage/14-release-pipeline`, already created from `main` at `8bd35f4` before step 0; the plan file travels with the branch**

| # | Step | Owner | Approval / human | Files |
|---|---|---|---|---|
| 1 | Design proposal, no edits. Covers §4: the job graphs, the steps that move into `provision`, inputs, permissions, the script's interface (input path, exit code, message format), M1 and M3 wording, and shows the generated note sentences (G11) and attestation in dry runs (G10) as decided. | `build-pipeline-engineer` | **HUM-38** | — |
| 2 | BPE-40: add the line-ending guard as the first implementation commit. `git add --renormalize .` must stage nothing except the two new files; otherwise stop and ask. Optional: you refresh the six LF files with delete + `git restore`. | `build-pipeline-engineer` (+ `code-reviewer`) | content approved; optional human refresh | `.gitattributes`, `.editorconfig`, `docs/development-environment.md` |
| 3 | BPE-43: rule and skill text: `scripts/` wording, A1, the `capl-export-contract` bullet plus the fix of its stale `.def` example (`CAPLDLLEntryPoint` → `caplDllGetTable4`, matching `src/module/exports.def`), the `project-docs` `### Development` line, the M2 line in `code-reviewer.md` item 1 "Export contract" (G7: for any change touching `src/module/exports.cpp`, record the `export-table` result on the reviewed commit; red is a Must-fix; if it was not run, say so) | `build-pipeline-engineer` | **HUM-43** for the `project-docs` line, the `code-reviewer.md` M2 line and the example fix | `CLAUDE.md`, `msvc-build-conventions`, `build-pipeline-engineer.md`, `capl-export-contract`, `project-docs`, `code-reviewer.md` |
| 4 | BPE-42: `pwsh` 7 in setup. Check for `pwsh` ≥ 7; otherwise install with `winget install -e --id Microsoft.PowerShell` (or Chocolatey `powershell-core`); otherwise **WARN** with the manual command. Compatible with Windows PowerShell 5.1; remind to open a new window. | `build-pipeline-engineer` | — | `scripts/setup-dev-env.ps1`, `docs/development-environment.md` |
| 5 | BPE-37: `provision` action, `arch-pipeline.yml`, restructured `ci.yml` (gates, no concurrency, `dumpbin` step, version-check step that is off in CI). Behaviour of the moved steps stays exactly the same (§7). | `build-pipeline-engineer` | covered by HUM-38 | `.github/actions/provision/action.yml`, `.github/workflows/arch-pipeline.yml`, `ci.yml`, `docs/ci-pipeline.md` |
| 6 | TEST-24: fixtures, one valid fragment plus at least one failing fragment for each check 1–12, written from the step 1 interface | `test-engineer` | — | `tests/export-table/*.fixture` |
| 7 | BPE-12: `list-export-table.ps1`, the `export-table` job in `ci.yml` (script plus fixtures), M1 | `build-pipeline-engineer` | covered by HUM-38 | `scripts/list-export-table.ps1`, `ci.yml` |
| 8 | BPE-11: `release.yml` (validate, two pipeline calls, export-table, assemble, attest, publish) and the release procedure doc | `build-pipeline-engineer` | covered by HUM-38 | `.github/workflows/release.yml`, `docs/release-process.md` |
| 9 | BPE-44: M3, one sentence in `auto-pr.yml`'s `exports.cpp` warning: check that the `export-table` job is green | `build-pipeline-engineer` | covered by HUM-38 | `.github/workflows/auto-pr.yml` |
| 10 | BPE-38: `[Unreleased]` entries (user-visible ones under Added/Changed; internal ones under `### Development`; no stage or task IDs). Local verification, reported separately: `make build-x86`, `make build-x64`, `make test ARCH=x86`, `make test ARCH=x64`, the script plus fixtures. | `build-pipeline-engineer` | — | `CHANGELOG.md` |
| 11 | Push. CI green on both legs; `export-table` green. Confirm the gate check names match the ruleset (T2); if not, **HUM-45**: update the ruleset by hand. | **human** + `build-pipeline-engineer` reads the logs | HUM-45 only if needed | — |
| 12 | REV-9: full-branch review (§7) | `code-reviewer` | zero Must-fix | — |
| 13 | Fold-in to the master plan (§8) as the last commit on the branch | `build-pipeline-engineer` | — | `docs/work/capl-rest-dll-rebuild/plans/plan.md` |
| 14 | Ready for review, merge `--no-ff` | **human** | — | — |

**Release**

| # | Step | Owner | Approval / human | Files |
|---|---|---|---|---|
| 15 | **HUM-40:** dry run on `main`, then read: export table, notes preview, `SHA256SUMS`, `dumpbin` output for both architectures, attestation created and verified. Expect a fix cycle; fixes go through `fix/` branches. | **human** starts it; `build-pipeline-engineer` reads | — | — |
| 16 | BPE-14: cut the CHANGELOG on `chore/bpe-14-changelog-v0.1.0`. `[Unreleased]` becomes `## [v0.1.0] - YYYY-MM-DD` with a new empty `[Unreleased]` above it. Sort entries per G1: internal entries that shipped (`auto-pr.yml`, the stage-branch skill, CI internals: first `ci.yml`, trigger filter, action bumps, caches, `version.lib` in `SYSLIBS`, libcurl link line) move to `### Development`; never-shipped items are dropped (vcpkg `builtin-baseline` fix, `VCPKG_ROOT` fix, `restifyGetVersion` → `restifyReadVersion` rename note); the main section keeps `restifyReadVersion`, the function groups in a few lines, the `capl/` framework, `setup-dev-env.ps1` as the build entry point, and the static `/MT` libcurl fact; existing stage and task IDs are removed. Add the verification statement, Reference line and contract sentence. Add tag links at the bottom. | `build-pipeline-engineer` | **HUM-44**: exact text | `CHANGELOG.md` |
| 17 | Second dry run on the cut branch, then the cut-PR checklist: both dry runs green on both architectures; notes and asset preview read; heading equals the tag to be pushed; date equals the planned tag date; verification statement current. Squash-merge. Wait for `main`'s CI to pass on the merge commit. | **human** | — | — |
| 18 | **HUM-17:** push an annotated tag `v0.1.0` on that merge commit. From here everything is automatic. | **human** | — | — |
| 19 | After publishing: read-only check of the run and release page; run `gh attestation verify` on both DLLs. On failure, apply OQ10. | `build-pipeline-engineer` (read-only), human for any OQ10 action | — | — |
| 20 | On a `docs/` branch: README install and download section; replace the "no tagged release" notices and the version-badge TODO; the Roadmap item. Close-out commit to the master plan (HUM-17 done, HUM-18 not done, release URL). | `docs-writer`; `build-pipeline-engineer` for the master plan | **HUM-41**: README and Roadmap text; human merge | `README.md`, master plan |

## 4. Workflow structure

```
.github/actions/provision/action.yml   composite, input: arch
  MSVC activation → re-pin VCPKG_ROOT → image version → make → tool cache → bootstrap
  → baseline pin check → install-root cache → vcpkg install → place libs → two-part /MT check

.github/workflows/arch-pipeline.yml    reusable, inputs: arch, release-version (optional)
  build (Windows): checkout(fetch-depth 0) → provision → make build-<arch> [VER_* if release-version]
                   → [version check: X.Y.Z.0 / "vX.Y.Z", if release-version] → dumpbin /exports
                     (only caplDllGetTable4) → upload restifycapl-<arch>
  test  (Windows): checkout → provision → make test ARCH=<arch>          (parallel with build)

ci.yml  (push: main/stage/chore/fix/docs; pull_request: main; no concurrency block)
  pipeline-x86 ──uses──► arch-pipeline ──► gate "build + test (x86)"  [required; if: always()]
  pipeline-x64 ──uses──► arch-pipeline ──► gate "build + test (x64)"  [required; if: always()]
  export-table (Linux): fixtures → list-export-table.ps1 → job summary   [not a gate; M1]

release.yml  (push tags v*.*.*; workflow_dispatch = dry run; permissions: contents: read;
              concurrency: release-<ref>, cancel-in-progress: false; all actions SHA-pinned)
  validate (Linux)
    ├─► pipeline-x86 ──uses──► arch-pipeline (release-version = X.Y.Z)
    ├─► pipeline-x64 ──uses──► arch-pipeline (release-version = X.Y.Z)
    └─► export-table (Linux)
            └─► assemble (Linux): SHA256SUMS, notes = section − Development + generated parts,
                          non-empty and ≤ 125,000 characters
                    └─► attest (Linux): id-token: write, attestations: write, contents: read
                            └─► publish (Linux, tag push only): contents: write
                                  draft → upload 3 assets → verify → make public (last command)
```

**`validate`**
- **Tag mode:**
  - the tag matches `^v(0|[1-9]\d*)\.(0|[1-9]\d*)\.(0|[1-9]\d*)$` and each field is ≤ 65535;
  - the tagged commit is an ancestor of `origin/main` (needs `fetch-depth: 0`);
  - **no published release with this tag exists** (safeguard 1);
  - `CHANGELOG.md` at that commit has `## [vX.Y.Z] - YYYY-MM-DD` and is non-empty after `### Development` is removed;
  - all CHANGELOG matching allows `\r?$` at line ends, because Linux checkouts are CRLF too.
- **Dry-run mode:** no tag checks. It uses the topmost section: `[Unreleased]` on `main`, `[v0.1.0]` on the cut branch.

**`attest`**
- GitHub's own `actions/attest-build-provenance`, latest release, **pinned to its commit SHA** (resolved and recorded in step 8; not invented here).
- Subjects: the two DLLs. Not `SHA256SUMS`: it is derived from the DLLs, and the attestations already bind their hashes.
- It runs `gh attestation verify` on both DLLs right after creating them, and fails if that does not succeed.
- **It also runs in dry runs** (G10). This tests it before the tag (R4). The dry-run attestations truthfully describe builds from `main` or the cut branch.
- Availability: artifact attestations are available for public repositories on every plan; the master plan treats this repository as public. **Confirm in HUM-40** that the first dry run creates one.
- **Failure:** the release is not public yet, so this is OQ10 case A (e.g. a Sigstore outage, then re-run) or case B (permissions or configuration, then a `fix/` PR with a `### Development` entry, then re-tag). Attestations made for builds that are never published are harmless and stay.

**`publish`** (tag push only)
1. Again: no published release may exist for the tag.
2. Delete its own leftover **draft** for this tag (safeguard 2; makes re-runs safe).
3. `gh release create --draft` with title `vX.Y.Z` and the notes file. Upload the three assets.
4. **Verify** (safeguard 3): exactly three assets with the expected names; the downloaded hashes match `SHA256SUMS`; notes length is within the limit.
5. `gh release edit vX.Y.Z --draft=false` as the **single last command**.
6. On failure, the job summary names the case (safeguard 6): "nothing is public; the tag may be deleted (B)" or "the release is public; never move the tag (C/D)".
- Safeguard 4: no `workflow_dispatch` path ever runs `publish`.
- Safeguard 5: concurrency is per tag, without cancelling.

**Generated note lines** (G11):
- the export table;
- "Take `capl/` from this tag's source archive; a DLL and `capl/` from different versions fail at CAPL compile.";
- "Verify a download: `gh attestation verify restifycapl-x64.dll --repo <owner/repo>`", where the repository name is filled from `github.repository`.

**`export-table` script, checks 1–12:**
1. The table start and end markers each occur exactly once; row 0 is `CDLL_VERSION_NAME`; the last row is `{0}`.
2. At least one function row.
3. Every function row has exactly 9 fields.
4. Field 1 is `(CAPL_FARCALL)` followed by the same name as field 0.
5. The name starts with `restify`, is at most 49 characters, and is unique.
6. Category and description are non-empty strings.
7. `parCount` ≤ 64 (Vector). The type, dimension and name lists are all written out, each exactly `parCount` long; both written forms count, and string length is measured after decoding escapes. This is a project rule, stricter than Vector, and the error message says so.
8. No `#if`/`#define` inside the table.
9. A type character not in Vector's table, or a `kRef…` identifier that can't be resolved from `exports.cpp`, fails.
10. Combinations Vector rules out fail: `C`/`B`/`I`/`W` with dimension 0, a reference parameter with a dimension, `V` as a parameter, a reference, array or unknown return type.
11. A dimension value other than 0, 1 or 2 fails.
12. Every name is non-empty, a valid identifier, and unique within its row.

Other script behaviour:
- Grouping compares whole strings case-sensitively (`-ceq` or an ordinal comparer); this gets a tier-2 comment in the script.
- The tokenizer skips comments, joins adjacent string literals, decodes escapes, and treats CRLF and LF alike.
- Expected size is about 200–250 lines.

**Comments:**
- One tier-3 trap comment (≤ 5 lines) for the gate's `if: always()`: a skipped required check counts as passing.
- Tier-2 comments for the `\r?$` matching and for case-sensitive grouping.
- Rationale goes in `docs/release-process.md` and `docs/ci-pipeline.md`.

## 5. Order of work

0L → step 0 (merged) → 0f (plan file committed, `main` merged into the stage branch, pushed) → step 1 / HUM-38 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10 → push / 11 → REV-9 → fold-in → merge → HUM-40 dry run (fix cycles if needed) → BPE-14 / HUM-44 → second dry run → cut merge → `main` CI green → HUM-17 tag → automatic release → step 19 checks → step 20 / HUM-41.

## 6. Risks

| ID | Risk | Mitigation / status |
|---|---|---|
| R1 | An unprotected environment silently skips the gate | **Obsolete**: no environment (OQ16a) |
| R2/R14 | Gate check names drift; a skipped required check counts as passing | Gates use `if: always()` and pass only on success; names confirmed in step 11 (HUM-45 if they differ) |
| R3 | The cache save inside the composite action may not run | Check for a cache save in the step 11 logs; fallback: explicit `cache/restore` and `cache/save` steps |
| R4 | First runs of `validate` (tag mode), the version check, `publish` and attestation | The dry run covers everything except tag parsing and `publish`; OQ10 B handles the rest. Expect at least one fix cycle (§13 evidence). |
| R5 | Contract freeze of rows 0–29, the status codes, the JSON limits and notation, at the tag push, with no look at the artifact afterwards | **Accepted by you.** Defects found later can only be fixed by appending rows. |
| R6 | x86 has never been loaded by CANoe; no measurement has ever run | Stated plainly in the verification statement |
| R9 | After the tag, local builds show `0.0.0.N` / `v0.1.0-N-g…` | Harmless; documented |
| R10 | Unattended jobs with write permissions; supply chain | Only `publish` has `contents: write`; only `attest` has `id-token`/`attestations` write; every action in `release.yml` pinned to a commit SHA; `publish` uses only the `gh` CLI. Open follow-up: the `ilammy` Node 20 warning. |
| R12 | The heading date is off if tagging slips by a day | Accepted |
| R13 | Line-ending guard side effects | `json.hpp` exempted; renormalize must stage nothing else; no `charset` in `.editorconfig` |
| R16 | A layout change in `exports.cpp` breaks the script | It fails loudly, never quietly; the fixtures pin its behaviour |
| R18 | A table defect does not block a merge | Accepted. It cannot be published (`publish` needs `export-table`; the dry runs are on the checklist). M1 job-summary message, M2 review rule, M3 PR-body sentence. |
| R19 (new) | Sigstore / attestation service outage | OQ10 A: re-run |
| R20 (new) | The `attest` job can mint OIDC tokens | Separate job with no `contents: write`; GitHub's own action, SHA-pinned |
| R21 (new) | Dry runs record attestations for builds that are never released | Accepted (G10); they are truthful |
| R-T | A mistaken tag publishes within minutes | `validate` (format, on `main`, matching CHANGELOG heading); agents can't create tags |

R8, R11, R15 and R17 are obsolete: there is no DLL-loading tool, no new compiled code, separate per-architecture pipelines, and `dumpbin` is kept.

## 7. Verification and the REV-9 checklist

**Verification:**
- Step 10: local runs on both architectures, reported separately.
- Step 11: CI on both legs plus `export-table`.
- Step 15 and the second dry run (step 17): dry runs.
- Step 19: after publishing.

**REV-9 checklist:**
- **Versions:** no hard-coded version anywhere. `VER_*` comes only from the tag; the version check compares against `X.Y.Z.0` and `vX.Y.Z`.
- **Provision parity:** every one of `ci.yml`'s old steps 2–12 is in `provision` with the same behaviour (three vcpkg traps, baseline check, both cache keys, two-part `/MT` check, lib placement). Trap comments moved, not duplicated.
- **`ci.yml`:** no concurrency block; gate names unchanged, gates use `if: always()` and pass only on success, and do **not** wait for `export-table`.
- **`release.yml` safety:**
  - `publish` runs only on a tag push and has the only `contents: write`;
  - no `environment:`;
  - `publish` needs `assemble` and `attest` (and therefore `export-table`);
  - no rebuild after `assemble`;
  - draft, then verify, then make public as the last command;
  - all six safeguards present;
  - all actions SHA-pinned;
  - `fetch-depth: 0` where `git describe` or the ancestor check needs it.
- **Attestation:** `id-token: write` and `attestations: write` only in `attest`; subjects are the two DLLs; verification runs right after creation.
- **Notes:** `### Development` removed, non-empty check after removal, length check, `\r?$` matching.
- **Script and fixtures:**
  - checks 1–12 as specified, with 7 and 12 in the "as today" form and the project-rule message;
  - case-sensitive whole-string grouping;
  - every check has at least one failing fixture;
  - fixtures run only in `export-table` and are never compiled;
  - M1 present.
- **Export contract:** `exports.cpp` and `exports.def` unchanged; no `LIBRARY` line. **M2:** for any branch touching `exports.cpp`, the `export-table` result is recorded, and red is a Must-fix.
- **Repo files:**
  - `.gitattributes` and `.editorconfig` content exactly as approved; renormalize staged nothing else;
  - `setup-dev-env.ps1` still works in Windows PowerShell 5.1 and warns rather than fails on `pwsh`;
  - rule and skill texts exactly as approved, including the M2 line in `code-reviewer.md` and the corrected `.def` example (`caplDllGetTable4`) in `capl-export-contract`;
  - CHANGELOG entries present, with no stage or task IDs;
  - comment discipline followed.

## 8. Master-plan changes at fold-in (step 13, plus the close-out in step 20)

- **§9 Stage 14:**
  - title becomes "Tag-driven automatic release";
  - BPE-11 text: no gate; draft, then verify, then make public; attestation;
  - "Human approval gate: YES" becomes "human acts: approving the CHANGELOG cut and pushing the tag";
  - BPE-12 by field position;
  - REV-9 checklist per §7.
- **§7.7:**
  - the "approval gate is a GitHub Environment" bullet is removed;
  - "after `main`'s CI is green" is a convention (OQ15b);
  - add the OQ10 policy.
- **§8 Stage 13:** the heading and "CI PENDING" are corrected to CI green on both legs, merged as `8bd35f4`. HUM-36 is recorded at `8bd35f4`, compiled from branch head `9758c04`, whose tree is identical.
- **§5:** add the "rows written out in full" rule (pointer to `capl-export-contract`), the CHANGELOG `### Development` rule (OQ19) with no stage or task IDs anywhere in `CHANGELOG.md` (G1), and M2 as a `code-reviewer` rule (G7).
- **§12:**
  - new: BPE-37 to BPE-40, BPE-42 to BPE-44, TEST-24 (redefined), REV-24;
  - HUM-38, HUM-40, HUM-41, HUM-43, HUM-44, and HUM-45 if used;
  - **HUM-18: dropped by user decision, not done.** HUM-39 and HUM-42 were proposed and never spent, so no entry.
  - CPP-45, TEST-25 and BPE-41 were proposed and dropped; never spent.
- **§13:** R1, R8, R11, R15 and R17 obsolete; add R18 to R21 and R-T.
- **§14:**
  - loose ends 7 and 9: `dumpbin` is now a build step on both architectures;
  - loose end 11: the CHANGELOG cut is done at Stage 14;
  - loose end 14: line endings resolved and the guard added.
- **§15** and the status line updated accordingly.

## 9. Decisions on the gaps

1. **G1, decided (a):** existing internal `[Unreleased]` entries that shipped in v0.1.0 move to `### Development` of `[v0.1.0]`:
   - `auto-pr.yml`;
   - the stage-branch skill;
   - CI internals: the first `ci.yml`, the trigger filter, the action bumps, the caches, `version.lib` in `SYSLIBS`, the libcurl link line.

   Dropped, because they never shipped: the vcpkg `builtin-baseline` fix, the `VCPKG_ROOT` fix, the `restifyGetVersion` → `restifyReadVersion` rename note.

   The main section keeps: `restifyReadVersion`, the function groups folded into a few lines, the `capl/` framework, `setup-dev-env.ps1` as the build entry point, and the libcurl static `/MT` fact.

   No stage or task IDs anywhere in `CHANGELOG.md`, including `### Development`; a bare ID citation is deleted (`project-docs`). Exact text is approved at HUM-44.
2. **G7, decided (b):** M2 goes into `.claude/agents/code-reviewer.md`, item 1 "Export contract" of "What to check": for any change touching `src/module/exports.cpp`, record the result of the `export-table` job (or of `scripts/list-export-table.ps1`) on the reviewed commit; a red result is a Must-fix; if it was not run, say so. It is not added to `capl-export-contract`. In the same BPE-43 commit, the stale example in `capl-export-contract` (lines 61–62, `CAPLDLLEntryPoint`) is corrected to `caplDllGetTable4`, matching `src/module/exports.def`. Exact wording is approved at HUM-43.
3. **G10, decided:** attestation also runs in dry runs (§4).
4. **G11, decided:** the split stands. CHANGELOG section: verification statement, Reference line, contract sentence. Generated by `assemble`: export table, `capl/` sentence, attestation verification line.
5. **Recorded only, not to be fixed now:**
   - The `stage-branch` skill says stage branches don't edit `plan.md`, which conflicts with §7.8's fold-in rule. This plan follows §7.8, as earlier stages did.
   - `project-docs` names "Stage 13" for the CHANGELOG cut; that reference is removed in the same HUM-43 edit.

## 10. Exact texts you will still approve

1. **HUM-38 (step 1):** the design, including the M1 job-summary line, the M3 `auto-pr.yml` sentence, the generated note sentences (`capl/` and attestation verification), the job graph, and permissions.
2. **HUM-43 (step 3):** the `project-docs` line introducing `### Development` (and the removal of the "Stage 13" reference); the M2 line in `code-reviewer.md` item 1 (G7); the corrected `.def` example in `capl-export-contract`.
3. **HUM-44 (step 16):** the whole `[v0.1.0]` CHANGELOG section, including the verification statement, the Reference line with links at the tag, the contract sentence, and the `### Development` content sorted per G1 (§9).
4. **HUM-41 (step 20):** the README install and download section and the Roadmap item text.
5. **HUM-45 (only if needed):** a ruleset change if the gate check names don't match.