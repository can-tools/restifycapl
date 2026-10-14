# Plan: release automation and CI consistency

## 1. Goal
The current goal is CI/CD consistency: one CHANGELOG parser in PowerShell instead of inline logic in YAML, scripts used only by CI in `scripts/ci/`, zero Python in the local toolchain, zero secrets and PATs. The direction is a zero-touch release: a bot maintains a release PR with a ready cut and a computed version, a human only verifies and merges (merge = release decision), and the tag and publication happen automatically.

## 2. Constraints that apply to every stage
- Version: the only source of the product version is the tag `vX.Y.Z`; no tracked file contains a version number; version fields ≤ 65535.
- CHANGELOG: a release section has the heading `## [<12-char hash of the cut's base>] - YYYY-MM-DD`; the tag sits on the cut's squash-merge commit, whose first parent is the base named in the heading; links in the section are pinned to the full 40-character hash, with the heading's link definition at the bottom of the file; `### Development` stays in the file but does not go into the release notes. Every stage adds its internal changes (CI, tooling, repo) under `### Development` in `[Unreleased]`. This plan does not change that format.
- `main` is protected by a ruleset: pull request required, required checks `build + test (x64)` and `build + test (x86)`, branch up to date before merging, force pushes blocked, deletions restricted. There is no ruleset on tags; there are no organization rulesets.
- Facts from the GitHub documentation: events triggered by `GITHUB_TOKEN` do not create new workflow runs, except `workflow_dispatch` and `repository_dispatch`; a dispatch requires the Actions: write permission, granted by an explicit `permissions:` block; a dispatch `ref` may be a branch or a tag. An explicit `permissions:` block works regardless of the default "Workflow permissions" setting (confirmed by this repo's practice: `contents: write` in `release.yml`, `pull-requests: write` in `auto-pr.yml`).
- No secrets, no PAT. No Python in the local toolchain; the parser is in PowerShell 7 (`pwsh`), available locally and on runners.
- `scripts/ci/` contains only scripts used solely by CI. Scripts that are local, or used both locally and in CI (`setup-dev-env.ps1`, `list-export-table.ps1`), stay in `scripts/`. Neither directory is a second build system.
- x86 and x64: every verification reports both architectures.
- Export contract (`CAPL_DLL_INFO_LIST4`, `exports.def`) and `/MT`: no stage touches them.
- Each stage = a separate branch from `main`, cut by `build-pipeline-engineer`; the push is done by a human. Documentation changes in the same PR as the behavior it describes. No plan identifiers (e.g. HG-x, R1, stage numbers) in tracked files outside plan documents, except the one-line section breadcrumbs in `docs/<topic>.md` that `project-docs` requires, written as `(decided during github-actions-optimization Stage N)`. Code comments follow the comment discipline in `project-docs`; the plan does not dictate their content.

## 3. Rule changes

Approved:
- R1. The cut mechanics are performed by a bot; `build-pipeline-engineer` owns the bot (changes the "Who writes what" line in `project-docs`).
- R2. The manual dry run before merge is replaced by the preview in the release PR description plus CI on the same SHA; the manual dry run remains available.
- R3. The release mode depends on the ref, not on the event: a dispatch on a tag publishes just like a tag push; a dispatch on a branch never publishes (new wording of safeguard 4 in `docs/release-process.md`).
- R4. The labels `bump:major` / `bump:minor` / `bump:patch` on the release PR override the computed bump level (a level, not a version number).
- R5. The branch `release/next` is reserved for the bot; manual commits on it are overwritten on rebuild.
- R6. `[Unreleased]` on `main` is the only place where release content is edited.
- R7. An entry in the Directory layout section of `CLAUDE.md` and in the directory conventions of the `msvc-build-conventions` skill, worded: "`scripts/ci/` — scripts invoked only by GitHub Actions workflows; never a second build system". The existing `scripts/` entry (with the `list-export-table.ps1` example) stays unchanged.
- R8. `scripts/list-export-table.ps1` stays in `scripts/` (also used locally); its path and references do not change.
- Release PR rebuild trigger: every push to `main`, plus `workflow_dispatch`.
- Parser language: PowerShell.
- Plan slug: `github-actions-optimization`; this plan replaces the previous content of `docs/work/github-actions-optimization/plans/plan.md`.

Open: none.

## 4. Stages

### Stage 0 — baseline
- Description: confirm that the revert of abf5d6a is complete (13 files in their pre-commit state); remove untracked leftovers in the working tree (`scripts/cut-changelog.ps1`, `scripts/cut-changelog.py`, `scripts/__pycache__/`); report discrepancies between `docs/ci-flow.md` / `docs/ci-optimized-flow.md` and reality (fixes go into the stage that changes the behavior in question).
- Files: working tree; no changes to tracked logic.
- Owner: `build-pipeline-engineer` (git operations — human).
- Verification: `gate-x86` and `gate-x64` green on `main`.
- Approval: no.

### Stage 1 — persist the plan
- Description: plan-writer saves this plan verbatim to `docs/work/github-actions-optimization/plans/plan.md`; a plan-only PR.
- Owners: plan-writer; branch cut by `build-pipeline-engineer`.
- Verification: CI green (x86 and x64).
- Approval: yes (plan approval).

### Stage 2 — checklist in `auto-pr.yml`
- Description: below the general line `- [ ] Human gate passed against the PR CI artifact, if this branch has one` (unchanged), add `- [ ] CANoe check: the CAPL nodes in capl/ compile in CANoe against the DLL from this PR's CI artifact (author before Ready for review, or reviewer before approval)`, printed only when the branch touches `src/module/exports.cpp`; remove from the `exports.cpp` warning block the clause that duplicates verification against the CI artifact.
- Files: `.github/workflows/auto-pr.yml`, `CHANGELOG.md` (`### Development`).
- Owners: `build-pipeline-engineer` → `code-reviewer`.
- Verification: CI green on x86 and x64; the generated PR body is reviewed for both cases — a branch touching `src/module/exports.cpp` (CANoe check line present) and a branch not touching it (line absent).
- Approval: no.

### Stage 3 — `scripts/ci/` and a single CHANGELOG parser
- Description: a new `scripts/ci/changelog.ps1` (the first file in `scripts/ci/`) with `notes` and `check` subcommands, replacing the inline Python in the `validate` job of `release.yml` with identical behavior (same conditions, messages and notes output). Fixtures in `tests/changelog/` (modeled on `tests/export-table/`), including the real v0.1.0 cut; run in CI, and `release.yml` runs them before parsing. The one-line `python3` that counts the notes' characters is replaced by a Python-free solution. The R7 entry in `CLAUDE.md` and in the `msvc-build-conventions` skill lands in the same PR. The `build-pipeline-engineer` agent already owns `scripts/`; its file changes only if `scripts/ci/` would otherwise not be unambiguously in its scope.
- Files: `scripts/ci/changelog.ps1`, `tests/changelog/`, `.github/workflows/release.yml`, `.github/workflows/ci.yml` (fixtures job, not a required check), `CLAUDE.md`, `.claude/skills/msvc-build-conventions/SKILL.md`, `docs/release-process.md`, `docs/ci-pipeline.md`, `CHANGELOG.md` (`### Development`).
- Owners: `build-pipeline-engineer` → `code-reviewer`.
- Verification: fixtures green; side-by-side comparison of the old and new parser's results on the same inputs; `release.yml` dry run on the branch passes with both pipelines (x86 and x64); `gate-x86` and `gate-x64` green.
- Approval: checkpoint before merge (release path, `CLAUDE.md` and skill).

### Stage 4 — cut and version computation
- Description: `cut` and `next-version` subcommands in `scripts/ci/changelog.ps1`, with fixtures. `cut` is deterministic (same input → identical output). `next-version` rules (the same at 0.x and from 1.0.0): `### Added`, `### Changed` or `### Deprecated` → minor; `### Removed` without a `bump:*` label → error; anything else → patch; major never automatic; a `bump:*` label overrides the level. A section left with only `### Development` is not a release. No workflow uses these subcommands yet.
- Files: `scripts/ci/changelog.ps1`, `tests/changelog/`, `CHANGELOG.md` (`### Development`).
- Owners: `build-pipeline-engineer` → `code-reviewer`.
- Verification: fixtures green; `gate-x86` and `gate-x64` green.
- Approval: the computation rules are approved by this plan.

### Stage 5 — inputs for the automation
- Description: `ci.yml` gets a `workflow_dispatch` trigger (the bot uses it to produce the required checks on its branch); `release.yml` selects the mode by ref (R3: tag ref → tag mode, branch ref → dry run); the dry run shows the computed version.
- Files: `.github/workflows/ci.yml`, `.github/workflows/release.yml`, `docs/release-process.md` (new wording of safeguard 4), `docs/ci-pipeline.md`, `CHANGELOG.md` (`### Development`).
- Owners: `build-pipeline-engineer` → `code-reviewer`.
- Verification: the dry run on a branch shows the version and does not publish; a manual dispatch of `ci.yml` on a branch produces the `build + test (x86)` and `build + test (x64)` checks attached to the branch tip's SHA.
- Approval: checkpoint before merge.

### Stage 6 — release PR and tag (one PR; the two parts only work together)
- Description:
  - `release-pr.yml` (trigger: `push: main` plus `workflow_dispatch`): skips a commit that is itself a merged cut; with no releasable content, closes the release PR and deletes the branch; otherwise builds `release/next` = `main` tip + a commit with the cut (`changelog.ps1 cut`), force-pushing only on change; creates or updates the PR with a description: computed version, notes preview, a checklist (CHANGELOG diff, version, green x86 and x64 checks), a note about R5 and R6; runs `gh workflow run ci.yml --ref release/next`; `concurrency` cancelling older rebuilds.
  - `tag-release.yml` (trigger: `push: main` plus `workflow_dispatch` with an optional `version` as retry or override): detects a merged, untagged cut (heading = first parent); reads the merged PR's `bump:*` labels (read failure = no tag and a red run); computes the version, creates an annotated tag on `GITHUB_SHA` and runs `gh workflow run release.yml --ref vX.Y.Z`.
  - Permissions: `contents: write`, `pull-requests: write`, `actions: write`. No secrets.
- Files: `.github/workflows/release-pr.yml`, `.github/workflows/tag-release.yml`, `docs/release-process.md` (new flow and fallbacks: dispatch with `version`, manual tag push), `docs/ci-pipeline.md`, `docs/ci-flow.md`, `docs/ci-optimized-flow.md` (rewritten to this plan's target state; content this plan does not implement — vcpkg `x-gha` caching, standalone GNU Make, bot auto-merge, the earlier script-based CHANGELOG automation — is removed), `.claude/skills/project-docs/SKILL.md` (R1), `CHANGELOG.md` (`### Development`).
- Owners: `build-pipeline-engineer` → `code-reviewer`.
- Verification: before merge — scripts on fixtures, review, `gate-x86` and `gate-x64` green. After merge (dispatched workflows must be on `main`) — the first push to `main` with content in `[Unreleased]` creates `release/next` and a PR whose heading hash equals the `main` tip; the x86 and x64 checks appear on the PR and satisfy the ruleset.
- Approval: checkpoint before merge.

### Stage 7 — first automated release (the next version after v0.1.0)
- Description: a human verifies the release PR and squash-merges it; `build-pipeline-engineer` observes.
- Verification: tag on the squash-merge commit; `release.yml` in tag mode passed both pipelines (x86 and x64); the release has 3 assets, a correct `SHA256SUMS` and verified attestations.
- Rollback: draft-first and paths A/B in `docs/release-process.md`.
- Approval: checkpoint before merging the release PR.

## 5. Risks
- Required checks from a dispatch: checks from a dispatch on the bot's branch are expected to count as required (attached to the SHA, matched by name). Stage 5 confirms this; if not, the effect is safe — the PR cannot be merged.
- CI cost: every merge to `main` triggers an additional full CI run on `release/next`.
- A miscategorized CHANGELOG entry → wrong version. Safety nets: preview in the PR description, `bump:*` labels. A published tag must never be moved; the fix is the next release.
- Rewriting the parser on the release path (stage 3): identical behavior is required, with a side-by-side comparison of the old and new parser's results.
- A rebase merge breaks the first-parent rule; recommended repo setting "squash only" (does not block the plan).
- Manual commits on `release/next` disappear on rebuild (R5); this must be stated in the PR description.
- Export contract, `/MT`, x86/x64 parity: no impact.

## 6. Open questions
None. Non-blocking repo settings to consider: "squash only", number of required PR approvals.
