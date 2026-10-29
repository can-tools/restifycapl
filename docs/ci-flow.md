## Purpose

This file describes the CI/CD flow of the repository as it runs on `main`.

The flow is made of seven workflows in `.github/workflows/` and two composite actions in `.github/actions/`.

| File | `name:` | What it does |
|---|---|---|
| `ci.yml` | CI | Builds and tests the DLL on x86 and x64, and runs the lint and check jobs. |
| `arch-pipeline.yml` | Architecture pipeline | Reusable workflow that builds and tests one architecture. `ci.yml` and `release.yml` call it. |
| `auto-pr.yml` | Auto PR | Opens a draft pull request for a working branch. |
| `bump-label.yml` | Bump label | Its check `bump label` requires exactly one `bump:*` label on the `release/next` pull request. |
| `release-pr.yml` | Release PR | The release bot. Builds the `release/next` branch and its pull request, and keeps the `bump:*` label and the version preview in the pull request body up to date. |
| `tag-release.yml` | Tag Release | The tagging bot. Tags a merged release commit on `main` and dispatches the release. |
| `release.yml` | Release | Checks the tag, builds both DLLs, packs them and publishes the GitHub release. |
| `.github/actions/export-table/action.yml` | export-table | Composite action. Runs the export-table fixtures, checks `src/module/exports.cpp` and writes the job summary. `ci.yml` and `release.yml` use it. |
| `.github/actions/provision/action.yml` | Provision build environment | Composite action. Activates MSVC, provisions the pinned vcpkg tool and the static dependencies, and places them where the Makefile expects them. `arch-pipeline.yml` uses it. |

## What starts what

### The platform rule

GitHub has a rule that is not visible in any file of this repository. It is a GitHub platform rule:

- Events created with `GITHUB_TOKEN` (a push, a tag, a label change, a pull request) start no workflow runs. The only exceptions are `workflow_dispatch` and `repository_dispatch`.
- A dispatch needs the `Actions: write` permission, which a job gets from an explicit `permissions:` block.
- The `ref` of a dispatch can be a branch or a tag.
- An explicit `permissions:` block works whatever the repository default for workflow permissions is.
- A `pull_request` trigger without `types:` uses the GitHub defaults: opened, synchronize and reopened.

Every workflow here that pushes, labels or opens something as the bot depends on this rule. The text below says so where it applies.

### Triggers of each workflow

| Workflow | Events | Filters | Concurrency |
|---|---|---|---|
| `ci.yml` | `push`, `pull_request`, `workflow_dispatch` | Push: branches `main`, `feat/**`, `stage/**`, `chore/**`, `fix/**`, `docs/**`. Pull request: base branch `main`, default types. Dispatch: any ref, no inputs. No path filters. | None |
| `auto-pr.yml` | `push` | Branches `feat/**`, `stage/**`, `chore/**`, `fix/**`, `docs/**`. No path filters. | None |
| `bump-label.yml` | `pull_request`, `workflow_dispatch` | Pull request types: opened, synchronize, reopened, labeled, unlabeled. No base-branch filter. Dispatch: no inputs. | Group `bump-label-labels-<pull request number>` for `labeled` and `unlabeled` events (an older run is not cancelled); group `bump-label-refs-<head branch or ref>` for the other events (an older run is cancelled). |
| `release-pr.yml` | `push`, `workflow_dispatch`, `pull_request` | Push: branch `main`. Dispatch: no inputs. Pull request: base `main`, types labeled and unlabeled. | Job `build`: group `release-pr-build`. Job `labels`: group `release-pr-labels`. Neither cancels. |
| `tag-release.yml` | `push` | Branch `main`. No dispatch, no path filter. | Job `tag`: group `tag-release`, no cancelling. |
| `release.yml` | `push`, `workflow_dispatch` | Push: tags `v*.*.*`. Dispatch: no inputs. | Group `release-<ref>`, no cancelling. |
| `arch-pipeline.yml` | `workflow_call` only | Inputs: `arch` (required, x86 or x64) and `release-version` (optional; empty means a development build). | None |

