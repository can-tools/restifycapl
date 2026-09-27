# Stage 10 — Expose synchronous REST to CAPL (first contract append)
## FINAL PLAN — approved for implementation

Branch `stage/10-capl-sync-exports`, cut from `main` @ `b1c21f8`. Slug `stage-10-capl-sync-exports`.

---

## 1. Goal

Make Stage 9's synchronous HTTP layer callable from CAPL by appending six rows to `CAPL_DLL_INFO_LIST4` in `src/module/exports.cpp`, and rename the one already-exported row while the window to do so is still open. Scope: the CAPL-visible signature design, a testable translation layer, the export rows, the `CHANGELOG.md` entry, review, and compile-time verification in real CANoe. No behavioural change to `src/http/http-client.*` or `src/http/sync-operations.*`.

---

## 2. Open questions — all closed

OQ1 naming (both sync and async) · OQ2 out-parameter form · OQ3 input length safety · OQ4 row count · OQ5a sequencing · OQ5b branch base · OQ6 table version · OQ7 filing · OQ8 status codes. **All ruled.**

**Two disclosures about how three of them closed.** OQ6, OQ7 and OQ8 were approved by not being contested when presented together, rather than by explicit ruling — flagged here and remains free to revisit if it matters later, since implementation is only now starting. The same applies to **D21** (`categoryName`), which reflects the planner's recommendation rather than a stated ruling.

**HUM-24 — SATISFIED.** The gate was "approve the final names and exact signature table before any row is written." The OQ1–OQ8 rulings discharge it. It remains in the ledger as satisfied rather than pending.

---

## 3. Blocking precondition — now precisely identified

**HUM-14 (runtime verification in a running measurement) is blocked by a CANoe licensing problem.** Not configuration, not hardware, not CAPL syntax, not an unknown. The user is resolving it outside this process and will flag when it is fixed.

Everything else is unblocked, and the blocker is narrower than the master plan previously stated:

- **CANoe is installed and works.** A DLL has been loaded and its export table read. **HUM-10 is satisfied in fact.**
- **The CAPL compiler accepts this DLL's table.** A script referencing `restifyGetVersion` compiled against the real export table.
- **HUM-13 is half satisfied** — the load-and-compile half succeeded; the call-and-observe half has not.
- **HUM-25 (compile all six new rows) is executable today** and is this stage's acceptance gate.

The master plan's flat "Stage 3 UNCONFIRMED / HUM-13 BLOCKED" framing is stale and is corrected at fold-in (§11).

---

## 4. Constraints carried in

- **The export contract is `CAPL_DLL_INFO_LIST4` in `src/module/exports.cpp`, not `exports.def`.** Append-only; never reorder or remove. Row 0 (`CDLL_VERSION_NAME`/`CDLL_VERSION`) untouched. **Row 1 is renamed once, under the closed one-time exception in D2.**
- `exports.def` stays exactly `EXPORTS` + `caplDllGetTable4`, with **no `LIBRARY` line**.
- **`restify<VerbNoun>`, fixed at Stage 5**, documented at the head of `exports.cpp`; that statement is protected — trim, never remove.
- **Every table function pointer is `extern "C" ... CAPLPASCAL` (`__stdcall`)** — load-bearing on x86.
- **Never return a raw text pointer.** Always a caller-supplied buffer plus its size.
- **`#pragma pack(push, 1)` / `pack(pop)` spans the whole table through the sentinel row.**
- **`Status` codes are absorbed, never renumbered.** Spent: `0`, `-1..-3`, `-10..-17`, `-18..-23`. This stage spends `-4..-6`. **`-25..-29` are fenced for Stage 11 — do not mint from that range.**
- **`src/module/` is the only place that may include the CAPL SDK headers, and the only place excluded from `make test`.**
- **Every export-table append gets a `CHANGELOG.md` `[Unreleased]` entry in the same change.**
- **Comment discipline:** no plan, stage or task identifiers in any comment. Plans specify tier and line budget, never "verbatim". Normative tables go to `docs/<topic>.md`.
- **`/MT` everywhere; x86 and x64 identical but for `/MACHINE:` and library path.** Both build clean at `/W4`.
- Branch-only work. **`git push`, PR and merge are human-only.**
- **Carried in from Stage 9:** the realtime-branch caveat must reach the export-table description text (D17), and **`skipTlsVerification` must not be exposed by reflex** (D9).

