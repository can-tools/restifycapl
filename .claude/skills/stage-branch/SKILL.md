---
name: stage-branch
description: Branch-naming convention and create-and-push procedure for starting a new unit of work (plan stage, chore, fix, or docs change) on this repo. Load this before running `git switch -c` for any new work.
---

# Stage branch

Owner: `build-pipeline-engineer` (file content). Any agent starting a new
unit of work invokes this skill — `cpp-implementer` for a stage,
`build-pipeline-engineer` for a `chore/` branch, `test-engineer` for a
test-only branch. See `docs/work/capl-rest-dll-rebuild/plans/plan.md`
section 7 for the full branching strategy this skill is one piece of.

**Blocked on HUM-21.** This skill is only usable once `.claude/settings.json`
allows `git switch -c` and `git push` for the four prefixes below. Before
that lands, branch creation and push are a human action.

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
3. `.github/workflows/ci.yml`'s `push.branches` filter (BPE-20 — see
   `msvc-build-conventions`).
4. `auto-pr.yml`'s PR base, which is always `main` regardless of prefix
   (BPE-27) — see below.

A branch named outside this convention gets no automatic PR and a
permission prompt on push, silently. Deliberately loud when it happens, but
know it in advance rather than discovering it mid-task.

**The bot's PR base is always `main`, never a stacked parent (BPE-27, plan.md
§7.2/§7.13's Defect 2).** `auto-pr.yml` hardcodes `--base main`; it cannot
infer stacking intent from a push alone. If an interim clean diff against a
stacked parent is wanted, a human retargets that PR's base by hand in the
GitHub UI. **Doing so silently costs the PR its `pull_request` CI runs** —
`ci.yml` filters that trigger to `branches: [main]`, so a PR based on
anything else stops getting them. `push` runs on the branch's own prefix
continue regardless, so required checks still report; only the doubled
`pull_request` run is lost. A squash-merged parent (`chore/*`, `fix/*`,
`docs/*`) needs the human-only rebase recipe in plan.md §7.13's "rebase
exception" instead of a retarget — see there, not re-derived here.

## Guardrails

- **Refresh long-lived branches by merging `main` in, never by rebasing.**
  Rebasing rewrites SHAs and severs the link between a commit
  `code-reviewer` already approved and the one that ends up merged. **One
  narrow, human-only exception** — a branch stacked on a squash-merged
  parent — is documented in plan.md §7.13's "rebase exception"; it does not
  apply to agents or to ordinary refreshes.
- **Never have two open PRs that both touch `src/module/exports.cpp`.**
  Export-table stages (10–13) are strictly serialized — see plan.md 7.9/7.10
  for why a stale branch reordering the export table on merge is a silent,
  runtime-only defect that no compiler catches.
- **Stage branches should not edit `plan.md`.** Keep plan revisions on their
  own `docs/plan-vNN` branch to avoid `plan.md` becoming a conflict hotspot.
- **Merging is human-only**, regardless of what this skill or `auto-pr.yml`
  automate. No agent merges a PR, ever.
