# Stage 13 — Typed JSON accessors (fourth append to the export table)

Slug: `stage-13-json-accessors`. Branch: `stage/13-json-accessors`, created from green `main`.

## 1. Goal

Add typed reads to the DLL. They work on the Stage 12 document (`documentId`, the 8-slot store) at an RFC 6901 path and return CAPL `long`, `float` (C++ `double`) or a boolean as `long`, plus one array helper that returns the element count at a path. They use the Stage 8 `To*` family, which is strict, so they never coerce a value to another type.

Expose them to CAPL as rows 26–29 of `CAPL_DLL_INFO_LIST4`, in the `Json` category. Each row gets its 1:1 `restLib` wrapper in `capl/includes/libs/restify-json.cin` in the same change.

Out of scope:
- No cache (OQ1).
- No coercion (OQ3).
- No struct mapping or body building (deferred modules).
- No `examples/` (OQ9).

## 2. Open questions and proposed decisions

Accepting this plan (HUM-35, step 1) means accepting the proposals below. Any you reject change §4 and the steps before work starts.

| OQ | Question | Proposal |
|---|---|---|
| OQ1 | Which rows? The master plan's sketch has "typed point reads (integer/float/bool/string), array helpers (length, element-by-index), optional cache". | **Four rows:** `ReadLong`, `ReadDouble`, `ReadBool`, `CountElements`. **Leave out:** (a) a strict string read, because `restifyJsonReadValue` with `valueType == 1` already does this; (b) element-by-index, because RFC 6901 paths (`/items/3`) already cover it; (c) the cache, because the store already keeps the parsed DOM per `documentId` (`FlattenResult::document`), so there is nothing left to cache. TEST-8's "cache invalidation between responses" becomes "reads follow the document id; a discarded id gives −31". This keeps the investment in path access small, since struct mapping is the long-term direction. |
| OQ2 | Names and row order. | Row 26 `restifyJsonReadLong`, 27 `restifyJsonReadDouble`, 28 `restifyJsonReadBool`, 29 `restifyJsonCountElements`. All follow `restify<Family><VerbNoun>` with no `Sync`/`Async` suffix. No strict prefix exists among the 29 names (`ReadLong`/`ReadDouble`/`ReadBool` against `ReadEntry`/`ReadValue`; `CountElements` against `CountEntries`). |
| OQ3 | How strict are the reads? | Exactly the `To*` semantics, no coercion. A JSON string `"42"` gives −14. `ReadBool` accepts only `true`/`false`; numeric 0/1 gives −14. `ReadLong` accepts an integer or a whole-valued float (`5.0` → 5); a fractional number gives −17 (never truncated); outside the `int32` range gives −16. JSON `null` gives −15 and `value` stays 0. Anyone who wants coercion reads the text with row 22 and converts it in CAPL. |
| OQ4 | Should a wrong value type get its own code? −14 already means a path/container kind mismatch. | **Reuse −14** and document both meanings in `docs/capl-json-surface.md`. No new `Status` code; −37..−39 stay free. The alternative would be a new −37 `ValueTypeMismatch` (`Status` is append-only, so once added it stays forever). |
| OQ5 | What does `CountElements` count? | **Arrays only.** An empty array gives 0. An object or scalar gives −14; `null` gives −15. Path `""` (the whole document) is allowed. Objects are left out because a member count without member names is of little use, and the flattened entries already list them (rows 20/21). |
| OQ6 | How does a `double` get back to CAPL? | Through a reference parameter `float&`: type character `'F' - 128`, a new constant `kRefDouble` next to `kRefLong`/`kRefDword`. This is the first `F` and the first reference to an 8-byte type in the table, and it has not been verified in CANoe (HUM-36/HUM-37). Non-finite values cannot be stored, because the parser rejects number overflow (e.g. `1e400`) at parse time with −10; TEST-8 pins this, so `ReadDouble` has no −16. |
| OQ7 | Realtime classification. | Same as rows 19–25 (D10/D11): allowed from any context. `hintText` is a plain behaviour description with no realtime clause. The risk is described only in `docs/capl-json-surface.md` (no new README sentence; the existing one covers the `Json` group). |
| OQ8 | Checks in CANoe. | Same pattern as Stage 12 (D14). Merge requires: REV-8 with no Must-fix, plus CI green on x86 and x64. Before merge we ask for a CANoe compile on x64 (HUM-36): rows 26–29 recognised, `restify-json.cin` and the node compile. It is **not merge-gating** if CANoe is not available. Runtime checks (HUM-37) are deferred until a CANoe licence is available. 32-bit CANoe is not checked; x86 is covered by CI. |
| OQ9 | Examples. | Stay deferred (Stage 12 D13); `examples/.gitkeep` stays. The `project-docs` rule "one example per group (… accessors)" is revisited when `examples/` is taken up. |
| OQ10 | README Roadmap. | Remove the item "Typed JSON path accessors …" because it is done; "Tagged releases" stays unchanged. The Roadmap text needs your explicit approval (`project-docs`), so this is a separate approval point in step 9. |
| OQ11 | Where the CAPL-side checks go. | Extend `capl/restify-verify-json.can` with key-triggered checks of rows 26–29. No new node. |
| OQ12 | Status of the CAPL framework. | Stays "preliminary" (Stage 12 §7 item 17 is unchanged). The framework rule "no conversion of value text to CAPL types" is changed only as far as needed: typed values reach CAPL **only** through thin wrappers of rows 26–29; helpers still only print or release. |

