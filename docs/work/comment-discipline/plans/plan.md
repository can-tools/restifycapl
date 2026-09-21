# Plan: Comment discipline as a binding rule for coding agents

**Status:** approved (HUM-22 rule wording approved). Standalone for now — to be folded into `docs/work/capl-rest-dll-rebuild/plans/plan.md` as §6b at the next plan version bump (v15), after which this file carries only a pointer. See §7 and §8.

---

## 1. Goal

Make "WHY-only, minimal inline comments; design rationale and bug narratives live in documentation" a **binding, loaded rule** for every agent with `Edit`/`Write` on source or config — not an after-the-fact `code-reviewer` nice-to-have — and bring the three worst existing offenders into line so the rule isn't contradicted by the repo's own most-read files.

---

## 2. Evidence

The complaint is understated, not overstated. Precise counts:

| File | Total lines | Comment-only evidence |
|---|---|---|
| `.github/workflows/ci.yml` | 379 | **66-line file header** before `name: CI` (line 68), enumerating traps 1–4 as four paragraphs; trap 4 alone is 14 lines narrating the `VCPKG_ROOT` clobbering bug fixed in `c11a5b6`. Plus 5–12-line preambles on most steps. |
| `.github/workflows/auto-pr.yml` | 124 | **39-line header** before `name:` (line 41). Lines 5–6 say *"Read that section for the full design rationale; this file implements its requirements contract, it does not restate it"* — and are immediately followed by a 5-bullet "Deliberate choices, not defaults" list that restates plan.md §7.5. The file contradicts its own stated policy within 25 lines. |
| `src/module/exports.cpp` | 269 | **56 comment lines before the first `#include`** (line 59) — a 13-line file header plus a 43-line naming-convention essay. 13 lines of comment for the one-line `version.lib` pragma. A 36-line doc block on a 60-line function. Roughly 150 of 269 lines are comment; the actual C++ is ~100 lines. |
| `Makefile` | — | 22-line header, same shape. |

**The important finding is that the project already diagnosed this and wrote it in the one place the coding agents never read.** `plan.md` §6a line 216 already ends: *"BPE-19 is the counterweight: the same instinct applied without restraint produces comment bloat."* BPE-19 exists (trim duplicated `version.lib` rationale). BPE-13 at Stage 15 already says *"Sweep for accumulated comment bloat (BPE-19's category)."*

And yet: **zero** of `cpp-implementer.md`, `build-pipeline-engineer.md`, `test-engineer.md`, `code-reviewer.md`, `project-docs`, `msvc-build-conventions`, `capl-export-contract`, `cpp-testing-conventions` contains the word "comment" as a rule. The knowledge is recorded in a document that is, by design, read by `planner` and nobody else. That is the actual defect — not the agents' judgement, but a policy stored where it can't fire.

**Counter-evidence — not all of this is junk.** Two categories are deliberate and recorded:

- §6a explicitly *defends* the Makefile's long `/SUBSYSTEM:CONSOLE` and `make -n` comments: *"both are traps a future reader hits while editing that exact recipe."*
- §5 line 88 explicitly *requires* the `exports.cpp` naming-convention block: *"Downstream agents do not need to read the plan to find this — it is written in the file they will be editing."*

A flat "one line max, never multi-line" rule imported from the top-level session would delete both and contradict two standing project decisions. The rule in §3 therefore has a narrow, named escape hatch rather than being absolute.

**A second finding, from auditing what BPE-24 would actually evict:** `ci.yml`'s traps 2 and 3 are near-verbatim duplicates of sections that **already exist** in `docs/development-environment.md` (`## Manifest-mode triplet installs need separate install roots`, `## Pinning the vcpkg tool itself`). Trap 1 (shallow clone) maps to the same document's `Repair-ShallowVcpkgClone` material. **Only trap 4 — `ilammy/msvc-dev-cmd` clobbering `VCPKG_ROOT` via `GITHUB_ENV` — is genuinely CI-only and genuinely undocumented.** Most of the 66-line header is already redundant with an existing document, which makes the trim materially safer than first framed.

---

## 3. The rule (this text is the deliverable BPE-23 installs verbatim into `project-docs`)

