Version preview: <!-- version -->v0.1.1 (v0.1.0 + patch)<!-- /version -->

> [!CAUTION]
> **Breaking change.** This release contains:
>
> - restifyFoo takes the buffer size first ([#21](https://github.com/can-tools/restifycapl/pull/21))
> - restifyGetSync returns -7 on a timeout ([#22](https://github.com/can-tools/restifycapl/pull/22))
> - remove the legacy include ([#23](https://github.com/can-tools/restifycapl/pull/23))

## Release notes

Development entries stay in CHANGELOG.md and are left out of the GitHub release notes.

### Breaking changes

- restifyFoo takes the buffer size first ([#21](https://github.com/can-tools/restifycapl/pull/21))
- restifyGetSync returns -7 on a timeout ([#22](https://github.com/can-tools/restifycapl/pull/22))
- remove the legacy include ([#23](https://github.com/can-tools/restifycapl/pull/23))

### Added

- add restifyBaz ([#21](https://github.com/can-tools/restifycapl/pull/21))
- change the default timeout ([#24](https://github.com/can-tools/restifycapl/pull/24))

## Review

The release bot maintains this pull request: commits pushed to `release/next` by hand are overwritten the next time it rebuilds the branch, and the checklist below is reset on every rebuild. Squash-merging the pull request releases the version.

- [ ] The release notes above read correctly for DLL and CAPL users
- [ ] The `bump:*` label matches the intended version (the preview is the last tag plus the label's level)
- [ ] The breaking changes in the caution block above are intended
