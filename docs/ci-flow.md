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
- A change of type `unknown` raises no level of its own. The generator gives a warning for it and, unless it has a breaking marker, lists it under Development.

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
- A breaking change is listed under "Breaking changes" in the release notes. A `chore/`, `docs/` or `unknown` pull request with a breaking marker is listed only under "Breaking changes", not under Development.

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
- The job sets `ACTIONLINT_VERSION` to `1.7.12`. The archive's SHA-256 is pinned in `ACTIONLINT_SHA256`.
- Step 1 is `actions/checkout`, pinned by SHA.
- Step 2, "Download actionlint and verify its SHA-256", has `shell: bash` and does three things in order. After each command, a non-zero exit code is turned into the error annotation below, followed by `exit 1`.
  1. It downloads `https://github.com/rhysd/actionlint/releases/download/v${ACTIONLINT_VERSION}/actionlint_${ACTIONLINT_VERSION}_linux_amd64.tar.gz` to `$RUNNER_TEMP/actionlint.tar.gz` with `curl --fail --silent --show-error --location --output`. On failure: `::error::actionlint download failed (exit $code): $out`.
  2. It checks the archive against `ACTIONLINT_SHA256` with `sha256sum --check`. On failure: `::error::actionlint archive does not match the pinned SHA-256: $out`.
  3. It extracts the member `actionlint` into `$RUNNER_TEMP` with `tar --extract --gzip`. On failure: `::error::actionlint extraction failed (exit $code): $out`.
- Step 3, "Lint the workflows", has `shell: bash`. It runs `"$RUNNER_TEMP/actionlint" -color`, echoes its standard output, and exits with the exit code of `actionlint`.

## Architecture pipeline (`arch-pipeline.yml`)

### Trigger and callers

- The only event is `workflow_call`.
- `ci.yml` calls the workflow twice, from `pipeline-x86` and `pipeline-x64`, and passes only `arch`.
- `release.yml` calls it twice, from `pipeline-x86` (`arch: x86`) and `pipeline-x64` (`arch: x64`). Both jobs have `needs: validate` and pass `release-version: ${{ needs.validate.outputs.version }}`.

### Inputs

- `arch` has the type `string` and is required. It has no default. Its description reads: "Target architecture, x86 or x64."
- `arch-pipeline.yml` does not check `arch`; the value is used in the make targets, the DLL path and the artifact name.
- `release-version` has the type `string` and is optional, with default `''`. Its description reads: "X.Y.Z to build and verify as a release; empty for a development build."

### Permissions

- The workflow has `permissions: contents: read`.
- The jobs `build` and `test` each repeat `permissions: contents: read`.

### Jobs

- The workflow has two jobs, `build` and `test`. Both have the `name:` equal to the job id and run on `windows-latest`.
- `build` and `test` have no `needs:`, so they run in parallel.
- `test` downloads no artifact and builds on its own via `make test`.
- Third-party actions in the file: `actions/checkout`, pinned by SHA with the label `# v7.0.1`, and `actions/upload-artifact`, pinned by SHA with the label `# v7.0.2`.

### What `release-version` changes

- When `release-version` is empty, the build step passes no `VER_*` variables to `make`, and the step "Check version resource against release-version" is skipped.
- When it is not empty, the build step validates it, passes the version variables to `make`, and the step "Check version resource against release-version" runs.
- The steps "Check DLL exports (dumpbin /exports)" and "Upload DLL artifact", the artifact name and the job `test` do not use the value.

### Job `build`

The job has six steps, in this order: "Checkout (full history)", "Provision build environment", "make build-${{ inputs.arch }}", "Check version resource against release-version", "Check DLL exports (dumpbin /exports)" and "Upload DLL artifact".

In the messages quoted in this section, a PowerShell variable or expression (for example `$env:RELEASE_VERSION`, `$dll` or `$($info.FileVersion)`) stands for its value at run time.

#### Step "Checkout (full history)"

