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
- So the only thing that starts `ci.yml` on `release/next` is the dispatch in the first row of the dispatch table. It runs the jobs of `ci.yml` on the tip of `release/next`. This gives the required checks `build + test (x86)` and `build + test (x64)` on that branch.
- `bump-label.yml` starts on a `pull_request` event (opened, synchronize, reopened, labeled, unlabeled) and on a dispatch. The bot dispatches it on `release/next` after it rebuilds the branch.
- A `bump:*` label change on the `release/next` pull request starts the job `labels` of `release-pr.yml`.
- `tag-release.yml` pushes the release tag itself. As a consequence of the platform rule, that push starts no run. This is why its last step dispatches `release.yml` on the tag.

### What starts CI on a working branch

A pull request opened by Auto PR does not start a `pull_request` run of CI (a consequence of the platform rule). The CI of a working branch comes from the developer's push, which matches the push filter of `ci.yml`.

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

`scripts/ci/changelog.ps1` assumes that every commit on `main` is a squash-merge commit whose subject is the pull request title plus ` (#N)`, where `N` is the pull request number. This is a repository setting; no file in the repository enforces it. `scripts/ci/changelog.ps1` reads the pull request number from the trailing ` (#N)` of the subject.

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