---

## 5. Findings from the SDK and the Stage 9 code

1. `CAPL_DLL_INFO4`: `cdlName[50]`, `parTypes[64]` (`char`), `array[64]` (`unsigned char`), `parNames[64]`, `categoryName`/`hintText` as `const char*`. Longest proposed name 20 characters.
2. `parTypes`/`array` are fixed arrays, so braced character lists are legal initializers — Vector's own documented form, and clearer than octal escapes.
3. **The ABI cannot express a struct.** No compound type character exists, and no field in any of the five table versions could describe a member layout. Confirmed independently by `VIACaplFunction::ParamType` returning one char per parameter. **This is a platform limitation, not a design choice, and no C++-side design recovers it.**
4. Legal type set: `V C B I W L D 6 U F`, plus `type - 128` by-reference. **`cdll.h`'s own trailing comment is stale** — it omits `6` and `U`; the CANoe 19.3.1 doc is authoritative.
5. **`MAXCAPLFUNCPARS` is 10** for `INFO`/`INFO2`/`INFO3`; 64 for `INFO4`/`INFO5`. A 15-parameter row permanently forecloses the older tables.
6. `CAPL_DLL_INFO5`'s `usageMask` flags are **positive requirements** (`CAPL_CONTEXT_TEST` = *needs* a test node), not prohibitions — see D16.
7. `src/http/http-client.cpp` does populate `HttpResponse::headers`, so D8's deferral is a choice, not a limitation.
8. `CopyToBuffer` never truncates — on `BufferTooSmall` the buffer is left empty. Drives D7.
9. `sync-operations::Request` already rejects a non-empty body on Get/Head/Delete; `HttpMethod::Head` has no verb helper.
10. `RequestOptions`' `0`-means-default convention (5000 ms / 30000 ms / 8 MiB) maps onto CAPL `dword`s with no new semantics.

---

## 6. Design decisions

**D1 — Six rows: one generic executor plus five verb helpers.** The generic carries the complete surface so no capability is permanently unreachable; the verbs stay short because CAPL has no default arguments. This split has now earned its keep three times: ergonomics, capability reach, and confining R11 to one rarely-called row.

**D2 — Names, and the one-time rename.** Scheme: **`restify<Operation><Sync|Async>`**, no family segment for HTTP.

*Stage 10:* `restifyRequestSync`, `restifyGetSync`, `restifyPostSync`, `restifyPutSync`, `restifyPatchSync`, `restifyDeleteSync`.
*Stage 11, fixed now so it is not re-litigated:* `restifyRequestAsync`, `restifyGetAsync`, `restifyPostAsync`, `restifyPutAsync`, `restifyPatchAsync`, `restifyDeleteAsync`, plus the three shared lifecycle rows `restifyPollResponse`, `restifyAwaitResponse`, `restifyReadResponse`.

**Row 1 is renamed `restifyGetVersion` → `restifyReadVersion`.** `Read` + `Version` keeps `restify<VerbNoun>` intact, so §5's convention needs no amendment and the protected comment's wording is unchanged — only its example name. It also matches `restifyReadResponse`, making "Read" mean *copy a value into my buffer* consistently. Side effect: with `restifyGetVersion` gone, the prefix ambiguity that started the naming discussion ceases to exist.

**This rename is a closed, one-time exception and is not a precedent.** It was permissible only because of a conjunction that cannot recur: the table had been compiled against by real CANoe, but the function had **never been called in a running measurement**, so no observed runtime behaviour depended on the name. From the moment HUM-13 first calls an exported operation and observes its result, append-only is absolute again for every row including this one. A future stage wanting a different name **adds a row and leaves the old one**.

**D3 — No `restifyHeadSync`.** `sync-operations` has no `Head` helper; HEAD is reachable through `restifyRequestSync` with `method = "HEAD"`.

**D4 — Final signatures.** `kRefLong` = `static_cast<char>('L' - 128)`, `kRefDword` = `static_cast<char>('D' - 128)`.