- The step uses `actions/checkout` with `fetch-depth: 0`.

#### Step "Provision build environment"

- The step uses the local composite action `./.github/actions/provision` with the input `arch: ${{ inputs.arch }}`. It has no `if:`.

#### Step "make build-${{ inputs.arch }}"

- The step has no `if:`. It runs with `shell: pwsh` and sets `RELEASE_VERSION` to `inputs.release-version` and `ARCH` to `inputs.arch`.
- The script sets `$ErrorActionPreference = 'Stop'` and starts with an empty list of version arguments.
- If `RELEASE_VERSION` is not empty, it must match this regular expression: `^(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\.(0|[1-9][0-9]*)\z`. Otherwise the step throws `release-version must be X.Y.Z without leading zeros, got '$env:RELEASE_VERSION'`.
- The script splits the value on `.`. A field is rejected when its length is greater than 5 or its integer value is greater than 65535. The step then throws `release-version field '$field' exceeds 65535 (version resource fields are 16-bit)`.
- For a valid value the version arguments are `VER_MAJOR`, `VER_MINOR` and `VER_BUILD`, set to the first, second and third field, and `VER_REV=0`.
- The step then runs `make "build-$env:ARCH"` with the version arguments. If the exit code is not 0, it throws `make build-$env:ARCH failed (exit $LASTEXITCODE)`.

#### Step "Check version resource against release-version"

- The step runs only if `inputs.release-version != ''`. It runs with `shell: pwsh` and sets `RELEASE_VERSION` and `ARCH` like the build step.
- The step checks the format of `release-version` again, with the same regular expression and the same error text as the build step, but not the 65535 limit.
- It reads the version resource of `build\<arch>\restifycapl-<arch>.dll` with `[System.Diagnostics.FileVersionInfo]::GetVersionInfo`. `$dll` is the resolved full path.
- The expected numeric version is `<major>.<minor>.<patch>.0`, built from the three fields. The expected string is `v` followed by `release-version`.
- The step makes three comparisons and collects a message for each one that fails:
  - The actual numeric version (`FileMajorPart`, `FileMinorPart`, `FileBuildPart` and `FilePrivatePart`, joined by `.`) is compared with `-ne` to the expected numeric version. The message is `numeric version is $actualNumeric, expected $expectedNumeric`.
  - `FileVersion` is compared with `-cne` (case-sensitive) to the expected string. The message is `FileVersion is '$($info.FileVersion)', expected '$expectedString'`.
  - `ProductVersion` is compared with `-cne` to the expected string. The message is `ProductVersion is '$($info.ProductVersion)', expected '$expectedString'`.
- If there is at least one message, the step throws `Version resource of $dll does not match release-version ${env:RELEASE_VERSION}:` followed by the messages, one per line.
- If there is none, the step prints `Version resource of $dll matches $expectedNumeric / $expectedString.`

#### Step "Check DLL exports (dumpbin /exports)"

- The step has no `if:` and runs for every build, development builds included. It runs with `shell: pwsh` and sets `ARCH`.
- It runs `dumpbin /nologo /exports` on `build\<arch>\restifycapl-<arch>.dll`. If the exit code is not 0, it throws `dumpbin /exports $dll failed (exit $LASTEXITCODE)`.
- It writes the dumpbin output to the job summary before it checks the exports. The summary has the heading `### dumpbin /exports <dll>` and the output in a fenced block.
- It reads the exported names from the dumpbin table and the count from the "number of functions" line.
- If there is no "number of functions" line, the step throws `No 'number of functions' line found in the dumpbin output for $dll`.
- The step throws unless the count is 1, exactly one name was read, and that name is `caplDllGetTable4` (case-sensitive comparison). The message is `$dll must export exactly one function, caplDllGetTable4; found $functionCount function(s), named: $($names -join ', ')`.
- If the check passes, the step prints `$dll exports exactly caplDllGetTable4.`

#### Step "Upload DLL artifact"