## 3. Carried-over constraints (for implementers)

**Export contract:**
- The contract is `CAPL_DLL_INFO_LIST4` in `src/module/exports.cpp`. Stay on `CAPL_DLL_INFO4`.
- Append-only: rows 0–25 stay byte-identical to `main`; rows 26–29 are appended after row 25, before the terminator.
- `exports.def` holds only `EXPORTS` + `caplDllGetTable4`, with no `LIBRARY` line, and does not change.
- Every pointer in the table is `extern "C" … CAPLPASCAL`; no shim loads a reference parameter into a local variable.
- `#pragma pack(push,1)`/`pack(pop)` wraps the whole table including the terminator.
- No name may be a strict prefix of another (29 names).

**Function shape:**
- Never return a raw text pointer.
- Every `char[]` input is followed by its `dword` size; a missing NUL gives −6.
- Out-parameters are zeroed on entry and written only on 0.
- The path is checked before the document id, as in row 22.
- No exception crosses `extern "C"`: a catch-all in each text-layer function maps anything to −35.

**`Status`:** append-only, never renumber. Under OQ4 no new code.

**Layers:**
- `src/mapping/` may depend on `src/core/`, never the other way round.
- Only `src/module/` includes the CAPL SDK headers.
- `To*` never calls `Parse*`.

**CAPL framework (Stage 12 D22):**
- Fixed layout.
- Wrapper name = row name with `restify` replaced by `restLib`, the same parameters with sizes taken from `elcount()`, status returned unchanged, wrappers contain no logic.
- First line of every `.can`/`.cin` is `/*@!Encoding:1250*/`; no CAPL keyword used as an identifier (`key`!).
- No associative fields, no `const` in `variables`, no version numbers, no absolute paths, nothing committed into `capl/includes/dll/`.

**Build:**
- `/MT` everywhere; no new dependency.
- x86 and x64 identical except `/MACHINE:` and the library path.
- `size_t` → `dword` conversions are explicit and checked.
- Builds only via `make`. If the build fails: stop and escalate, no bespoke scripts.
- Every verification report explicitly covers **x86 and x64**.

**Process and documentation:**
- One CHANGELOG `[Unreleased]` entry per row, in the same change as the append.
- `.gitkeep` files are removed together with the first real file in their directory.
- Comment discipline per `project-docs` (also in `.cin`/`.can`): no stage numbers or task IDs; normative content goes in `docs/`.
- Push, PR and merge are done by a human.

## 4. Table of rows 26–29 (HUM-35 sign-off)

| # | Name | Parameters | Returns | Realtime | Codes |
|---|---|---|---|---|---|
| 26 | `restifyJsonReadLong` | `dword documentId`, `char path[]`, `dword pathSize`, `long& value` | `long` | allowed ¹ | 0, −1, −6, −11, −12, −13, −14, −15, −16, −17, −31, −35 |
| 27 | `restifyJsonReadDouble` | `dword documentId`, `char path[]`, `dword pathSize`, `float& value` | `long` | allowed ¹ | 0, −1, −6, −11, −12, −13, −14, −15, −31, −35 |
| 28 | `restifyJsonReadBool` | `dword documentId`, `char path[]`, `dword pathSize`, `long& value` | `long` | allowed ¹ | 0, −1, −6, −11, −12, −13, −14, −15, −31, −35 |
| 29 | `restifyJsonCountElements` | `dword documentId`, `char path[]`, `dword pathSize`, `dword& elementCount` | `long` | allowed ¹ | 0, −1, −6, −11, −12, −13, −14, −15, −31, −35 |