> ### Comment discipline
>
> Inline comments explain **why**, never **what** or **how** — the code already says those. Three tiers, in order of preference:
>
> 1. **No comment.** The default. A clear name beats a comment.
> 2. **One line, WHY only.** When the reason for a non-obvious construct isn't recoverable from the code. One line, no wrapping.
> 3. **A multi-line block — only as an editing trap.** Permitted *only* when a future editor of that exact construct would otherwise reintroduce a specific, previously-observed bug, or violate a one-way-door decision. Must state the trap in its first line, must be ≤ 10 lines, must contain no historical narrative and no rejected-options discussion. If you cannot name the bug it prevents, it is not tier 3.
>
> **Never write, anywhere in a source, build, or CI file:**
>
> - a file-header block summarising "what this does and does not do", or enumerating traps/decisions;
> - multi-paragraph rationale, or "why we chose Option A over Option B";
> - restatement of a `plan.md` section — point to it instead (`see plan.md §7.5`), never repeat it;
> - commit/PR archaeology ("the bug just fixed in `c11a5b6`", "found the hard way on the first real run");
> - the same rationale in two files. One substantive explanation plus a pointer from the other.
>
> **Where explanatory material goes instead.** The discriminator is: **is this still a decision, or is it now just a fact?**
>
> | Material | Home | Test |
> |---|---|---|
> | Rationale that still **constrains future work** — one-way doors, rejected options that must not be re-litigated, open trade-offs | `docs/work/<slug>/plans/plan.md` | *Could a future stage still decide this differently?* |
> | A **settled** bug: what broke, how it was found, why the fix has the shape it has | `docs/<topic>.md` (e.g. `development-environment.md`, `ci-pipeline.md`) | *Is this now simply a fact about the world?* |
> | The user-visible **fact** that something changed | `CHANGELOG.md` | — |
> | Contributor/agent **conventions** | `CLAUDE.md` or the relevant skill | — |
>
> Archival documents under `docs/` are **topic-scoped, never stage-scoped.** Each section carries a one-line breadcrumb — `(found during Stage 6, BPE-9)` — so stage attribution survives without fragmenting the topic.
>
> **Protected existing comments — do not strip these under this rule:** the `Makefile`'s `/SUBSYSTEM:CONSOLE` and `make -n` trap comments (tier 3, blessed by plan.md §6a), and `exports.cpp`'s CAPL naming-convention statement (required by plan.md §5 to live in the file being edited — trim it, but never remove it).

### 3.1 Why `plan.md` is the wrong home for settled bug narratives

`plan.md` is a **living** document — revised wholesale v12 → v14, stages rewritten as their status changes, §7 absorbed an entire superseded standalone document. Material that stops being contested stops earning its place there, and a settled bug narrative is never contested again by definition. Putting archival material in a document that gets rewritten wholesale is how it quietly disappears at the next version bump.

`docs/development-environment.md` already *is* the archival home — its section headings are literally a catalogue of historical bugs and how they were found (`curl: no [schannel] vcpkg feature`, `where probes chained with bare & , not &&`, `gmock.lib: real copy-noise, not a link-time leak`, `Pinning the vcpkg tool itself`). The pattern was already established; it just wasn't named.

### 3.2 Why topic-scoped, not stage-scoped

1. **Stages dissolve; topics don't.** Stage 6 stops being a meaningful index the moment CI is just "the CI pipeline." Someone hitting the `LIBCMTD` false-positive in 2027 searches for `/MT` or `LIBCMT` — never for "Stage 6."
2. **The existing precedent is already topic-scoped and already spans stages.** `development-environment.md` holds Stage 2 material (vcpkg tool pin) *and* Stage 4 material (json.hpp SHA-256) in one document, correctly, because both are "how this environment is provisioned."
3. **Per-stage would fragment the stories that matter most.** The `/MT` narrative alone would be split across Stages 2, 4 and 6 — exactly the failure §6a names: *"A rule scoped to one file type does not generalise itself."*
4. **17 stages → 17 mostly-empty files.**

---

## 4. Where the rule lives — extend `project-docs`, do not create a new skill

