# CI pipeline

Archival, CI-only rationale for `.github/workflows/ci.yml`,
`.github/workflows/arch-pipeline.yml`, the `.github/actions/provision` and
`.github/actions/export-table` composite actions and `auto-pr.yml`. Local
provisioning material (shallow clones, per-triplet install roots, the pinned
vcpkg tool tag) lives in `docs/development-environment.md` and
`scripts/setup-dev-env.ps1`'s `Repair-ShallowVcpkgClone` doc comment instead —
the local and CI paths share the pin, never the mechanism or the output, so
this document only covers what has no local-script equivalent.

## Per-architecture pipelines and gate jobs (decided during Stage 14, BPE-37)

`ci.yml` calls the reusable `arch-pipeline.yml` once per architecture
(`pipeline-x86`, `pipeline-x64`). Each call runs two jobs in parallel on
Windows: `build` (`make build-<arch>`, a `dumpbin /exports` check that the
DLL exports only `caplDllGetTable4`, the artifact upload) and `test`
(`make test ARCH=<arch>`). Both jobs start with the `provision` action, so
the MSVC environment, the pinned vcpkg tool and the static dependencies are
set up identically for build and test. `build` also has a version-check step
that runs only when the caller passes a `release-version`; `ci.yml` passes
none, so it is skipped there.

The required status checks are two gate jobs in `ci.yml`, named exactly
`build + test (x86)` and `build + test (x64)`. A gate needs its pipeline and
passes only if the pipeline's result is `success`. It uses `if: always()`
because a job whose `needs` did not succeed is skipped by default, and a
skipped required check counts as passing: without `if: always()` and the
explicit comparison, a failed, cancelled or skipped pipeline would let a pull
request merge. The result is passed to the step through `env:` and compared
there, not interpolated into the script.

Check names are the gate jobs' `name:` values. The jobs inside the called
workflow report as `pipeline-x86 / build` and similar, which is why the
required checks are the separately named gates and not those.

## `VCPKG_ROOT` clobbered by `ilammy/msvc-dev-cmd` (found during Stage 6, BPE-9)

`windows-latest` ships a full VS Enterprise install with its own
pre-cloned, pre-bootstrapped vcpkg under `<VS install>\VC\vcpkg` (VS 17.6+
bundles vcpkg, and `vsdevcmd.bat` sets its own `VCPKG_ROOT` pointing
there). `ilammy/msvc-dev-cmd@v1` captures every env var `vsdevcmd.bat`
sets/changes and persists it to `GITHUB_ENV` for all later steps — which
silently overwrote the job's own `VCPKG_ROOT` the moment the "Activate
MSVC developer environment" step ran, so the "Bootstrap vcpkg" step's
unconditional `git clone` landed on that already-populated VS-bundled
directory and failed ("already exists and is not an empty directory",
exit 128).

Fixed by re-pinning `VCPKG_ROOT` to a CI-only path under `$RUNNER_TEMP`
immediately after MSVC activation (see the "Pin VCPKG_ROOT..." step in the
`provision` action) — the full clone + pinned-tag checkout + bootstrap
sequence itself is unchanged, it just no longer targets the VS-bundled
path. `$RUNNER_TEMP` is unique per runner, so each job (every job runs on its
own runner) cannot collide with the others either. Fixed in commit
`c11a5b6`.

No `env:` block can set `VCPKG_ROOT`, for the same reason — it would just
get clobbered by the same step a few lines later.

## `/MT` provenance check: `LIBCMTD` substring trap (found during Stage 6, BPE-9 / REV-4)

The provenance check (`dumpbin /directives`) greps for
`/DEFAULTLIB:LIBCMT` to confirm a provisioned `.lib` was built `/MT`. A
bare substring match on `LIBCMT` also matches `/DEFAULTLIB:LIBCMTD` (the
*debug* static CRT) as a false positive, since `LIBCMTD` contains `LIBCMT`
as a substring. REV-4 caught this. The check therefore requires the
release `LIBCMT` token **and** the absence of the debug `LIBCMTD` token,
so a lib declaring only `LIBCMTD` correctly fails instead of passing.

## Caching: tool checkout vs. install-root keys (found during Stage 6, BPE-9)

Two independent caches, deliberately keyed differently:

- **vcpkg tool checkout** — keyed only on the pinned tag (plus OS/image),
  never on the triplet. The `vcpkg.exe` binary isn't triplet-specific, so
  all pipeline jobs (build and test, both architectures) intentionally
  share this one cache entry. On a cold cache the jobs race to bootstrap and
  save; the cache action no-ops the losing save with a harmless "already
  exists" warning.
- **vcpkg install root** — keyed on `vcpkg.json` content, the triplet
  (x86 and x64 must never share a cache entry — that would reintroduce
  the per-triplet install-root collision the separate roots exist to
  avoid, via the cache instead of the install root), and the runner image
  version. The build and test jobs of one architecture share that
  architecture's entry.

Both keys include the runner image version (`$env:ImageVersion`, captured
into `GITHUB_ENV` early since GitHub's default runner-image env vars
aren't exposed through the `env` expression context until written there
explicitly) so a GitHub-side toolset bump on `windows-latest` misses the
cache and rebuilds against the new compiler/SDK automatically, rather
than depending on anyone hand-invalidating a key. Caching is purely a
performance concern — nothing about correctness depends on a cache hit,
since every provisioned binary is rebuilt from the same `vcpkg.json` +
pinned tool tag regardless.

## Branch filter avoids racing `release.yml` on a tag push (found during Stage 7, BPE-20)

`ci.yml`'s `push:` trigger is filtered to `main` plus the five
working-branch prefixes (`feat/**`, `stage/**`, `chore/**`, `fix/**`,
`docs/**`) so
that a release tag push (`vX.Y.Z`) does not also match this trigger and
race `release.yml`. A branch filter means tag pushes no longer match `push`
at all. The five prefixes here must stay in sync with `auto-pr.yml`'s
trigger filter and the `stage-branch` skill's naming convention.

## `ci.yml` can be dispatched (decided during github-actions-optimization Stage 5)

`ci.yml` also has a `workflow_dispatch` trigger without inputs. A dispatch on
a branch runs the same jobs as a push and attaches `build + test (x86)` and
`build + test (x64)` to that branch's tip commit, so a branch that no `push`
filter covers can still get the required checks. Events created with
`GITHUB_TOKEN` start no new runs except `workflow_dispatch` and
`repository_dispatch`, and dispatching needs `actions: write`. A check from a
dispatch is expected to satisfy the branch ruleset, which matches by name;
this is to be confirmed on the first branch that relies on it.

## `bash` steps run with `-e -o pipefail` (found during github-actions-optimization Stage 5)

GitHub runs a `shell: bash` step as `bash --noprofile --norc -e -o pipefail`.
`set -uo pipefail` at the top of a step does not turn `-e` off, so a command
substitution that exits non-zero, as in `next=$(cmd)`, ends the step before a
following `code=$?` runs. A status the step means to handle is captured on
the same line with `|| code=$?` (after `code=0`) or tested in an `if`. The
"Preview the next release" step in `release.yml` does this for every
`changelog.ps1` call it makes, `next-version` and `level` included, whose
exit status 1 (nothing to release) is an expected case.

## No concurrency block: every run completes (decided during Stage 14, OQ13)

`ci.yml` has no `concurrency` block, so a later push never cancels an
earlier run. A push to a branch with an open pull request starts two runs
(`push` and `pull_request`); both run to completion and both report the gate
checks. Runs shown as "cancelled" in older Actions history come from an
earlier configuration that cancelled superseded runs on the same ref; they
are not failures.

## Pinned actions and token permissions (decided during Stage 14, BPE-37)

Every `uses:` in a workflow or action file is pinned to a full 40-character
commit SHA with a trailing `# vX.Y.Z` label; local references
(`./.github/...`) are not pinned. The SHA is the commit the release tag
points to, resolved with `git ls-remote` (the peeled `^{}` commit for
annotated tags, not the tag object) and re-resolved independently in
review. Updates are manual until Dependabot is set up.

`ci.yml` and `arch-pipeline.yml` set `permissions: contents: read` at
workflow level, and every job in `arch-pipeline.yml` declares
`contents: read` itself, so a job keeps read-only access to the repository
even if the workflow-level setting changes. A called workflow can only
narrow its caller's token permissions, so the caller's `contents: read`
is the ceiling.

