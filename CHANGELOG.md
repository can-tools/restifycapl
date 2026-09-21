# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
Version headings correspond to Git tags on this repository; there are no
hand-invented version numbers.

## [Unreleased]

### Added

- `restifyGetVersion(char buffer[], dword bufferSize) : long` -- the
  project's first CAPL-exported operation (Stage 5, ABI proof). Writes the
  DLL's build version string into a caller-supplied buffer; returns 0 on
  success or a negative error code (see `src/module/exports.cpp`).
- `scripts/setup-dev-env.ps1`: development environment bootstrap script
  that provisions MSVC Build Tools, `make`, vcpkg-built libcurl (x86 and
  x64, static, SChannel), and the pinned `nlohmann/json` single header.
  Verified working end to end on a real machine (14 OK, 0 WARN, 0 FAIL).
- `.github/workflows/auto-pr.yml`: bot-side workflow that opens a draft PR
  into `main` on a push to a `stage/`, `chore/`, `fix/`, or `docs/` branch,
  guarded against no-op reruns and branches with no commits ahead of `main`
  (Stage 7, BPE-21).
- `.claude/skills/stage-branch/SKILL.md`: branch-naming convention and
  create-and-push procedure for starting new work (Stage 7, BPE-22).

### Changed

- `.github/workflows/ci.yml`: `push`/`pull_request` triggers now filtered
  to `main` and the four working-branch prefixes, so a future release tag
  push no longer also matches this workflow's `push` trigger (Stage 7,
  BPE-20).
- `.github/workflows/auto-pr.yml`: the draft-PR body is now derived from
  the branch's compare-API data instead of a fully static scaffold --
  plan-only classification with a pointer to `plan.md` §7.8/§7.10, a
  pre-seeded `TODO: stage and task IDs` line for `stage/`/`chore/`
  branches, the branch's own commit subjects, and a safety warning when
  the branch touches `src/module/exports.cpp`. `plan.md` §7.2 now also
  documents that the bot always opens against `main`, never a stacked
  parent (BPE-27, `plan.md` §7.13's Defect 2).
- `.claude/skills/stage-branch/SKILL.md`: records the PR-base behavior
  above as a fourth load-bearing use of the four branch-prefix
  conventions, and notes that retargeting a PR's base off `main` costs it
  its `pull_request` CI runs (BPE-27).

### Fixed

- `vcpkg.json`: `builtin-baseline` was a newer vcpkg registry commit than
  the pinned vcpkg tool (`VCPKG_PINNED_TAG` in `ci.yml` /
  `$VcpkgPinnedTag` in `scripts/setup-dev-env.ps1`), so `vcpkg install`
  resolved baseline versions (curl 8.22.0, gtest 1.18.0) that don't exist
  in the older, checked-out version database, failing both triplets with
  "no version database entry for curl/gtest at X.Y.Z" (BPE-25).
  `builtin-baseline` is now pinned to
  `9e593bb18ea69cc5095e012465dcd675a822ed0d`, the exact commit
  `VCPKG_PINNED_TAG`'s tag (`2026.07.29`) dereferences to, and the
  now-redundant `curl` version override was removed. This downgrades the
  DLL's linked dependencies to libcurl 8.21.0#1 and (test-only) GoogleTest
  1.17.0#3. A drift guard (CI step + `Assert-VcpkgBaselinePin` in
  `scripts/setup-dev-env.ps1`) now fails loudly if these two pins ever
  diverge again instead of only being documented.
- `.github/workflows/ci.yml`: the vcpkg tool checkout step cloned
  unconditionally into `VCPKG_ROOT`, which `ilammy/msvc-dev-cmd@v1` had
  silently repointed at the VS-bundled vcpkg checkout already present on
  `windows-latest` runners (`<VS install>\VC\vcpkg`), so the clone failed
  with "already exists and is not an empty directory" on the workflow's
  first real run. `VCPKG_ROOT` is now explicitly re-pinned to a CI-only
  path under `$RUNNER_TEMP` immediately after the MSVC activation step,
  leaving the pinned-tag clone/checkout/bootstrap sequence itself
  unchanged.
