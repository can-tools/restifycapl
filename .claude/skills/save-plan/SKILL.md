---
name: save-plan
description: Persists an already-written plan (from the planner agent, plan mode, or the current conversation) verbatim to a file under docs/work/<slug>/plans/. Use only when a finished plan already exists in the conversation — never invent a plan to satisfy this skill.
---

# save-plan

## What this skill does

Takes a plan that has already been produced (by the `planner` agent, by
Claude Code's built-in Plan Mode, or written directly in the conversation)
and writes it **verbatim** to disk, in a predictable location. It does not
generate, rewrite, shorten, or "improve" the plan — it only persists it.

This skill requires the `Write` tool. It is used by the `plan-writer` agent
(which has `Write`), not by `planner` (which does not) — `planner` delegates
the actual save step to `plan-writer`.

## Before saving

- If no finished plan exists yet in the conversation, say so and stop — do
  not draft one just to have something to save.
- If it's not clear what work item (slug) this plan belongs to, ask.
  Prefer a short, kebab-case slug describing the feature/change, e.g.
  `patch-support`, `async-timeout-handling`.

## Directory layout

Work is organized by **unit of work**, not by document type, so everything
related to one feature/change/investigation lives together:

```
docs/work/<slug>/
├── plans/
│   └── plan.md          <- this skill writes here
├── reviews/              <- code-reviewer output, if persisted
└── notes/                <- anything else worth keeping
```

- `<slug>` is a short kebab-case name for the unit of work (no date prefix
  needed unless the same slug is reused for unrelated work later — if so,
  prefix with the date: `<yyyy-mm-dd>-<slug>`).
- If `docs/work/<slug>/plans/` doesn't exist yet, create it.
- If `plan.md` already exists for this slug, treat the new save as a new
  revision: ask whether to overwrite, or save as `plan-v2.md`,
  `plan-v3.md`, etc., rather than silently overwriting a plan that may
  still be in use.

## Temporary plans

If the plan is exploratory and not meant to be kept in version control, save
it instead to `.plans/<slug>/plan.md` (add `.plans/` to `.gitignore` if not
already present) rather than under `docs/work/`.

## What to write

- Save the plan text exactly as produced — do not summarize, reformat
  headings, or drop sections.
- Add a one-line header above the pasted plan noting the date and, if known,
  which agent or mode produced it, e.g.:
  `<!-- Produced by: planner agent, 2026-09-16 -->`
