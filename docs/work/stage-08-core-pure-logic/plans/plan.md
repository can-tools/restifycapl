# Stage 8 — Core pure logic (level 0)

**Unit of work:** `stage-08-core-pure-logic`
**Branch:** `stage/08-core-pure-logic` (already cut locally off `main` @ `1ef097f`, not pushed)
**Master plan:** `docs/work/capl-rest-dll-rebuild/plans/plan.md` §8, Stage 8 — this document finalizes and expands that entry; it does not replace it.
**Human approval gate: NO** (see §7 below for why, stated explicitly rather than assumed).

---

## 1. Goal

Populate `src/core/` with the project's first real product logic — JSON→C++ type conversion, JSON path resolution, and a bounds-checked buffer copy — as pure, I/O-free, CANoe-unaware code, with GoogleTest coverage that runs outside CANoe on both architectures. Stage 8 adds **zero rows** to `CAPL_DLL_INFO_LIST4` and changes **zero** observable CAPL behavior.

---

## 2. Scope

**In scope**

| Area | Files |
|---|---|
| Status codes | `src/core/status.h` (new) |
| Type conversion | `src/core/type-conversion.h`, `src/core/type-conversion.cpp` (new) |
| JSON path | `src/core/json-path.h`, `src/core/json-path.cpp` (new) |
| Buffer copy | `src/core/buffer-copy.h`, `src/core/buffer-copy.cpp` (new) |
| Export glue rewiring | `src/module/exports.cpp` (modified — private helper internals only) |
| Build | `Makefile` (two `/I src` additions) |
| Tests | `tests/core/type-conversion_test.cpp`, `tests/core/json-path_test.cpp`, `tests/core/buffer-copy_test.cpp` (new); `tests/core/sanity-test.cpp` (deleted) |
| Plan fold-in | `docs/work/capl-rest-dll-rebuild/plans/plan.md` (§8 Stage 8 + §12 task ledger) |

**Explicitly not in scope**

- Any change to `CAPL_DLL_INFO_LIST4` — no new rows, no reordering, no renaming. The table diff must be **empty**.
- Any new CAPL-visible operation. Nothing in `src/core/` is exported this stage.
- `src/registry/`, `src/mapping/`, `src/http/` — later stages.
- `CHANGELOG.md` — no entry. Per `project-docs`, the CHANGELOG obligation is tied to an export-table change; Stage 8 has none, and CPP-16 is explicitly behavior-preserving, so there is no user-visible fact to record.
- Pushing, opening the PR, or merging. Per standing policy no agent pushes; those steps are human-only and must not be performed by any agent in this plan, including "optional" sub-actions.

---

## 3. Constraints repeated for downstream agents

Downstream agents are **not** assumed to have read `CLAUDE.md`, the skills, or the master plan. These constraints are binding on every task below.

1. **Export contract.** The real contract is `CAPL_DLL_INFO_LIST` / `CAPL_DLL_INFO4` in `src/module/exports.cpp`, not `exports.def`. Never rename, reorder, or remove an existing entry. Stage 8 appends nothing; the table must be byte-identical before and after.
2. **`/MT` is mandatory** for the project and every static dependency (libcurl, zlib, GoogleTest). Never mix `/MT` and `/MD` in one link. No task here adds a dependency.
3. **Bitness parity.** Both x86 and x64 must build and both test binaries must pass. The Makefile's single parameterized rule makes flag drift structurally impossible — do not add a second recipe, and do not special-case an architecture anywhere in `src/core/`.
4. **Nothing throws at the API boundary.** No `json::at()`, no unguarded `get<T>()`, no `std::stoi`/`std::stod` used where an exception can escape. Every failure is a returned `Status`.
5. **Nothing is `noexcept`.** Marking these functions `noexcept` converts a `std::bad_alloc` into `std::terminate` inside CANoe's host process. Do not add it, anywhere, including the tests' helpers.
6. **Narrowing discipline.** Every `size_t` → `int32_t`/`uint32_t` conversion needs an explicit range check. This is an x64-only defect class. Critically: **any range check inside `ToLong` must use fixed-width types (`std::int32_t` / `std::int64_t`) — never `size_t`, `ptrdiff_t`, or `intptr_t`** — or the overflow threshold silently differs between x86 and x64, and `NumericOverflow` stops being architecture-independent.
7. **Locale independence.** Number↔text conversion must use `nlohmann::json`'s own serializer or `std::to_chars`/`std::from_chars`. Never `sprintf`, `atof`, `std::stod`, or anything else that consults the global locale.
8. **Dependency direction.** `src/core/` must not include anything from `src/http/`, `src/registry/`, or `src/mapping/`, and must not include any CAPL SDK header. Only `src/module/` may include the CAPL SDK. `src/module/` → `src/core/` is an allowed direction (level 4 → level 0) and is what CPP-16 introduces.
9. **`/W4` clean** on both architectures. New files must not introduce warnings.
10. **Tests run outside CANoe.** The Makefile compiles `src/core`, `src/http`, `src/registry`, `src/mapping` into the test executable and deliberately excludes `src/module`. Nothing added to `src/core/` may need the export glue to be testable.