- The step has no `if:`. It uses `actions/upload-artifact`.
- The artifact name is `restifycapl-<arch>` and the path is `build/<arch>/restifycapl-<arch>.dll`.
- The step sets `if-no-files-found: error` and `overwrite: true`.

### Job `test`

- The job has three steps, in this order: "Checkout (full history)", "Provision build environment" and "make test (ARCH=${{ inputs.arch }})".
- "Checkout (full history)" uses `actions/checkout` with `fetch-depth: 0`, like the same step in `build`.
- "Provision build environment" uses `./.github/actions/provision` with the input `arch: ${{ inputs.arch }}`, like the same step in `build`.
- "make test (ARCH=${{ inputs.arch }})" runs with `shell: pwsh` and sets `ARCH` to `inputs.arch`. It runs `make test "ARCH=$env:ARCH"`. If the exit code is not 0, it throws `make test ARCH=$env:ARCH failed (exit $LASTEXITCODE)`.
- The job does not use `release-version`; it runs the same way for a development and a release call.

## Composite action `provision`

### Header

- The action is in `.github/actions/provision/action.yml`. Its `name:` is `Provision build environment`.
- Its `description:` reads: "Activates MSVC, provisions the pinned vcpkg tool and the static dependencies, and places them where the Makefile expects them."
- It has one input, `arch`. The input is required and has no default. Its description reads: "Target architecture, x86 or x64."
- `runs.using` is `composite`.
- The action has twelve steps, in this order: "Validate arch input", "Activate MSVC developer environment (${{ inputs.arch }})", "Pin VCPKG_ROOT to a CI-only path (avoid VS-bundled vcpkg collision)", "Expose runner image version for cache keying", "Ensure make is available", "Cache vcpkg tool checkout", "Bootstrap vcpkg (full clone, pinned tool tag)", "Verify vcpkg tool pin matches vcpkg.json's builtin-baseline", "Cache vcpkg install root (${{ inputs.arch }}-windows-static)", "vcpkg install (${{ inputs.arch }}-windows-static)", "Place libs where the Makefile expects them" and "Verify /MT provenance (dumpbin /directives)".
- The jobs `build` and `test` of `arch-pipeline.yml` use the action, each with `arch: ${{ inputs.arch }}`.
- The first step throws `arch must be x86 or x64, got '$env:ARCH'` unless `arch` is exactly `x86` or `x64` (case-sensitive).
- The steps "Ensure make is available", "Bootstrap vcpkg (full clone, pinned tool tag)", "Verify vcpkg tool pin matches vcpkg.json's builtin-baseline", "vcpkg install (${{ inputs.arch }}-windows-static)", "Place libs where the Makefile expects them" and "Verify /MT provenance (dumpbin /directives)" set `$ErrorActionPreference = 'Stop'`.
- The steps "Validate arch input", "Pin VCPKG_ROOT to a CI-only path (avoid VS-bundled vcpkg collision)", "vcpkg install (${{ inputs.arch }}-windows-static)", "Place libs where the Makefile expects them" and "Verify /MT provenance (dumpbin /directives)" set `ARCH` to `inputs.arch`.
- In the messages quoted in this section, a PowerShell variable or expression (for example `$env:ARCH`, `$lib` or `$LASTEXITCODE`) stands for its value at run time.

### Step "Validate arch input"

- It runs `if ($env:ARCH -cne 'x86' -and $env:ARCH -cne 'x64') { throw "arch must be x86 or x64, got '$env:ARCH'" }`.

### Step "Activate MSVC developer environment (${{ inputs.arch }})"

- The step uses `ilammy/msvc-dev-cmd`, pinned by SHA with the label `# v1.13.0`.
- It passes `arch: ${{ inputs.arch }}`.

### Step "Pin VCPKG_ROOT to a CI-only path (avoid VS-bundled vcpkg collision)"

