Version preview: <!-- version -->v0.1.1 (v0.1.0 + patch)<!-- /version -->

## Release notes

Development entries stay in CHANGELOG.md and are left out of the GitHub release notes.

### Fixed

- report timeouts ([#22](https://github.com/can-tools/restifycapl/pull/22))

## Warnings

- the fix/ pull request #22 changed src/module/exports.cpp; confirm that no export table row was renamed, reordered or removed

## Review

The release bot maintains this pull request: commits pushed to `release/next` by hand are overwritten the next time it rebuilds the branch, and the checklist below is reset on every rebuild. Squash-merging the pull request releases the version.

- [ ] The release notes above read correctly for DLL and CAPL users
- [ ] The `bump:*` label matches the intended version (the preview is the last tag plus the label's level)