The two composite actions have no events of their own. They run only as steps of other jobs.

Some jobs inside these workflows run only for part of the events:

- In `ci.yml`, the job `actionlint` is skipped on a dispatch. The other jobs run.
- In `release-pr.yml`, the job `build` runs when the event is not `pull_request`, so on a push or a dispatch. The job `labels` runs only for a `pull_request` event whose head is `release/next`, from this repository, with a label whose name starts with `bump:`.
- In `tag-release.yml`, the job `classify` runs on every push to `main`. The job `tag` runs only when `classify` answered `tag` or `tagged`.
- In `release.yml`, the mode is `tag` on a tag ref and `dry-run` on any other ref. The job `publish` runs only on a tag ref.
- `bump-label.yml` does nothing and succeeds when the head branch is not `release/next`.

The jobs of `ci.yml` are `pipeline-x86` and `pipeline-x64` (both call `arch-pipeline.yml`), `gate-x86` (check `build + test (x86)`), `gate-x64` (check `build + test (x64)`), `export-table`, `changelog-fixtures` and `actionlint`.

### Dispatches made by the bots

The bots start other workflows with `gh workflow run`. They authenticate with the built-in `GITHUB_TOKEN`, not with a personal access token. A dispatch is the one way for them to start a run, as a consequence of the platform rule. No file in the repository uses `repository_dispatch`.

| Who dispatches (workflow / job / step) | What it starts | Ref | When |
|---|---|---|---|
| `release-pr.yml` / `build` / "Dispatch CI and the bump label check on release/next" | `ci.yml` | `release/next` | When `release-pr.yml` rebuilt the branch (`steps.build.outputs.skip == 'false'`) |
| `release-pr.yml` / `build` / "Dispatch CI and the bump label check on release/next" | `bump-label.yml` | `release/next` | Same step and same condition as the row above |
| `tag-release.yml` / `tag` / "Dispatch the release pull request workflow on main" | `release-pr.yml` | `main` | When `steps.tip.outputs.proceed != 'false'` |
| `tag-release.yml` / `tag` / "Dispatch the release on the tag" | `release.yml` | The release tag (`vX.Y.Z`) | When `steps.tip.outputs.proceed != 'false'` and no published release exists for that tag |

The first step starts two workflows, `ci.yml` and `bump-label.yml`, in a loop.

The job `build` of `release-pr.yml` has `contents: write`, `pull-requests: write` and `actions: write`. The job `tag` of `tag-release.yml` has `contents: write`, `pull-requests: read` and `actions: write`. The `actions: write` permission is what allows the dispatch.

### What starts CI on `release/next`

- `release-pr.yml` pushes `release/next` with `git push --force-with-lease`. As a consequence of the platform rule, that push starts no run.
- `release/next` is not in the push filter of `ci.yml`.
- The bot opens the release pull request with `gh pr create --base main --head release/next --title 'release/next'`. As a consequence of the platform rule, this pull request does not start a `pull_request` run of `ci.yml`.
- So the only thing that starts `ci.yml` on `release/next` is the dispatch in the first row of the dispatch table. It runs the jobs of `ci.yml` on the tip of `release/next`. This gives the check runs `build + test (x86)` and `build + test (x64)` (the job names in `ci.yml`) on that branch.
- `bump-label.yml` starts on a `pull_request` event (opened, synchronize, reopened, labeled, unlabeled) and on a dispatch. The bot dispatches it on `release/next` after it rebuilds the branch.
- A `bump:*` label change on the `release/next` pull request starts the job `labels` of `release-pr.yml`.
- `tag-release.yml` pushes the release tag itself. As a consequence of the platform rule, that push starts no run. This is why its last step dispatches `release.yml` on the tag.

### What starts CI on a working branch