- It appends three lines to `$GITHUB_ENV`, each with `echo "..." >> $env:GITHUB_ENV`.
- The first line is `VCPKG_ROOT=$env:RUNNER_TEMP\vcpkg`.
- The second line is `VCPKG_PINNED_TAG=2026.07.29`.
- The third line is `VCPKG_INSTALL_ROOT=${{ github.workspace }}\vcpkg_installed-$env:ARCH`.

### Step "Expose runner image version for cache keying"

- The step runs `echo "IMAGE_VERSION=$env:ImageVersion" >> $env:GITHUB_ENV`.

### Step "Ensure make is available"

- It runs `choco install make -y --no-progress` only if `Get-Command make -ErrorAction SilentlyContinue` finds no `make`.
- It then always runs `make --version`.

### Step "Cache vcpkg tool checkout"

- The step has the id `vcpkg-tool-cache`. It uses `actions/cache`, pinned by SHA with the label `# v6.1.0`.
- The cache path is `${{ env.VCPKG_ROOT }}`.
- The cache key is `vcpkg-tool-${{ env.VCPKG_PINNED_TAG }}-${{ runner.os }}-${{ env.IMAGE_VERSION }}`.

### Step "Bootstrap vcpkg (full clone, pinned tool tag)"

- The step runs only if `steps.vcpkg-tool-cache.outputs.cache-hit != 'true'`.
- It runs `git clone https://github.com/microsoft/vcpkg.git "$env:VCPKG_ROOT"`. If the exit code is not 0, it throws `git clone of vcpkg failed (exit $LASTEXITCODE)`.
- It runs `git -C "$env:VCPKG_ROOT" checkout --quiet "$env:VCPKG_PINNED_TAG"`. If the exit code is not 0, it throws `git checkout $env:VCPKG_PINNED_TAG failed (exit $LASTEXITCODE)`.
- It reads the checked-out commit with `git -C "$env:VCPKG_ROOT" rev-parse HEAD` and prints `vcpkg tool pinned at $env:VCPKG_PINNED_TAG ($actualCommit)`.
- It runs `& "$env:VCPKG_ROOT\bootstrap-vcpkg.bat" -disableMetrics`. If the exit code is not 0, it throws `bootstrap-vcpkg.bat failed (exit $LASTEXITCODE)`.
- If `$env:VCPKG_ROOT\vcpkg.exe` does not exist, it throws `bootstrap-vcpkg.bat ran but vcpkg.exe still not found under $env:VCPKG_ROOT`.

### Step "Verify vcpkg tool pin matches vcpkg.json's builtin-baseline"

- The step has no `if:`, so it also runs on a cache hit.
- The step reads `builtin-baseline` from `vcpkg.json` and compares it with the commit checked out in `VCPKG_ROOT`.
- If they differ, the step throws a four-line message. Lines 1, 2 and 4 read:

  ```
  vcpkg.json's builtin-baseline ($baseline) does not match the pinned vcpkg TOOL's checked-out commit ($toolCommit, from VCPKG_PINNED_TAG=$env:VCPKG_PINNED_TAG).
  These are two independent pins (WHICH port versions resolve vs. WHICH vcpkg.exe resolves them) that must name the same commit -- otherwise version resolution reads baseline versions from one commit but checks them against a version database pinned at a different (often older) commit, producing 'no version database entry' errors.
  Fix by re-deriving the commit VCPKG_PINNED_TAG dereferences to (git ls-remote --tags https://github.com/microsoft/vcpkg.git <tag>) and setting vcpkg.json's builtin-baseline to that exact commit.
  ```

- Line 3 points to a section of a project document.
- If they are equal, the step prints `vcpkg tool pin ($toolCommit) matches vcpkg.json's builtin-baseline.`

### Step "Cache vcpkg install root (${{ inputs.arch }}-windows-static)"

