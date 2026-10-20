Version preview: <!-- version -->v0.2.0 (v0.1.0 + minor)<!-- /version -->

## Release notes

Development entries stay in CHANGELOG.md and are left out of the GitHub release notes.

### Added

- feat/new-api ([#21](https://github.com/can-tools/restifycapl/pull/21))
- feat/no-number

## Warnings

- "feat/new-api" has no feat, stage, fix or breaking commit message; it is listed by its branch name
- the subject "feat/no-number" has no "(#N)"; its commit messages and breaking markers were not read
- "feat/no-number" has no feat, stage, fix or breaking commit message; it is listed by its branch name
- the subject "weird-branch" does not start with a feat/, stage/, fix/, chore/ or docs/ branch prefix; it is listed under Development

## Review

The release bot maintains this pull request: commits pushed to `release/next` by hand are overwritten the next time it rebuilds the branch, and the checklist below is reset on every rebuild. Squash-merging the pull request releases the version.

- [ ] The release notes above read correctly for DLL and CAPL users
- [ ] The `bump:*` label matches the intended version (the preview is the last tag plus the label's level)
