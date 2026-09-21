# Plan — untrack `lib/README` (BPE-26)

## 1. Goal

Stop versioning `lib/README` in git while leaving the file on disk and leaving the script's generation of it exactly as-is. Remove it from the index, ignore it going forward, and correct every place in the repo that currently documents the opposite (tracked-on-purpose) rationale — including the plan records that justified it — so the decision can't be silently reinvented later.

## 2. What changes, and what explicitly does not

| Changes | Does not change |
|---|---|
| `lib/README` leaves git's index | The file stays on your disk, byte-identical, untouched |
| `.gitignore` gains an entry + a rewritten comment | `scripts/setup-dev-env.ps1`'s generation behaviour |
| `msvc-build-conventions` skill reworded | The Makefile, CI, `vcpkg.json`, anything shipped |
| `plan.md` records corrected + new decision record | The export contract, `/MT`, bitness parity — all untouched |

## 3. Two things decided before this runs

Both items below were raised as open questions and **approved by the user as written**.

### 3a. The `zs.lib` fact — the only real casualty (APPROVED)

The decision was that `lib/README` isn't needed *in the repository*. That decision was not meant to also decide what happens to the ~65 lines of static prose the file carries, so it was checked rather than assumed.

**Almost all of it is already duplicated in `C:\Workspace\restifycapl\.claude\skills\msvc-build-conventions\SKILL.md`** — a file that is tracked *and* that agents actually load, which makes it a strictly better home than `lib/README` ever was:

| Fact | Already in the skill? |
|---|---|
| product-linked `lib/<arch>/` vs test-only `lib/gtest/<arch>/` | Yes — "Directory conventions" |
| `gtest_main.lib` lands in `manual-link/` (the vcpkg port's patch) | Yes — "Dependency acquisition", in full detail |
| `json.hpp` is pinned, SHA-256 verified, committed source | Yes |
| `/MT` verification via `dumpbin /directives`, expect `LIBCMT` never `MSVCRT` | Yes |
| SChannel arrives via curl's default `ssl` feature | Yes |
| **zlib's import library is `zs.lib`, not `zlib.lib`** | **No — the skill only says "zlib arrives transitively with libcurl"** |

So the loss is **one fact, not 65 lines.** And it's a load-bearing one: it's the name that has to appear in the Makefile's `LIBS` at Stage 9 (BPE-10), and it is genuinely unguessable from the port name.

**Decision: do not create `docs/dependencies.md`.** Relocating the prose would build a second file that drifts from the script's template for no gain, since the skill already covers it. Instead, **add one line to the skill's zlib bullet.** Approved text:

> `- zlib arrives transitively with libcurl. Its import library is named ``zs.lib``, not ``zlib.lib`` — not guessable from the port name, and it is the name the Makefile's ``LIBS`` must use.`

One line, in the file agents load, closes the entire gap. This is the only accepted loss from untracking `lib/README`.

### 3b. Both generator "defects" — fix neither (APPROVED)

**Both findings were entirely contingent on the file being tracked, and untracking dissolves both.** Concretely:

**The `Last refreshed:` timestamp — keep it. Removing it would now be actively wrong.** The only argument for deleting it was git-diff noise, and there is no more diff. Worse, the secondary argument was "`git log -1 -- lib/README` answers the question better" — that argument dies with tracking, because the file won't be in git at all. For a local-only artifact, "when was my dev environment last provisioned?" becomes a question *only this line can answer*. It goes from misleading-on-a-tracked-file to genuinely useful-on-a-local-file. Leave it alone.

**The `<unknown>` overwrite on `-SkipVcpkg` — leave it too.** The framing was "destructive to a tracked file," and that's gone. What remains is arguably correct behaviour: if a run didn't probe vcpkg, `<unknown -- vcpkg step did not run or failed; re-run setup-dev-env.ps1>` is a *truthful* statement about that run. Preserving stale-but-real numbers would make the file assert versions the run never verified — which is precisely the BPE-25 mistake in miniature. Loud and honest beats quiet and plausible.

Net effect: **no changes to `scripts/setup-dev-env.ps1` behaviour at all.** The branch gets meaningfully smaller and lower-risk. The one optional script touch is prose-only (item 4 in the stage table).

## 4. Branch

**`chore/bpe-26-untrack-lib-readme`**, off current `main`.

Not the earlier `chore/bpe-25-lib-readme-regen` name — nothing is being regenerated or committed about the README any more, and the work isn't BPE-25's stopgap closure. Worth recording the relationship precisely: **untracking doesn't *resolve* BPE-25's stopgap, it *moots* it.** The hand-edited note was a claim in the repo; the repo stops carrying the file, so the claim stops existing. (The real run still happened and still verified libcurl 8.21.0#1 / GoogleTest 1.17.0#3 against a live toolchain — that evidence should be recorded in plan.md rather than lost, and Stage 5 below does that.)

Naming follows §7.3's `chore/<task-id>-<slug>`. New IDs: **BPE-26** (the work), **REV-15** (the review).

## 5. Stages

All stages are owned by **`build-pipeline-engineer`**, including the `plan.md` edits. That last part is a deliberate deviation from the usual plan-ownership — see the note under the table.

| # | Outcome | Files | Human approval first? |
|---|---|---|---|
| 1 | Branch created off up-to-date `main` | git only | No |
| 2 | `.gitignore`: add `lib/README`, rewrite the now-false comment | `C:\Workspace\restifycapl\.gitignore` lines 44–50 | No |
| 3 | `git rm --cached lib/README` — stages the index removal, leaves the working-tree file untouched | index only | No |
| 4 | Skill: reword the `lib/README` mandate; add the `zs.lib` line (3a) | `.claude\skills\msvc-build-conventions\SKILL.md` ~line 138 and the zlib bullet | No |
| 5 | `plan.md`: new decision record + correct the five stale spots | `docs\work\capl-rest-dll-rebuild\plans\plan.md` | No |
| 6 | *(optional, prose only)* Template's own ignore-list mentions `lib/README` | `scripts\setup-dev-env.ps1` ~line 1306 | No |
| 7 | Commit, push; `auto-pr.yml` opens the draft PR; fill the `TODO` line with `BPE-26 / REV-15` | — | No |
| 8 | `REV-15` — `code-reviewer` pass against the acceptance checks below | — | No |
| 9 | Merge to `main` after both CI legs green | — | **Yes — the merge is the gate** |

**Why `build-pipeline-engineer` edits `plan.md` here rather than `plan-writer`.** The plan changes are five surgical corrections plus one new subsection in an 800-plus-line file. `plan-writer` has `Write` but not `Edit`, so it would have to read and re-emit the entire document — unacceptable risk of collateral damage on a file this size and this load-bearing. `build-pipeline-engineer` has `Edit`, and already owns `.gitignore`, the skill and the script, so one agent carries the whole branch coherently. `plan-writer` stays reserved for persisting *new* plan documents, which is what it's for.

### Stage 2 — proposed `.gitignore` text

Replacing lines 44–50. The current comment (*"Scoped to subdirectories so lib/README itself stays tracked"*) states exactly the opposite of the new policy and must go:

```
# Provisioned dependency trees -- populated by vcpkg via
# scripts/setup-dev-env.ps1 (local) or the CI provisioning step. NOTHING
# under lib/ is tracked: the .lib files are per-environment build output,
# and lib/README is per-environment GENERATED output, rewritten in full by
# the "lib skeleton + README" step of setup-dev-env.ps1 on every run. It is
# local reference output, not a repo document -- do not re-add it to git.
# See plan.md Stage 7 (BPE-26) for the decision and its rationale.
lib/x86/
lib/x64/
lib/gtest/
lib/README
include/vendor/gtest/
```

Explicit `lib/README` rather than collapsing everything to a bare `lib/`: it keeps the entries readable, and it doesn't silently swallow some future file that genuinely does belong under `lib/`. The "do not re-add it" sentence is deliberate — it's the anti-reinvention marker in the place someone will actually be standing when they're tempted.

### Stage 3 — confirming the git incantation

`git rm --cached lib/README` is the correct and exact operation for "stop versioning it, don't delete it." It removes the path from the **index only**; the working-tree file is not touched. Three things worth knowing:

- **It stages a deletion that must be committed.** Until stage 7's commit lands, the file is still tracked in `HEAD`. This is not optional cleanup — it's the actual change.
- **Order matters within the commit.** Do stages 2 and 3 in the same commit. With the `.gitignore` entry absent, the file would reappear in `git status` as untracked; with both together, it simply vanishes from git's view.
- **One consequence to be aware of, not a problem here.** A commit that deletes a tracked file deletes it from the working tree of *anyone who pulls it* — `.gitignore` does not protect against that, since ignore rules only govern untracked files. The committing machine is unaffected (`--cached` leaves its copy alone). CI is unaffected — per BPE-24/REV-14, CI never runs `setup-dev-env.ps1` and never reads this file. A future fresh clone simply won't have `lib/` until the script creates it, which Step 8 does via `New-Item -Force`.

### Stage 4 — proposed skill wording

Replace line ~138, currently `- Record exact versions in `lib/README`.`, with:

> `- ``scripts/setup-dev-env.ps1`` writes the resolved versions into ``lib/README`` on every run. That file is **local, generated and deliberately untracked** (see ``.gitignore``) — per-environment output, not a repo document. Never commit it, never hand-edit it, and never treat it as a version source: the authoritative pin is ``vcpkg.json``.`

Reworded rather than deleted, deliberately. Dropping the rule outright would leave the script's behaviour undocumented in the file agents load, and the next well-meaning agent would find an untracked generated file, read it as an oversight, and helpfully `git add` it. The rule now carries its own guardrail.

Plus the `zs.lib` line from 3a, in the zlib bullet.

### Stage 5 — what goes into `plan.md`

**Placement.** Topically this is a Stage 2 / provisioning matter, but the stale rationale lives in Stage 2's records and the *decision* arose from Stage 7's closeout re-run, so splitting by kind is right: **one canonical narrative in a new §7.x subsection, with Stage 2's records corrected in place and pointing at it.** Leaving a now-false rationale at line 144 while the correction sits 400 lines away is exactly how a plan starts contradicting itself.

**New subsection — `#### 7.12 lib/README: from tracked-on-purpose to untracked (BPE-26)`** (renumber if the v15 draft already claims 7.12), covering four things:

1. **The original problem.** BPE-25's implementer had no MSVC/vcpkg toolchain, hand-edited `lib/README`'s versions as a stopgap with an explicit STOPGAP NOTE, and that hand-edit rode through PR #1 and PR #2 onto `main`. The real run on 2026-09-21 (23 OK / 0 WARN / 0 FAIL, including `Assert-VcpkgBaselinePin`) confirmed **libcurl 8.21.0#1 and GoogleTest 1.17.0#3**, matching CI, and regenerated the file. **Record those versions and that run in the plan text** — with the file leaving the repo, plan.md becomes the durable home for that evidence.
2. **The retracted rationale, recorded as retracted.** The original justification for tracking was partly that a git diff on the file would surface local-vs-committed divergence. **That argument does not hold and should not be reinvented:** the unconditional `Last refreshed:` timestamp makes the file dirty after 100% of runs, so the "signal" fires always and carries no information; the comparison isn't against CI at all (the file has no CI relationship — it records whatever the last committer happened to have); and every divergence it could surface is *already* caught earlier and louder by the script itself — `Assert-VcpkgBaselinePin` FAILs on baseline/tool drift (lines 678–748), and lines 1207–1209 WARN on an x86/x64 gtest version mismatch. Writing this down as retracted, not merely deleting it, is the point.
3. **The two generator "defects" found and deliberately not fixed** — the unconditional timestamp and the `<unknown>` overwrite — with the reasoning from 3b: both were defects *only because the file was tracked*; untracked, the timestamp becomes the only way to answer "when was this environment provisioned?", and `<unknown>` is a truthful report of a run that didn't probe. Record this so nobody "fixes" them later on the strength of the earlier analysis.
4. **The decision.** `lib/README` is not needed in the repository. It stays generated locally and on disk; it leaves the index. The static prose it carried survives in `msvc-build-conventions`, which was always the better home, with the `zs.lib` name added to close the one real gap.

**Five existing spots to correct:**

| Line | Currently says | Action |
|---|---|---|
| ~144 (BPE-2 record) | *"Scoping to subdirectories rather than `lib/` keeps `lib/README` tracked."* | Reverse; point to §7.12 |
| ~154–156 (BPE-6 record) | Praises the template and says editing the template was right *because the file is tracked-and-generated* | Keep the template praise (still true), correct the tracked premise, point to §7.12 |
| ~651 (task table) | `BPE-2 … keeps `lib/README`` | Reword the row |
| ~655 (task table) | `BPE-6 … `lib/README` template` | Add "(untracked, see §7.12)" |
| ~176 (BPE-16) | Suggests the `lib/README` template as "a reasonable second home" for the `version-string` note | Demote — an untracked file is a poor home for a note meant to be read; the skill is the only home now |

**Line ~791 needs no change** — *"Generated files silently reverting hand edits"* is still exactly true; the hazard is about generation, not tracking. Confirm it reads correctly rather than editing it.

**Add BPE-26 / REV-15 rows to §7.11's task table.**

## 6. Sequencing — one hard precondition

**Do not create `docs/plan-v15` until this branch merges.**

§7.10 says *"Keep plan revisions on their own `docs/plan-vNN` branch. Stage branches should not edit `plan.md`"* — and this branch edits `plan.md`. The rule exists to stop `plan.md` becoming a conflict hotspot when branches run concurrently. With one developer and strict ordering, the hazard simply doesn't arise: land `chore/bpe-26-untrack-lib-readme` first, then branch `docs/plan-v15` off a `main` that already contains the correction. Two open branches both editing `plan.md` is the one thing to avoid, and sequencing avoids it completely.

This is a conscious, narrow exception to §7.10's letter that preserves its purpose. Worth one sentence in §7.12 so it's a recorded judgment rather than a lapse.

## 7. Acceptance checks for REV-15

1. `git ls-files lib/` returns **nothing**.
2. `git check-ignore -v lib/README` reports the new `.gitignore` rule.
3. `lib/README` **still exists on disk**, unmodified (the whole point).
4. Repo-wide grep for `lib/README` — every surviving hit is consistent with untracked. Expected survivors: `.gitignore` (new entry + comment), the skill (reworded), `scripts/setup-dev-env.ps1` (the template, still correct), `plan.md` (§7.12 + the five corrected spots). **Zero hits asserting or implying it is tracked.**
5. `.gitignore` and the skill don't contradict each other.
6. No `CHANGELOG.md` entry — and say so explicitly in the PR body. Nothing shipped changes, no version moves, no build behaviour changes. Stating the decision is what stops it reading as the BPE-18 oversight repeating.

## 8. Risks

- **Export contract, `/MT`, bitness parity: all untouched.** No C++, no `exports.cpp`, no `exports.def`, no Makefile, no triplet, no link line. Zero ABI surface. No human gate needed beyond the merge itself.
- **Build breakage: none.** Nothing in the build reads `lib/README` — not the Makefile, not CI.
- **Real risk 1 — an incomplete sweep.** Half-corrected records leave the plan self-contradicting, which is worse than leaving it alone. Acceptance check 4 is the mitigation and should be run as written.
- **Real risk 2 — silent re-tracking later.** A future agent finds an untracked generated file and "helpfully" commits it. Mitigated by putting the do-not-re-add instruction in both places someone would be standing: `.gitignore` and the skill.
- **Real risk 3 — losing BPE-25's verification evidence.** The live-verified versions currently live in a file that's about to leave the repo. Mitigated by Stage 5 item 1 recording them in `plan.md`. Do not skip that.
- **Accepted cost.** A fresh clone has no `lib/` directory and no dependency documentation at that path until `setup-dev-env.ps1` runs; toolchain-less readers rely on `msvc-build-conventions` instead. That is the accepted trade, and 3a shrinks it to near zero.