- The step has the id `vcpkg-installed-cache`. It uses `actions/cache`, pinned by SHA with the label `# v6.1.0`.
- The cache path is `${{ env.VCPKG_INSTALL_ROOT }}`.
- The cache key is `vcpkg-installed-${{ inputs.arch }}-windows-static-${{ hashFiles('vcpkg.json') }}-${{ env.IMAGE_VERSION }}`.

### Step "vcpkg install (${{ inputs.arch }}-windows-static)"

- The step has `working-directory: ${{ github.workspace }}`.
- It runs `& "$env:VCPKG_ROOT\vcpkg.exe" install "--triplet=$env:ARCH-windows-static" "--x-install-root=$env:VCPKG_INSTALL_ROOT"`.
- If the exit code is not 0, it throws `vcpkg install --triplet=$env:ARCH-windows-static failed (exit $LASTEXITCODE)`.
- It then checks that the directory `<VCPKG_INSTALL_ROOT>\<arch>-windows-static\lib` exists.
- If the directory does not exist, it throws `vcpkg install exited 0 but expected lib directory not found: $tripletLibDir -- do not trust exit code alone (see scripts/setup-dev-env.ps1's Install-ManifestTriplet).`

### Step "Place libs where the Makefile expects them"

- The source directories are `<VCPKG_INSTALL_ROOT>\<arch>-windows-static\lib`, its subdirectory `manual-link`, `<VCPKG_INSTALL_ROOT>\<arch>-windows-static\include\gtest` and `<VCPKG_INSTALL_ROOT>\<arch>-windows-static\include\curl`.
- The destination directories are `lib\<arch>`, `lib\gtest\<arch>`, `include\vendor\gtest` and `include\vendor\curl`. The script creates them with `New-Item -ItemType Directory -Force`.
- The loop covers `libcurl.lib` and `zs.lib`. It copies each file from the source `lib` directory to `lib\<arch>` with `-Force`. If a file is missing, it throws `Expected product lib not found: $src`.
- `gtest.lib` comes from the source `lib` directory and `gtest_main.lib` from `manual-link`. Both are copied to `lib\gtest\<arch>`.
- If a file is missing, the step throws `Expected gtest.lib not found: $gtestLibSrc` or `Expected gtest_main.lib not found: $gtestMainLibSrc`.
- The step copies the content of the gtest header directory recursively to `include\vendor\gtest`. If the directory is missing, it throws `Expected gtest header directory not found: $srcGtestInclude`.
- The step copies the content of the curl header directory recursively to `include\vendor\curl`. If the directory is missing, it throws `Expected curl header directory not found: $srcCurlInclude`.
- It prints `Placed: $libDest\libcurl.lib, $libDest\zs.lib, $gtestLibDest\gtest.lib, $gtestLibDest\gtest_main.lib, headers under $gtestIncludeDest and $curlIncludeDest`.

### Step "Verify /MT provenance (dumpbin /directives)"

- It checks four files: `lib\<arch>\libcurl.lib`, `lib\<arch>\zs.lib`, `lib\gtest\<arch>\gtest.lib` and `lib\gtest\<arch>\gtest_main.lib`.
- For each file it runs `dumpbin /nologo /directives "$lib"` and joins the output lines with a line break.
- A file passes the first check when the output matches `/DEFAULTLIB:LIBCMT` and does not match `/DEFAULTLIB:LIBCMTD`.
- If the first check fails, the step prints `::error::$lib does not declare /DEFAULTLIB:LIBCMT (release static CRT) -- expected for /MT. A lib declaring only /DEFAULTLIB:LIBCMTD (debug static CRT) also fails this check.` and records the failure.
- Independently of the first check, if the output matches `/DEFAULTLIB:MSVCRT`, the step prints `::error::$lib declares /DEFAULTLIB:MSVCRT -- built with /MD, not /MT. Never mix /MT and /MD in one link (see msvc-build-conventions).` and records the failure.
- The loop goes through all four files before the step fails.
- If a failure was recorded, the step throws `/MT provenance check failed for one or more provisioned libs.`
- Otherwise it prints `All provisioned libs confirmed /DEFAULTLIB:LIBCMT, no MSVCRT.`