## Checkout depth (decided during Stage 14, BPE-37)

Every job that runs `make`, and the release workflow's `validate` job, checks
out with `fetch-depth: 0`: the Makefile runs `git describe` and `git rev-list`
on every call, including `make test`, and a shallow checkout would degrade the
version values silently. The other jobs (for example `export-table`) use the
default shallow checkout. The release workflow's jobs are covered by the
`fetch-depth` note in `docs/release-process.md`.

## The `export-table` job (decided during Stage 14, BPE-12)

`ci.yml` has a short `export-table` job on `ubuntu-latest` (`pwsh` 7, default
shallow checkout, `permissions: contents: read`) that calls the composite
action `.github/actions/export-table`. The action runs
`scripts/list-export-table.ps1` twice: first with `-FixtureDir
tests/export-table`, then on `src/module/exports.cpp` with the 12 table
checks. Each step runs even if the one before it failed, and a failing step
fails the job. The job summary carries one status line (the M1 line, stating
whether the check passed and, if not, how many of the 12 checks failed)
followed by the full generated table: one heading per category, then row
number, signature in CAPL declaration style and description. The action
exposes the markdown path as its `markdown-path` output; only `release.yml`
uploads it.

The job is not a gate and not a required check, and no gate waits for it
(accepted risk R18: a table defect does not block a merge). The mitigations
are M1, the summary line, which says the check is not required but that a
failing table cannot be released; M2, the reviewer rule that the result is
recorded for any change touching `src/module/exports.cpp` and red is a
Must-fix; and M3, a reminder in the `exports.cpp` warning of the
auto-created pull-request body. The release workflow runs the same action in
its own `export-table` job, and `publish` requires it, so a defective table
cannot be released.

The fixtures in `tests/export-table/*.fixture` are plain text, never
compiled. The first line of each is a directive, `// expect: pass` or
`// expect: fail N [N...]`. The runner executes the same checks on the
fixture content (the file need not be named `exports.cpp`), sorted by file
name, and compares the set of failing check numbers with the directive by
exact set equality; a first line that is not a valid directive fails that
fixture. The script exits 0 when everything passed, 1 when a check or fixture
failed, and 2 on a usage or I/O error. It needs only text processing, which is
why the job runs on Linux.

## The changelog-fixtures job (decided during github-actions-optimization Stage 3)

`ci.yml` has a `changelog-fixtures` job on `ubuntu-latest` (`pwsh` 7, default
shallow checkout, `permissions: contents: read`) that runs
`scripts/ci/changelog.ps1 fixtures -FixtureDir tests/changelog`. The release
workflow's `validate` job runs the same command before it parses
`CHANGELOG.md`. Each `tests/changelog/<case>/` directory holds `CHANGELOG.md`,
`case.json` (mode, line endings, the simulated git history, the expected exit
code, `::` output lines and summary lines) and, for a successful case,
`expected-notes.md`. A case for `generate`, `level`, `prs` or `next-version` runs that subcommand
instead (the last three need no `CHANGELOG.md`); a successful `generate` is
compared with `expected-CHANGELOG.md` and run twice to prove its output is
deterministic. The `case.json` format is
described in the script's help (`Get-Help scripts/ci/changelog.ps1 -Full`).
The runner replaces git with the case's simulated history,
so the cases need no repository; line endings are applied in memory, so the
result does not depend on the checkout's `eol` setting. Only the real git
calls are not covered by the fixtures; the release workflow's dry run covers
them. The job is not a gate and not a required check, and no gate waits for
it. The script exits 0 when every case matched, 1 when one did not, and 2 on a
usage error.

## The generated CHANGELOG and the dry-run preview (decided during github-actions-optimization Stage 6a)

`scripts/ci/changelog.ps1` generates release sections instead of cutting a
hand-written one: `level`, `generate`, `prs` and `next-version`, with the rules
in `docs/release-process.md`. The script never touches the network; the
workflow fetches the commit messages of each pull request named by `(#N)` with
`gh api` and passes them as a JSON file. The `validate` job of `release.yml`
therefore has `pull-requests: read` besides `contents: read`. In a dry run its
"Preview the next release" step lists the pull requests with `prs`, fetches
their messages, and writes the level, the version and the generated section to
the summary without changing any file; every outcome, an error included, leaves
the step green. Tag mode is unchanged.