¹ Allowed from any context (OQ7). The risk is described only in `docs/capl-json-surface.md`.

**Check order (all four):**
1. `path` null or size 0 → −1.
2. No NUL within `pathSize` → −6.
3. Path syntax → −11.
4. `documentId` 0, unknown or discarded → −31.
5. Path resolution → −12 / −13 / −14.
6. Type of the target node → −14 / −15 (and −16 / −17 for `ReadLong`).
7. 0.

Any exception → −35. `value`/`elementCount` are zeroed on entry and written only on 0. `ReadBool` writes 1 or 0.

**Table fields:**
- `parCount` 4, `array` `{0,1,0,0}`, `categoryName "Json"`.
- `parTypes`: rows 26 and 28 `{'D','C','D',kRefLong}`; row 27 `{'D','C','D',kRefDouble}`; row 29 `{'D','C','D',kRefDword}`.
- `parNames`: `{"documentId","path","pathSize","value"}` for rows 26–28 and `{…,"elementCount"}` for row 29.
- `hintText`: a plain description, no realtime clause.

**CAPL wrapper names (framework, not part of the export contract):**

| Row | DLL function | Wrapper | File |
|---|---|---|---|
| 26 | `restifyJsonReadLong` | `restLibJsonReadLong(dword documentId, char path[], long &value)` | `restify-json.cin` |
| 27 | `restifyJsonReadDouble` | `restLibJsonReadDouble(dword documentId, char path[], float &value)` | `restify-json.cin` |
| 28 | `restifyJsonReadBool` | `restLibJsonReadBool(dword documentId, char path[], long &value)` | `restify-json.cin` |
| 29 | `restifyJsonCountElements` | `restLibJsonCountElements(dword documentId, char path[], dword &elementCount)` | `restify-json.cin` |

No new helpers. `restLibStatusText` already covers −15..−17, so it does not change; step 4 confirms this.

## 5. Steps

IDs marked *prov.* are provisional. An ID is spent only once it is written into §12 of the master plan.