**Decision: extend `.claude/skills/project-docs/SKILL.md` with §3 above.** Reasoning, against the alternative:

- `project-docs`'s entire existing job is *"where does explanatory material belong, and do not let two places duplicate it"* — its opening line is *"Three documents, three audiences — do not let them duplicate each other"* and it already ends a section with *"If you're about to write the same sentence in two of these files, stop."* Comments-vs-docs is that exact question one level down. It is the natural home, not a stretch.
- A **new** skill about where explanatory text belongs, sitting beside an existing skill about where explanatory text belongs, is precisely the antipattern both `project-docs` and §6a were written in response to. §6a records *three demonstrated drift instances* in this project from the same fact living in two places.
- `msvc-build-conventions` and `capl-export-contract` are both wrong homes — this rule applies to `.cpp`, `.yml`, `Makefile` and tests alike; scoping it under either would recreate exactly the failure §6a names: *"A rule scoped to one file type does not generalise itself."*

**The one real cost, and the highest-risk detail in this whole change:** `project-docs`'s current frontmatter says *"Load this before touching README.md, anything under examples/, or CHANGELOG.md."* If only the body is extended, the skill will never load when an agent edits `ci.yml` or `exports.cpp` — the rule would exist and never fire. **The `description:` must be rewritten** to trigger on any source/build/CI edit. Do **not** rename the skill: four agent files reference it by name and the rename buys nothing.

### 4.1 The rule text lives in exactly ONE file

**The full §3 rule text — tiers, banned list, redirect table, protected-comments list — goes into `.claude/skills/project-docs/SKILL.md` and nowhere else.**

Each of the four agent definition files gets **one pointer bullet**, roughly:

> - Comment discipline is non-negotiable: WHY-only, minimal. Design rationale, rejected options and bug narratives go in documentation, never inline. See `project-docs` for the tiers, the banned list, and where each kind of material belongs.

That is the whole edit to each agent file — one bullet under **Hard rules**, no substance duplicated. Two reasons this is structural rather than a preference:

1. **Duplicating the rule into four files would violate the rule it installs** — the banned list explicitly forbids "the same rationale in two files."
2. **`project-docs`'s own opening cites this precise failure as already having happened here:** *"This project already hit real drift twice from keeping the same fact in two places (**agent frontmatter vs. agent body**; `04-FLOW` vs. the plan)."* Agent-file duplication is one of the two named historical drift instances in this repository.

The pointer only works if the skill actually loads on a `.cpp`/`.yml` edit — which is why the `description:` rewrite in BPE-23 is load-bearing rather than cosmetic, and is re-checked in REV-14.

---

## 5. Stages

### Stage A — BPE-23: install the rule

