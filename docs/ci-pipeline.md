# CI pipeline

Archival, CI-only rationale for `.github/workflows/ci.yml` and
`auto-pr.yml`. Local provisioning material (shallow clones, per-triplet
install roots, the pinned vcpkg tool tag) lives in
`docs/development-environment.md` and `scripts/setup-dev-env.ps1`'s
`Repair-ShallowVcpkgClone` doc comment instead — the local and CI paths
share the pin, never the mechanism or the output, so this document only
covers what has no local-script equivalent.

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
immediately after MSVC activation (see the "Pin VCPKG_ROOT..." step in
`ci.yml`) — the full clone + pinned-tag checkout + bootstrap sequence
itself is unchanged, it just no longer targets the VS-bundled path.
`$RUNNER_TEMP` is unique per runner, so the two matrix legs (separate
runners) cannot collide with each other either. Fixed in commit `c11a5b6`.

Nothing in the job-level `env:` block can set `VCPKG_ROOT`, for the same
reason — it would just get clobbered by the same step a few lines later.

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
  never on `matrix.triplet`. The `vcpkg.exe` binary isn't triplet-specific,
  so both matrix legs intentionally share this one cache entry. On a cold
  cache both legs race to bootstrap and save; `actions/cache@v4` no-ops
  the losing save with a harmless "already exists" warning.
- **vcpkg install root** — keyed on `vcpkg.json` content, the triplet
  (x86 and x64 must never share a cache entry — that would reintroduce
  the per-triplet install-root collision the separate roots exist to
  avoid, via the cache instead of the install root), and the runner image
  version.

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

`ci.yml`'s `push:` trigger is filtered to `main` plus the four
working-branch prefixes (`stage/**`, `chore/**`, `fix/**`, `docs/**`) so
that a release tag push (`vX.Y.Z`) does not also match this trigger and
race the (future) `release.yml`. A branch filter means tag pushes no
longer match `push` at all. The four prefixes here must stay in sync with
`auto-pr.yml`'s trigger filter and the `stage-branch` skill's naming
convention.

`push` and `pull_request` are deliberately left as separate concurrency
groups (not unified to dedupe the two runs on an open PR): unifying them
would let `cancel-in-progress` cancel a run that a required status check
still needs to see succeed.