| # | Step | Files | Agent | Human approval |
|---|---|---|---|---|
| 0 | Create branch `stage/13-json-accessors`: `git switch main`, `git pull --ff-only`, confirm that local `main` equals `origin/main` and contains `cd6d649` (the Stage 12 merge), then `git switch -c stage/13-json-accessors`. **Conditions:** `main` green on x86 and x64 in CI; no open PR touching `exports.cpp`. Push is done by a human (no agent push). | — | `build-pipeline-engineer` | no |
| 1 | **HUM-35 (prov.)**: accept this plan, i.e. the proposals OQ1–OQ12 and the §4 table. A rejected OQ changes §4 and the steps before step 2. | — | human | **YES, gate** |
| 2 | **CPP-12** (narrowed by OQ1):<br>(1) New `src/mapping/json-accessors.{h,cpp}`, pure logic on `const JsonValue&`: `ReadLongAt`, `ReadDoubleAt`, `ReadBoolAt`, `CountElementsAt` = `ResolvePath` + the matching `To*`, or array size per OQ5. No cache.<br>(2) `JsonDocumentStore`: one method that resolves and converts against the stored `document` under the existing lock (no DOM copy). Out-parameters only on Ok. The path is parsed before the lock is taken.<br>(3) `json-text-api.{h,cpp}`: four text-layer functions (pointer+size; reuse `input-text`; check order per §4; out-parameters zeroed; catch-all → −35).<br>(4) Remove the `.gitkeep` only if a new directory is created (none expected).<br>Comments: header tier 2, one line each; no tier 3.<br>Docs:<br>– `docs/capl-json-surface.md`: signature table, one section per row (check order, codes, out-parameter rule), both meanings of −14 (OQ4), the realtime summary extended by rows 26–29.<br>– `docs/type-conversion.md`: one sentence saying that rows 26–28 apply `To*` strictly.<br>– `docs/status-codes.md`: −15/−16/−17 are now visible to CAPL; which rows return them.<br>Verification: `make test ARCH=x64` **and** `make test ARCH=x86` locally; report the test count per architecture. | `src/mapping/json-accessors.{h,cpp}`, `json-document-store.{h,cpp}`, `json-text-api.{h,cpp}`, docs | `cpp-implementer` | no |
| 3 | **TEST-8** (redefined by OQ1), new `tests/mapping/json-accessors_test.cpp` + **TEST-23 (prov.)** in `tests/mapping/json-text-api_test.cpp` and `json-document-store_test.cpp`.<br>`ReadLong`: integer; whole-valued float `5.0`; fractional → −17; `int32` min/max and one beyond → −16; integer beyond 64 bits → −16; string `"42"` → −14; bool → −14; null → −15; container → −14.<br>`ReadDouble`: integer and float; exponent; string → −14; null → −15; `1e400` rejected already at parse with −10 (pins OQ6).<br>`ReadBool`: true/false; 0/1 → −14; null → −15.<br>`CountElements`: `[]` → 0; nested array; root `""` array; object → −14; scalar → −14; null → −15.<br>Path errors −11/−12/−13/−14 (mid-path).<br>Text layer: −1, −6, path checked before id (bad path + bad id → −11), id 0/unknown/discarded → −31; out-parameter pre-filled with non-zero is zeroed on every failure; −35 via injection like the existing `TextApiInternalError` tests; two documents read independently.<br>No change to the global locale (the HUM-34 observation).<br>Verification: x86 **and** x64 locally plus CI on both legs; report counts per architecture. | `tests/mapping/**` | `test-engineer` | no |
| 4 | **CPP-13**: rows 26–29 in `exports.cpp` exactly per §4, appended after row 25.<br>– New constant `kRefDouble = static_cast<char>('F' - 128)` next to the existing two.<br>– Four `extern "C" … CAPLPASCAL` shims forwarding to step 2's text layer (the double shim takes `double*`).<br>– One single-line comment before the block pointing to `docs/capl-json-surface.md`.<br>– Extend the existing `kRefLong`/`kRefDword` trap comment to name `kRefDouble`: at most +1 line, no new block.<br>**In the same change:** the four wrappers in `capl/includes/libs/restify-json.cin` per §4 (no comments, `elcount(path)`); four CHANGELOG `[Unreleased]` → `Added` bullets with signature and codes; confirm that `restLibStatusText` covers −15..−17.<br>Verification: `make build-x64` **and** `make build-x86`; `dumpbin /exports` on both DLLs shows only `caplDllGetTable4`. | `exports.cpp`, `restify-json.cin`, `CHANGELOG.md` | `cpp-implementer` | **YES** (contract change; re-sign if anything differs from §4) |
| 5 | **CPP-43 (prov.)**: `capl/restify-verify-json.can` gets new key-triggered checks: parse an offline document (in a declared `char[]`) with an integer, a fraction, an `int32` boundary, a bool, a null, a numeric string and an array. Then print `ReadLong`/`ReadDouble`/`ReadBool`/`CountElements` with values and statuses through `restLibStatusText`, including the expected −14/−15/−16/−17 cases. Existing checks unchanged; D22 (6) restrictions apply.<br>`docs/capl-framework.md`:<br>– wrapper table rows 26–29;<br>– the "not allowed" rule changed per OQ12;<br>– the "verified" list gets new pending items: a `float &` reference parameter in a `.cin` function, and `float` (8 bytes) through `kRefDouble`.<br>No CHANGELOG entry (preliminary framework, already covered). | `capl/restify-verify-json.can`, `docs/capl-framework.md` | `cpp-implementer` | checked in step 8 |
| 6 | **BPE-36 (prov.)**: full verification on both architectures, from a new shell with the `RESTIFY_MSVC_*` variables set:<br>`make clean` → `make build-x64` → `make build-x86` → `make test ARCH=x64` → `make test ARCH=x86` → `make all`.<br>`dumpbin /exports` on `build/x64/restifycapl-x64.dll` **and** `build/x86/restifycapl-x86.dll` shows only `caplDllGetTable4`.<br>`dumpbin /directives` shows `LIBCMT`, no `LIBCMTD`/`MSVCRT`.<br>Report per architecture: number of tests, number passed, list of failures. Then (after the human push) CI green on both legs. A failure on one architecture: stop and escalate. | — | `build-pipeline-engineer` | no |
| 7 | **REV-8**: review the whole branch (`git diff main...HEAD`) against §8. | — | `code-reviewer` | no Must-fix |
| 8 | **HUM-36 (prov.)**: CANoe compile on x64 at a commit with CI green on both legs: rows 26–29 recognised, `includes.cin`, `restify-json.cin` and `restify-verify-json.can` compile. **Not merge-gating** if CANoe is not available (OQ8).<br>**HUM-37 (prov.)**: runtime checks of the step 5 node on x64: values and statuses as expected; `float &` returns the correct double. Deferred until a CANoe licence is available; open together with HUM-14, HUM-27, the rest of HUM-29 and HUM-30. 32-bit CANoe is not checked. | — | human | **YES, human check** (compile requested before merge; runtime deferred) |
| 9 | README:<br>– add rows 26–29 to the `Json` table (purpose in one line each);<br>– one sentence that the typed reads are strict (no conversion from text) with a link to `docs/capl-json-surface.md`;<br>– Roadmap: remove the typed-accessors item **only after your approval of the exact Roadmap text** (OQ10).<br>No stage numbers. The existing `examples/` TODO stays. Verified behaviour in CANoe is described only as the HUM-36 result. | `README.md` | `docs-writer` | **YES** for the Roadmap text |
| 10 | Fold the plan into the master plan per §7, as the last commit before merge. `planner` prepares the text, `plan-writer` saves it. | master plan | `planner` → `plan-writer` | **YES, before merge** |
| 11 | Push, PR (opened automatically by `auto-pr.yml`), merge `--no-ff`. | — | human | **YES** |

