---
name: conventional-commits
description: Commit-message rules for this repo. Load before writing any commit message; the messages feed the generated CHANGELOG and release notes.
---

# Conventional Commits

Format: `type(scope)!: description`, an optional body, optional footers.
The scope is optional. Reference: docs/recommendations/Conventional_Commits_v1.0.0.md.

## Type matches the branch prefix

| Branch prefix | Commit type | Release effect |
|---|---|---|
| `feat/` | `feat:` | minor |
| `stage/` | `stage:` | minor |
| `fix/` | `fix:` | patch |
| `chore/` | `chore:` | none (Development) |
| `docs/` | `docs:` | none (Development) |

The release effect comes from the branch prefix, not from the commit types: a `feat/`, `stage/` or `fix/` pull request releases even when all its messages are `chore:` (it is then listed by its branch name), so a change with nothing for users goes on a `chore/` branch.

`stage:` means `feat:` and is used only for user-visible changes. Internal
changes on a `stage/` branch use `chore:`.

## Breaking changes

Mark them with `type!:` or a `BREAKING CHANGE:` footer, on any branch type. The
bot reads the marker from the PR's commit messages. It gives minor while the major
version is 0 and major from 1.0.0; 1.0.0 is reached with a `bump:major` label.

## Subjects are release notes

The description of a `feat:`, `stage:` or `fix:` commit becomes a release-note
entry for DLL and CAPL users. Write it for them: what changed in the
DLL or the CAPL surface, using CAPL declaration style for function names. No
plan, stage or task identifiers.

## Not covered

The squash-merge subject is the PR title, which is the branch name; do not edit
it. Commit-message hooks are not decided.