A pull request opened by Auto PR does not start a `pull_request` run of CI (a consequence of the platform rule). The first CI run of a working branch comes from the developer's push, which matches the push filter of `ci.yml`. Each later push to a branch with an open pull request starts both a `push` run and a `pull_request` run: `ci.yml` also starts on `pull_request` with base `main` and the default types (this follows from the GitHub rule that a push to the head branch of an open pull request is a `synchronize` event).

## Branches, pull request titles and release levels

### Working branches

Five branch prefixes are working branches: `feat/`, `stage/`, `chore/`, `fix/` and `docs/`.

- A push to such a branch starts `ci.yml` and `auto-pr.yml`. Both use the same five prefixes.
- A push to `main` also starts `ci.yml`, but not `auto-pr.yml`.
- `auto-pr.yml` opens a draft pull request with base `main`. Its title is the branch name (`--title "${BRANCH}"`, where `BRANCH` is the branch name).
- Guard 1 ("Guard 1: branch ahead of main?") skips the creation when the branch has no commits ahead of `main`.
- Guard 2 ("Guard 2: PR already open for this branch?") skips the creation when an open pull request for the branch already exists. This is why a second push does nothing.
- No workflow changes the title afterwards.

### Squash-merge subject

`scripts/ci/changelog.ps1` assumes that every commit on `main` is a squash-merge commit whose subject is the pull request title plus ` (#N)`, where `N` is the pull request number. Whether this holds depends on the repository's squash-merge setting; no file in the repository enforces it. `scripts/ci/changelog.ps1` reads the pull request number from the trailing ` (#N)` of the subject.

### Release levels

`scripts/ci/changelog.ps1` takes the type of a change from the branch prefix at the start of the squash subject. A subject that starts with none of these prefixes, and not with `release/`, has the type `unknown`.

| Prefix | Level |
|---|---|
| `feat/` | minor |
| `stage/` | minor |
| `fix/` | patch |
| `chore/` | no release |
| `docs/` | no release |
| unknown | no release |

- The level is the highest of the changes since the last tag: minor if there is any `feat/` or `stage/` change, else patch if there is any `fix/` change, else none.
- The level comes from the branch prefix, not from the commit types. A `feat/`, `stage/` or `fix/` pull request that has only `chore:` commits still releases. It is listed by its branch name, with a warning.
- The bot computes the level with `changelog.ps1 level`. If it is major, minor or patch, the job rebuilds `release/next` and opens or updates the release pull request. If it is none, the job closes every open release pull request with a comment and deletes `release/next` if the branch exists.
- The bot labels the release pull request `bump:<level>`. A person can change the label, but the next rebuild that is not skipped puts the computed label back and comments. The `bump label` check and the tagging require exactly one `bump:*` label; the tagging also requires it to be `bump:major`, `bump:minor` or `bump:patch`.
- A change of type `unknown` raises no level of its own. The generator lists it under Development and gives a warning.

### Reserved branch `release/next`

- `release/next` belongs to the release bot. The bot creates the branch and its pull request, whose title is `release/next`.
- Commits pushed to `release/next` by hand are overwritten the next time the bot rebuilds the branch.
- The squash commit of that pull request has the subject `release/next (#N)`. The generator gives it the type `release` and ignores it when it computes the level and the entries. The release pull request itself gives no release level.
- Neither `ci.yml` nor `auto-pr.yml` lists `release/` in its push filter.

### Breaking changes

- A breaking change is marked in a commit message of the pull request by `type!:` or a `BREAKING CHANGE:` footer. This works on any branch type.
- The marker is read from the commit messages of the pull request named by ` (#N)` in the squash subject. A subject without ` (#N)` is not read, and the generator gives a warning.
- A breaking change gives minor while the major version of the last tag is 0, and major from 1.0.0.
- While the major version of the last tag is 0, the level is never major; version 1.0.0 is reached only with the `bump:major` label.
- A breaking change is listed under "Breaking changes" in the release notes. A `chore/`, `docs/` or `unknown` pull request with a breaking marker is not listed twice under Development.