| Row | Signature | n |
|---|---|---|
2 | `long restifyGetSync(char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char responseBody[], dword responseBodySize, long& httpStatusCode, dword& responseBodyLength)` | 8 |
3 | `long restifyDeleteSync(…identical to row 2…)` | 8 |
4–6 | `long restifyPostSync(char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char requestBody[], dword requestBodySize, char responseBody[], dword responseBodySize, long& httpStatusCode, dword& responseBodyLength)` — and `restifyPutSync`, `restifyPatchSync` identically | 10 |
7 | `long restifyRequestSync(char method[], dword methodSize, char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char requestBody[], dword requestBodySize, char responseBody[], dword responseBodySize, dword connectTimeoutMs, dword totalTimeoutMs, dword maxResponseBytes, long& httpStatusCode, dword& responseBodyLength)` | 15 |

Encodings — rows 2–3: `parTypes {'C','D','C','D','C','D',kRefLong,kRefDword}`, `array {1,0,1,0,1,0,0,0}`. Rows 4–6: `{'C','D','C','D','C','D','C','D',kRefLong,kRefDword}`, `{1,0,1,0,1,0,1,0,0,0}`. Row 7: `{'C','D','C','D','C','D','C','D','C','D','D','D','D',kRefLong,kRefDword}`, `{1,0,1,0,1,0,1,0,1,0,0,0,0,0,0}`. `parNames` fully populated on every row — it is R11's in-product mitigation.

**D5 — Transport `Status` is the return value; HTTP status is a separate out-param.** Return answers *did we get a response*; `httpStatusCode` answers *what did the server say*. HTTP 500 → return `0`, `httpStatusCode` 500. DNS failure → return `-18`, `httpStatusCode` `0`. **Normative write ordering: `httpStatusCode` and `responseBodyLength` are written whenever a response was received, regardless of whether the body copy then succeeds** — so `-2` still reports the status and the size needed.

**D6 — Every input `char[]` is paired with an explicit `dword` size, placed immediately after it.** The size is **the caller's array capacity** — the same meaning as `responseBodySize`, the value `elcount()` returns. The DLL scans for the terminator **only within that bound**: found → text is everything before it; **none within the stated size → `UnterminatedInputText` (-6)**, no request issued; **stated size `0` → `InvalidArgument` (-1)**; null pointer → `-1`. Over-read is structurally impossible when the caller states its real capacity. **No ceiling constants exist anywhere in the design.**

The size must come from `elcount(theArray)`. **Hand-counted literal lengths are the documented anti-pattern** — a too-large count reproduces the over-read this rule exists to prevent. **Carve-out:** the empty string with size 1 (`""`, `1`) is the sanctioned way to say "no headers" and is the one case where writing the size by hand is trivially safe.

**D7 — `CopyToBuffer` is reused exactly as Stage 8 built it; nothing is extended.** Never-truncate stands. The residual gap — body lost on `-2`, after a request that for POST may not be safely repeatable — is made *diagnosable* by `responseBodyLength` and is **deliberately not closed by retaining the response.** Stage 11 owns response state from scratch.

**D8 — Response headers are not exposed.** Available, but exposing them forces a two-buffer failure-precedence rule with no clean answer through one `Status`; the natural CAPL shape is a by-name accessor over retained state, i.e. Stage 11/12/13 territory. Deferring costs a future *new name*, which is the normal legal mechanism.

**D9 — `skipTlsVerification` is NOT exposed.** The Stage 9 obligation discharged, not deferred by omission. The failure is invisible — everything appears to work, including against a hostile peer — and a parameter slot that exists gets set to `1` while debugging and left there. If ever exported it gets its own row, its own unambiguous name, and its own approval. The field is forwarded at its `false` default and no CAPL-reachable path can influence it.

**D10 — Request headers are one delimited text block: `Name: Value`, one per line.** `\n` separates; a trailing `\r` is tolerated; empty string means no headers; name and value are trimmed; a line with no colon is an error; an empty name is an error; duplicates are preserved in order. Rejected: parallel arrays (two lengths that can disagree), a 2-D array (stride is not communicable — the same hazard class as passing a struct as bytes), and a stateful accumulator (global state before Stage 11 designs any).

**D11 — An empty header value is rejected, not passed through.** `curl_slist_append("X-Foo:")` means *remove this header* in libcurl; an author writing that means the opposite. Text-layer rule only — `http-client.cpp` is unchanged.

