---
name: stage-branch
description: Branch-naming convention and create-and-push procedure for starting a new unit of work (plan stage, chore, fix, or docs change) on this repo. Load this before running `git switch -c` for any new work.
---

# Stage branch

Owner: `build-pipeline-engineer` (file content). Any agent starting a new
unit of work invokes this skill — `cpp-implementer` for a stage,
`build-pipeline-engineer` for a `chore/` branch, `test-engineer` for a
test-only branch. See `docs/work/capl-rest-dll-rebuild/plans/plan.md` for
the full branching strategy this skill is one piece of.

**Requires `.claude/settings.json`'s allowlist.** This skill's
create-and-push actions only work once `.claude/settings.json` allows
`git switch -c` and `git push` for the four prefixes below. If it doesn't,
branch creation and push are a human action instead.

## Naming convention

Two-digit zero-padded stage numbers, lowercase kebab-case, no personal-name
or date prefixes (one developer — the plan ID is the identity).

| Prefix | Form | Example |
|---|---|---|
| Stage work | `stage/<nn>-<kebab-slug>` | `stage/08-core-pure-logic` |
| Combined stages | `stage/<nn>-<nn>-<slug>` | `stage/08-09-core-and-http` |
| Non-stage follow-up owned by a task ID | `chore/<task-id>-<slug>` | `chore/bpe-18-changelog-entries` |
| Bug fix against merged work | `fix/<slug>` | `fix/stage5-abi-calling-convention` |
| Docs-only | `docs/<slug>` | `docs/plan-v15` |

`release/*` is deliberately absent — there are no release branches; tags are
cut from `main` directly.

## Create-and-push procedure

```
git switch main
git pull --ff-only
git switch -c <prefix>/<slug>
# ... work, git add, git commit ...
git push -u origin <prefix>/<slug>
```

Only these four prefixes (`stage/`, `chore/`, `fix/`, `docs/`) are permitted
by `.claude/settings.json`'s allowlist — do not invent a fifth.

## Scope stops at the push

This skill's job ends at `git push -u origin <prefix>/<slug>`. It does
**not** open the pull request — `.github/workflows/auto-pr.yml` does that
automatically on the push, opening a draft PR into `main` once the branch is
ahead of `main` and no open PR already exists for it. Do not run `gh pr
create` manually as part of this procedure.

## Why the naming convention is load-bearing, not cosmetic

The same four prefixes are hardcoded in four separate places, and they must
stay in sync:

1. `.github/workflows/auto-pr.yml`'s `push.branches` trigger filter.
2. `.claude/settings.json`'s permission allowlist (`git switch -c`/`git push`
   entries).
3. `.github/workflows/ci.yml`'s `push.branches` filter — see
   `msvc-build-conventions`.
4. `auto-pr.yml`'s PR base, which is always `main` regardless of prefix —
   see below.

A branch named outside this convention gets no automatic PR and a
permission prompt on push, silently. Deliberately loud when it happens, but
know it in advance rather than discovering it mid-task.

**The bot's PR base is always `main`, never a stacked parent.**
`auto-pr.yml` hardcodes `--base main`; it cannot infer stacking intent from
a push alone. If an interim clean diff against a stacked parent is wanted,
a human retargets that PR's base by hand in the GitHub UI. **Doing so
silently costs the PR its `pull_request` CI runs** — `ci.yml` filters that
trigger to `branches: [main]`, so a PR based on anything else stops
getting them. `push` runs on the branch's own prefix continue regardless,
so required checks still report; only the doubled `pull_request` run is
lost. A squash-merged parent (`chore/*`, `fix/*`, `docs/*`) needs the
rebase exception under Guardrails below instead of a retarget.

## Guardrails

- **Refresh long-lived branches by merging `main` in, never by rebasing.**
  Rebasing rewrites SHAs and severs the link between a commit
  `code-reviewer` already approved and the one that ends up merged. **One
  narrow, human-only exception:** a branch stacked on a squash-merged
  parent (`chore/*`, `fix/*`, `docs/*`) may be rebased instead, because
  merging `main` in is the *more* dangerous option there — both sides
  present different content for files absent from the merge base, and
  hand-resolving that conflict is worse than a clean rebase. The licence
  carries a mechanical proof obligation: `git diff <pre-rebase-tip> HEAD`
  must print nothing before pushing, proving the rebase rewrote SHAs and
  dropped duplicated commits without altering a single byte. Where the
  rebased branch touched `src/module/exports.cpp`, a confirmatory review
  must additionally verify the `CAPL_DLL_INFO_LIST4` rows are byte-identical
  to `main`'s. Both are required; neither alone is sufficient. This does
  not apply to agents or to ordinary refreshes.
- **Never have two open PRs that both touch `src/module/exports.cpp`.**
  Export-table stages are strictly serialized: a stale branch reordering
  the export table on merge is a silent, runtime-only defect that no
  compiler catches.
- **Stage branches should not edit `plan.md`.** Keep plan revisions on their
  own `docs/plan-vNN` branch to avoid `plan.md` becoming a conflict hotspot.
- **Merging is human-only**, regardless of what this skill or `auto-pr.yml`
  automate. No agent merges a PR, ever.