## Auto PR (`auto-pr.yml`)

`auto-pr.yml` has the `name:` Auto PR. It opens a draft pull request for a pushed working branch when the branch is ahead of `main` and has no open pull request.

### Trigger and permissions

- The only event is `push`, for the branches `feat/**`, `stage/**`, `chore/**`, `fix/**` and `docs/**`. There is no path filter, no tag filter and no other event.
- The workflow has no concurrency group.
- The workflow has `contents: read` and `pull-requests: write`. Its one job, `open draft PR`, runs on `ubuntu-latest` with `GH_TOKEN` set to `secrets.GITHUB_TOKEN`. The file's header comment says that the workflow requires the repository setting "Allow GitHub Actions to create and approve pull requests" to be on.
- The job also sets `GH_REPO` to `github.repository` and `BRANCH` to `github.ref_name`.
- The job has four steps, in this order: "Guard 1: branch ahead of main?", "Guard 2: PR already open for this branch?", "Compose PR body" and "Create draft PR".

### Step "Guard 1: branch ahead of main?"

- The step has the id `ahead` and no `if:`.
- It calls `gh api "repos/${GH_REPO}/compare/main...${BRANCH}"` and writes the whole response to `$RUNNER_TEMP/compare.json`. "Compose PR body" reads this file instead of calling the API again.
- It reads `.ahead_by` from the file and prints "Branch '<branch>' is ahead of main by N commit(s)."
- If `ahead_by` is 0, it prints "No commits ahead of main -- nothing to open a PR for. Exiting." and writes `proceed=false` to `$GITHUB_OUTPUT`. Otherwise it writes `proceed=true`. In both cases the step succeeds.
- The file gives the reason for the guard: `gh pr create` fails with "No commits between main and <branch>" on a branch that was just cut, so the workflow skips the creation instead.
- The script runs with `set -euo pipefail`. If the API call or `jq` fails, the step fails, the later steps are skipped and no pull request is created.

### Step "Guard 2: PR already open for this branch?"

- The step has the id `existing` and runs only if `steps.ahead.outputs.proceed == 'true'`.
- It runs `gh pr list --head "${BRANCH}" --state open --json number --jq '.[0].number // empty'`. Only an open pull request with this head branch counts.
- If a number is returned, it prints "Open PR #N already exists for '<branch>' -- nothing to do. Exiting." and writes `proceed=false`. Otherwise it prints "No open PR found for '<branch>' -- will create one." and writes `proceed=true`. In both cases the step succeeds.
- The file says why the guard exists: the workflow runs on every push, and only the first push that carries a commit should create a pull request.

### Step "Compose PR body"

- The step runs only if `steps.ahead.outputs.proceed == 'true' && steps.existing.outputs.proceed == 'true'`. It writes `$RUNNER_TEMP/pr-body.md` and has no output.
- Its inputs are `compare.json` and the `BRANCH` variable.
- The body is composed once, when the pull request is created; later pushes do not change it.
- If the step fails, "Create draft PR" is skipped.

#### What the step reads from the compare file

| Value | Rule |
|---|---|
| `plan_only` | True when the compare lists at least one file and every file name matches `^docs/work/[^/]+/plans/.*\.md$`. False when the list of files is empty. |
| `exports_touched` | True when one of the files is `src/module/exports.cpp`. |
| Commit subjects | The first line of the message of each commit in the compare, one per line, in `$RUNNER_TEMP/subjects.txt`. |

#### The TODO line

The last line of the checklist depends on the branch prefix. `--` is two ASCII hyphens.