## The actionlint job (decided during github-actions-optimization Stage 5a)

`ci.yml` has an `actionlint` job on `ubuntu-latest` (default shallow checkout,
`permissions: contents: read`) that lints every file in `.github/workflows/`.
It has no path filter, so a change to anything lints all workflows. Its `if:`
is `github.event_name != 'workflow_dispatch'`: a dispatch exists only to attach
the gate checks to a branch, and the job is not needed for that.

The job is not a gate and not a required check, and no gate waits for it. In
this form it must never become a required check: a job skipped by its `if:`
reports as skipped, and a skipped required check counts as passing, so the
dispatch runs would satisfy the requirement without linting anything.

The version and its SHA-256 are job-level `env:` values
(`ACTIONLINT_VERSION`, `ACTIONLINT_SHA256`) and are pinned together. The
checksum is the `linux_amd64` line of the release's own
`actionlint_<version>_checksums.txt` (the same value GitHub shows as the
asset's digest), not a value computed from a download. The first step
downloads `actionlint_<version>_linux_amd64.tar.gz`, runs `sha256sum --check`
against the pin and extracts the binary only if it matches. Updating means
changing both values from the new release's checksums file, and the version and
Windows checksum in `scripts/setup-dev-env.ps1`, which fails when its pin
differs from `ACTIONLINT_VERSION`. Every `run:` step
captures the status of a failing command with `code=0; out=$(cmd) || code=$?`
because the runner's `bash -e -o pipefail` would otherwise end the step before
the error message is printed (see the `bash` steps section above).

`actionlint` runs `shellcheck` on `run:` scripts when it finds one on `PATH`.
`ubuntu-latest` has it installed, so the job can report shell findings that a
local run without `shellcheck` does not.

`scripts/setup-dev-env.ps1` installs `actionlint` and `shellcheck` for Windows
(see `docs/development-environment.md`). After running it and opening a new
window, lint locally from the repository root with `actionlint -color`.

## The `bump label` check (decided during github-actions-optimization Stage 6c)

`bump-label.yml` is a separate workflow with one job, named exactly `bump label`
(`ubuntu-latest`, no checkout, `contents: read` and `pull-requests: read`, no
third-party actions). It is meant to become a required check on `main`. The
result comes only from the step's exit code: the job has no `if:` and the
workflow no path filter, because a skipped required check counts as passing.
Every run evaluates, and a branch other than `release/next` is green at once.

For a head branch `release/next` the check is green only when the pull request
carries exactly one label starting with `bump:`; zero, or two or more, ends
red with an `::error::` message. The workflow only reads: it never adds or
removes a label.

- Triggers: `pull_request` (`opened`, `synchronize`, `reopened`, `labeled`,
  `unlabeled`) and `workflow_dispatch`. The release bot dispatches it on
  `release/next` after every rebuild, because events created with
  `GITHUB_TOKEN` start no runs. A dispatch attaches the check to the tip SHA of
  the branch it runs on. A label change made by a person on the release pull
  request starts a `pull_request` run that re-evaluates and lands on the head
  SHA. Label events cannot be filtered by label at the trigger, so a label
  change on any pull request runs the job, which is green outside
  `release/next`.
- Reading the labels: always live through the API
  (`gh api repos/<repo>/pulls/<n>`), never from the event payload, because a
  re-run of a run replays the original payload and would show labels that are
  no longer there. For `pull_request` the number comes from the event; for a
  dispatch it comes from the open pull request whose head is
  `<owner>:release/next`.
- Fails closed: on `release/next` a dispatch with no open pull request, with
  more than one, or an API error ends red, as does a label read failure. A red
  result from a dispatch that ran before the pull request existed is replaced
  by the next run.
- Concurrency: `opened`, `synchronize`, `reopened` and dispatch runs share a
  group per branch and a newer run cancels an older one. `labeled` and
  `unlabeled` runs have a group per pull request and never cancel a running
  one, so the last label change is evaluated last.

The `bump label` job is not a required check until the user adds it to the
`main` ruleset after this workflow is on `main`; until then it is informational.

## The release bot dispatches checks and rewrites the body preview on label changes (decided during github-actions-optimization Stage 6c)

Events created with `GITHUB_TOKEN` (a push, a label change) start no workflow
runs, except `workflow_dispatch` and `repository_dispatch`. `release-pr.yml`
has two jobs because of that.

- `build` (triggers `push` to `main` and `workflow_dispatch`; skipped on the
  `pull_request` trigger). After pushing `release/next`, opening or updating
  the pull request and keeping the single bump label, it runs
  `gh workflow run ci.yml --ref release/next` and the same for
  `bump-label.yml`, so both see the final label and the pushed commit. Neither
  workflow has dispatch inputs. A failed dispatch ends the job with
  `::error::`, and nothing is dispatched when the rebuild is skipped. The
  job needs `actions: write` for this. It looks `release/next` up on the
  remote by its full ref, `refs/heads/release/next`, because a short name also
  matches a ref that merely ends in it.
- `labels` (trigger `pull_request` `labeled` and `unlabeled` on `main`).
  Runs only when the head branch is `release/next` in this repository (a fork
  is excluded by comparing the head repository) and the changed label starts
  with `bump:`. Permissions are `contents: read` and `pull-requests: write`.
  It checks out `main` (not the pull request head) with
  `persist-credentials: false` and never pushes. It reads the labels and the
  body from one API snapshot of the pull request taken at run time, not from
  the event, because a re-run replays the original payload. With exactly one
  valid `bump:*` label it runs `changelog.ps1 body-version`, which rewrites
  only the version preview line of that body, and then `gh pr edit
  --body-file`. Every other byte of the body is kept, reviewer checklist ticks
  included; the checklist resets only when `build` rebuilds the pull request.
  With zero, several or invalid `bump:*` labels nothing changes: the job warns
  and stays green, because the `bump label` check is the authoritative red
  signal. If the version preview line is missing from the body (deleted by
  hand) or the body is empty, `changelog.ps1 body-version` exits 1, the job
  goes red with an `::error::` and changes nothing; the next rebuild by the
  `build` job restores the body. It adds no comment, never changes a label and
  never pushes.

A label change made by a person starts the `labels` job; the bot's own label
change in `build` does not (it was made with `GITHUB_TOKEN`), and the body it
wrote already has the computed level. The job has its own concurrency group
that never cancels a running job; GitHub keeps at most one waiting run per
group, and since the label is read when the job runs, the surviving run still
sees the latest state. A push to `main` rebuilding the pull request at the
same moment can leave a body one change behind; the next rebuild corrects it.
Every label event on any pull request into `main` creates a `Release PR` run
whose jobs are skipped.

## `ci.yml` never calls `scripts/setup-dev-env.ps1` (found during Stage 6/7, BPE-24)

That script provisions a local development machine: installing MSVC Build
Tools, `make`, and bootstrapping vcpkg for a box that starts with none of
them. A GitHub-hosted runner starts from a different baseline, so none of
that applies — MSVC is already preinstalled and activated via
`ilammy/msvc-dev-cmd`, and `make`/vcpkg are provisioned directly by the
`provision` action's own steps, CI-native rather than by shelling out to the
script.

## CI x86 stayed green while a local x86 run showed a locale leak (found during Stage 12, HUM-34)

The failing `FlattenJson` tests depended on the process-wide C locale
(`LC_NUMERIC`) being unchanged, but an earlier test in the same process
altered it. The leak appeared only in the local run order and environment;
the CI x86 leg ran the same tests green. The test-engineer's first
hypothesis was a dangling `setlocale` pointer; the planner's analysis
established that the CI environment did not reproduce the leak.

Fixed in commit `569fc6a`: the locale is restored by value, and
`std::from_chars` is used for number parsing. `make test` and
`make test ARCH=x86` passed locally afterwards.

## Locale-leak hardening: proposed, not implemented (found during Stage 12, HUM-34)

Observation only; no change was made. The planner proposed two hardenings,
and neither is implemented, by user decision:

- a GoogleTest listener that checks `LC_NUMERIC` after every test;
- a CI guarantee that the de-DE locale test is not silently skipped.

The user will observe during later stages and return to them if the problem
reappears. A `--gtest_shuffle` run and changing the CI machine language were
considered and not adopted.