**Sequence:** 0 → 1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 (compile part) → 9 → 10 → 11. Fixes from REV-8 or HUM-36 come back as new provisional steps, followed by a delta REV-8 and a repeat of step 6 on both architectures.

## 6. Files

**New:**
- `src/mapping/json-accessors.{h,cpp}`
- `tests/mapping/json-accessors_test.cpp`
- `docs/work/stage-13-json-accessors/plans/plan.md` (this plan)

**Changed:**
- `src/mapping/json-document-store.{h,cpp}`
- `src/mapping/json-text-api.{h,cpp}`
- `src/module/exports.cpp` (append only: rows 26–29, `kRefDouble`)
- `tests/mapping/json-text-api_test.cpp`, `tests/mapping/json-document-store_test.cpp`
- `capl/includes/libs/restify-json.cin` (4 wrappers), `capl/restify-verify-json.can`
- `docs/capl-json-surface.md`, `docs/capl-framework.md`, `docs/status-codes.md`, `docs/type-conversion.md`
- `CHANGELOG.md`, `README.md`
- master plan (step 10)

**Unchanged:**
- `exports.def`, rows 0–25, `src/core/**`, `src/core/status.h`
- `Makefile`, `scripts/`, `ci.yml`, `.gitignore`
- `capl/includes/includes.cin`, `restify-common.cin`, `restify-sync.cin`, `restify-async.cin`, `capl/restify-verify-http.can`
- `examples/` (only `.gitkeep`), `CLAUDE.md`, skills

## 7. Master plan changes at fold-in

File `docs/work/capl-rest-dll-rebuild/plans/plan.md`:

1. **§5, export table:** add rows 26–29 (`Json`, append from Stage 13). `kRefDouble` (`'F' - 128`) is the first `float&` in the table.
2. **§5, `Status`:** no new code (OQ4); −14 also means "wrong value type" for rows 26–29; −15..−17 are visible to CAPL from Stage 13 on; −37..−39 still free.
3. **§5, `To*`/`Parse*`:** rows 26–28 are the first CAPL-visible use of `To*`; no coercion (OQ3).
4. **§5, realtime:** rows 26–29 follow D10/D11 like rows 19–25.
5. **§5, CAPL framework:** the "no conversion" rule as changed by OQ12.
6. **Stage 13 entry:** rewrite as a summary (what shipped, reviews, HUM-35..37). Record that the cache and the strict string read were dropped (OQ1) and why: the store already keeps the DOM, and struct mapping is the long-term direction.
7. **§12:** IDs CPP-12, CPP-13, CPP-43, TEST-8, TEST-23, BPE-36, REV-8, HUM-35, HUM-36, HUM-37 with status.
8. **§13:** risks from §9.
9. **§3/§15:** Stage 13 complete; next is Stage 14. Conditional Stage 16: the trigger "typed accessors prove insufficient" can now be assessed (no change of status without your decision).
10. **Open human checks (licence):** HUM-37 joins the list with HUM-14, HUM-27, the rest of HUM-29 and HUM-30.