| Branch | TODO line |
|---|---|
| `stage/<N>-...`, where `<N>` is digits or two groups of digits joined by `-` | `Stage <N> -- TODO: task IDs (e.g. CPP/TEST/REV/HUM)` |
| Any other `stage/` branch, for example `stage/7a-x` | `Stage stage/7a-x -- TODO: task IDs (e.g. CPP/TEST/REV/HUM)`: the whole branch name, with `stage/`, takes the place of `<N>` |
| `chore/<id>-...`, where `<id>` is lower-case letters, `-` and digits, optionally followed by more `-<digits>` groups, for example `chore/rev-12-...` | `<ID> -- TODO: any additional REV/HUM IDs`, with `<ID>` in upper case |
| Any other `chore/` branch, for example `chore/ci-verify-draft-retry` | `TODO: stage and task IDs` |
| `feat/`, `fix/` and `docs/` | `TODO: stage and task IDs` |

#### The body

The body is built in this order.

1. The heading `## Merge checklist` and a blank line.
2. The plan-only paragraph, only if `plan_only` is true.
3. The first block of three items, always.
4. The CANoe item, only if `exports_touched` is true.
5. The second block of three items, always.
6. A blank line and the TODO line, always.
7. A separator and the exports.cpp warning, only if `exports_touched` is true.
8. A separator, the heading `### Commits on this branch` and one `- <subject>` line per commit, only if the list of subjects is not empty. The subjects are written with `printf -- '- %s\n'`.

The text that the file writes for the plan-only paragraph (item 2):

```
Plan-only branch: the `code-reviewer` and human-gate items below do not
apply. CI green and up-to-date-with-`main` still apply.
```

The text that the file writes for the first block (item 3):

```
- [ ] CI green on both legs (`build + test (x86)`, `build + test (x64)`)
- [ ] `code-reviewer` clean on this branch (`git diff main...HEAD`), zero Must-fix
- [ ] Human gate passed against the PR CI artifact, if this branch has one
```

The text that the file writes for the CANoe item (item 4):

```
- [ ] CANoe check: the CAPL nodes in capl/ compile in CANoe against the DLL from this PR's CI artifact (author before Ready for review, or reviewer before approval)
```

The text that the file writes for the second block (item 5):

```
- [ ] Commit messages follow the Conventional Commits skill
- [ ] Branch up to date with `main`
- [ ] Plan folded into `plan.md`, incl. any task-ID/section renumbering (last commit before marking Ready for review)
```

The text that the file writes for the exports.cpp warning (item 7), after a blank line, a line `---` and a blank line:

```
⚠️ This branch touches `src/module/exports.cpp`. Never have two open PRs that
both touch this file. Also confirm the `export-table` check is green: it is
not a required check, so a red result does not block the merge.
```

### Step "Create draft PR"

- The step runs under the same condition as "Compose PR body": both guards answered `true`.
- It runs `gh pr create --draft --base main --head "${BRANCH}" --title "${BRANCH}" --body-file "$RUNNER_TEMP/pr-body.md"`. The pull request is a draft, its base is `main`, its head is the pushed branch, its title is the branch name and its body is the composed file.
- The step has no `|| true`. If the creation fails, the step and the job fail, and no pull request exists.

### Later pushes and failures

- Second push to a branch whose pull request is open: Guard 1 passes, Guard 2 finds the open pull request and answers `false`, and "Compose PR body" and "Create draft PR" are skipped. The title, the body and the draft state stay as they are.
- If the branch's pull request was closed, the next push to the branch opens a new one, provided the branch is still ahead of `main`.
- If the creation fails, no pull request exists, and the next push to the branch tries again with a newly composed body.
- The workflow does not change the title, the body or the draft state.

## CI (`ci.yml`)

### Permissions

- The workflow has `permissions: contents: read`.
- `export-table`, `changelog-fixtures` and `actionlint` each have their own block, `permissions: contents: read`.
- `pipeline-x86`, `pipeline-x64`, `gate-x86` and `gate-x64` have no `permissions:` block of their own.
- The only third-party action in `ci.yml` is `actions/checkout`, used in three jobs and pinned by SHA with the label `# v7.0.1`.

### Job dependencies

