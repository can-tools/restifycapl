# Stage 12 — JSON flattening (third append to the export table)

Slug: `stage-12-json-flatten`. Branch: `stage/12-json-flatten`, created from green `main`.

## 1. Goal

Add to the DLL a parsed JSON document stored on the DLL side and addressed by a handle number (`documentId`). The document is flattened to a list of key/value/type entries, where keys are JSON Pointer (RFC 6901), in document order. Added to this are reads by index and by path.

Expose this to CAPL as rows 19–24 of the `CAPL_DLL_INFO_LIST4` table, in the `Json` category.

In passing:
- replace the interim path syntax from Stage 8 with RFC 6901 syntax, which is now the contract;
- move in core to `ordered_json`;
- deliver the first `.can` examples (sync, async, flatten).

Stage 13 (typed accessors) will use the same document.

## 2. Decisions (all approved)

| D | Source | Decision |
|---|---|---|
| D1 | OQ1 | Document stored in DLL under handle `dword documentId`; 0 = "no document". |
| D2 | OQ2 | New status-code block `Status` −30..−39 for `src/mapping/`. Assigned −30..−34, free −35..−39. |
| D3 | OQ3 | Paths in `ReadValue` and keys from flatten are JSON Pointer (RFC 6901), in the same format, with the guarantee that each key from flatten resolves back to its value. Escaping `~0`/`~1`. Old syntax `data.items[0].name` is removed, does not work in parallel. Own parser, without `nlohmann::json_pointer` (which throws exceptions). |
| D4 | OQ3b | Empty key is reachable: `/x/`, `//0`. |
| D5 | OQ3c | `""` = whole document; `/` = key `""` at root; text without `/` at start → −11; `~` with a character other than 0/1 → −11; token at object (including `"0"`) = key, absent → −12; at array `0` or `[1-9][0-9]*` = index, out of range → −13; `"-"` and number greater than `uint32` → −13; `"01"`, `"x"`, `""` at array → −14; scalar or `null` in middle of path → −14; `string_view` without data → −1. |
| D6 | OQ4 | Empty `{}`/`[]` are emitted as entries. `valueType` is in `ReadEntry` and `ReadValue`. One `number` type. Values: 0 = absent, 1 = string, 2 = number, 3 = bool, 4 = null, 5 = emptyObject, 6 = emptyArray. Text value from `ValueToText`: `null` → `null`, fractional number with integer value → `5.0`, exponents `1e-05`/`1e+300`, numbers over 64 bits lose precision (described in documentation). |
| D7 | OQ5 | Document order: `nlohmann::ordered_json` via a single alias `JsonValue` in core. With duplicate keys the last value wins, at the position of first occurrence; value is replaced in full, not merged. Flatten traverses the document in depth (pre-order): object fields in document order, arrays by index, only leaves emitted. Indices are fixed throughout `documentId` lifetime. |
| D8 | OQ6 | Signature table in §4, including `discardedCount` and code assignment. Accepting this plan means signing HUM-28. |
| D9 | OQ7 | `categoryName = "Json"`. |
| D10 | OQ8 | All 6 operations allowed from any context, also from Simulation Setup. This is a deliberate departure from the project principle "allocation disqualifies", which does not change rows 2–18. Risk described **only** in `docs/capl-json-surface.md` and in one sentence in README. Without clauses in `hintText`, without comments in `.can` and in C++. |
| D11 | — | `hintText` of rows 19–24 is a regular function-behavior description, without realtime classification. Deliberate inconsistency with rows 1–18. |
| D12 | OQ9 | Constants in code: 8 slots, 1 MiB input (counted to NUL), depth 64, 10,000 entries. Checked **during** parsing (nlohmann's SAX interface with abort). Before merge, measurement of worst case (one flat object with 10,000 fields; threshold around 50 ms on CI) and memory multiplier. Values are described in documentation. Raising later is safe, lowering breaks compatibility. |
| D13 | OQ10 | Examples `examples/sync.can`, `async.can`, `json-flatten.can`, marked as compile-verified. JSON comes from HTTP response, not from literal. Declared `char[]` + `elcount()`. Without comments on realtime. |
| D14 | OQ11 | Merge after: REV-7 without Must-fix, tests green on x86 and x64 in CI, HUM-29. HUM-30 (runtime verification) remains open. |
| D15 | — | No exception crosses the `extern "C"` boundary: parsing without exceptions plus catch-all at the text-layer boundary. |
| D16 | — | `Parse` counts everything outside the lock, and under one short lock only inserts the document. `CountEntries`, `ReadEntry` and `ReadValue` copy under that lock with buffer-size limit. Key and value texts are ready from `Parse` time. |
| D17 | — | `ParseError` (−10) covers invalid JSON, empty text, and invalid UTF-8. |
| D18 | — | Out-parameters (`documentId`, `entryCount`, `valueType`, `discardedCount`) are zeroed on entry and overwritten only on result 0. `−2` means read can be retried with a larger buffer, because the document does not change; behavior on truncation as in `CopyToBuffer`. `ReadValue` on non-empty container → −14. |

## 3. Carried-over constraints (for implementers)

**Export contract:**
- Contract is `CAPL_DLL_INFO_LIST4` in `src/module/exports.cpp`.
- Append-only: rows 0–18 stay byte-identical.
- `exports.def` contains only `EXPORTS` + `caplDllGetTable4`, without `LIBRARY` line.
- Stay on `CAPL_DLL_INFO4`.
- Every pointer in the table is `extern "C" … CAPLPASCAL`.
- `#pragma pack(push,1)`/`pack(pop)` wraps the entire table including terminator.
- No name can be a strict prefix of another.
- Shim does not load a reference parameter into a local variable.

**Function shape:**
- Never return a raw text pointer.
- Every `char[]` input has a `dword` with size right after it; no NUL → −6.

**`Status` codes:** append-only, never change numbering.

**Layers:**
- `src/mapping/` may depend on `src/core/`, not vice versa.
- CAPL SDK headers are includable only in `src/module/`.

**Build:**
- `/MT` everywhere. No new dependencies.
- x86 and x64 identical except `/MACHINE:` and library path. `size_t` → `dword` conversions are explicit and checked.
- Build only via `make` per `msvc-build-conventions`. Locally x64, x86 via CI. If build fails: stop and escalate.

**Process and documentation:**
- CHANGELOG `[Unreleased]` in the same change as row append.
- `.gitkeep` removed together with the first real file in a directory.
- Comment discipline per `project-docs`: no stage numbers or IDs; normative content goes in `docs/`.
- Push, PR and merge are performed by a human.

## 4. Table of rows 19–24 (HUM-28 sign-off)

| # | Name | Parameters | Returns | Realtime | Codes |
|---|---|---|---|---|---|
| 19 | `restifyJsonParse` | `char json[]`, `dword jsonSize`, `dword& documentId` | `long` | allowed ¹ | 0, −1, −6, −10, −30, −32, −33, −34 |
| 20 | `restifyJsonCountEntries` | `dword documentId`, `dword& entryCount` | `long` | allowed ¹ | 0, −31 |
| 21 | `restifyJsonReadEntry` | `dword documentId`, `dword entryIndex`, `char key[]`, `dword keySize`, `char value[]`, `dword valueSize`, `long& valueType` | `long` | allowed ¹ | 0, −1, −2, −13, −31 |
| 22 | `restifyJsonReadValue` | `dword documentId`, `char path[]`, `dword pathSize`, `char value[]`, `dword valueSize`, `long& valueType` | `long` | allowed ¹ | 0, −1, −2, −6, −11, −12, −13, −14, −31 |
| 23 | `restifyJsonDiscardDocument` | `dword documentId` | `long` | allowed ¹ | 0, −31 |
| 24 | `restifyJsonDiscardAllDocuments` | `dword& discardedCount` | `long` | allowed ¹ | 0 |

¹ Allowed from any context. Risk is described in `docs/capl-json-surface.md` (D10). This is documentation information, not text in the code.

**New `Status` codes:**

| Code | Name | Meaning |
|---|---|---|
| −30 | `NoFreeDocumentSlot` | no free space for document |
| −31 | `UnknownDocumentId` | `documentId` unknown or already freed |
| −32 | `DocumentTooLarge` | input larger than 1 MiB |
| −33 | `NestingTooDeep` | nesting deeper than 64 |
| −34 | `TooManyEntries` | more than 10,000 entries |

## 5. Steps

IDs with *prov.* are provisional. ID is issued only after writing to §12 of the master plan.

| # | Step | Files | Agent | Human approval |
|---|---|---|---|---|
| 0 | Create branch `stage/12-json-flatten`. **Condition: `main` green on x86 and x64** (see R19) and no open PR touching `exports.cpp`. | — | `build-pipeline-engineer` | no |
| 1 | **HUM-28 (prov.)**: sign-off on decisions D1–D18 and the table from §4, i.e., acceptance of this plan. | — | human | **YES, gate** |
| 2 | **HUM-16**: in CANoe check associative-field syntax `char[N] m[char[]]`, write `char[]` as its value, and `const` in `variables`. Record availability by CANoe edition. Can run in parallel with steps 3–9; blocks step 11. | — | human | the action itself |
| 3 | **CPP-33 (prov.)**: codes −30..−34 in `src/core/status.h` and `docs/status-codes.md`. | `status.h`, docs | `cpp-implementer` | no |
| 4 | **CPP-35 (prov.)**: `src/core/json-value.h` (alias `JsonValue = nlohmann::ordered_json`); `json-path.h` and `type-conversion.{h,cpp}` to `JsonValue`; one line in `docs/type-conversion.md`. Comment at alias: tier 3, max 3 lines (pitfall: swapping back to `nlohmann::json` silently changes entry order, which is a contract). **TEST-20 (prov.)**: `tests/core/type-conversion_test.cpp` to alias; all 45 cases pass with unchanged expectations. | core, tests | `cpp-implementer`, `test-engineer` | no |
| 5 | **CPP-34 (prov.)**: rewrite `src/core/json-path.{h,cpp}` to RFC 6901 per D5. `PathSegment::Kind` disappears, tokens are textual. Comment at decoding: tier 3, max 3 lines (pitfall: decode `~1` first, then `~0`). Rewrite `docs/json-path.md`: RFC syntax, D5 table, interim caveat is gone. Header: tier 2, one line. **TEST-19 (prov.)**: rewrite `tests/core/json-path_test.cpp` mapping each of 23 `TEST` blocks (master plan counts 24 — to clarify) to equivalent or with justification for removal; new cases: `~0`/`~1`, decode order, `/`, `//0`, `"0"` at object, `-`, `01`, exceeding `uint32`, examples from RFC §5. | core, tests, docs | `cpp-implementer`, `test-engineer` | no |
| 6 | **CPP-9**: `src/mapping/json-flatten.{h,cpp}`: SAX parsing with limits from D12, without exceptions; iterative flatten pre-order (D7); key escaping; `valueType`; texts counted once. Remove `src/mapping/.gitkeep`. New `docs/json-flatten.md`: key format, order, duplicates (two examples), special containers, numbers, limits, cost. **Measurement** of worst case and memory multiplier, result in report to REV-7. | mapping, docs | `cpp-implementer` | no |
| 7 | **TEST-7**: `tests/mapping/json-flatten_test.cpp`, remove `tests/mapping/.gitkeep`. Coverage: round-trip each key via `ResolvePath`; pinned document order (keys in reverse alphabetical order and example from D7); duplicates; `{}`/`[]`/`null`/empty string; scalar document (key `""`); `/x/`, `//0`, `/a~1b`, `/m~0n`; invalid JSON and UTF-8 (−10); each limit just below and just above (−32/−33/−34). | tests | `test-engineer` | no |
| 8 | **CPP-32 (prov.)**: `src/mapping/json-document-store.{h,cpp}` (8 slots, D16) and `src/mapping/json-text-api.{h,cpp}` (pointer+size, reuse `input-text`/`buffer-copy`, D15, D18). New `docs/capl-json-surface.md`: signature table, codes, `valueType`, out-parameter write rules, "Realtime-safety summary" table (all "Allowed" with notes) and "Risk" paragraph (D10). | mapping, docs | `cpp-implementer` | no |
| 9 | **TEST-18 (prov.)**: `tests/mapping/json-text-api_test.cpp`. Coverage: boundaries of each input, −6, −2 with retry, 8 slots and 9th rejected, ID 0/unknown/freed (−31), out-parameters zeroed on error, two independent documents, discard-all with counter. | tests | `test-engineer` | no |
| 10 | **CPP-10**: rows 19–24 in `exports.cpp` exactly per §4, `hintText` as plain description (D11). Single-line comment before the block pointing to `docs/capl-json-surface.md` (like the async block). Six entries in CHANGELOG `[Unreleased]`. | `exports.cpp`, `CHANGELOG.md` | `cpp-implementer` | **YES** (contract change; re-sign if anything deviates from §4) |
| 11 | **CPP-11**: `examples/sync.can`, `async.can`, `json-flatten.can` per D13 and HUM-16 outcomes. Remove `examples/.gitkeep`. | `examples/` | `cpp-implementer` | checked in step 13 |
| 12 | **REV-7**: review entire branch per checklist in §8. | — | `code-reviewer` | no Must-fix |
| 13 | **HUM-29 (prov.)**: DLL from commit green on CI on both legs. CANoe recognizes rows 19–24, examples compile. x64 mandatory, x86 if available; otherwise gap recorded explicitly. **HUM-30 (prov.)**: runtime verification (parse, iterate, path read, discard, 8 documents, call from Simulation Setup). Remains open, together with HUM-14 and HUM-27. | — | human | **YES, stage acceptance** |
| 14 | README: summary of `Json` operations and one sentence in "Realtime caveat" block (D10), no stage numbers. | `README.md` | `docs-writer` | no |
| 15 | Fold-in to master plan, as final commit before merge (§7). `planner` prepares text, `plan-writer` saves. | master plan | `planner` → `plan-writer` | **YES, before merge** |

**Sequence:** 0 → 1 → (2 in parallel) → 3 → 4 → 5 → 6 → 7 → 8 → 9 → 10 → 11 (after 2) → 12 → 13 → 14 → 15.

## 6. Files

**New:**
- `src/core/json-value.h`
- `src/mapping/json-flatten.{h,cpp}`
- `src/mapping/json-document-store.{h,cpp}`
- `src/mapping/json-text-api.{h,cpp}`
- `tests/mapping/json-flatten_test.cpp`
- `tests/mapping/json-text-api_test.cpp`
- `docs/json-flatten.md`
- `docs/capl-json-surface.md`
- `examples/sync.can`, `examples/async.can`, `examples/json-flatten.can`

**Changed:**
- `src/core/status.h`
- `src/core/json-path.{h,cpp}`
- `src/core/type-conversion.{h,cpp}`
- `tests/core/json-path_test.cpp`
- `tests/core/type-conversion_test.cpp`
- `src/module/exports.cpp` (append only)
- `docs/json-path.md`, `docs/status-codes.md`, `docs/type-conversion.md`
- `CHANGELOG.md`, `README.md`

**Removed:** `src/mapping/.gitkeep`, `tests/mapping/.gitkeep`, `examples/.gitkeep`.

**Unchanged:** `exports.def`, `Makefile` (auto-picks `src/mapping/*.cpp` and `tests/mapping/*.cpp`), `ci.yml`.

## 7. Master plan changes at fold-in

File `docs/work/capl-rest-dll-rebuild/plans/plan.md`:

1. **§5, realtime (line ~123):** part about Stage 12 in sentence *"…Stage 12 into the `.can` examples"* removed by user decision. Note on D10: rows 19–24 allowed from Simulation Setup as a departure, not a precedent for rows 2–18. Note on D11: no classification in `hintText` of rows 19–24.
2. **Stage 9 entry, "Obligations" (~812):** *"Stage 12 must carry the realtime-branch caveat into the `.can` examples."* Removed.
3. **Stage 10 entry, "Obligations" (~834):** realtime part removed. **Declared-array requirement stays.**
4. **Stage 11 entry, "Obligations" (~864):** same as point 3.
5. **Stage 12 entry (~872):** *"Carried in from Stage 9: the `.can` examples must carry the realtime-branch caveat…"* Replace with decision record. Rewrite Stage 12 entry as summary of D1–D18.
6. **§5, export-table list:** add rows 19–24 and `Json` category. In the `categoryName` point, note that `Json` is the fourth group.
7. **§5, paths (line ~112):** replace *"the `data.items[0].name` path syntax is interim"* with "RFC 6901 JSON Pointer, contract from Stage 12".
8. **§5, `Status` (line ~110):** block −30..−39 for mapping; −30..−34 issued; `ParseError` −10 settled. Update "the `-10..-29` block is now full" with new block.
9. **§5, `To*`/`Parse*` boundary (line ~111):** `const JsonValue&` instead of `const nlohmann::json&`.
10. **§5:** new standing rules: entry order and key format are contract; limits D12 (raising safe, lowering breaks compatibility).
11. **§3 and §15, status:** Stage 12 complete. Fix outdated "Status" paragraph at end of §15 ("Next action: Stage 10…").
12. **§12:** record IDs (CPP-9/10/11/32–35, TEST-7/18/19/20, REV-7, HUM-16/28/29/30) with status. Clarify 23 vs 24 `json-path` tests discrepancy.
13. **§13:** risks R5, R15, R16 and R19 from §9.
14. Out of scope, for separate decision: `project-docs` assigns separating release section in CHANGELOG to "Stage 13" instead of 14; `msvc-build-conventions` does not mention `iphlpapi`.

## 8. REV-7 checklist

1. Rows 0–18 and `exports.def` byte-identical; no `LIBRARY` line.
2. Rows 19–24 match §4: names, order, `parCount`, `parTypes`, `array`, parameter names, `categoryName "Json"`. Shim matches table, `CAPLPASCAL`, no reference loading to local variable.
3. No strict prefixes among 24 names. `dumpbin /exports` yields only `caplDllGetTable4`.
4. **No realtime clauses in `hintText` 19–24 and in `.can` examples is not an error** (D10, D11). Information is in `docs/capl-json-surface.md` and in "Realtime caveat" block in README.
5. No exception crosses boundary; no `nlohmann::json_pointer`.
6. Limits checked during parsing; each limit tested. Measurements (worst-case time ≤ around 50 ms, memory multiplier) included; exceeding threshold means decision review before merge.
7. No `nlohmann::json` in `src/`, only alias. Result of 45 `type-conversion` tests identical.
8. `json-path` matches D5. Each old test has equivalent or justification. Decode order for `~1`/`~0` is correct.
9. Round-trip guarantee tested. Document order pinned by test. Duplicates per D7.
10. D16: parsing outside lock, reads with limited copy. D18: out-parameter write order.
11. `Status` append-only, −30..−34 match §4.
12. Explicit `size_t` → `dword` conversions; green on x86 and x64 in CI.
13. Comment discipline: only comments from step budgets 4, 5, and 10, no stage numbers or IDs.
14. CHANGELOG has 6 entries. `.gitkeep` files removed in same changes.
15. Examples: names match table, syntax per HUM-16, declared `char[]` + `elcount()`, JSON from HTTP response.
16. `/MT`: authoritative is `dumpbin /directives` in CI.

## 9. Risks

- **R1. Contract freeze.** Key format (RFC 6901), entry order, `valueType` values, rules D5 and limits become contract from first release. Lowering limits breaks compatibility.
- **R2. Exceptions across C boundary.** Exception from nlohmann in a function called by CANoe crashes host process. Mitigation: D15 and point 5 of REV-7.
- **R3. Deep nesting.** Mitigation: limit 64 and iterative flatten.
- **R4. Bitness.** `size_t` vs `dword`; memory in 32-bit CANoe (8 documents × 1 MiB × multiplier). Mitigation: measure multiplier.
- **R5. Simulation Setup parse disrupts simulation time** (D10). DLL raises no error; only limits D12 are boundary.
- **R6. `Status` space.** Remains −35..−39 for mapping and −7..−9.
- **R7. Examples verified compile-only.** HUM-14, HUM-27 and HUM-30 are open.
- **R8. Associative fields available only in some CANoe editions** (documentation: "Valid for: CANoe DE…").
- **R9. Serialization of changes in `exports.cpp`.** Mitigation: "require branches up to date" rule and single-PR convention.
- **R10. Clean review is not ABI proof.** Calling-convention correctness on x86 confirmed only by HUM-30.
- **R11. Regression in reviewed Stage 8 modules.** Mitigation: test mapping and 45 unchanged expectations.
- **R12. Edge rules D5 are contract.** Must be documented.
- **R13. Pitfall of decoding `~01`.** Mitigation: pitfall comment and test.
- **R14. Old syntax entered by hand.** Gives −11, i.e., loud error.
- **R15. Future type swap back to `nlohmann::json`** silently changes entry order. Mitigation: pitfall comment and order-pinning test.
- **R16. Quadratic cost of parsing wide objects.** Limited by 10,000-entry limit and measurement.
- **R17. `hintText` inconsistency** between rows 19–24 and 1–18 (D11). Deliberate.
- **R18. Master plan fold-in omission.** REV-7 would then report false errors, and future stages inherit outdated obligations. Mitigation: list in §7.
- **R19. CI dependence on downloading `vcpkg.exe` from GitHub Releases on cache miss.** In `ci.yml` the step "Bootstrap vcpkg (full clone, pinned tool tag)" runs only on cache miss for "Cache vcpkg tool checkout"; then `bootstrap-vcpkg.bat` downloads ready-built `vcpkg.exe` from GitHub Releases (`microsoft/vcpkg-tool`), no retry and no mirror. One HTTP 504 error occurred on this download on x86 leg on `main`; re-running CI passed (user report, not independently verified). Cause of 504 and cache-miss reason are not established. Impact: Stage 12 requires green CI on both legs (merge, x86 verification, before HUM-29). Mitigation: step 0 condition (`main` green on x86 and x64 before branch create). Options on recurrence — **not planned Stage 12 work**; each would need separate `chore` on separate branch, performed by `build-pipeline-engineer` before step 0, and user acceptance before merge: (a) retry with backoff around `bootstrap-vcpkg.bat`; (b) review tool-cache key — first check justification in `docs/ci-pipeline.md`, "Caching" section; (c) own `vcpkg.exe` download with retry and SHA-256 checksum — considered overkill for now.