**D12 — The translation layer is testable; `exports.cpp` stays a thin shim.** Because `src/module/` is excluded from `make test`, none of this stage's real logic may live there. Two files (D-level filing per OQ7):

- **`src/core/input-text.{h,cpp}`** — `Status BoundedText(const char* p, std::uint32_t size, std::string_view& out)`. Pure, level 0, the single place D6's bound is enforced. Filed in `core` because Stage 11's dispatch and Stages 12–13's accessors will all receive CAPL strings as pointer+size. It completes a symmetry: `buffer-copy.h` owns C++-value-out-to-raw-buffer and says so explicitly, so the inverse direction needs its own home.
- **`src/http/sync-text-api.{h,cpp}`** — `ParseMethodText`, `ParseHeaderBlock`, and the executor that composes them, assembles `RequestOptions`, calls `sync-operations`, writes the out-params in D5's order, and copies the body via `CopyToBuffer`. No SDK header, no CAPL knowledge, `HttpClient&` injectable.

Each `extern "C" CAPLPASCAL` function in `exports.cpp` is then a single forwarding call.

**D13 — Method text is matched case-insensitively against the six `HttpMethod` names.** Anything else → `UnknownHttpMethod` (-5). No aliasing, no defaulting to GET.

**D14 — Three new `Status` codes from the reserved block:** `MalformedHeaderBlock = -4`, `UnknownHttpMethod = -5`, `UnterminatedInputText = -6`. `-7..-9` stay reserved. `-6` is now an **exact** diagnostic — *no terminating NUL within the size the caller stated* — with no second interpretation and no policy number behind it; it is what makes D6's bound observable to an author who has no debugger. `InvalidArgument` (-1) keeps its Stage 9 meaning: caller-side programming error.

**D15 — Binary response bodies are out of scope, documented rather than guarded.** Full bytes are copied and `responseBodyLength` is exact, but a CAPL `char[]` reader stops at the first NUL. This surface is text/JSON-oriented by intent.

**D16 — `CAPL_DLL_INFO4` is retained; no migration to `INFO5`.** Two reasons, the first decisive: **`usageMask`'s flags are positive requirements, not prohibitions.** `CAPL_CONTEXT_TEST` means *needs a test node*, so it would also block legitimate Measurement Setup use — **too narrow for our actual rule**, which permits Measurement Setup or a test node and forbids only simulation nodes. No flag expresses "anything but a simulation node", and whether some OR-combination does is not determinable from the available material. Second: `INFO4` has positive evidence behind it in this user's CANoe; `INFO5` has none, precedence between tables is undocumented, and maintaining both is the duplicate-source-of-truth antipattern. Deferring stays cheap — names and signatures would be identical.

**D17 — The realtime caveat lives in each row's `hintText`, and nowhere else in `exports.cpp`.** `hintText` is not a source comment — it is metadata CANoe displays to a CAPL author, so it sits in the exempt category (program output read by a user). The exemption is from *mechanical flagging*, not from the rule: no stage numbers, no task IDs, no `§` references, no rationale. **Budget: two short sentences per row**, one of them the caveat, in the shape of:

> Blocking call — use from Measurement Setup or a test node only, never from a Simulation Setup node.

Plus one sentence naming what the function does and that it returns 0 on success, negative on error. **No comment above the rows restating any of it.**

**D18 — Comment budgets and homes.** Normative material — the signature table, the header-block grammar, the status-code table, D5's write ordering, D6's `elcount` rule and carve-out, D15's limitation — goes to **`docs/capl-sync-surface.md`**, new and topic-scoped. `input-text.h` and `sync-text-api.h`: **≤ 5 comment lines each**, pointing there. `exports.cpp`: **one tier-3 trap block, ≤ 10 lines**, naming exactly two traps — that `parCount` must equal the entry count in `parTypes`/`array`/`parNames` with nothing checking it, and that a reference parameter is `type - 128` with a pointer on the C++ side. New source files: **≤ 35% comment lines.**

