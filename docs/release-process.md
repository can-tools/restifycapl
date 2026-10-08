# Release process

How `.github/workflows/release.yml` turns a Git tag into a GitHub release,
and what to do when it does not go cleanly. Build-side CI rationale (pins,
caches, permissions) lives in `docs/ci-pipeline.md`; the version mechanism in
the `msvc-build-conventions` skill.

## What a release contains (decided during Stage 14, BPE-11)

- `restifycapl-x86.dll` and `restifycapl-x64.dll`. The names carry no version
  because `capl/includes/includes.cin` loads them by name.
- `SHA256SUMS` for the two DLLs.
- Release notes: the tag's `CHANGELOG.md` section without its
  `### Development` part, then a generated `### Using this release` block and
  a generated `### Export table` inside `<details>`.

`capl/` is not packaged. Take it from the source archive of the same tag; a
DLL and `capl/` from different versions fail at CAPL compile.

There is no approval gate, no GitHub Environment and no pre-release flag.
Pushing the tag is the decision to publish.

## Cutting a release

1. Wait until `main`'s CI is green. This is a convention, not enforced.
2. Cut the CHANGELOG on a `chore/` branch: `## [Unreleased]` becomes
   `## [vX.Y.Z] - YYYY-MM-DD` with a new empty `## [Unreleased]` above it.
   The heading must equal the tag you are about to push.
3. Run the dry run (below) on that branch, read the result, and merge.
4. Push an annotated tag on the merge commit: `git tag -a vX.Y.Z -m "vX.Y.Z"`,
   then `git push origin vX.Y.Z`. The release workflow also accepts a
   lightweight tag. Nothing in it reads the tag's annotation; release notes
   come from `CHANGELOG.md`.

Everything after the tag push is automatic.

## Dry run

Run the `Release` workflow from the Actions tab with `workflow_dispatch` on
any branch. It never publishes. Because there is no tag, it builds with
development versions and skips the version-resource check, which therefore
runs for the first time on a real tag. It still runs both pipelines, the
export-table check, the notes assembly and the attestation.

Read, in the run's summaries: the export table, the `notes.md` preview, the
`SHA256SUMS`, the `dumpbin /exports` output of both architectures, and the
attestation step. The notes come from the first CHANGELOG section that has
content (`[Unreleased]` before the cut, the cut section after it); the
`validate` summary names the section used.

## Jobs

| Job | Runs on | Purpose |
|---|---|---|
| `validate` | Linux | Tag mode: tag format `vX.Y.Z`, each field at most 65535, tagged commit is an ancestor of `origin/main`, no published release for the tag, matching CHANGELOG heading with non-empty notes. Both modes: extract the notes body. |
| `pipeline-x86`, `pipeline-x64` | Windows | `arch-pipeline.yml` with `release-version` set to `X.Y.Z` in tag mode, empty in a dry run. |
| `export-table` | Linux | The same composite action `ci.yml` uses; uploads the generated markdown. |
| `assemble` | Linux | `SHA256SUMS`, `notes.md` (at most 125,000 characters), both written to the job summary. No rebuild after this point. |
| `attest` | Linux | Build-provenance attestation for the two DLLs, then `gh attestation verify` on each with `--repo` and `--signer-workflow`. Also runs in a dry run. |
| `publish` | Linux | Tag push only. Needs `validate`, `export-table`, `assemble` and `attest`. |

Only `publish` has `contents: write`; only `attest` has `id-token: write` and
`attestations: write`. `publish` uses the `gh` CLI only and does not verify
attestations.

Every job that runs `make` and `validate` check out with `fetch-depth: 0`:
the Makefile runs `git` on every call and the ancestor check needs
`origin/main`. The other jobs download artifacts and need no checkout.

CHANGELOG matching is line by line and allows `\r?` at line ends, because
Linux checkouts are CRLF too. The section ends at the next `## [` heading or
at a column-0 link definition (`[v1.2.3]: https://...`), so the link
definitions at the bottom of the file never enter the notes. More than one
`### Development` block, or an empty section after removing it, fails.

## Safeguards in `publish`

1. No published release may exist for the tag (checked in `validate` and again
   in `publish`).
2. A leftover draft for the same tag is deleted first, so a re-run is safe.
3. The release is created as a draft, its three assets, their hashes against
   `SHA256SUMS` and the notes length are verified, and only then is it made
   public, with `gh release edit --draft=false` as the single last command.
4. No `workflow_dispatch` run ever reaches `publish`.
5. Concurrency is per ref and never cancels a running release.
6. When the job fails, its summary says which state the release is in.

## When a release fails

- **A, transient failure before anything is public** (a Sigstore outage, a
  runner error): re-run the failed jobs.
- **B, defect found before anything is public:** delete the draft and the tag,
  fix through a `fix/` PR with its entry in the unpublished CHANGELOG section
  (release-machinery fixes under `### Development`), and tag again. Never
  reuse the tag name for a different version.
- **C, the release is public and defective:** never move the tag. Ship the
  next patch version, mark the bad release "Withdrawn" and its CHANGELOG
  heading `[YANKED]`.
- **D, the release went public but the job reports failure:** handled as a
  normal defect, like any other. There is no extra post-release check.

Attestations created for builds that are never published are harmless and
stay.

## Verifying a download

In PowerShell, compare the hash of each downloaded DLL with the line for that
file in `SHA256SUMS`:

```
Get-FileHash restifycapl-x64.dll
Get-FileHash restifycapl-x86.dll
```

With the GitHub CLI installed, the build-provenance attestation can be
verified as well:

```
gh attestation verify <dll> --repo <owner>/<repo> --signer-workflow <owner>/<repo>/.github/workflows/release.yml
```

This is the command the `attest` job runs for both DLLs on every release.
