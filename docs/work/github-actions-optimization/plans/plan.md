# Plan: release automation and CI consistency

## 1. Goal
The current goal is CI/CD consistency: one CHANGELOG generator and parser in PowerShell instead of inline logic in YAML, scripts used only by CI in `scripts/ci/`, zero Python in the local toolchain, zero secrets and PATs. The target is a zero-touch release: a bot maintains a release PR with a generated CHANGELOG section, the computed version and exactly one `bump:*` label; a human only reviews and squash-merges it (merge = release decision); the tag and the publication happen automatically. Nobody writes release notes by hand: they are generated from commit messages.

## 2. Constraints that apply to every stage
- Version: the only source of the product version is the tag `vX.Y.Z`; no tracked file contains a version number; version fields ≤ 65535. The next version is the highest `vX.Y.Z` tag plus the bump level of the single `bump:*` label on the merged release PR.
- CHANGELOG: the bot generates each release section in the release PR's cut commit. A release section has the heading `## [<12-char hash of the cut's base>] - YYYY-MM-DD`; the heading's link definition is at the bottom of the file. Section order: "Breaking changes", `### Added`, `### Fixed`, `### Development` (stays in the file, excluded from the release notes). There is no `## [Unreleased]`; no stage writes CHANGELOG entries by hand. `CHANGELOG.md` is output only: no workflow decides from its content whether, what or as which version to release; `release.yml` reads it only to locate the notes of a tag.
- Release commit: a first-parent commit on `main` after the highest valid `vX.Y.Z` tag whose subject is exactly `release/next (#N)`, where pull request N, read live through the API, is merged, has `merge_commit_sha` equal to that commit, has base `main` and has head ref `release/next` in this repository. It is faithful when its parent and tree equal the parent and tree of that pull request's head commit (`head.sha`). Only a faithful release commit is tagged; its first parent is then the base named in the heading.
- Change type: taken from the squash-merge commit subject, which is the PR title plus "(#N)"; the PR title is the branch name set by `auto-pr.yml` and nobody edits it. Branch prefix → level: `feat/` and `stage/` minor, `fix/` patch, `chore/` and `docs/` no release; the bot's `release/next` PR no release.
- Breaking change: marked in the commit messages of the PR (`type!:` or a `BREAKING CHANGE:` footer), on any branch type; read through the API from the PR named by "(#N)". A breaking change gives minor while the major version is 0 and major from 1.0.0; 1.0.0 is reached by a deliberate `bump:major` label.
- Repository settings confirmed by the user: required checks `build + test (x64)` and `build + test (x86)` on `main`; "Default commit message for squash merging" = "Pull request title"; labels `bump:major`, `bump:minor`, `bump:patch` exist; "Allow GitHub Actions to create and approve pull requests" is on; automatic deletion of head branches is on; one repository ruleset, targeting only `main`; no classic branch protection rules; no ruleset or protection on tags; no organization rulesets. "Require branches to be up to date before merging" stays off (user decision: the small merge window is accepted; a release PR merged after `main` moved gives a release commit that is not faithful, which is not tagged and is replaced per R17). Not confirmed, to be confirmed before stage 7: within the `main` ruleset, pull request required, required approving reviews, force pushes blocked and deletions restricted; squash merge only. The user adds the `bump label` check to the required checks after stage 6c.
- Facts from the GitHub documentation: events triggered by `GITHUB_TOKEN` do not create new workflow runs, except `workflow_dispatch` and `repository_dispatch`; a dispatch requires the Actions: write permission, granted by an explicit `permissions:` block; a dispatch `ref` may be a branch or a tag. An explicit `permissions:` block works regardless of the default "Workflow permissions" setting (confirmed by this repo's practice: `contents: write` in `release.yml`, `pull-requests: write` in `auto-pr.yml`). Label changes, pushes and tags made by the bot therefore start no workflow run.
- No secrets, no PAT. No Python in the local toolchain; the parser is in PowerShell 7 (`pwsh`), available locally and on runners.
- `scripts/ci/` contains only scripts used solely by CI. Scripts that are local, or used both locally and in CI (`setup-dev-env.ps1`, `list-export-table.ps1`), stay in `scripts/`. Neither directory is a second build system.
- x86 and x64: every verification reports both architectures.
- Export contract (`CAPL_DLL_INFO_LIST4`, `exports.def`) and `/MT`: no stage touches them.
- Each stage = a separate branch from `main`, cut by `build-pipeline-engineer`; the push is done by a human. Documentation changes in the same PR as the behavior it describes. No plan identifiers (e.g. HG-x, R1, stage numbers) in tracked files outside plan documents, except the one-line section breadcrumbs in `docs/<topic>.md` that `project-docs` requires, written as `(decided during github-actions-optimization Stage N)`. Code comments follow the comment discipline in `project-docs`; the plan does not dictate their content.
- Commits only on the user's instruction; the user pushes and merges.
- `docs/ci-flow.md` and `docs/ci-optimized-flow.md` describe an earlier flow (manual cut, `## [Unreleased]`, manual tag push, recognition of a cut by its CHANGELOG heading) until stage 6f; where they disagree with this plan, this plan applies.

## 3. Rule changes

Approved:
- R1. The cut mechanics are performed by a bot; `build-pipeline-engineer` owns the bot (changes the "Who writes what" line in `project-docs`).
- R2. The manual dry run before merge is replaced by the preview in the release PR description plus CI on the same SHA; the manual dry run remains available.
- R3. The release mode depends on the ref, not on the event: a dispatch on a tag publishes just like a tag push; a dispatch on a branch never publishes (new wording of safeguard 4 in `docs/release-process.md`).
- R4. Replaced by R13.
- R5. The branch `release/next` is reserved for the bot; manual commits on it are overwritten on rebuild.
- R6. Withdrawn: release content is generated from commit messages.
- R7. An entry in the Directory layout section of `CLAUDE.md` and in the directory conventions of the `msvc-build-conventions` skill, worded: "`scripts/ci/` — scripts invoked only by GitHub Actions workflows; never a second build system". The existing `scripts/` entry (with the `list-export-table.ps1` example) stays unchanged.
- R8. `scripts/list-export-table.ps1` stays in `scripts/` (also used locally); its path and references do not change.
- R9. Change type and level come from the squash-merge commit subject (branch prefix), as in §2; `feat/` is a new branch prefix.
- R10. Breaking changes are marked in commit messages and read through the API, as in §2; the release PR shows a `> [!CAUTION]` "Breaking Change" block listing them, plus an informational reviewer checklist item.
- R11. Release notes are generated: one entry per commit of type `feat`, `stage` or `fix`, or with a breaking marker, with a link to the PR (#N); a releasing PR without such a commit gets one fallback entry (branch name and #N) and a warning. `feat` and `stage` → `### Added`, `fix` → `### Fixed`, breaking → "Breaking changes"; `chore/` and `docs/` PRs → `### Development`. Commits up to a one-time cut-off SHA in the script count as `### Development` and raise no level.
- R12. A new skill holds the Conventional Commits rules for this repo: the commit type matches the branch prefix (`feat:`, `stage:`, `fix:`, `chore:`, `docs:`); `stage:` has the meaning of `feat:` and is used only for user-visible changes, internal changes on a `stage/` branch use `chore:`; the breaking marker is `type!:` or a `BREAKING CHANGE:` footer; subjects of `feat:`, `stage:` and `fix:` commits are written for DLL and CAPL users, because they become release-note entries. `CLAUDE.md` does not change for this. Git hooks for commit messages are a later topic, not decided.
- R13. Labels `bump:major`, `bump:minor`, `bump:patch` exist (created by the user); the bot never creates labels; `release-pr.yml` has `pull-requests: write`, no `issues: write`. On every push to `main` and every dispatch the bot computes the level from the commits; on every run that is not skipped it reconciles the label: with exactly one `bump:*` label that differs from it, the bot replaces it and comments on the change; with no `bump:*` label, it adds the computed one; with two or more, it changes nothing. A push to `main` is skipped when `release/next` already holds a commit with the same parent and tree as the one it would build and exactly one release PR is open; a skipped run pushes nothing, dispatches nothing and leaves the body, checklist and label as they are; a dispatch is never skipped. The bot keeps no state. A human may change the label before merging (as the last step: the next run that is not skipped resets it). Exactly one `bump:*` label is required; zero or two or more fail.
- R14. `tag-release.yml` reads the labels of the merged release PR live and takes the level from its single `bump:*` label, which must be `bump:major`, `bump:minor` or `bump:patch`; zero, two or more `bump:*` labels, or any other `bump:*` value, end the run red without a tag. The release PR checklist has an informational reviewer item: the label matches the intended version (preview = last tag + level).
- R15. A separate lightweight workflow with one job, `bump label`, is a required check: green outside `release/next`, green on `release/next` only with exactly one `bump:*` label; the result comes from the exit code, never from a job-level `if:`; no path filter. The user adds it to the ruleset once it is on `main`.
- R16. `release-pr.yml`: triggers push to `main`, `workflow_dispatch`, and `pull_request` `labeled`/`unlabeled` limited to head `release/next`, same repository, labels starting with `bump:`. Level "no release" closes the release PR and deletes `release/next` without an error; otherwise the bot generates the section and opens or updates the PR as ready for review (not a draft). An empty generated section ends the run red and leaves the PR unchanged. Non-blocking warnings in the description: a releasing PR without a typed commit, a subject without "(#N)", a `fix/` PR that changed `src/module/exports.cpp`. After every rebuild that is not skipped (R13), including every `workflow_dispatch` that builds `release/next`, the bot dispatches `ci.yml` and the `bump label` workflow on `release/next`. Push runs share one group and never cancel a running run; cancelling is manual only. Label-event runs have their own group, never change labels, post no comment and never push; with exactly one valid `bump:*` label they rewrite only the version preview line of the existing description (checklist ticks kept), otherwise they change nothing and warn. Review applies to the head SHA at merge time; the checklist resets on every rebuild. A release PR closed without merging is replaced on the next push to `main` or manual dispatch.
- R17. Tagging: `tag-release.yml` runs only on push to `main`, without dispatch inputs. A push to `main` whose commit is not a release commit ends green without a tag, with one summary line; a release commit that is not faithful ends green with a warning, without a tag; a squash subject `release/next (#N)` whose pull request N is missing, not merged, has another `merge_commit_sha` or another base, and any API error, end the run red without a tag. A pull request merged as this commit into `main` from a head other than `release/next` in this repository is not a release commit. Only a faithful release commit (§2) is tagged. Immediately before pushing the tag, the tagging job checks live that the release commit is still the `main` tip and does not tag it otherwise; that run ends green with a warning. A `vX.Y.Z` tag already on the release commit is reused, so a re-run only repeats the dispatches. After tagging, the job dispatches `release-pr.yml` on `main` and then `Release` on the tag. If its tagging run fails, "Re-run jobs" of that run repairs it while the release commit is the `main` tip or already carries its tag; until then `release-pr.yml` reports "waiting for the tag" and changes nothing. A release commit that is not faithful, or that is untagged and no longer the `main` tip, is abandoned: it is never tagged, and the next run of `release-pr.yml` generates one section for the full range since the last tag, without the sections of abandoned release commits, and opens a new release PR. No workflow decides from `CHANGELOG.md` content which commit is a cut. The only manual escape hatch is a dispatch of `Release` on an existing tag; a manual tag push is not a documented path. There is no manual cut procedure.
- R18. A defect found before publication is handled like a defect after publication: the tag and the generated section stay, no `[YANKED]` marking, the draft release may be deleted (optional), the fix goes through an ordinary `fix/` PR and the bot releases the next version; a tag name is never reused. The notes of the next version cover only the range from the burned tag.
- R19. The version exists only in the Git tag; the `CLAUDE.md` versioning rule does not change. No exact version can be requested through a PR; 1.0.0 is reached with `bump:major`.
- R20. A release PR is not merged before `tag-release.yml` is on `main`.
- R21. The export table in the release notes (job `assemble`) stays as it is.
- R22. Post-release inspection is optional, not a gate.
- Plan slug: `github-actions-optimization`; this plan replaces the previous content of `docs/work/github-actions-optimization/plans/plan.md`.

Superseded by the above: the stage 4 level rules taken from CHANGELOG headings ("`### Removed` without a label is an error", "major never automatic"); the dispatch of `tag-release.yml` with an optional `version`; "dispatch only when the parent or the tree changed, one concurrency group"; "the release PR skips a merged cut"; the manual cut and manual tag in `docs/release-process.md`; the defect-before-publication case that deleted the tag and cut again; the recognition of a merged cut by its `CHANGELOG.md` heading (the heading-based form of `pending-cut`, the dry-run cut checks) and `check -Mode tag` as the tagging gate; the red state for an untagged cut deeper than the `main` tip and the PR that removes an untagged section; the dry-run messages on whether a cut is merged or released.

Open: none.

## 4. Stages

Stages 0–5, 5a, 6a, 6b, 6c, 6c-fix, 6d, 6e and 7a are done and merged on `main`; their text is kept as the record. Stage 7 has produced its first release (the first release after v0.1.0); its remaining checks and stage 6f are open.

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

### Stage 5a — actionlint
- Description: an `actionlint` job in `ci.yml`: no path filter, `if: github.event_name != 'workflow_dispatch'`, not a required check, pinned version with the download verified by SHA-256, `contents: read`. In that form it must never become a required check (a skipped required check counts as passed). No CHANGELOG entry (commits before the cut-off of stage 6a count as `### Development`).
- Files: `.github/workflows/ci.yml`, `.github/workflows/auto-pr.yml`, `.github/workflows/release.yml`, `scripts/setup-dev-env.ps1`, `README.md`, `docs/development-environment.md`, `docs/ci-pipeline.md`.
- Owners: `build-pipeline-engineer` → `code-reviewer`.
- Verification: `actionlint` green on all workflows; `gate-x86` and `gate-x64` green.
- Approval: no.

### Stage 6a — generated CHANGELOG
- Description: in `scripts/ci/changelog.ps1`: `level` (level from the subjects of `git log <last tag>..<sha>` and from a file of PR commit messages fetched by the workflow; the script stays offline), `generate` (section per R11, heading per §2, link definition), `next-version` taking a level or a label, `cut` removed, `notes` and `check` unchanged, the one-time cut-off SHA (R11; set to the `main` tip the last version of this PR is based on), and the dry-run notice "re-run the failed `tag-release.yml` run" instead of "push the tag". Fixtures: `cut` cases removed and replaced by `generate` cases; `next-version` cases built on headings removed, tag/label/overflow cases adapted; dry-run cases using `[Unreleased]` adapted. `CHANGELOG.md`: remove `## [Unreleased]` with its hand-written `### Development` block and update the introduction; the v0.1.0 section stays. `release.yml`: the dry run computes the level from the commit range and previews the generated section without changing files; the `validate` job gets `pull-requests: read`; tag mode unchanged; the failure message of `publish` no longer says "delete the draft and the tag" (R17, R18). New Conventional Commits skill (R12). `project-docs` CHANGELOG section, `.claude/agents/cpp-implementer.md` (the `[Unreleased]` bullet duty) and `.claude/agents/build-pipeline-engineer.md` (the `[Unreleased]` rename duty) updated to generated sections. `docs/release-process.md` (cutting a release, dry run, failure cases per R17 and R18) and `docs/ci-pipeline.md`.
- Files: `scripts/ci/changelog.ps1`, `tests/changelog/`, `CHANGELOG.md`, `.github/workflows/release.yml`, `.claude/skills/<new conventional-commits skill>/SKILL.md`, `.claude/skills/project-docs/SKILL.md`, `.claude/agents/cpp-implementer.md`, `.claude/agents/build-pipeline-engineer.md`, `docs/release-process.md`, `docs/ci-pipeline.md`.
- Owners: `build-pipeline-engineer` → `code-reviewer`.
- Verification: fixtures green, including the `generate` fixtures, which are the only coverage of a generated section until the first releasing merge; a `Release` dry run on the branch publishes nothing, its notes step passes with the warning that the newest section was already released as v0.1.0, and its preview reports level `none` ("Nothing to release") because every commit on `main` is at or before the cut-off — so no pull request is listed and `pull-requests: read` is not exercised, which stays a Stage 7 first-run fact; `gate-x86` and `gate-x64` green.
- Approval: checkpoint before merge; the user approves the texts of the new skill, the `project-docs` change and the agent files item by item.

### Stage 6b — `feat/` branch prefix
- Description: add `feat/` to the push filters of `auto-pr.yml` and `ci.yml`, and to the `stage-branch` skill (naming table; the "do not invent a fifth" sentence; the line saying `release/*` is absent now names the bot's `release/next`). In `auto-pr.yml` also: remove the checklist item "`CHANGELOG.md` `[Unreleased]` entry present", adjust the plan-only note, add an informational item that commit messages follow the Conventional Commits skill. The `.claude/settings.json` allowlist for `feat/` is changed by the user. `docs/release-process.md`: the manual cut (step 3) and the manual tag push sentences are removed; the document states that no release is made until the release bot and the tagging workflow are on `main` (R17, R20).
- Files: `.github/workflows/auto-pr.yml`, `.github/workflows/ci.yml`, `.claude/skills/stage-branch/SKILL.md`, `.claude/agents/code-reviewer.md`, `docs/release-process.md`, `docs/ci-pipeline.md`; `.claude/settings.json` (user).
- Owners: `build-pipeline-engineer` → `code-reviewer`; user for `.claude/settings.json`.
- Verification: `gate-x86` and `gate-x64` green on the 6b PR; `actionlint` clean; the PR body for `feat/`, `stage/`, `fix/`, `docs/` and for `chore/` with and without a task ID checked by running the extracted step script. That a `feat/` push starts `auto-pr.yml` and `ci.yml` is confirmed by the first `feat/` branch pushed in normal work and recorded then; no branch is pushed only to test it.
- Approval: checkpoint before merge.

### Stage 6c — release PR bot and the `bump label` check
- Description: `release-pr.yml` per R13, R16 and R17 (its part), in one file with two jobs. The `build` job (push to `main`, `workflow_dispatch`) produces the version preview, the generated notes preview, the `> [!CAUTION]` block (R10), the checklist with the reviewer items (R14, R10), and the comment when a push run replaces the label (R13); permissions `contents: write`, `pull-requests: write`, `actions: write`. It checks whether `release/next` exists on the remote with `git ls-remote` on the full ref `refs/heads/release/next`. When the rebuild is not skipped, it dispatches `ci.yml` and the `bump label` workflow on `release/next` after the label step. The `labels` job (label events on the release PR) checks out `main` and takes the labels and the body from one `gh api` read of the PR. With exactly one valid `bump:*` label it runs `changelog.ps1 body-version`, which rewrites only the version preview line of the existing body: checklist ticks are kept, and the checklist resets only on a rebuild. With zero, two or more, or an invalid `bump:*` label it changes nothing and writes a warning. It posts no comment, never changes labels, never pushes, and has its own concurrency group; permissions `contents: read`, `pull-requests: write`. The `bump label` workflow per R15, permissions `contents: read`, `pull-requests: read`; its `gh` lookups keep stderr out of the output they parse (no `2>&1`). The release PR must not be merged before stage 6d is on `main` (R20).
- Files: `.github/workflows/release-pr.yml`, `.github/workflows/bump-label.yml`, `.github/workflows/release.yml` (dry-run preview reads the PR commit messages from a file), `scripts/ci/changelog.ps1`, `tests/changelog/`, `docs/release-process.md`, `docs/ci-pipeline.md`.
- Owners: `build-pipeline-engineer` → `code-reviewer`; the user adds `bump label` to the `main` ruleset after the merge.
- Verification: before merge — review, fixtures, `gate-x86` and `gate-x64` green. After merge — the first push to `main` with releasing commits creates `release/next` and a ready release PR with exactly one `bump:*` label; `build + test (x86)`, `build + test (x64)` and `bump label` appear on its head and satisfy the ruleset; changing the label to two labels turns `bump label` red, back to one turns it green, and with one label the version preview line follows it, checklist ticks kept.
- Approval: checkpoint before merge.

### Stage 6c-fix — content-free release recognition
- Description: in `scripts/ci/changelog.ps1`, the release-commit and faithfulness rules of §2 are written once. The script stays offline: the workflow passes it the pull request data (merge state, `merge_commit_sha`, base, head ref and repository, parent and tree of `head.sha`) as a file. `pending-cut` uses them instead of `CHANGELOG.md` headings: no release commit after the last tag → proceed; a faithful release commit that is the `main` tip and untagged → waiting; a release commit that is not faithful, or untagged and not the `main` tip → abandoned (R17), proceed. `generate` keeps only the release sections already present in `CHANGELOG.md` at the last tag, so the sections of abandoned release commits are dropped. The dry-run cut checks (Test-Cut) and their messages are removed; the dry run keeps a pure preview. The "re-run the failed `tag-release.yml` run" messages are adjusted to R17. `release-pr.yml` fetches the pull request data of the release commits after the last tag and passes it to `pending-cut`. `bump-label.yml` and the code of `release.yml` do not change. `docs/release-process.md`: the squash-merge and first-parent paragraph, the dry-run checks, the untagged-section case, and "validation" of the notes becomes "locating the notes"; `docs/ci-pipeline.md`: a section on the release-commit rule. On a `chore/` branch; level none.
- Files: `scripts/ci/changelog.ps1`, `tests/changelog/`, `.github/workflows/release-pr.yml`, `docs/release-process.md`, `docs/ci-pipeline.md`.
- Owners: `build-pipeline-engineer` → `code-reviewer`.
- Verification: fixtures green, including new cases for a faithful release commit, one that is not faithful, one deeper than the `main` tip, none after the last tag, and dropped sections; a no-op case built from the real v0.1.0 history, where `pending-cut` answers proceed and `generate` drops no section (with nothing to release it reports so and writes nothing); `actionlint` clean; a `Release` dry run on the branch publishes nothing and shows the preview; `gate-x86` and `gate-x64` green. After merge: the next `release-pr.yml` run on `main` ends with level none and opens no release PR.
- Approval: checkpoint before merge (release path).

### Stage 6d — tagging bot
- Description: `tag-release.yml` per R14 and R17. Trigger: push to `main` only, no dispatch inputs. Top-level `permissions: contents: read`.
  - Job `classify`:
    - Permissions: `contents: read`, `pull-requests: read`. No concurrency.
    - Checkout: `fetch-depth: 0` (full history and tags), `persist-credentials: false`.
    - Runs `scripts/ci/fetch-pr-releases.sh`, then a `changelog.ps1` classification of `GITHUB_SHA` under the §2 rules. The first failing check decides:
      1. subject is not `release/next (#N)` → `not-release` (green, one summary line);
      2. PR N is missing, not merged, has another `merge_commit_sha`, or a base other than `main` → red;
      3. head ref is not `release/next`, or the head repository is not this repository → `not-release` (green, one summary line);
      4. not faithful → green with `::warning::`;
      5. faithful, and the highest valid `vX.Y.Z` tag points at the commit → `tagged`;
      6. faithful, otherwise → `tag`.
    - Any API error → red. `pending-cut` does not change.
  - Job `tag` (runs for `tag` and `tagged`):
    - Permissions: `contents: write`, `pull-requests: read`, `actions: write`. No secret or PAT.
    - Concurrency group `tag-release`, `cancel-in-progress: false`.
    - Checkout: full history and tags; credentials kept for the tag push.
    - For `tag`, in this order, unless the highest valid `vX.Y.Z` tag already points at the commit (a re-run after the tag push), in which case the job continues as for `tagged`:
      1. Live check that `main` still points at the commit. If not: green with `::warning::` that the commit is abandoned; no tag, no dispatch.
      2. Live labels: exactly one of `bump:major|minor|patch`, otherwise red without a tag.
      3. Next version.
      4. Annotated tag on `GITHUB_SHA`, then push.
    - For `tagged`: steps 1–4 are skipped and the labels are not read.
    - Then: dispatch `release-pr.yml` on `main`; then dispatch `Release` on the tag, unless a published release exists for the tag. No retries.
  - Documentation:
    - `docs/release-process.md`: re-run while the release commit is the `main` tip or carries its tag; abandonment per R17.
    - `msvc-build-conventions`: "triggered by pushing a tag" becomes "a tag ref from a push or a dispatch".
    - `project-docs`: where the tag goes.
    - `docs/ci-pipeline.md`: the tagging section, and the known limitation narrowed as in the §5 risk "Second merge during tagging".
- Files: `.github/workflows/tag-release.yml`, `.claude/skills/msvc-build-conventions/SKILL.md`, `.claude/skills/project-docs/SKILL.md`, `docs/release-process.md`, `docs/ci-pipeline.md`, `.github/workflows/release-pr.yml` (calls the shared fetch script), `scripts/ci/fetch-pr-releases.sh`, `scripts/ci/changelog.ps1`, `tests/changelog/`. `ci.yml` does not change; no shellcheck.
- Owners: `build-pipeline-engineer` → `code-reviewer`.
- Verification: before merge — review; fixtures green, including the 11 release-commit fixtures (no workflow harness); `actionlint` clean; `gate-x86` and `gate-x64` green. After merge — the `tag-release.yml` run on the 6d merge commit is green with `not-release` and creates no tag; the rest is checked in stage 7.
- Approval: checkpoint before merge.

### Stage 6e — user-facing documentation fixes (first releasable content)
- Description: README wording on how CHANGELOG sections are identified (commit hashes, not tags); `docs/capl-framework.md` setup steps name the GitHub release assets. On `fix/` branches with `fix:` commits, merged after stage 6d: they are the first releasable content, and no release PR may be merged before `tag-release.yml` is on `main` (R20).
- Files: `README.md`, `docs/capl-framework.md`.
- Owners: `docs-writer` (wording approved by the user) → `code-reviewer`.
- Verification: `gate-x86` and `gate-x64` green. The first real generated section is checked in the release PR that `release-pr.yml` opens after stage 6d.
- Approval: the user approves the wording.

### Stage 6f — flow documents
- Description: a new revision of `docs/ci-optimized-flow.md` (base: the committed version) and an update of `docs/ci-flow.md` to the implemented flow. Both describe recognition of a release only by the pull request, its label and the squash subject (§2), with no manual cut and no manual tag push; `CHANGELOG.md` appears only as generated output, and `notes -Mode tag` in `release.yml` only as locating the notes.
- Files: `docs/ci-optimized-flow.md`, `docs/ci-flow.md`.
- Owners: `build-pipeline-engineer` → `code-reviewer`.
- Verification: every claim about existing behavior matches the workflows on `main`; `gate-x86` and `gate-x64` green.
- Approval: the user approves the text.

### Stage 7 — first automated release (the next version after v0.1.0)
- Description: a human reviews the release PR (one label, version preview, generated notes, CAUTION block if any) and squash-merges it; `build-pipeline-engineer` observes.
- Verification: tag on the squash-merge commit; `release.yml` in tag mode passed both pipelines (x86 and x64); the release has 3 assets, a correct `SHA256SUMS` and verified attestations. First-run facts to confirm: `pull-requests: write` adds and removes an existing label, posts a comment and edits the pull request body; `pulls/N/commits` works with pull-request read; `pulls/N/files` is readable by the release bot; a label change on the release PR rewrites only the version preview line and keeps the checklist ticks; the squash subject contains "(#N)", and that of the release PR is exactly `release/next (#N)`; the head commit (`head.sha`) of a merged release PR is still readable after its branch is deleted; dispatched checks on the bot PR satisfy the ruleset; a `pull_request` check lands on the head SHA; the newest run of a same-named check counts; which workflow file version "Re-run jobs" uses; the dispatch of `release.yml` on the tag created seconds earlier by the same job succeeded on the first attempt (if it fails with a missing-ref error, a retry limited to that error is added in a small follow-up).
- Rollback: re-run (R17); a defect is handled per R18.
- Approval: checkpoint before merging the release PR.

### Stage 7a — draft listing delay in `release.yml`
- Description: observed in stage 7 (v0.1.1): "Verify the draft" in job `publish` ran about 1 s after the draft was created and did not find it in the releases list; "Re-run failed jobs" published it. The step reads the list up to 10 times, 3 s apart, only while it finds no draft, and on failure prints the count and each listed draft's `id`, `tag_name` and `name`. "Delete a leftover draft" and "Make the release public" do not change; a retry there is added only once a failure there is observed. Takes effect from the next tag on, because `release.yml` runs from the tag ref. On a `chore/` branch; level none.
- Files: `.github/workflows/release.yml`, `docs/release-process.md`.
- Owners: `build-pipeline-engineer` → `code-reviewer`.
- Verification: `actionlint` clean; `gate-x86` and `gate-x64` green; a `Release` dry run on the branch publishes nothing (`publish` is skipped for a branch ref, so the loop first runs at the next release); the next release publishes without a re-run, and its log shows how many attempts the step needed.
- Approval: checkpoint before merge (release path).

## 5. Risks
- Required checks from a dispatch on the bot's branch: confirmed on an ordinary branch; on the release PR only once the first release PR exists (after stage 6e). If not accepted, the PR cannot be merged (safe).
- A skipped required check counts as passed: `actionlint` is never required; `bump label` decides by exit code, never by `if:`.
- Label override: every run that is not skipped (R13) resets a human-changed label to the computed level and comments on the replacement; an identical push leaves it alone; the label is set as the last step before merging.
- Body race: the `labels` job (own concurrency group) reads the PR body and writes it back with only the version line rewritten; a `build` rebuild or a reviewer's checklist tick in that seconds-wide window is overwritten by the stale body (a lost tick must be re-ticked; a stale body can carry old ticks onto a new head) until the next rebuild that is not skipped, or at once by a manual dispatch. Accepted, same class as "one change behind; the next rebuild corrects it" in `docs/ci-pipeline.md`; a fix (e.g. re-checking the head SHA before editing) is not planned unless stage 7 shows the race in practice.
- Notes quality depends on commit messages; the skill and the warnings mitigate; the export table in the notes lists the CAPL functions.
- "(#N)" missing from a subject: the type still comes from the branch prefix, the breaking marker is not read; a warning shows it.
- Re-run may use the original workflow file (unverified until stage 7). A defect in `tag-release.yml` itself is then fixed by an ordinary PR; its merge moves `main`, so the untagged release commit is abandoned (R17) and the next release PR covers the full range again.
- A burned version leaves a gap in the numbering, and its changes appear in no public release notes (R18).
- CI cost: every rebuild of `release/next` runs CI on x86 and x64.
- A rebase merge of the release PR produces no release commit: nothing is tagged, and the next release PR generates the section again for the full range; recommended repo setting "squash only" (does not block the plan).
- Manual commits on `release/next` disappear on rebuild (R5); the PR description says so.
- A failed tagging run that is not re-run before the next push to `main` abandons a good release commit: one more review and squash-merge of the replacement release PR; no version is burned.
- Second merge during tagging: if `main` moves before the live check, the release commit is abandoned (green with a warning, no tag) and the `release-pr.yml` run of the second push opens a release PR for the full range; nothing is released twice. If it moves after the check but before the tag push, that run may list changes that are already released until the `release-pr.yml` dispatch after the tag rebuilds the PR. A failed dispatch is not retried; "Re-run jobs" repeats it.
- Export contract, `/MT`, x86/x64 parity: no impact.

## 6. Open questions
None. Repository settings not yet confirmed are listed in §2 and are confirmed before stage 7.