**D19 — Branch and slug.** `stage/10-capl-sync-exports`, cut from **`main` @ `b1c21f8`** (Stage 9 merged via PR #10, CI green). Slug `stage-10-capl-sync-exports`. No branch-base ambiguity remains.

**D20 — Out-parameters are CAPL reference parameters.** `base - 128`, dimension `0`, C++ side a pointer. Spelled with file-local `constexpr char` constants to keep the arithmetic visible and avoid a `/W4` narrowing diagnostic in a braced initializer, while remaining a static initializer as `cdll.h` requires. **HUM-25 is the gate**; if CANoe rejects the form, falling back to 1-element arrays is a two-bytes-per-parameter table edit with zero implementation rework, free until merge.

**D21 — `categoryName` differentiation is deferred to Stage 12.** Grouping by category was approved in principle, but there is nothing to group until JSON operations exist — differentiating now yields a one-row inconsistency for no benefit. Every Stage 10 row uses `"restifycapl"`, matching row 1. *(Planner's recommendation, not an explicit ruling — see §2.)*

---

## 7. Tasks

| ID | Agent | Task | Approval |
|---|---|---|---|
**HUM-24** | Human | Approve names and the exact signature table | **SATISFIED** by the OQ1–OQ8 rulings |
— | `build-pipeline-engineer` | Cut `stage/10-capl-sync-exports` from `main` @ `b1c21f8` | No |
**CPP-24** | `cpp-implementer` | **The rename.** Committed alone, ahead of everything — a rename and an append are the two most contract-sensitive operations in the project and must not share a diff. Criteria: `cdlName` reads `"restifyReadVersion"`; **row 1 stays in position 1** — renamed, not moved, not re-added; the `extern "C"` function and `(CAPL_FARCALL)` cast agree; **`CopyOwnVersionString` is not modified at all**; no `Status` value changes; behaviour identical for every input class; row 1's `parCount`/`parTypes`/`array`/`categoryName`/`hintText` **unchanged**; `src/core/buffer-copy.h:8` and `docs/status-codes.md:19` updated in the same commit; the line-8 protected comment's **example name only** updated, wording and tier intact; `CHANGELOG.md` per below; **the two completed stage plans left untouched**; `exports.def` untouched; both architectures clean at `/W4`; `make test` still green. | **YES** — the rename exception, already granted |
**CPP-23** | `cpp-implementer` | `src/core/status.h`: append `-4`, `-5`, `-6` per D14. Every pre-existing value byte-identical; `-7..-9` reserved; `-25..-29` untouched. | No |
**CPP-22** | `cpp-implementer` | `src/core/input-text.{h,cpp}` and `src/http/sync-text-api.{h,cpp}` per D12, plus **`docs/capl-sync-surface.md`** per D18. | No |
**TEST-14** | `test-engineer` | `tests/core/input-text_test.cpp` and `tests/http/sync-text-api_test.cpp`, against the existing `tests/http/fake-transport.h`. Required: bound enforcement; header grammar; method parse; **`-2` with `httpStatusCode` and `responseBodyLength` still written**; HTTP 4xx/5xx returning `0` with the code out; each transport error returning its `-18..-23` code with `httpStatusCode == 0`; body-on-GET rejected; `skipTlsVerification` unreachable. Offline only, both architectures. | No |
**CPP-6** | `cpp-implementer` | `src/module/exports.cpp`: six shims + six rows with D17's `hintText`, D18's trap comment, D20's `constexpr` spelling, D21's `categoryName`. Add the `static_assert` that `long` is 32 bits. Both architectures clean at `/W4`. **Six `CHANGELOG.md` bullets**, plus README line. | **YES** — the export-contract append |
**REV-5** | `code-reviewer` | Full-branch contract review covering **both** the rename and the append. Row by row verification; **R11 — argument order on the 15-parameter row verified against `parNames` explicitly**; strict-prefix-freedom across all names; `dumpbin /exports` shows only `caplDllGetTable4`; D18 dispositions verified against actual target documents; nothing describes renaming as generally available. | — |
**HUM-25** | Human | **Compile-only verification in real CANoe — this stage's acceptance gate, executable today.** A `.can` script referencing all six new operations plus `restifyReadVersion`, confirmed to compile. Also: (a) does CAPL accept `\"` and `\n` in a string literal — **determines whether Stage 17 should be pulled forward**; (b) is a literal accepted for an input `char[]`. No associative fields, so HUM-16 does not gate it. | — |
**HUM-14** | Human | **Runtime verification. Blocked by the CANoe licensing problem.** Plus: evaluate Stage 17's trigger. | — |

**No `build-pipeline-engineer` task.** The Makefile's globs pick up all four new files.

**CHANGELOG handling — edit in place, no `Changed` entry.** One edited bullet (CPP-24), six new bullets (CPP-6), no `Changed` entry.

---

## 8. Execution order

1. `build-pipeline-engineer` — cut the branch from `main` @ `b1c21f8`.
2. **CPP-24** — the rename, alone.
3. **CPP-23** — `Status` codes.
4. **CPP-22** — the two translation-layer files plus `docs/capl-sync-surface.md`.
5. **TEST-14** — coverage. Before CPP-6, deliberately.
6. **CPP-6** — the six rows, shims, CHANGELOG, README.
7. **REV-5** — full-branch review.
8. **HUM-25** — compile-time verification in CANoe. **The acceptance gate.**
9. Fold Stage 10 into the master plan as the last commit before merge.
10. Human — push, PR, merge.
11. **HUM-14** — runtime verification, once licensing is resolved.

### Sequencing decision — CONFIRMED

Merge happens **after REV-5 + HUM-25**, with **HUM-14 carried forward as an open, tracked obligation** rather than holding the merge until it passes. This is a deliberate, confirmed deviation from "never merge an unverified contract", accepted because CANoe's licensing problem has no known resolution date and stacking further stages on an unmerged branch would recreate the pre-Stage-9 problem. Residual risk tracked as R13.

---

## 9. Risk register

**R1 — Permanence versus partial verifiability. Narrowed, not eliminated.**
**R2 — x86 stack corruption from a table/signature mismatch. UNCHANGED by the compile-time evidence.**
**R3 — Reference parameters unproven. Now caught pre-merge by HUM-25**, with a free fallback (D20).
**R4 — Input over-read. CLOSED by the OQ3 ruling.**
**R5 — `-2` after a non-idempotent request. Relief arrives at Stage 11, not Stage 17.**
**R6 — Prose-only enforcement of the realtime prohibition.**
**R7 — Bitness parity.** Add the `long`-is-32-bit `static_assert`.
**R8 — Empty header value.**
**R9 — Name permanence.** Binds Stages 10–13 and 16–17.
**R10 — Static review does not substitute for execution.**
**R11 — Four consecutive `dword` parameters on `restifyRequestSync`.** No ordering removes this; mitigated by `parNames` and the verb helpers.
**R12 — Two unverified CAPL syntax facts** (string escapes, literal-as-array) — both checked in HUM-25.
**R13 — Merging on compile-time verification alone, with HUM-14 open.** Accepted per §8's confirmed sequencing decision.

---

## 10. What Stage 10 deliberately does not do

No response headers (D8) · no `skipTlsVerification` (D9) · no response store or re-read (D7) · no async anything · no JSON parsing · no `examples/*.can` (CPP-11, Stage 12, behind HUM-16) · no `INFO5` (D16) · no request builder (Stage 17) · no `categoryName` differentiation (D21) · nothing struct-shaped, because the ABI cannot express it.

---

## 11. Master-plan fold-in obligations

Last commit on the branch, before the merge — 13 items covering: the row-1 rename as a closed exception, widening `-4..-9`'s description, the strict-prefix-freedom invariant, the three standing naming rules, narrowing the async-realtime-safety claim, correcting Stage 3/HUM-13's status, recording the `INFO`/`INFO2`/`INFO3` foreclosure, recording the `INFO5`/`usageMask` finding, the BPE-12 case-sensitivity trap, fixing Stage 11's entry with the naming scheme, flagging Stage 17's now-live trigger, updating the master plan's 14 occurrences of `restifyGetVersion` (leaving the two completed stage plans untouched), and the new §12 ledger rows.

---

## 12. Approval status

**Approved in full.** Both approval items are resolved:

1. The complete plan is approved for hand-off to `plan-writer`.
2. §8's sequencing detail is confirmed: merge after REV-5 + HUM-25, with HUM-14 carried forward as an open obligation.

Still free to revisit at no cost once implementation begins, since these were approved by not being contested rather than by explicit ruling: **D16, D12's filing, D14** and **D21**.