---

## 4. Decisions carried in (all approved, do not relitigate)

**D1 — Path syntax.** `data.items[0].name` — dot for object keys, brackets for array indices.
**This is explicitly interim, not the target architecture.** Struct mapping (currently deferred per `CLAUDE.md` Scope) is the real destination for structured access. This syntax must be documented as a stated caveat in `json-path.h`'s header comment — a named interim contract, not a silent permanent one — so a future struct-mapping stage is free to supersede it without archaeology.

**D2 — `To*` / `Parse*` boundary.** JSON→C++ only, entirely inside `src/core/`, zero CAPL awareness. The `To*` family takes `const nlohmann::json&`. `ParseLong`/`ParseDouble` take `std::string_view` and are a **deliberately named lenient escape hatch** — they are never silently invoked by the `To*` family. A `To*` function receiving a JSON string where a number was requested returns `TypeMismatch`; it does not quietly fall back to `ParseLong`.

**D2a — Filing.** `CopyToBuffer` lives in its own `src/core/buffer-copy.{h,cpp}`, not in `type-conversion.h`. Rationale, to be reflected in both headers' comments: `type-conversion.h` holds only functions whose direction is *JSON/text in, C++ value out*; `buffer-copy.h` holds the one function whose direction is the opposite — *C++ value out to a raw caller-owned buffer* — and it has exactly one consumer, `src/module/exports.cpp`.

**D3 — `Status` enum.** Absorbs the existing numeric codes; does not start fresh. See §5.1 (normative).

**D3.2 / D3.3 — Segment-kind resolution rule.** See §5.2 (normative). Mid-path scalar is **not** a special case — it is the last row of that table applied.

**D3.4 — `NullValue` stays distinct from `TypeMismatch`.** Confirmed against a real CAPL use case: a JSON `null` is expected to receive a default silently, while a wrong-typed value is logged or treated as an error. Merging them would destroy that distinction at the CAPL boundary.

**D3.5 — Non-integral number into an integer accessor** (`ToLong` on JSON `3.7`) returns the dedicated `NotIntegral`. **Explicitly not silent truncation.** TEST-2 must assert this directly.

**D4 — CPP-16 / TEST-12 fold-in: YES, now, as part of Stage 8.** No longer "optional, skip without ceremony" — the master plan's conditional is resolved in favour of doing it.

**D5 — Branch.** `stage/08-core-pure-logic`, off `main` @ `1ef097f`. Pushing stays manual.

**D6 — Makefile.** Add `/I src` to both `INCLUDES` and `TEST_INCLUDES`. `build-pipeline-engineer`'s task, not `cpp-implementer`'s.

**D7 — Delete `tests/core/sanity-test.cpp`** as part of landing the real tests. The file self-documents this as its intended fate ("Once real src/core/ logic lands … this file should be replaced"). Not a separate task.

**ValueToText leaf-stringify behavior.** See §5.3 (normative).

---

## 5. Normative artifacts

These three blocks are the specification. They go into the source headers **verbatim** (the enum as code, the tables as header comments) and are the reference `code-reviewer` checks against.

### 5.1 `src/core/status.h`

The enum lives in its own header, shared by `json-path.h`, `type-conversion.h`, and `buffer-copy.h`. It is **not** folded into any one of them, precisely because all three need it and any choice of host would create an arbitrary include dependency.

