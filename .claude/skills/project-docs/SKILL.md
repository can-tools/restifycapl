---
name: project-docs
description: Conventions for README.md, examples/*.can, and CHANGELOG.md — what each document is for, who updates it, and when — plus the project's comment-discipline rule for inline comments. Load this before touching README.md, anything under examples/, or CHANGELOG.md, and before any edit to src/, tests/, Makefile, scripts/, or .github/workflows/.
---

# Project documentation conventions

## Comment discipline

Inline comments explain **why**, never **what** or **how** — the code already says those. Three tiers, in order of preference:

1. **No comment.** The default. A clear name beats a comment.
2. **One line, WHY only.** When the reason for a non-obvious construct isn't recoverable from the code. One line, no wrapping.
3. **A multi-line block — only as an editing trap.** Permitted *only* when a future editor of that exact construct would otherwise reintroduce a specific, previously-observed bug, or violate a one-way-door decision. Must state the trap in its first line, must be ≤ 10 lines, must contain no historical narrative and no rejected-options discussion. If you cannot name the bug it prevents, it is not tier 3.

**Never write, anywhere in a source, build, or CI file:**

- a file-header block summarising "what this does and does not do", or enumerating traps/decisions;
- multi-paragraph rationale, or "why we chose Option A over Option B";
- **in an inline comment:** any reference to a plan section, stage number, or task ID (`plan.md §7.5`, `Stage 5`, `CPP-16`, `REV-3`, `D15`, `OQ3`), including bare criterion or section *numbers* that only resolve against a plan document. A comment must stand on its own and explain the current *why* without depending on an external tracker. Material worth recording at length goes in `docs/<topic>.md` — point **there**;
- commit/PR archaeology ("the bug just fixed in `c11a5b6`", "found the hard way on the first real run");
- the same rationale in two files. One substantive explanation plus a pointer from the other.

**Three categories are exempt from mechanical flagging, not from the rule:** program output read by a user at runtime (error messages, log lines, `echo`/`Write-Host` text); PowerShell comment-based help (`<# .SYNOPSIS … #>`), which `Get-Help` consumes; and generated artefacts such as auto-filled PR bodies. A sweep must not auto-file these as Must-fix — it cannot distinguish them from developer-facing comments, and a wrong edit breaks a working feature. **Each hit goes to human judgment, under the same principle: the citation never survives; what varies is only whether there is substance worth inlining in its place.**

**Worked patterns — the three shapes a fix takes, applied to real instances:**

- **Delete the whole sentence** (nothing to inline). `setup-dev-env.ps1`'s `.DESCRIPTION` block once read *"See docs/work/capl-rest-dll-rebuild/plans/plan.md (Stage 2, task BPE-1) for the authoritative spec."* The three preceding sentences already said what the script does, so the citation was removed outright.
- **Inline the substance** (the citation stood in for real content). `auto-pr.yml`'s checklist once read *"Plan folded into `plan.md`, incl. any renumbering (§7.8 criterion 6)"*; it became *"Plan folded into `plan.md`, incl. any task-ID/section renumbering (last commit before marking Ready for review)"* — the fact survives, the pointer doesn't.
- **Drop the prefix only** (the rest already stands alone). `auto-pr.yml`'s `exports.cpp` warning once opened with *"Per plan.md §7.10:"* before a sentence that was already self-contained; only the prefix was removed.

**Where explanatory material goes instead.** The discriminator is: **is this still a decision, or is it now just a fact?**

| Material | Home | Test |
|---|---|---|
| Rationale that still **constrains future work** — one-way doors, rejected options that must not be re-litigated, open trade-offs | `docs/work/<slug>/plans/plan.md` | *Could a future stage still decide this differently?* |
| A **settled** bug: what broke, how it was found, why the fix has the shape it has | `docs/<topic>.md` (e.g. `development-environment.md`, `ci-pipeline.md`) | *Is this now simply a fact about the world?* |
| The user-visible **fact** that something changed | `CHANGELOG.md` | — |
| Contributor/agent **conventions** | `CLAUDE.md` or the relevant skill | — |
| A bare plan/stage/task-ID citation with no substance of its own | Nowhere — delete it; only real substance (if any) migrates per the rows above | *Does removing it lose a fact, or only a pointer?* |

Archival documents under `docs/` are **topic-scoped, never stage-scoped.** Each section carries a one-line breadcrumb — `(found during Stage 6, BPE-9)` — so stage attribution survives without fragmenting the topic.

**Protected existing comments — protection covers substance, not identifiers.** These stay under this rule; any stage/task identifier they carry does not. The `Makefile`'s `/SUBSYSTEM:CONSOLE` and `make -n` trap comments are tier 3, blessed by plan.md §6a — the `make -n` trap runs to roughly 19 lines, a named, bounded exception to tier 3's own ≤10-line limit, kept because the bug it documents is real and non-obvious. `exports.cpp`'s CAPL naming-convention statement is required by plan.md §5 to live in the file being edited — trim it, but never remove it; its own `(Stage 5)` tag is not protected and comes out, while the statement itself stays.

## Three documents, three audiences — do not let them duplicate each other

This project already hit real drift twice from keeping the same fact in two
places (agent frontmatter vs. agent body; `04-FLOW` vs. the plan). The same
risk exists here, so each document has exactly one job:

- **`README.md`** — for a human finding this repo on GitHub, deciding
  whether/how to use the DLL from CAPL. Short: what the DLL does, how to
  build it (a pointer, not a duplicated tutorial), how a CAPL script
  references it, a link to `examples/`.
- **`CLAUDE.md`** — for agents and contributors working *on* this repo.
  Authoritative for build commands, directory layout, and conventions.
  `README.md` may restate the two or three facts a GitHub visitor needs
  (e.g. "run `make all`"), but never duplicates `CLAUDE.md`'s full detail —
  if a build instruction changes, it should need updating in one place,
  not two.
- **`examples/*.can`** — working, verified CAPL code showing real usage of
  exported operations. Not prose documentation; runnable samples.
- **`CHANGELOG.md`** — a dated, tag-linked record of what changed release
  over release, for someone upgrading between DLL versions.

If you're about to write the same sentence in two of these files, stop —
one of them should link to the other instead.

## README.md

- Keep it short. Link to `CLAUDE.md` for anything a contributor or agent
  needs; don't duplicate the Directory layout or Build sections there.
- Update it at these points, not continuously:
  - Development-environment-setup guidance (what `scripts/setup-dev-env.ps1`
    provisions and how to run it, linking to
    `docs/development-environment.md` for the detailed rationale) can land
    as soon as the script itself is real and proven — this is independent
    of, and not gated by, the build/usage-from-CAPL content below.
  - After Stage 5 (Hello DLL proven in CANoe): add a real, verified
    build/usage-from-CAPL snippet — not before, since there's nothing real
    to show.
  - After each export-contract-append stage (Stages 9–12): update the
    operations summary to reflect what's actually exported.
  - After Stage 13 (release pipeline live): add install/download
    instructions pointing at GitHub Releases.
- Do not write "coming soon" sections for the conditional Stage 15/16
  modules (struct mapping, request building) — same rule as `CLAUDE.md`'s
  `## Scope` section: don't document what hasn't been built and may never
  be.

## examples/*.can

- One example per operation group (sync, async, flattening, accessors),
  minimal and runnable — not a kitchen-sink demo file.
- **Mandatory before writing any example:** verify the CAPL syntax used
  against the official CANoe help, the same rule `capl-export-contract`
  and the plan's Stage 11 (HUM-16) already establish for associative
  fields. Do not trust a generated snippet's syntax without checking —
  an invented associative-field keyword was copied across many files in
  this project's previous iteration.
- Use the exact CAPL-visible operation names fixed at Stage 5 (see
  `capl-export-contract`) — an example using a name that doesn't match
  the export table is worse than no example.

## CHANGELOG.md

- Format: [Keep a Changelog](https://keepachangelog.com/) — an
  `## [Unreleased]` section at the top with `Added`/`Changed`/`Fixed`
  subheadings, moved to a dated, tag-linked section
  (`## [vX.Y.Z] - YYYY-MM-DD`) at release time.
- Tied to the project's existing git-tag versioning mechanism (see
  `msvc-build-conventions`): the version in a CHANGELOG heading is never
  hand-invented — it's the same tag that triggers the release workflow.
  Never write a CHANGELOG heading for a version that isn't an actual git
  tag.
- **Who writes what, and when:**
  - Whoever appends an entry to the export table (`cpp-implementer`, after
    `code-reviewer` sign-off) adds a one-line bullet under
    `## [Unreleased]` describing the new/changed operation, *at the time
    the export table changes* — not deferred, the same principle as "tests
    aren't optional, add them with the change."
  - `build-pipeline-engineer` owns the mechanics of cutting a release
    section (Stage 13): renaming `[Unreleased]` to the tagged version and
    date, and starting a fresh empty `[Unreleased]` above it.