- The only `needs:` in the file are those of the two gates; each gate waits only for its own pipeline. No job has `needs:` on `export-table`, `changelog-fixtures` or `actionlint`.
- `pipeline-x86` and `pipeline-x64` have no `needs:`, so they run in parallel.

### Jobs `pipeline-x86` and `pipeline-x64`

- Each job calls `./.github/workflows/arch-pipeline.yml` and passes one input: `arch: x86` for `pipeline-x86`, `arch: x64` for `pipeline-x64`.
- `release-version` is not passed; its default in `arch-pipeline.yml` is `''`.
- The two jobs have no `name:`.

### Jobs `gate-x86` and `gate-x64`

- The `name:` of `gate-x86` is `build + test (x86)`. The `name:` of `gate-x64` is `build + test (x64)`.
- `gate-x86` has `needs: pipeline-x86` and `gate-x64` has `needs: pipeline-x64`. Both have `if: always()` and run on `ubuntu-latest`.
- The file's comment above the gates reads: "if: always() runs the gate even when its pipeline failed or was skipped; a skipped required check counts as passing, so the gate must fail explicitly on anything but success."
- Each gate has one step, "Require a successful pipeline", with `shell: bash`.
- The step sets the environment variable `PIPELINE_RESULT` to `needs.pipeline-x86.result` (`gate-x86`) or `needs.pipeline-x64.result` (`gate-x64`).
- The script of `gate-x86`:

  ```bash
  if [ "$PIPELINE_RESULT" != "success" ]; then
    echo "::error::pipeline-x86 finished with result '$PIPELINE_RESULT', expected 'success'"
    exit 1
  fi
  ```

- The script of `gate-x64` is the same, with `pipeline-x64` in the message.
- Any other result prints the error message and ends the step with `exit 1`.

### Job `export-table`

- The job runs on `ubuntu-latest`.
- Step 1 is `actions/checkout`, pinned by SHA.
- Step 2 is the local composite action `./.github/actions/export-table`.

### Job `changelog-fixtures`

- The job runs on `ubuntu-latest`.
- Step 1 is `actions/checkout`, pinned by SHA.
- Step 2, "Run the CHANGELOG fixtures", has `shell: bash` and runs `pwsh -NoProfile -File scripts/ci/changelog.ps1 fixtures -FixtureDir tests/changelog`.

### Job `actionlint`

- The job has `if: github.event_name != 'workflow_dispatch'`, so it is skipped on a dispatch. It runs on `ubuntu-latest`.
- The file's comment above the job reads: "Never make this a required check: it is skipped on a dispatch, and a skipped required check counts as passing."
- The job sets `ACTIONLINT_VERSION` to `1.7.12`. The archive's SHA-256 is pinned in `ACTIONLINT_SHA256`.
- Step 1 is `actions/checkout`, pinned by SHA.
- Step 2, "Download actionlint and verify its SHA-256", has `shell: bash` and does three things in order. After each command, a non-zero exit code is turned into the error annotation below, followed by `exit 1`.
  1. It downloads `https://github.com/rhysd/actionlint/releases/download/v${ACTIONLINT_VERSION}/actionlint_${ACTIONLINT_VERSION}_linux_amd64.tar.gz` to `$RUNNER_TEMP/actionlint.tar.gz` with `curl --fail --silent --show-error --location --output`. On failure: `::error::actionlint download failed (exit $code): $out`.
  2. It checks the archive against `ACTIONLINT_SHA256` with `sha256sum --check`. On failure: `::error::actionlint archive does not match the pinned SHA-256: $out`.
  3. It extracts the member `actionlint` into `$RUNNER_TEMP` with `tar --extract --gzip`. On failure: `::error::actionlint extraction failed (exit $code): $out`.
- Step 3, "Lint the workflows", has `shell: bash`. It runs `"$RUNNER_TEMP/actionlint" -color`, echoes its standard output, and exits with the exit code of `actionlint`.