```cpp
enum class Status : int32_t {
  Ok = 0,
  InvalidArgument = -1,
  BufferTooSmall = -2,
  VersionResourceUnavailable = -3,   // exports.cpp only; src/core/ can never produce this
  // -4..-9 reserved, currently empty, for future module-local glue codes
  ParseError = -10,          // no Stage 8 caller yet — document as forward reservation; likely Stage 9 (sync-operations) or Stage 12 (json-flatten)
  PathSyntaxError = -11,     // ParsePath: malformed path text, no document needed
  PathNotFound = -12,        // object key absent; also fires for a mid-path scalar node (D3.3 — see rule below)
  IndexOutOfRange = -13,     // array index >= size
  TypeMismatch = -14,        // segment/container kind mismatch (see rule below), or ValueToText on non-leaf node
  NullValue = -15,           // JSON null encountered where a typed value was requested (kept as distinct from TypeMismatch — user confirmed a real CAPL use case: null gets a default silently, wrong-type gets logged/treated as error)
  NumericOverflow = -16,     // valid JSON number, out of int32_t range for ToLong
  NotIntegral = -17,         // valid JSON number, has a fractional part, requested as an integer type — must NOT silently truncate
};
```

Two notes for the implementer, both load-bearing:

- The comment on `PathNotFound` says "also fires for a mid-path scalar node (D3.3)". **D3.3 as finally decided resolves a mid-path scalar to `TypeMismatch`, per the last row of the table in §5.2.** The enum block above is reproduced verbatim as approved; where its inline comment and §5.2 disagree, **§5.2 governs**, and the shipped comment on `PathNotFound` must be corrected to read *"object key absent"* only. `code-reviewer` should confirm the shipped header does not carry the contradiction forward.
- `VersionResourceUnavailable = -3` is declared in `src/core/status.h` but is **unreachable from `src/core/`**. It is declared here so the numbering space has a single owner and `-3` can never be re-minted for a core-layer meaning. The header comment must say so.

### 5.2 Segment-kind resolution rule for `ResolvePath` (verbatim into `json-path.h`)

| Segment kind | Container kind at that point | Result |
|---|---|---|
| `[n]` | array, `n >= size` | `IndexOutOfRange` |
| `.key` | object, key absent | `PathNotFound` |
| `[n]` | object (not an array) | `TypeMismatch` |
| `.key` | array (not an object) | `TypeMismatch` |
| either | scalar (string/number/bool) where a container was expected | `TypeMismatch` |

A JSON `null` encountered mid-path where a container was expected falls under the last row (`TypeMismatch`) — `NullValue` is reserved for a *terminal* node being read by a typed accessor, not for a traversal failure.

### 5.3 `ValueToText` leaf-stringify behavior (verbatim into `type-conversion.h`)

| Input node | Output | Status |
|---|---|---|
| JSON `null` | the text `"null"` | `Ok` |
| JSON number `42` | `"42"` | `Ok` |
| JSON `true` | `"true"` | `Ok` |
| JSON string | the string's own contents, unquoted | `Ok` |
| JSON object or array (non-leaf) | *(out untouched)* | `TypeMismatch` |

Rationale, to be recorded in the header:

- **`null` → `"null"` with `Ok`** guarantees `ValueToText` always succeeds for a genuine leaf — the same guarantee `ToText` gives for strings. No special-cased failure for null.
- **Container → `TypeMismatch`.** Stage 12's flattening logic (master plan §12, `src/mapping/json-flatten.*`) recurses through containers itself and only ever calls `ValueToText` on genuine leaves it has already discovered. A container arriving here therefore indicates a bug in the caller's recursion, not a normal runtime case, and must fail loudly rather than be silently serialized into a nested-JSON string that would then appear as a flattened "value".
- Numbers and bools are locale-independent per constraint §3.7.

---

## 6. Tasks

Task IDs are next-free against the master plan's §12 ledger as of `1ef097f` (highest allocated: CPP-17, BPE-28, TEST-12, REV-18). **Per §7.9 condition 2, an ID is spent when it reaches §12 — all five new IDs below must be written into the ledger in the fold-in commit.**

---

### BPE-29 — Makefile include path
**Agent:** `build-pipeline-engineer`
**Files:** `Makefile` (lines ~83 and ~177)

Add `/I src` to both include lists:

- `INCLUDES` (feeds `CXXFLAGS`, currently `/I include /I include/vendor /I include/vendor/capl-dll-sdk`)
- `TEST_INCLUDES` (feeds `TEST_CXXFLAGS`, currently `/I include /I include/vendor`)

**Why:** lets any file — product source, export glue, or test — write `#include "core/type-conversion.h"` regardless of its own location, consistent with the existing `include/`-style convention, and avoids relative `../../` paths from `tests/core/` and `src/module/`.

**Acceptance criteria**
- Both variables gain `/I src`; no other flag changes; no new recipe, no architecture-conditional anything.
- `make build-x86`, `make build-x64`, `make test ARCH=x86`, `make test ARCH=x64` still succeed on the pre-Stage-8 tree (i.e. the change is inert until CPP-18 lands).
- Both architectures continue to flow through the single parameterized rule.