## 8. REV-8 checklist

1. Rows 0–25 and `exports.def` are byte-identical to `main`; no `LIBRARY` line.
2. Rows 26–29 match §4: names, order, `parCount`, `parTypes` (including `kRefDouble`), `array`, `parNames`, `categoryName "Json"`, plain `hintText`.
3. Each shim matches its table row, uses `CAPLPASCAL` and does not load a reference parameter into a local variable.
4. No strict prefix among the 29 names. `dumpbin /exports` on **both** DLLs shows only `caplDllGetTable4`.
5. Check order and codes match §4. Out-parameters are zeroed on entry and written only on 0. Each of the four text-layer functions has a catch-all mapping to −35.
6. The `To*` semantics are unchanged; `To*` never calls `Parse*`; no coercion. `src/core/` is not touched.
7. The store resolves under the existing lock without a DOM copy; the path is parsed outside the lock; no cache.
8. `Status` unchanged (OQ4).
9. One wrapper per row, exactly per §4 (parameters, `elcount()`, references, status unchanged); no new helpers.
10. The `.can`/`.cin` restrictions of D22 (6) hold.
11. Comment discipline, with the budgets of steps 2 and 4.
12. CHANGELOG has exactly 4 new `Added` bullets.
13. Docs match the code (both meanings of −14, realtime summary, framework pending items).
14. TEST-8/TEST-23 cover every case from step 3.
15. The step 6 report gives results for **x86 and x64** separately; CI is green on both legs.
16. `examples/` is unchanged; `CLAUDE.md` and skills are unchanged.

## 9. Risks

- **R1. Contract freeze.** Names, signatures, strictness (OQ3), the `CountElements` semantics (OQ5) and the code assignments become contract from the first release. Loosening them later (e.g. coercing `"42"`) is a behaviour change, and existing scripts may depend on the −14.
- **R2. `float&` (`'F' - 128`) in CANoe.** This is the first reference to an 8-byte type; it is not verified in CANoe and only HUM-37 can confirm it. On x86 a wrong type character corrupts the stack silently. Mitigation: REV-8 items 2/3; CI on both legs. 32-bit CANoe is not available (as Stage 12 R30), so the x86 calling convention for `double*` stays unverified in CANoe.
- **R3. A `float &` reference parameter in a user-defined `.cin` function** is not verified (it extends Stage 12 HUM-29 item (3)). Fallback: a one-element `float` array in the wrapper. That is a framework-only change and does not touch the export table.
- **R4. −14 has two meanings (OQ4).** A script cannot tell a path mismatch from a wrong value type without calling row 22 for `valueType`. This is documented. If it turns out to matter, −37 can be added later; that is compatible for new rows but would not change rows 26–29.
- **R5. Investing in the wrong direction.** Struct mapping is the long-term direction for reading JSON. Mitigation: the minimal row set (OQ1); nothing that only extends path access (wildcards, batch reads).
- **R6. No local x86 run before CI.** Stage 12 R28 (CI green while local x86 failed). Mitigation: step 6 runs x86 **and** x64 locally and reports each separately; the HUM-34 observation continues (the new tests do not touch the locale; `ReadDouble` returns the stored `double` without any text round-trip).
- **R7. Serialized changes to `exports.cpp`.** Mitigation: the step 0 condition (no other open PR touching `exports.cpp`).
- **R8. Open CANoe checks pile up.** HUM-37 joins HUM-14/27/29/30, all waiting for a licence. A defect found later is fixed in a follow-up change; export rows are affected only if the defect is in the DLL.
- **R9. Stale DLL against a newer `restify-json.cin`.** A library calling rows 26–29 does not compile against a Stage 12 DLL. This fails loudly; covered by `docs/capl-framework.md` (take the DLL and `capl/` from the same commit).
- **R10. Realtime (OQ7).** The reads take the lock and parse the path, which allocates on the caller's thread. This extends the deliberate D10 departure to four more rows; the risk is described only in `docs/capl-json-surface.md`.