**Agent:** `build-pipeline-engineer`. **Human approval: YES, before execution** (HUM-22 — approve the exact §3 wording; same shape as HUM-21's settings.json gate, since this changes how every agent behaves project-wide). **HUM-22 is APPROVED.**

Files touched:

1. `.claude/skills/project-docs/SKILL.md` — add §3 verbatim as a new **first** body section (so it's seen before the README/CHANGELOG material); rewrite `description:` to fire on edits to `src/`, `tests/`, `Makefile`, and `.github/workflows/` as well as the current three targets.
2. `.claude/agents/cpp-implementer.md` — one pointer bullet under **Hard rules** (see §4.1).
3. `.claude/agents/build-pipeline-engineer.md` — same pointer bullet, with the explicit addition that YAML step preambles and Makefile section headers are covered.
4. `.claude/agents/test-engineer.md` — same pointer bullet, **and add `project-docs` to its `skills:` list** (currently absent).
5. `.claude/agents/code-reviewer.md` — see Stage B; same edit pass, same agent.

**On including `test-engineer` pre-emptively — yes, include it.** There is no evidence against it yet only because `tests/` is still empty (Stage 8 hasn't started). Test files are a classic site for "explain the scenario" comment bloat, the marginal cost is one bullet, and the cost of waiting is a second round of edits to the same four files after the damage is already written. The alternative — wait for evidence — means the rule arrives *after* `tests/` is populated, which is the wrong order.

**On `build-pipeline-engineer` as the owner:** precedent is direct. plan.md §7.4 line 346 already assigns it skill-file ownership: *"`build-pipeline-engineer` owns the skill's content as a file"* (BPE-22, `stage-branch`). It also already owns `project-docs`-adjacent mechanics (CHANGELOG release mechanics, README build sections). One owner for one coherent policy edit beats splitting five near-identical edits across three agents. **Named caveat:** it is also the agent that wrote `ci.yml`'s trap header, so it is authoring its own constraint — acceptable because it is applying spec text handed to it verbatim, but it makes REV-14 non-optional rather than courtesy.

### Stage B — part of BPE-23: `code-reviewer` checklist item

**Agent:** `build-pipeline-engineer` (same pass). **Human approval: folded into Stage A's gate.**

**This deserves a named checklist item.** The proof is BPE-19 itself: comment verbosity surfaced once, as a "Nice to have", and has sat open-and-optional ever since while the problem got worse. Severity placement that produces no action is the wrong placement.

Add as item **7** in "What to check" (below the correctness items, above "General code quality"):

> 7. **Comment discipline** (`project-docs`): flag file-header rationale blocks, multi-paragraph "why we chose X", historical bug narratives, commit-hash archaeology, and any comment restating a `plan.md` section. A tier-3 block that does not name the bug it prevents is not tier 3.

And in **Output format**: rationale duplicated across two files, or restating `plan.md`, goes under **Should fix**; merely verbose goes under **Nice to have**. Add `project-docs` to `code-reviewer`'s `skills:` list (currently absent).

### Stage C — BPE-24 + CPP-17: trim the existing offenders

**Human approval: YES for CPP-17 before it lands** (it touches the export-contract file, even comment-only).

**Decision: trim now, in this same unit of work, scoped tightly — not deferred to Stage 15.** A rule introduced while the repo's three most-read files openly violate it teaches the opposite of the rule; every future agent reads `ci.yml` and `exports.cpp` as the house style. And this isn't new scope — BPE-19 already exists for exactly this category, and BPE-13 already promises the sweep; this pulls BPE-19 forward and widens it from `version.lib` to the three files. What stays deferred to BPE-13/Stage 15: `setup-dev-env.ps1` and everything under `docs/`.

**BPE-24** (`build-pipeline-engineer`) — `ci.yml`, `auto-pr.yml`, `Makefile` header:

1. **First, capture before deleting.** Create `docs/ci-pipeline.md` holding the CI-only material: trap 4 (`msvc-dev-cmd` clobbering `VCPKG_ROOT` via `GITHUB_ENV`), the `LIBCMTD` two-part-check rationale, the image-version cache-key reasoning, and the tag-collision branch-filter reasoning currently in the `on:` block comment. Each section takes a `(found during Stage N, TASK-ID)` breadcrumb.
   - A **new** file rather than extending `development-environment.md` because plan.md §5 is emphatic that the local and CI paths *"share the pin, never the mechanism and never the output"* — folding CI-only material into a document explicitly scoped to local provisioning would blur the one distinction that document exists to keep sharp.
   - For traps 1–3, add a pointer to `development-environment.md` rather than re-writing them; the material is already there.
2. Then delete both file-header essays outright, replacing each with ≤ 3 lines plus a pointer. Reduce step preambles to one WHY line each.
3. **Keep** the `/MT` `LIBCMTD` substring explanation inline (genuine tier-3 editing trap — it is the exact bug REV-4 caught) and the `Makefile`'s `/SUBSYSTEM:CONSOLE` and `make -n` comments (§6a-protected).
4. Comment-only diff; zero behaviour change; both CI legs must go green after.

**CPP-17** (`cpp-implementer` — `exports.cpp` is its file, not BPE's):

1. Trim the 43-line naming-convention essay to ~8 lines keeping the convention, the never-rename consequence, and the flat-namespace collision reason — **dropping the cdll.h archaeology, never the block itself** (§5 requires it in-file).
2. Trim the `version.lib` comment to one line plus a Makefile pointer (this closes BPE-19's `exports.cpp` half).
3. Trim the `CopyOwnVersionString` block to its return-code table plus the not-unit-testable line.

**CPP-17 needs no new document.** Its evicted material splits cleanly: the cdll.h archaeology (why not a bare `HelloWorld`; the flat CAPL-global namespace) supports a one-way door that still governs every future stage — that is genuinely live plan.md §5 material, and §5 already states the convention. The `version.lib` narrative compresses to one line plus a pointer. Inventing `docs/export-contract.md` would create a fourth near-duplicate of material already split across the `capl-export-contract` skill and plan.md §5 — the exact antipattern this whole change exists to stop.

**Hard constraint on CPP-17:** every `CAPL_DLL_INFO4` row, every `parTypes`/array string, and the `#pragma pack(push,1)`/`pop` pair must be byte-identical before and after. Verify with a diff restricted to comment lines.

### Stage C.1 — the disposition list (required deliverable of BPE-24 and CPP-17)

So that the migration check in REV-14 is checkable rather than a judgement call, **BPE-24 and CPP-17 must each report, alongside the diff, every comment block removed, with exactly one disposition each:**

- `captured → <file>#<section>` — text now exists there;
- `already covered → <file>#<section>` — text already existed there (this is where traps 1–3 land);
- `dropped as redundant → <reason>` — permitted **only** if the fact is recoverable from the code itself or already stated in a loaded skill. Without that bar this becomes an escape hatch that swallows the whole rule.

### Stage D — REV-14: review

**Agent:** `code-reviewer`. Reviews BPE-23 + BPE-24 + CPP-17 together, plus the rewritten `project-docs`.

Confirms:

1. **Rationale migration (Must fix).** For every comment block removed, verify its disposition against the actual target file — open it and confirm the text is there. A `captured →` pointing at a section that doesn't exist, or a `dropped as redundant` whose fact is not recoverable from the code or a loaded skill, is **Must fix**, not Nice-to-have.
   - Must-fix, deliberately: a lost design rationale is unrecoverable after merge, which puts it in the same class as the other Must-fix categories — silent, invisible to the compiler, and expensive later. Filing it as "Nice to have" is exactly what happened to BPE-19, and BPE-19 is why we're here.
   - REV-14 is the only moment this is cheaply verifiable. The deleted text is still recoverable from the branch diff; once the branch merges and the diff ages out, "was that rationale captured?" becomes unanswerable without archaeology.
2. The export table in `exports.cpp` is byte-identical.
3. Both protected comment categories survived (`/SUBSYSTEM:CONSOLE`, `make -n`, the `exports.cpp` naming convention).
4. The new `project-docs` `description:` frontmatter would actually cause the skill to load on a `.cpp` edit.

`code-reviewer` reviewing the checklist item just added to itself is fine — it reads its own definition at load time.

---

## 6. Task IDs and delegation summary

| ID | Task | Agent | Human gate |
|---|---|---|---|
| **HUM-22** | Approve the exact rule wording in §3 | Human only | **APPROVED** |
| **BPE-23** | `project-docs` new section + rewritten `description:`; pointer bullet in `cpp-implementer`, `build-pipeline-engineer`, `test-engineer`; checklist item 7 + output-bucket rule in `code-reviewer`; `project-docs` added to `test-engineer` and `code-reviewer` skills lists | `build-pipeline-engineer` | HUM-22 satisfied |
| **BPE-24** | Create `docs/ci-pipeline.md`; then trim `ci.yml`, `auto-pr.yml`, `Makefile` headers (absorbs BPE-19's Makefile half); produce disposition list | `build-pipeline-engineer` | No; CI must go green |
| **CPP-17** | Trim `exports.cpp` comments (absorbs BPE-19's `exports.cpp` half); produce disposition list | `cpp-implementer` | **YES** before landing |
| **REV-14** | Review all three together, incl. Must-fix rationale-migration check | `code-reviewer` | — |

BPE-19 closes as absorbed by BPE-24 + CPP-17.

**Branch:** `chore/bpe-23-comment-discipline`, per Stage 7 — the second real exercise of the branching flow after `chore/bpe-21-auto-pr`.

---

## 7. Where this plan lives

**`plan-writer` saves it standalone at `docs/work/comment-discipline/plans/plan.md`; it is folded into the main plan as a new §6b at the next plan version bump (v15), and reduced to a pointer.**

Justification — the same call made for the branching-strategy document, which is now precedent rather than theory. plan.md §7 line 279 records: *"Folded in from the former standalone `docs/work/branching-strategy/plans/plan.md`, which is superseded and now carries only a pointer here."* That document went standalone → folded in → pointer. This one follows the identical lifecycle, for the identical reason: it belongs **next to §6a**, because §6a already contains the half-statement of this rule (the "ceiling" sentence) and BPE-19, and splitting them across two permanent documents recreates the duplication both documents warn against.

It is **§6b, not a numbered stage** — §6a's own argument applies verbatim: *"a standing obligation attached to other work... Encoding it as a stage would imply it finishes."* Comment discipline never finishes either.

**Why standalone first:** `plan-writer`'s skill saves a plan verbatim; it is not built to surgically insert §6b, a §5 bullet, and four §12 table rows into an 830-line v14 document. The main plan is revised wholesale at each version bump (v12 → v14), and a v15 is due anyway once Stage 7's REV-13 closes — that is the natural, low-risk moment to absorb this. Transient duplication is time-boxed and has precedent.

---

## 8. Condition on the v15 fold-in

The fold-in is a verification step with an exit condition, not a bookkeeping deletion:

> This standalone document may not be reduced to a pointer until REV-14's rationale-migration check is recorded as passed, **and** the resulting `docs/` sections (`ci-pipeline.md`, any `development-environment.md` additions) are listed in §6b so they remain findable from the main plan.

Additional edits the v15 fold-in must make to the main plan:

- **§5** — one standing-constraint bullet stating the comment rule and pointing at `project-docs`.
- **§6a** — point its existing "ceiling" sentence at §6b instead of leaving it as an orphaned observation.
- **§12** — add BPE-23, BPE-24, CPP-17, REV-14, HUM-22 to the task tables; mark BPE-19 absorbed.

---

## 9. Risks

1. **Over-correction deletes load-bearing comments.** A naive application strips the `Makefile`'s `/SUBSYSTEM:CONSOLE`/`make -n` traps (§6a-protected) and `exports.cpp`'s naming-convention block (§5-required). *Mitigation:* tier 3 exists precisely for these, and the protect-list is written into the rule text itself, not just the task brief.
2. **The rule never fires.** If `project-docs`'s `description:` isn't rewritten, the skill loads only on README/examples/CHANGELOG edits and the rule is invisible where it's needed. Single highest-risk detail; called out explicitly in BPE-23 and re-checked in REV-14.
3. **Rationale lost rather than moved.** Trimming without capturing destroys material that is unrecoverable after merge. *Mitigation:* the §C.1 disposition list plus REV-14's Must-fix migration check.
4. **Two artifacts that must move together.** §6a already records the downside of relocating rationale into `docs/`: *"keeps the script readable but creates two artifacts that must move together"* — followed by **three demonstrated drift instances**. Moving more material into `docs/` increases that exposure. *Mitigation:* the stage/task breadcrumb in each section, and REV-14 verifying every pointer actually resolves. This is a real trade, taken deliberately — the alternative is 66-line headers.
5. **Export contract.** CPP-17 edits the contract file. Comment-only, but the file is guarded. *Mitigation:* byte-identical table rows, human gate, REV-14.
6. **Bitness parity:** unaffected. Comment-only changes; `ci.yml`/`Makefile` edits touch header text only, never the single parameterized rule or the matrix.
7. **YAML fragility.** A botched comment edit can break workflow parsing. *Mitigation:* both CI legs green is the exit condition for BPE-24, not "the diff looks right" — this project's most reliable defect predictor is "static review does not substitute for execution" (§13, seven recorded instances).
8. **Under-specification → re-litigation per comment.** *Mitigation:* the rule carries an explicit banned-list, not just a principle, and a falsifiable tier-3 test ("if you cannot name the bug it prevents, it is not tier 3").
9. **No tests.** Correctly — this is a comment-only and policy-only change. `make test` must still pass on both arches, as a regression check only.
