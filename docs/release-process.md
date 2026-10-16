# Release process

How `.github/workflows/release.yml` turns a Git tag into a GitHub release,
and what to do when it does not go cleanly. Build-side CI rationale (pins,
caches, permissions) lives in `docs/ci-pipeline.md`; the version mechanism in
the `msvc-build-conventions` skill.

## What a release contains (decided during Stage 14, BPE-11)

- `restifycapl-x86.dll` and `restifycapl-x64.dll`. The names carry no version
  because `capl/includes/includes.cin` loads them by name.
- `SHA256SUMS` for the two DLLs.
- Release notes: the generated `CHANGELOG.md` section for the release (found
  through the tagged commit's first parent) without its `### Development`
  part, then a generated `### Using this release` block and a generated
  `### Export table` inside `<details>`.

`capl/` is not packaged. Take it from the source archive of the same tag; a
DLL and `capl/` from different versions fail at CAPL compile.

There is no approval gate, no GitHub Environment and no pre-release flag.
Pushing the tag, or dispatching the `Release` workflow on the tag, is the
decision to publish.

## Cutting a release

A release section is generated from commit messages; nobody writes it by hand
and `CHANGELOG.md` has no `## [Unreleased]`.

1. Wait until the CI of `main` is green. This is a convention, not enforced.
2. `scripts/ci/changelog.ps1 level` computes the level from the squash-merge
   commits since the last tag, `next-version` the version from it, and
   `generate` writes the section `## [<12-char hash of origin/main>] - YYYY-MM-DD`
   and its link definition at the bottom of the file. The hash is the base commit
   of the section, taken from git. Rules:
   - The type comes from the commit subject, which is the pull request title
     (the branch name) plus `(#N)`: `feat/` and `stage/` minor, `fix/` patch,
     `chore/` and `docs/` no release. A subject starting with `release/` is
     skipped; a subject with no known branch prefix is listed under Development
     with a warning.
   - Entries come from the commit messages of the pull request, which the
     workflow fetches (the script itself stays offline): one entry per `feat`,
     `stage` or `fix` message, each linking to the pull request. A releasing
     pull request without such a message gets one entry made of its branch name
     and a warning. A subject without `(#N)` also warns, and its messages are
     not read.
   - A breaking change is `type!:` or a `BREAKING CHANGE:` footer in any
     message, on any branch type. It gives minor while the major version is 0
     and major from 1.0.0; 1.0.0 itself is reached only with a `bump:major`
     label. A non-empty footer text replaces the description as the entry text,
     and types are recognized only in lowercase followed by `: ` (`Fix:` is
     ignored).
   - Sections are Breaking changes, Added, Fixed and Development, in that order.
     `chore/` and `docs/` pull requests are listed under Development, and so is
     every commit up to the one-time cut-off commit named in the script, which
     raises no level. A section with nothing to release besides Development is
     an error.
3. Until the release pull request is created automatically, generate the
   section on a `chore/` branch that is up to date with `origin/main`, run the
   dry run on that branch, and squash-merge it only while `origin/main` is still
   the commit named in the heading; if `main` has moved, generate again. Do not
   use a rebase merge: the check in `validate` needs the merge commit's first
   parent to be the `main` commit named in the heading, which a squash merge
   guarantees and a rebase merge does not.
4. The tag `vX.Y.Z` goes on the squash-merge commit; the version exists only in
   the tag. The release workflow accepts an annotated or a lightweight tag and
   reads nothing from its annotation.

The release mode depends on the ref, not on the event: a tag ref publishes,
whether it came from a tag push or from a `workflow_dispatch` on the tag
(`gh workflow run release.yml --ref vX.Y.Z`, or the Actions tab with the tag
chosen as the ref), and a branch ref is a dry run. A tag pushed with
`GITHUB_TOKEN` starts no workflow run, so the dispatch is how automation
starts a release.

Everything after the tag push or the dispatch on the tag is automatic.

## Dry run

Run the `Release` workflow from the Actions tab with `workflow_dispatch` on
any branch (a branch ref, not a tag). It never publishes and changes no file.
Because there is no tag, it builds with development versions and skips the
version-resource check, which therefore runs for the first time on a real tag.
It still runs both pipelines, the export-table check, the notes assembly and
the attestation. The `validate` summary previews the next release from
`origin/main`: the level `changelog.ps1 level` computes from the commits since
the last tag, the version `next-version` derives from it (no bump label is
considered) and the section `generate` would write, or why none can be
computed, for instance when nothing is left to release. `validate` has
`pull-requests: read` to fetch the commit messages of the pull requests.

Read, in the run's summaries: the export table, the `notes.md` preview, the
`SHA256SUMS`, the `dumpbin /exports` output of both architectures, and the
attestation step. The notes come from the first CHANGELOG section that has
content; the `validate` summary names the section used. When that section is a
hash heading, the dry run checks it against `origin/main` and ends in one of
three ways:

- Before the merge it fails unless the heading's hash equals the first 12
  characters of the `origin/main` tip and the run's commit contains that tip.
  It reports every failed condition; the fix is to generate the section again
  from the new tip.
- Once the section is merged (the first-parent commit after the heading's hash
  carries the heading) it passes and names that commit. The notice says to
  re-run the failed `tag-release.yml` run; `tag-release.yml` does not exist yet
  and is added in a later change, so until then the tag is pushed by hand on
  that commit.
- When that commit already carries a `v*.*.*` tag it passes with a warning
  that the section was already released; the notes are previewed again.

## Jobs

| Job | Runs on | Purpose |
|---|---|---|
| `validate` | Linux | Tag mode: tag format `vX.Y.Z`, each field at most 65535, tagged commit is an ancestor of `origin/main`, no published release for the tag, the tagged commit has a parent, exactly one CHANGELOG hash heading matches that first parent, with non-empty notes. Dry run: the heading check against `origin/main` and the release preview, both described under Dry run. Both modes (tag and dry run): run the CHANGELOG fixtures first, where a red fixture fails `validate` and so blocks the release, then extract the notes body with `scripts/ci/changelog.ps1 notes`. |
| `pipeline-x86`, `pipeline-x64` | Windows | `arch-pipeline.yml` with `release-version` set to `X.Y.Z` in tag mode, empty in a dry run. |
| `export-table` | Linux | The same composite action `ci.yml` uses; uploads the generated markdown. |
| `assemble` | Linux | `SHA256SUMS`, `notes.md` (at most 125,000 characters), both written to the job summary. No rebuild after this point. |
| `attest` | Linux | Build-provenance attestation for the two DLLs, then `gh attestation verify` on each with `--repo` and `--signer-workflow`. Also runs in a dry run. |
| `publish` | Linux | Tag ref only (a tag push or a dispatch on a tag). Needs `validate`, `export-table`, `assemble` and `attest`. |

Only `publish` has `contents: write`; only `attest` has `id-token: write` and
`attestations: write`. `publish` uses the `gh` CLI only and does not verify
attestations.

Every job that runs `make` and `validate` check out with `fetch-depth: 0`:
the Makefile runs `git` on every call and the ancestor check needs
`origin/main`. The other jobs download artifacts and need no checkout.

The section selection, the hash-heading checks, the notes body and the
generated section come from `scripts/ci/changelog.ps1` (`pwsh` 7);
`tests/changelog/` holds its fixtures.

A hash heading has the form `## [<12 hex characters>] - YYYY-MM-DD`; the
parent of a tagged merge commit is its first parent. CHANGELOG matching is
line by line and allows `\r?` at line ends, because Linux checkouts are CRLF
too. The section ends at the next `## [` heading or at a column-0 link
definition
(`[<12-char hash>]: https://github.com/<owner>/<repo>/commit/<full 40-char hash>`),
so the link definitions at the bottom of the file never enter the notes. More
than one `### Development` block, or an empty section after removing it, fails.

## Safeguards in `publish`

1. No published release may exist for the tag (checked in `validate` and again
   in `publish`).
2. A leftover draft for the same tag is deleted first, so a re-run is safe.
3. The release is created as a draft, its three assets, their hashes against
   `SHA256SUMS` and the notes length are verified, and only then is it made
   public, with `gh release edit --draft=false` as the single last command.
4. The release mode depends on the ref, not on the event: a `workflow_dispatch`
   run on a tag ref publishes exactly like a push of that tag, with the same
   validation and safeguards, and a run on a branch ref never reaches `publish`.
5. Concurrency is per ref and never cancels a running release.
6. When the job fails, its summary says which state the release is in.

## When a release fails

- **Transient failure before anything is public** (a Sigstore outage, a runner
  error): re-run the failed jobs.
- **Defect found before or after publication:** handled the same way. The tag
  and the generated section stay, nothing is marked `[YANKED]`, the draft
  release may be deleted (optional), and the fix goes through an ordinary
  `fix/` pull request; the next version is released from it. A tag name is
  never reused, never moved, and the notes of the next version cover only the
  commits since the burned tag. A public defective release may be edited on
  GitHub to say it is withdrawn.
- **The release went public but the job reports failure:** handled as a normal
  defect, like any other. There is no extra post-release check.
- **A merged section that stays untagged:** nothing generates it a second time.
  Once `tag-release.yml` exists (it does not yet), re-run its failed run
  ("Re-run jobs"); until then push the tag by hand on the merge commit. If the
  defect is in the tagging workflow itself and a re-run cannot repair it, an
  ordinary pull request fixes the workflow and also removes the generated, untagged section from
  `CHANGELOG.md`; the next generation then covers the full range since the last
  tag.
- **Wrong or missing hash heading:** `validate` fails with the expected hash,
  before anything is built or public. A heading whose hash is not a commit
  matches no first parent, so it fails the same way ("found 0"); only a dry
  run reports it as not a commit in the run's history. The tag is burned: fix
  through a pull request and release the next version.

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