**Must land first** — every other task's includes depend on it.

---

### CPP-18 — `src/core/status.h`
**Agent:** `cpp-implementer`
**Files:** `src/core/status.h` (new, header-only — no `.cpp`)

Implement §5.1 verbatim, with the one correction noted there (`PathNotFound`'s comment drops the mid-path-scalar clause; §5.2 governs). Include `<cstdint>`; use `#pragma once`. Header comment must record:

- that the values `0`/`-1`/`-2`/`-3` are **pre-existing, already-shipped codes** from `restifyGetVersion`, absorbed rather than renumbered;
- that `-4..-9` are reserved for module-local glue codes and currently empty;
- that `VersionResourceUnavailable` is unreachable from `src/core/`.

**Acceptance criteria**
- Underlying type is `int32_t`, explicitly.
- Every numeric value matches §5.1 exactly.
- No other header is included beyond `<cstdint>`; no CAPL SDK, no `json.hpp`.

---

### CPP-2 — `src/core/type-conversion.{h,cpp}`
**Agent:** `cpp-implementer`
**Files:** `src/core/type-conversion.h`, `src/core/type-conversion.cpp` (new)

Exactly seven functions. Suggested shape (the implementer may adjust the out-parameter style if it is uniform across all seven, but must not change the direction of the boundary or the Status semantics):

```
Status ToLong      (const nlohmann::json& value, std::int32_t& out);
Status ToDouble    (const nlohmann::json& value, double&       out);
Status ToBool      (const nlohmann::json& value, bool&         out);
Status ToText      (const nlohmann::json& value, std::string&  out);
Status ValueToText (const nlohmann::json& value, std::string&  out);
Status ParseLong   (std::string_view text,       std::int32_t& out);
Status ParseDouble (std::string_view text,       double&       out);
```

Semantics:

- **`ToLong`** — JSON integer in `int32_t` range → `Ok`. Out of range → `NumericOverflow`. Number with a fractional part (`3.7`) → `NotIntegral`, never truncation. A whole-valued float (`3.0`) → `Ok` with `3`. `null` → `NullValue`. Any non-number → `TypeMismatch`. Range check uses `std::int64_t`/`std::int32_t` only (constraint §3.6).
- **`ToDouble`** — any JSON number (integer or float) → `Ok`. `null` → `NullValue`. Non-number → `TypeMismatch`.
- **`ToBool`** — JSON `true`/`false` only → `Ok`. `null` → `NullValue`. Numbers `0`/`1` → `TypeMismatch` (strict; no truthiness).
- **`ToText`** — JSON string only → `Ok`, unquoted contents. `null` → `NullValue`. Number/bool/container → `TypeMismatch`. `ToText` is the strict string accessor; `ValueToText` is the lenient stringifier. Both exist on purpose; the header must say why.
- **`ValueToText`** — §5.3 verbatim.
- **`ParseLong` / `ParseDouble`** — lenient text→number, `std::from_chars`-based. Empty input → `InvalidArgument`. Unparseable → `ParseError`. Trailing non-numeric characters after a valid prefix → `ParseError` (strict-tail; "lenient" refers to accepting raw text at all, not to tolerating garbage). Out of `int32_t` range → `NumericOverflow`. `ParseLong` on text with a fractional part (e.g. `"3.5"`) → `NotIntegral`, for the same reason `ToLong` returns `NotIntegral` on a non-integral JSON number (D3.5) — never `ParseError` for this specific case, and never silent truncation. Never called from the `To*` family.

**Acceptance criteria**
- All seven present; no eighth, and no `CopyToBuffer` (it is CPP-16's, in a different header).
- No `at()`, no unguarded `get<T>()`, no throwing path reachable from any of the seven.
- Nothing `noexcept`.
- No `sprintf`/`atof`/`std::stod`/`std::stoi` anywhere.
- Header carries §5.3 verbatim, plus the `ToText`-vs-`ValueToText` and `To*`-vs-`Parse*` rationale.
- Includes `core/status.h` and `json.hpp`; nothing from `src/http/`, `src/registry/`, `src/mapping/`; no CAPL SDK header.
- `/W4` clean, x86 and x64.

---

### CPP-3 — `src/core/json-path.{h,cpp}`
**Agent:** `cpp-implementer`
**Files:** `src/core/json-path.h`, `src/core/json-path.cpp` (new)

Two public functions plus a segment type:

```
struct PathSegment { enum class Kind { Key, Index }; Kind kind; std::string key; std::uint32_t index; };

Status ParsePath  (std::string_view path, std::vector<PathSegment>& out);
Status ResolvePath(const nlohmann::json& document, std::string_view path,
                   const nlohmann::json*& out);
```

- `ParsePath` is **document-free**: it validates text only and can return only `Ok`, `InvalidArgument`, or `PathSyntaxError`. It is public and directly tested.
- `ResolvePath` parses, then walks, applying §5.2 at every step. On success `out` points **into the caller's document** — a non-owning pointer whose lifetime is the document's. The header must state this explicitly.

**Edge cases resolved by default in this plan** (none were separately decided; each is recorded here so it is a decision rather than an implementer's coin-flip — flag before implementation if any is wrong):

| Case | Resolution |
|---|---|
| Empty path `""` | `PathSyntaxError`. An accessor asking for "nothing" is a caller bug; whole-document access has no CAPL use case. |
| Leading dot, `.items[0]` | `PathSyntaxError`. The first segment is a bare key or a bracket index. |
| Top-level index, `[0].name` on a root array | Valid — `Ok`. |
| Negative or non-numeric index, `[-1]`, `[x]` | `PathSyntaxError` (structural — `ParsePath` never sees a document). |
| Index too large for `uint32_t` | `PathSyntaxError`. |
| Unclosed bracket, `items[0`, empty brackets `[]`, `..`, trailing `.` | `PathSyntaxError`. |
| Key containing `.` or `[` | Unreachable via this syntax. Not an error, not escaped — a documented limitation of the interim syntax (D1). |
| Whitespace | Not trimmed; part of the key. |

**Acceptance criteria**
- §5.2's table is in the header **verbatim, as a normative table**.
- The D1 interim-syntax caveat is in the header, naming struct mapping as the intended successor.
- No `at()`, no unguarded `get<T>()`, no exception escapes, nothing `noexcept`.
- Every `size_t` → `uint32_t`/`int32_t` conversion range-checked (index parsing especially).
- No dependency on `type-conversion.h` (the two modules are siblings and must stay independent).
- `/W4` clean, x86 and x64.

---

### CPP-16 — `src/core/buffer-copy.{h,cpp}` + rewire `exports.cpp`
**Agent:** `cpp-implementer`
**Files:** `src/core/buffer-copy.h`, `src/core/buffer-copy.cpp` (new); `src/module/exports.cpp` (modified)
**Load `capl-export-contract` before starting** — even though no table row changes.

**Part A — the new module**

```
Status CopyToBuffer(std::string_view text, char* buffer, std::uint32_t bufferSize);
```

Semantics, reproducing today's `CopyOwnVersionString` tail exactly:

1. `buffer == nullptr || bufferSize == 0` → `InvalidArgument`, writes nothing.
2. Otherwise `buffer[0] = '\0'` first.
3. If the text does not fit with its terminator → `BufferTooSmall`, buffer left **empty, never truncated**.
4. Otherwise copy, NUL-terminate, `Ok`.

**Overflow-free fit test:** compare `text.size() >= static_cast<std::size_t>(bufferSize)` rather than `text.size() + 1 > bufferSize`. The `+ 1` form can wrap at `SIZE_MAX`; the two are otherwise equivalent. This is exactly the narrowing class in constraint §3.6 and must not be written the naive way.

**Part B — the rewiring, and its one real hazard**

`CopyOwnVersionString` today does two things in order that must not be collapsed:

- it validates `buffer`/`bufferSize` and sets `buffer[0] = '\0'` **before** any Win32 work, so that the five `-3` resource-reading failures also leave the caller's buffer empty rather than untouched;
- it performs the bounds-checked copy at the end.

Only the second is CPP-16's extraction target. **`exports.cpp` must retain its own early `-1` guard and its own early `buffer[0] = '\0'`.** Moving those into `CopyToBuffer` would push the null-pointer check behind `GetModuleHandleExA` and would stop the `-3` paths from clearing the buffer — a silent behavior change in already-shipped, already-reviewed code. `CopyToBuffer`'s own guard is then defense-in-depth for its future callers, not the only guard.

The five Win32 branches producing `-3` stay in `exports.cpp` verbatim. The existing `strnlen(versionText, versionTextLen)` length computation stays in `exports.cpp` too; its result is handed to `CopyToBuffer` as a `std::string_view{versionText, textLen}`.

`CopyOwnVersionString` keeps its current signature `long(char*, unsigned long)` and converts at the boundary with `static_cast<long>(status)`. On Windows `unsigned long` is 32 bits on both x86 and x64, so `unsigned long` → `std::uint32_t` is width-preserving on both targets — note it in review rather than assuming it.

Replace the bare literals `0`/`-1`/`-2`/`-3` in `exports.cpp` with the `Status` symbols. **The numeric values must remain byte-for-byte identical to today.** This is a symbol introduction, not a renumbering. `restifyGetVersion` (CPP-1, DONE, already reviewed and merged) must return the same value for every input it can receive.

**Acceptance criteria**
- `git diff` on the `CAPL_DLL_INFO_LIST4[]` array literal is **empty**.
- `exports.def` unchanged; no `LIBRARY` line anywhere.
- `-1`, `-2`, `-3` numeric values unchanged; `0` unchanged.
- The `-1` guard and the early `buffer[0] = '\0'` remain in `exports.cpp`, before the first Win32 call.
- All five `-3` branches remain in `exports.cpp`, unmoved and unmerged.
- `restifyGetVersion` observably identical for every input: valid buffer with room, buffer exactly one byte too small, `bufferSize == 0`, `buffer == nullptr`, and each resource-failure path.
- `src/module/exports.cpp` now includes `core/buffer-copy.h` and `core/status.h` — an allowed direction. It remains the only file under `src/` including the CAPL SDK.
- `buffer-copy.h` includes only `core/status.h`, `<cstdint>`, `<string_view>`; no `json.hpp` (it has nothing to do with JSON), no CAPL SDK.
- The "not unit-tested, CAPL/Win32 module glue" comment in `exports.cpp` stays accurate for what remains there.

---

### TEST-2 — `tests/core/type-conversion_test.cpp`
**Agent:** `test-engineer`

Required cases, at minimum:

- **`ToLong`:** valid integer; `3.7` → **`NotIntegral`** (assert the status, and assert `out` was not set to `3`); `3.0` → `Ok`/`3`; `2147483648` and `-2147483649` → `NumericOverflow`; a value beyond `int64_t` too; `null` → `NullValue`; string `"42"` → `TypeMismatch` (proves no silent `ParseLong` fallback); `true` → `TypeMismatch`; object and array → `TypeMismatch`.
- **The overflow case must be exercised in both the x86 and x64 test binaries** (it is the same test source; the requirement is that `make test ARCH=x86` and `make test ARCH=x64` both run and both pass) — this is the architecture-parity check for constraint §3.6.
- **`ToDouble`:** float; integer-typed number; `null` → `NullValue`; string → `TypeMismatch`.
- **`ToBool`:** `true`/`false`; `0`/`1` → `TypeMismatch`; `"true"` → `TypeMismatch`; `null` → `NullValue`.
- **`ToText`:** string → unquoted contents; number → `TypeMismatch`; `null` → `NullValue`; container → `TypeMismatch`.
- **`ValueToText`:** all five rows of §5.3, individually — including `null` → `"null"` with `Ok`, and object *and* array → `TypeMismatch` with `out` untouched.
- **`ParseLong` / `ParseDouble`:** valid; empty → `InvalidArgument`; `"abc"` → `ParseError`; `"12abc"` → `ParseError`; `"99999999999"` → `NumericOverflow`; `"3.5"` into `ParseLong` → `NotIntegral` (assert this directly; not `ParseError`).
- **Locale independence:** a decimal-comma-locale regression test if it can be done portably; otherwise a comment recording that `from_chars`/nlohmann were chosen for this reason and the test is a static guarantee, not a runtime one.
- Malformed/missing-field coverage per `cpp-testing-conventions`.

### TEST-3 — `tests/core/json-path_test.cpp`
**Agent:** `test-engineer`

- `ParsePath` alone: every row of the edge-case table in CPP-3.
- `ResolvePath`: a valid nested hit (`data.items[0].name`); **all five rows of §5.2, one test each**; the D3.3 mid-path scalar (`user.name.first` where `name` is a string) asserting `TypeMismatch`; mid-path `null` → `TypeMismatch`; empty array with `[0]` → `IndexOutOfRange`; deeply nested; document root that is an array.
- A test asserting the returned pointer aliases the document (not a copy).

### TEST-12 — `tests/core/buffer-copy_test.cpp`, and delete the sanity test
**Agent:** `test-engineer`

- `CopyToBuffer`: exact fit; one byte short → `BufferTooSmall` **with the buffer left empty, asserted**, not truncated; `bufferSize == 0` → `InvalidArgument`; `nullptr` buffer → `InvalidArgument`; empty text into a 1-byte buffer → `Ok`; a guard-byte test proving no write past `bufferSize`.
- **Delete `tests/core/sanity-test.cpp`** in the same change (D7). It self-documents this as its purpose; leaving it would be a second, weaker claim about what the suite proves.

**Naming note, flagged rather than assumed.** The new files use the `_test.cpp` suffix as specified in D2a. The only existing test file uses `-test.cpp`, and it is being deleted — so after Stage 8 the suite is uniformly `_test.cpp`. All three new files must use the same suffix; do not mix. The Makefile's `TEST_CASE_SRCS` is `$(wildcard tests/core/*.cpp …)`, so either suffix is picked up and no build change is needed.

**Acceptance criteria for all three test tasks**
- `make test ARCH=x86` and `make test ARCH=x64` both pass.
- No test is `noexcept`; no test depends on `src/module/`.
- Each new source module has exactly one mirroring test file.

---

### REV-19 — Final review
**Agent:** `code-reviewer`
**Subject:** `git diff main...HEAD` on `stage/08-core-pure-logic`

Checklist, in priority order:

1. **Export contract.** `CAPL_DLL_INFO_LIST4[]` diff empty. `exports.def` untouched, no `LIBRARY` line. No row renamed, reordered, or removed.
2. **`restifyGetVersion` behavior preservation.** `-1`/`-2`/`-3`/`0` unchanged numerically; the early guard and early `buffer[0]='\0'` still precede the first Win32 call; all five `-3` branches still in `exports.cpp`; same return for every input class.
3. **`Status` enum** matches §5.1 exactly, with the `PathNotFound` comment corrected per §5.1's note.
4. **§5.2 and §5.3** present verbatim in the respective headers.
5. **D1 interim-syntax caveat** present in `json-path.h`.
6. **Architecture parity:** no `size_t`/`ptrdiff_t`/`intptr_t` in any range check; no architecture-conditional code in `src/core/`; both test legs pass.
7. **`/MT`** unchanged; no new dependency; no `/MD` anywhere.
8. **No `noexcept`; no `at()`; no unguarded `get<T>()`;** no `sprintf`/`atof`/`std::stod`/`std::stoi`.
9. **Dependency direction:** `src/core/` includes nothing from `http`/`registry`/`mapping` and no CAPL SDK; `exports.cpp` remains the only CAPL-SDK includer.
10. **Makefile:** only the two `/I src` additions; single parameterized rule intact; no hardcoded version number anywhere in the diff.
11. **Comment discipline** (§6b of the master plan): rationale where it will be read; the protected comments in `exports.cpp` trimmed at most, never removed.
12. **`tests/core/sanity-test.cpp` deleted**, and the suite still proves the pipeline works (the real tests now do).

---

## 7. Human approval gate: NO — stated explicitly

The master plan's Stage 8 entry says "Human approval: no." **That still holds after folding in CPP-16**, and the reasoning is recorded here rather than assumed:

- The gate exists for changes to the export contract, to `/MT`, to what gets published, or to CI's shipping behavior. Stage 8 does none of these.
- CPP-16 touches `exports.cpp`, but only the **internals of a file-local helper in an anonymous namespace**. `CAPL_DLL_INFO_LIST4` is untouched, `exports.def` is untouched, no CAPL-visible name or signature changes, and no new operation appears.
- There is therefore no new artifact for a human with CANoe to verify — `restifyGetVersion`'s CANoe-side gate (HUM-13) already covers the behavior CPP-16 is required to preserve, and preservation is verified by REV-19 plus TEST-12, not by a second CANoe session.
- `code-reviewer` (REV-19) is still **mandatory**, because `exports.cpp` is in the diff. "No human gate" is not "no review".

If REV-19 finds that CPP-16 changed observable behavior in any way, that finding **escalates to a human gate immediately** — the no-gate status is conditional on the preservation claim holding.

---

## 8. Execution order

1. **BPE-29** — Makefile `/I src`. Must be first; everything else's includes depend on it. Verify the tree still builds and tests green before moving on.
2. **CPP-18** — `src/core/status.h`. Must precede all three modules.
3. **CPP-2** and **CPP-3** — independent of each other; either order, or in parallel.
4. **CPP-16** — `buffer-copy` + `exports.cpp` rewiring. Independent of CPP-2/CPP-3; sequence it last among the implementation tasks so the highest-risk diff is written against a known-good tree.
5. **TEST-2**, **TEST-3**, **TEST-12** — after their respective modules; `sanity-test.cpp` deleted with TEST-12.
6. **Verification sweep:** `make test ARCH=x86`, `make test ARCH=x64`, `make all`. All four must be green, `/W4` clean, before review.
7. **REV-19** — `code-reviewer` on the full branch diff.
8. **Fold-in commit** — the last commit on the branch, before the PR is marked Ready (master plan §7.9 condition 2). It must:
   - rewrite master plan §8's Stage 8 entry to the finalized scope (CPP-16/TEST-12 no longer "optional");
   - add §12 ledger rows for **CPP-18, BPE-29, REV-19** and update **CPP-2, CPP-3, CPP-16, TEST-2, TEST-3, TEST-12** to DONE;
   - record the disposition of this working document — **recommendation: left in place as the detailed record**, with the master plan taking a summary subsection, since the `Status` enum and the two normative tables are long and belong with the code they specify;
   - note that the `-4..-9` reservation and the `ParseError` forward reservation are now live commitments future stages inherit.
9. **Human-only, no agent action:** push the branch, review/merge the auto-opened PR, confirm CI green on both legs. No agent runs any `git push`, and no agent performs any sub-step of this item, optional or otherwise.

---

## 9. Risks

**R1 — CPP-16 silently changes `restifyGetVersion`.** The highest-stakes item in the stage. The specific failure mode is collapsing the early `-1` guard and early `buffer[0]='\0'` into `CopyToBuffer`, which reorders the null check behind `GetModuleHandleExA` and stops the `-3` paths clearing the buffer. Compiles fine, tests fine, wrong in CANoe. *Mitigation:* explicit requirement in CPP-16 Part B, explicit REV-19 checklist item 2, plus TEST-12's buffer-left-empty assertions.

**R2 — Export-table diff through carelessness.** Nothing in this stage needs the table edited, so any diff there is by definition unintended — including a reformat or a comment reflow. *Mitigation:* REV-19 item 1 checks for an **empty** diff on the array literal, not a "reasonable" one.

**R3 — Architecture-dependent overflow threshold.** A `size_t`-based range check in `ToLong` makes `NumericOverflow` fire at different inputs on x86 and x64 — a parity break invisible to a single-architecture test run. *Mitigation:* constraint §3.6, CPP-2 acceptance criteria, REV-19 item 6, and TEST-2's requirement that the overflow case run in both binaries.

**R4 — `Status` numbering drift.** Any future contributor who reads `-10` as "the first code" and renumbers `-1`/`-2`/`-3` breaks shipped CAPL scripts. *Mitigation:* CPP-18's header comment states the absorption explicitly and marks `-4..-9` reserved.

**R5 — The `PathNotFound` comment contradiction.** §5.1's inline comment and §5.2's table disagree about mid-path scalars. Shipped as-is, the header would be self-contradicting documentation of a normative rule. *Mitigation:* §5.1's correction note, REV-19 item 3.

**R6 — D1's interim syntax hardening into a permanent contract by accident.** Once a `.can` example or a CAPL script uses `data.items[0].name`, changing it becomes a breaking change even though nothing is exported yet. *Mitigation:* the caveat lives in the header, not only in this plan; and Stage 8 exports nothing, so the window is still open.

**R7 — `ValueToText` containers.** Returning `TypeMismatch` is a deliberate loud failure. If Stage 12's flattening turns out to *want* a lenient serialization, this decision must be revisited **there, explicitly**, not patched by making `ValueToText` quietly serialize containers. *Mitigation:* the rationale is in the header so Stage 12's implementer sees it at the point of temptation.

**R8 — Unused code linked into the product DLL.** `type-conversion` and `json-path` are compiled into both DLLs by the Makefile's `$(wildcard)` but called by nothing until Stage 12/13. Harmless, expected, and worth not "fixing": the wildcard picking them up automatically is the designed behavior. Only `buffer-copy` has a caller this stage.

**R9 — Test-file suffix divergence.** `-test.cpp` vs `_test.cpp`. No build impact (wildcard), pure consistency. Resolved in favour of `_test.cpp` per D2a, uniformly, with the only counterexample deleted in the same change.

---

## 10. Residual open items

None blocking. One thing is resolved *by this plan* rather than by prior discussion, and should be flagged now if it is wrong:

- **The `json-path` edge-case table in CPP-3** (empty path, leading dot, negative index, unclosed bracket, keys containing `.`). Defaults chosen and documented; no prior decision existed.