## Composite action `export-table`

### Header

- The action is in `.github/actions/export-table/action.yml`. Its `name:` is `export-table`.
- Its `description:` reads: "Runs the export-table fixtures, checks src/module/exports.cpp and writes the job summary."
- The action has one output, `markdown-path`, set only when `export-table.md` exists after the check.
- The description of the output reads: "Path of the generated export table markdown. Set only when the file exists after the check ran (the table could be parsed); not set otherwise, so consumers read an empty string."
- `runs.using` is `composite`.
- The action has three steps, in this order: "Run export-table fixtures" (id `fixtures`), "Check src/module/exports.cpp" (id `table`) and "Write job summary".
- All three steps run with `shell: pwsh`.
- The job `export-table` of `ci.yml` uses the action as a step with `uses: ./.github/actions/export-table`.
- The job `export-table` of `release.yml` uses the action in the step "Check the export table" (id `table`); a later step of that job reads the output `markdown-path`.

### Step "Run export-table fixtures"

- The step has the id `fixtures` and sets `FIXTURE_DIR` to `tests/export-table`.
- It runs `./scripts/list-export-table.ps1 -FixtureDir $env:FIXTURE_DIR`, which runs every `*.fixture` file in that directory, and then `exit $LASTEXITCODE`.

### Step "Check src/module/exports.cpp"

- The step has the id `table`, has `if: always()` and sets `EXPORTS_CPP` to `src/module/exports.cpp`.
- It creates the directory `export-table` under `$env:RUNNER_TEMP` with `New-Item -ItemType Directory -Force`. `$md` is `export-table.md` in that directory.
- It runs `./scripts/list-export-table.ps1 -Path $env:EXPORTS_CPP -MarkdownPath $md | Tee-Object -FilePath (Join-Path $dir 'output.txt')`.
- The script checks the table and, with `-MarkdownPath`, writes it as markdown to that file.
- For `-Path`, the script prints `export-table passed: <n> functions in <m> categories, checks 1-12 clean.` or `export-table: <n> of 12 checks failed.`
- The step keeps `$LASTEXITCODE` in `$code`.
- If `$md` exists, the step appends `markdown-path=$md` to `$env:GITHUB_OUTPUT`.
- The step ends with `exit $code`.

### Step "Write job summary"

- The step has `if: always()` and sets `FIXTURES_OUTCOME` to the outcome of the step `fixtures`.
- It takes the last line of `output.txt` (written by the `Tee-Object` in "Check src/module/exports.cpp") that matches `^export-table( passed)?:` and writes one result line to the job summary. If `output.txt` does not exist, the third text below is written.
- In the result texts below, `$($Matches[1])` and `$($Matches[2])` stand for the first and second number matched in that line.
- If the line matches `^export-table passed: (\d+) functions in (\d+) categories`, the result line is:

  ```
  export-table passed: $($Matches[1]) functions in $($Matches[2]) categories, checks 1-12 clean. Not a required check; read it on any branch that touches src/module/exports.cpp.
  ```

- If the line matches `^export-table: (\d+) of 12 checks failed`, the result line is:

  ```
  export-table FAILED: $($Matches[1]) of 12 checks failed in src/module/exports.cpp. This check is not required and does not block the merge, but a failing table cannot be released: release.yml requires it. Fix it before merging any change that touches src/module/exports.cpp.
  ```

- Otherwise the result line is:

  ```
  export-table FAILED: no result was produced for src/module/exports.cpp; see the step log.
  ```

- If `FIXTURES_OUTCOME` is not `success`, the line is preceded by `The export-table fixture run did not succeed; see the "Run export-table fixtures" step.` and a blank line.
- If `export-table.md` exists, its content follows.
- The text is appended to `$env:GITHUB_STEP_SUMMARY` as UTF-8 without BOM.
