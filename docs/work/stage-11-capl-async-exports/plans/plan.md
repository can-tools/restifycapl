# Stage 11 — Expose asynchronous REST to CAPL (second contract append)
## FINAL PLAN — for user sign-off, then hand-off to `plan-writer`

Branch `stage/11-capl-async-exports`, cut from `main` @ `86ab5f4`. Slug `stage-11-capl-async-exports`.

---

## 1. Goal

Make REST callable from CAPL without blocking. This stage adds:

- a concurrent asynchronous execution layer: 8 request slots, a lazily grown pool of worker threads, and DLL-resident response state;
- a pointer+size translation layer;
- a cooperative cancel hook in the existing HTTP transport;
- **eleven rows appended to `CAPL_DLL_INFO_LIST4`**: six `…Async` dispatch rows and five lifecycle rows (`restifyPollResponse`, `restifyAwaitResponse`, `restifyReadResponse`, `restifyDiscardResponse`, `restifyDiscardAllResponses`).

**Two properties are designed in deliberately:**

- **Dispatch, poll, read and single discard are safe to call from a Simulation Setup node.** This is the conforming way to reach REST from the realtime branch.
- **A response survives the call that produced it.** A too-small read can be retried without re-sending the request.

**No behavioural change to the sync surface** (`sync-operations.*`, `sync-text-api.*`, rows 0–7). The only change to Stage 9's reviewed code is the cancel hook in `http-client.*` (D18). It is behaviour-preserving for every existing caller.

---

## 2. Open questions — all closed

All eighteen were ruled explicitly by the user, one at a time.

| OQ | Ruling |
|---|---|
| OQ1 | `requestId` is a `dword` reference out-parameter. 0 is reserved as "no id". A global counter, seeded per load (see OQ15). Every lifecycle row takes `requestId`. |
| OQ1c | Dispatch writes `requestId = 0` on entry and overwrites it only on `Ok` (D5). |
| OQ2 | Persistent worker threads calling the existing `HttpClient::Perform` unchanged. Rejected: (d) a cooperative multi-interface loop, and the multi interface inside the worker. Reason: Simulation Setup use is in scope, so libcurl's work must stay off CANoe's realtime thread. |
| OQ3 / OQ3b | **Design 3.** No pin. Workers hold a module reference, exit themselves after idle time T = 30 s via `FreeLibraryAndExitThread`, and are never joined or signalled from `DllMain` (D9). |
| OQ4 | The full per-slot state table and memory rules (D7, D8). |
| OQ5 | Dispatch is rejected only when all 8 slots are occupied, and never evicts an unread response. Closed by approving OQ4's definitions, not asked separately. |
| OQ6 | Status codes `-24..-29` (D6). |
| OQ7 | `restifyPollResponse` returns 0 plus `state` 1 (in flight) / 2 (complete). `-27` is its only error return. An enumeration, not a boolean (D11). |
| OQ8 | Read keeps its two outcomes separate, with a normative write order. (a) The copy happens under the lock, bounded by the caller's buffer. (b) Dispatch validates all input up front, so `requestStatus` carries only transport outcomes (D12, D5). |
| OQ9 | `restifyAwaitResponse` blocks and returns 0 / `-27` / `-28`. `waitTimeoutMs = 0` means the request's own total timeout. A discard during the wait wakes it with `-27` (D13). |
| OQ10 / OQ13 | The final 11-row signature table, row order and `categoryName "Async"` (D4). |
| OQ11 | Sync responses are **not** cached. Stage 9's obligation is discharged. |
| OQ12 | File layout per D16, including the cancel hook in `http-client.*` and a module-lifetime seam inside `async-operations`. |
| OQ14 | Merge on REV-6 + HUM-15 + offline unit tests; HUM-27 is carried forward. **The real-DLL smoke harness was proposed and declined** (R-E). |
| OQ15 | The id counter is seeded from a high-resolution clock on the first dispatch after each load (D10). |
| OQ16 | The full `maxResponseBytes` range is accepted and documented, not clamped. |
| OQ17 / OQ17b | Two lifecycle rows added: `restifyDiscardResponse` (10th) and `restifyDiscardAllResponses` (11th). |
| OQ18 | Cooperative cancel hook; `-24 RequestCancelled` spent (D18). |

**HUM-26 — SATISFIED.** Its gate was "rule on every open question and sign off the signature table before any row is written". It is discharged by the rulings above and the OQ10 sign-off.

---

## 3. Constraints carried in

Restated so downstream agents need no other document.

- **The export contract is `CAPL_DLL_INFO_LIST4` in `src/module/exports.cpp`, not `exports.def`.** Append-only: never rename, reorder or remove. Rows 0–7 stay byte-identical. The one-time rename at Stage 10 is closed and is not a precedent.
- `exports.def` stays exactly `EXPORTS` + `caplDllGetTable4`, with **no `LIBRARY` line**.
- **Naming:** `restify<VerbNoun>`, with `<Operation><Sync|Async>` for dispatch. Lifecycle rows have no suffix. **No exported name may be a strict prefix of another**; this is verified for all 18 names (D4).
- **Every table function pointer is `extern "C" … CAPLPASCAL` (`__stdcall`).** This is load-bearing on x86.
- **Never return a raw text pointer.** Always write to a caller-supplied buffer with its size.
- **Every input `char[]` is paired with its `dword` size immediately after it**, meaning the caller's `elcount()`. No terminator within that size gives `UnterminatedInputText` (-6).
- **Out-parameters are CAPL reference parameters:** `type - 128`, array depth 0, a pointer on the C++ side, spelled with the existing `kRefLong` / `kRefDword` constants.
- **`#pragma pack(push, 1)` / `pack(pop)` covers the whole table**, through the terminating row.
- **Stay on `CAPL_DLL_INFO4`. Do not revisit `INFO5` / `usageMask`.**
- **`skipTlsVerification` is not exposed** and no CAPL-reachable path may influence it. TLS revocation checking stays enabled; it is TLS policy and not a thing to turn off by reflex.
- **`Status` codes are appended, never renumbered.** After this stage the spent codes are `0`, `-1..-6`, `-10..-29`, and `-7..-9` remain reserved.
- **`src/module/` is the only place that may include CAPL SDK headers, and the only place excluded from `make test`.** No real logic may live there.
- **`/MT` everywhere. x86 and x64 identical except `/MACHINE:` and library path. Both clean at `/W4`.** There is no new dependency; threads and mutexes come from the static CRT already linked.
- **Every export-table append gets a `CHANGELOG.md` `[Unreleased]` entry in the same change.**
- **Comment discipline (`project-docs`):**
  - no plan/stage/task identifiers in any comment;
  - normative material goes in `docs/<topic>.md`;
  - this plan gives tier and line budget only, never comment wording.
- **Only `build-pipeline-engineer` touches the build mechanism.** Every other agent uses `make <target>` via the single-call activation pattern in `msvc-build-conventions`. **If the build won't run, stop and escalate; do not script around it.**
- **Branch-only work.** `git push`, the PR, retargeting and merge are human-only. Branch cutting is `build-pipeline-engineer`'s.
- **Two call-site facts from Stage 10 carry into docs and descriptions:**
  - CAPL requires a declared `char[]` variable for text arguments; a string literal is not accepted.
  - The realtime caveat must be stated correctly for each row.

---

## 4. Findings

1. **Stage 9 already made the transport thread-safe** (`docs/http-layer.md`): `CURLOPT_NOSIGNAL=1`, one easy handle per `Perform`, `curl_global_init` behind `std::call_once`. No transport change is needed except D18.
2. **Vector prescribes the polling model.** Callbacks and CAPL calls from DLL threads are forbidden; a CAPL `on timer` collects data produced by a DLL thread under a mutex (`CAPLExportTable.htm.md`, line 141). Vector also documents that several CAPL nodes may use one DLL at the same time.
3. **The installed libcurl is 8.21.0, not 8.22.0.** Evidence: `include/vendor/curl/curlver.h:35` and both `libcurl.pc`. `vcpkg.json` no longer carries the 8.22.0 override; the master plan's BPE-15 snippet is stale.
4. **libcurl's DNS threads are joined, not detached, when an easy handle is cleaned up** (verified in 8.21.0 source under `C:\Users\darek\AppData\Local\vcpkg\buildtrees\curl\src\url-8_21_0-3ffa9f46b7.clean\`):
   - `curl_easy_cleanup` → `Curl_close` → `curl_multi_cleanup(multi_easy)`, in `url.c:220-223`;
   - which calls `Curl_async_thrdd_multi_destroy(multi, !quick_exit)`, in `multi.c:3027`;
   - `Curl_thrdpool_destroy` then joins every thread, in `thrdpool.c:342-347`.

   Threads are detached only if `CURLOPT_QUICK_EXIT` is set. Nothing in `src/` sets it. **Consequence: no curl-created thread outlives `Perform`**, which makes design 3 fully race-free.
5. **`AsynchDNS` is compiled in** (`libcurl.pc supported_features`). A request that times out during DNS still blocks its worker in cleanup until `getaddrinfo` returns: `totalTimeoutMs` is not a hard ceiling. The same is true, as a "may", of Schannel revocation checks (revocation checking is on). Both affect only the worker, not the realtime thread.
6. **`http-client.cpp` passes the request body with `CURLOPT_POSTFIELDS`, which libcurl does not copy.** Async dispatch must therefore copy its input into slot-owned storage that lives until the slot is reclaimed.
7. **The SDK names execution environments besides the desktop**: CAPL-on-Board (Green Hills), ELF mapping (`caplDllElfMappingCOB`), RT standalone, and VN89 devices. The user confirmed their VN/VT runtime is Windows. How that runtime loads and unloads the DLL has not been observed.
8. **The CAPL compiler fully loads this DLL to read its table** (`cdll.h:249-281`; we export `caplDllGetTable4` as a function). Nothing heavy may happen at load: no thread, no module reference, no allocation before the first dispatch.
9. `docs/status-codes.md`'s "Reserved ranges" section documents neither `-18..-23` nor any of `-24..-29`. This is a doc gap; CPP-26 closes it.
10. `tests/http/fake-transport.h` is already mutex-guarded. It needs per-request **gates**, not time delays, to support deterministic concurrency tests.

---

## 5. Design decisions

**D1 — Scope of the append.** Eleven rows (D4). The async family is N-agnostic at the contract level: no row exposes N, so raising N later is non-breaking.

**D2 — N = 8.** A compile-time constant, identical on both architectures, documented and not part of the contract.

**D3 — Worker model.** A lazily grown pool of **at most 8 workers**, not tied to specific slots. Workers share one `workAvailable` condition variable, and each calls `HttpClient::Perform` unchanged. A `Complete` slot occupies a slot but not a worker.

**D4 — The contract.** Rows 8–18. Every row returns `'L'` and has `categoryName "Async"`. `parNames` are identical to the sync rows wherever the parameter is the same.

| Row | Name | Signature | n | `parTypes` | `array` |
|---|---|---|---|---|---|
| 8 | `restifyGetAsync` | `(char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, dword& requestId)` | 5 | `{'C','D','C','D',kRefDword}` | `{1,0,1,0,0}` |
| 9 | `restifyDeleteAsync` | same as row 8 | 5 | same as row 8 | same as row 8 |
| 10 | `restifyPostAsync` | `(char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char requestBody[], dword requestBodySize, dword& requestId)` | 7 | `{'C','D','C','D','C','D',kRefDword}` | `{1,0,1,0,1,0,0}` |
| 11 | `restifyPutAsync` | same as row 10 | 7 | same as row 10 | same as row 10 |
| 12 | `restifyPatchAsync` | same as row 10 | 7 | same as row 10 | same as row 10 |
| 13 | `restifyRequestAsync` | `(char method[], dword methodSize, char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char requestBody[], dword requestBodySize, dword connectTimeoutMs, dword totalTimeoutMs, dword maxResponseBytes, dword& requestId)` | 12 | `{'C','D','C','D','C','D','C','D','D','D','D',kRefDword}` | `{1,0,1,0,1,0,1,0,0,0,0,0}` |
| 14 | `restifyPollResponse` | `(dword requestId, long& state)` | 2 | `{'D',kRefLong}` | `{0,0}` |
| 15 | `restifyAwaitResponse` | `(dword requestId, dword waitTimeoutMs)` | 2 | `{'D','D'}` | `{0,0}` |
| 16 | `restifyReadResponse` | `(dword requestId, char responseBody[], dword responseBodySize, long& requestStatus, long& httpStatusCode, dword& responseBodyLength)` | 6 | `{'D','C','D',kRefLong,kRefLong,kRefDword}` | `{0,1,0,0,0,0}` |
| 17 | `restifyDiscardResponse` | `(dword requestId)` | 1 | `{'D'}` | `{0}` |
| 18 | `restifyDiscardAllResponses` | `(dword& stillRunning)` | 1 | `{kRefDword}` | `{0}` |

**Name checks on the final 18 names.**

- **Strict-prefix-freedom: verified.** Every pair sharing a stem diverges before either name ends: `…GetSync`/`…GetAsync` (and the other verbs), `…ReadVersion`/`…ReadResponse`, `…DiscardResponse`/`…DiscardAllResponses`, `…PollResponse`/`…PostAsync`, `…PutAsync`/`…PatchAsync`.
- **Length:** the longest name is 26 characters, within the 50-character limit.

HEAD is reachable only through row 13, as in sync.

**D5 — Dispatch semantics (normative).**

1. On entry, set `requestId = 0`.
2. Run **all validation that sync performs**, before any slot is taken, with **identical codes**:
   - text bounds: `-1` or `-6`;
   - method: `-5`;
   - header grammar: `-4`;
   - empty URL: `-1`;
   - a body on GET/HEAD/DELETE: `-1`.

   Reuse `BoundedText`, `ParseMethodText` and `ParseHeaderBlock` unchanged.
3. Under the lock, find a `Free` or `Consumed` slot. If none, return `-25`. If the slot is `Consumed`, free its old buffers.
4. Mint the id, then copy the input into slot-owned storage. Mark the slot `Pending`.
5. If `idleWorkers == 0`, create a worker (D9). If that fails, roll the slot back to `Free` and return `-29`.
6. Signal `workAvailable`, write `requestId`, and return 0.

**The id is minted and stored in the slot before any worker is signalled.** A non-zero `requestId` means `Ok`, and nothing else.

**D6 — Status codes.** Appended to `src/core/status.h`.

| Code | Name | Where it comes from |
|---|---|---|
| `-24` | `RequestCancelled` | Transport layer: `CURLE_ABORTED_BY_CALLBACK`. Used internally and in tests; **CAPL never sees it** (D8). |
| `-25` | `NoFreeRequestSlot` | Dispatch: all 8 slots are occupied. |
| `-26` | `RequestNotComplete` | Read on a live id that is still `Pending` or `Running`. |
| `-27` | `UnknownRequestId` | Any lifecycle row given an id that is 0, never issued, `Consumed`, `Abandoned` or `Free`. |
| `-28` | `WaitTimeout` | Await reached its deadline. Distinct from HTTP `Timeout` (`-19`). |
| `-29` | `AsyncStartFailed` | A worker thread could not be created. |

**D7 — Per-slot state table (normative).** Every transition happens under the single table mutex. `Perform` always runs outside it.

| # | From | Event | To | Memory, and which thread |
|---|---|---|---|---|
| 1 | `Free` | dispatch | `Pending` | allocates (dispatch) |
| 2 | `Consumed` | dispatch reclaims the slot | `Pending` | frees the old buffers, then allocates (dispatch) |
| 3 | `Pending` | a worker claims it | `Running` | none |
| 4 | `Running` | `Perform` returns; result stored | `Complete` | worker allocates |
| 5 | `Complete` | read returns 0 | `Consumed` | **none** |
| 6 | `Complete` | read returns `-1` or `-2` | `Complete` | none |
| 7 | `Pending` / `Complete` | single discard | `Consumed` | none |
| 8 | `Running` | single discard | `Abandoned` | none; sets the cancel flag |
| 9 | `Abandoned` | `Perform` returns | `Free` | worker frees; the result is dropped |
| 10 | `Pending` / `Complete` / `Consumed` | discard-all | `Free` | **frees immediately** (caller's thread) |
| 11 | `Running` | discard-all | `Abandoned` | sets the cancel flag; counted in `stillRunning` |
| 12 | any | DLL unload (static destructor) | — | frees everything |

**Definitions.**

- **Lookup by id** matches only `Pending`, `Running` or `Complete`.
- **Occupied** = `Pending`, `Running`, `Complete` or `Abandoned`.
- **Free for dispatch** = `Free` or `Consumed`.

**Invariants.**

- A `Pending` slot always has a worker able to claim it.
- A `Running` or `Abandoned` slot always has its own worker.
- A transport failure still ends in `Complete` and holds its slot until it is read or discarded.

**D8 — Memory and realtime rules.** **Only dispatch and `restifyDiscardAllResponses` allocate or free on the caller's thread.** Poll, await, read and single discard never do. Workers allocate and free only on their own threads.

- **Retention bound:** at most 8 × each request's `maxResponseBytes`.
- **A 0 read does not free memory.** It is reclaimed on slot reuse, on discard-all, or at unload.
- **Unload timing is CANoe's decision and has not been observed.**
- A cancelled request never becomes `Complete`, so **`RequestCancelled` never reaches CAPL.**

**D9 — Worker lifecycle: design 3 (normative).**

**Creation.** A worker is created only from dispatch, under the lock. Before `CreateThread`, take a module reference with `GetModuleHandleExW(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS, …)`, **without PIN**. Close the thread handle immediately and increment `liveWorkers`. On `CreateThread` failure, call `FreeLibrary`, roll the slot back, and return `-29`. **Use raw `CreateThread`, not `std::thread`.**

**Loop.** The worker waits on `workAvailable` with idle timeout **T = 30 s**.

- If a slot is `Pending`, claim it, release the lock, call `Perform`, re-lock, then apply row 4 or row 9.
- On timeout, or on the "exit now" flag, with no `Pending` slot and **still holding the lock**:
  1. decrement `liveWorkers` and `idleWorkers`;
  2. unlock;
  3. call `FreeLibraryAndExitThread`, which is the **only** way a worker exits.

**Invariant:** references held ≥ live workers.

- `DllMain` does nothing thread-related.
- The slot table's static destructor frees memory only: it never waits, joins or signals.
- Nothing runs at load time: no thread, reference or allocation before the first dispatch.

**Lifetime seam.** Acquiring and releasing the module reference, and starting and ending a thread, sit behind a small seam inside `async-operations`. Production uses the Win32 calls; tests use a counting fake.

**The two curl invariants that keep this race-free:**

- `CURLOPT_QUICK_EXIT` is never set.
- The easy handle is created and destroyed inside `Perform`.

**D10 — Id counter.**

- Global, not per slot.
- Its starting value is seeded from the low 32 bits of `QueryPerformanceCounter` on the first dispatch after each load.
- It then increments, skipping 0 and any id currently live in a slot.
- A test hook seeds it.

**D11 — Poll.** Behaviour:

1. Set `state = 0` on entry.
2. If the id matches `Pending` or `Running`: return 0 with `state = 1`.
3. If it matches `Complete`: return 0 with `state = 2`. "Complete" means readable, including failed requests and a pending `-2` retry.
4. Otherwise return `-27`.

Poll never blocks, allocates, or creates or wakes a worker.

**D12 — Read write order (normative).**

1. Zero `requestStatus`, `httpStatusCode` and `responseBodyLength`.
2. Not live: return `-27`. `Pending` / `Running`: return `-26`.
3. If `Complete`: write all three out-parameters **before** attempting the copy.
4. Copy with `CopyToBuffer`, which never truncates:
   - null buffer or zero size: `-1`;
   - too small: `-2`, and the buffer is left empty;
   - fits: `0`, and the slot moves to `Consumed`.

The copy happens under the lock and is bounded by `responseBodySize`. A retry needs a buffer of at least `responseBodyLength + 1`. `requestStatus` can only be 0 or a code in `-18..-23`. HTTP 4xx/5xx gives 0 with the code in `httpStatusCode`. Binary bodies are handled as in Stage 10's D15.

**D13 — Await.**

- Not live: `-27`. Already `Complete`: 0 immediately.
- Otherwise it waits on the `slotChanged` condition variable with a steady-clock deadline, using `wait_until` on **the table mutex**, which releases the lock for the whole wait. It re-checks **by id, not by slot index** on every wake-up.
- On completion: 0. Id invalidated (discarded, discard-all, or slot reused): `-27`, promptly. Deadline reached: `-28`.
- `waitTimeoutMs = 0` means the request's resolved `totalTimeoutMs`, measured from the call.
- A worker storing a result, single discard and discard-all all `notify_all` on `slotChanged`.
- **Await blocks**: Measurement Setup or test node only.

**D14 — Single discard.** Id matches: return 0 and apply row 7 or row 8. Otherwise return `-27`. It is realtime-safe.

**D15 — Discard-all.** Applies rows 10 and 11 to every slot, writes `stillRunning`, wakes idle workers with "exit now", and returns 0. **It frees memory on the caller's thread:** call it from `on stopMeasurement` or a test node.

**D16 — File layout.**

- `src/http/async-operations.{h,cpp}`: the engine, D7–D15.
- `src/http/async-text-api.{h,cpp}`: pointer+size translation (D5, D12). Includes `sync-text-api.h`.
- `src/http/http-client.{h,cpp}`: the cancel hook (D18).
- `src/core/status.h`: D6.
- `src/module/exports.cpp`: 11 thin shims. **The shims must not pre-load a local from a reference parameter** (Stage 10's pattern): the translation layer owns every out-parameter write (D5, D11, D12).

**D17 — `hintText` guidance.** Not contract. **At most two short sentences per row**, with no identifiers. Required content:

- dispatch rows: non-blocking, returns a `requestId` at once;
- poll, read, single discard: non-blocking, safe from a Simulation Setup timer;
- await: the same blocking caveat as the sync rows;
- discard-all: frees memory; call it from `on stopMeasurement` or a test node.

**D18 — Cancel hook.**

- `CURLOPT_NOPROGRESS=0` plus an `XFERINFOFUNCTION` that returns non-zero once the transfer's cancel flag is set.
- `CURLE_ABORTED_BY_CALLBACK` maps to `-24`.
- The check is pulled out as a pure function, so `curl.h` stays in one translation unit.
- **Behaviour-preserving for sync**: sync callers never set the flag.
- It cannot interrupt a blocking `getaddrinfo` or a revocation fetch.
- The "within about a second" claim should be confirmed against `lib/progress.c` in the 8.21.0 source during CPP-28.

**D19 — Branch.** `stage/11-capl-async-exports` from `main` @ `86ab5f4`. No stacking: Stage 10 is merged. Local `main` may be stale, so fetch and confirm the base before cutting.

**D20 — Merge basis (OQ14).**

- Merge on REV-6, HUM-15 and green TEST-6 / TEST-15 on both architectures.
- HUM-27 is carried forward.
- A real-DLL smoke harness was declined (R-E).

---

## 6. Comment homes and budgets

**Normative material goes in `docs/capl-async-surface.md`** (new, topic-scoped): the D4 table, D5, D6, D7, D8, D11–D15, the id-lifetime and redeploy guidance, the single-consumer-pool and cross-measurement notes, and the outbound network + TLS requirement.

**`docs/http-layer.md` gets these additions:**

- the cancel hook;
- the `CURLOPT_QUICK_EXIT` never-set invariant;
- the easy-handle-inside-`Perform` invariant;
- "`totalTimeoutMs` is not a hard ceiling" (DNS cleanup join, and possibly revocation).

**`docs/status-codes.md`:** the range documentation for `-18..-29`.

**Source-comment budgets:**

| File | Budget |
|---|---|
| `async-operations.h`, `async-text-api.h` | ≤ 5 comment lines each, pointing to the doc |
| `async-operations.cpp` | **One tier-3 trap block, ≤ 10 lines**, naming four traps: the lock is never held across `Perform`; nothing thread-related in `DllMain` and none in the static destructor; a worker exits only via `FreeLibraryAndExitThread` while holding its own reference; poll/await/read/single discard never allocate or free |
| `http-client.cpp` | **One tier-3 trap, ≤ 3 lines**, naming the `CURLOPT_QUICK_EXIT` / handle-inside-`Perform` invariant |
| `exports.cpp` | **No new comment block.** The existing `parCount` / `type - 128` trap covers the new rows |
| New source files | ≤ 35% comment lines |

**Comment rules for every file above:** no plan, stage or task identifiers. Where this plan names a trap, it gives no wording for the comment.

---

## 7. Tasks

| ID | Agent | Task | Approval |
|---|---|---|---|
| **HUM-26** | Human | Rule on OQ1–OQ18 and sign off the D4 table. | **SATISFIED** |
| — | `build-pipeline-engineer` | Fetch, confirm the base is `86ab5f4`, and cut `stage/11-capl-async-exports`. | No |
| **CPP-26** | `cpp-implementer` | `src/core/status.h`: append `-24..-29` per D6. Every existing value stays byte-identical; `-7..-9` stay reserved. Close the doc gap in `docs/status-codes.md`. | No |
| **CPP-28** | `cpp-implementer` | `src/http/http-client.{h,cpp}`: cancel hook per D18, the extracted abort check, the `-24` mapping, the trap line, and the `docs/http-layer.md` additions. **Behaviour-preservation for every existing path is re-derived from the diff by REV-6.** | No |
| **CPP-7** | `cpp-implementer` | `src/http/async-operations.{h,cpp}`: D2, D3 and D7–D15, the lifetime seam (D9), the seeded counter (D10), the tier-3 block. | No |
| **CPP-27** | `cpp-implementer` | `src/http/async-text-api.{h,cpp}` per D5/D12, reusing the existing parsers and buffer helpers unchanged. Plus **`docs/capl-async-surface.md`**. | No |
| **TEST-6** | `test-engineer` | Details below. | No |
| **TEST-15** | `test-engineer` | Details below. | No |
| **CPP-8** | `cpp-implementer` | Details below. | **YES — export-contract append** |
| **REV-6** | `code-reviewer` | Full-branch review of `git diff main...HEAD`; checklist below. | — |
| **HUM-15** | Human | Compile-only verification in real CANoe (**the acceptance gate**): a `.can` script referencing all 18 exported names, confirmed to compile. | — |
| **HUM-27** | Human | Runtime verification, blocked by CANoe licensing. Checklist below. | — |

**TEST-6.** Extend `FakeTransport` with **per-request gates**, then write `tests/http/async-operations_test.cpp`. **No sleeps.** Time is used only for deadline assertions, with generous margins. Cases:

1. 8 requests in flight at once; a 9th returns `-25`.
2. Completions released out of order reach the correct ids.
3. Read followed immediately by dispatch reclaims the `Consumed` slot deterministically.
4. A `-2` read leaves the slot `Complete`, and a larger-buffer retry succeeds.
5. A 0 read consumes the slot; reading it again gives `-27`.
6. Read before completion gives `-26`.
7. A transport failure holds its slot until read, and comes back as `requestStatus` in `-18..-23` with `httpStatusCode == 0`.
8. HTTP 4xx/5xx gives `requestStatus == 0`.
9. Single discard of a `Pending`, `Running` and `Complete` slot; the abandoned result is dropped and the slot freed.
10. Discard-all frees memory, reports `stillRunning`, and wakes idle workers to exit.
11. Poll returns state 1, then state 2, then `-27`.
12. Await: 0 on completion, `-28` at the deadline, `-27` promptly on discard and on discard-all, and `-27` after the slot is reused under a new id. `waitTimeoutMs = 0` follows the request's total timeout.
13. The counter's seed is used, and wrap-around skips 0 and live ids.
14. **Worker lifecycle through the counting seam:** idle exit after an injected T, recreation on the next dispatch, the exit decision racing a new dispatch, and references held ≥ live workers at every step.
15. The extracted cancel check from CPP-28.

**TEST-15.** `tests/http/async-text-api_test.cpp`:

- bounds on every input;
- **dispatch validation codes identical to the sync equivalents, case by case**;
- `requestId == 0` after every dispatch failure, with the out-parameter pre-seeded to a non-zero value;
- read's write order, per D12.

Offline only; both architectures.

**CPP-8.** `src/module/exports.cpp`:

- 11 `extern "C" CAPLPASCAL` shims, each a single forwarding call. No reference parameter is pre-loaded into a local (D16).
- Rows 8–18 exactly as in D4, in that order.
- `hintText` per D17.
- **11 `CHANGELOG.md` bullets** and the README operations-summary update.
- Both architectures clean at `/W4`.

**REV-6 checklist.**

- Rows 0–7 byte-identical; rows 8–18 match D4 exactly (names, order, `parCount` = entry counts, `parTypes`, `array`, `parNames`, `categoryName`).
- **Every shim's C++ signature checked against its table encoding, row by row**, including `__stdcall` and the position of every reference parameter. This carries extra weight because the harness was declined.
- Strict-prefix-freedom across all 18 names.
- `dumpbin /exports` shows only `caplDllGetTable4`; `exports.def` unchanged, with no `LIBRARY` line.
- `skipTlsVerification` unreachable from CAPL.
- The D5, D11 and D12 write orders.
- Lock discipline: the lock is never held across `Perform`, and no sleep-with-lock anywhere.
- No allocation or free in poll/await/read/single discard.
- D9 line by line: raw `CreateThread`, a reference taken before each thread, `FreeLibraryAndExitThread` as the only exit, the exit decision made under the lock, nothing in `DllMain`, a static destructor that only frees.
- `CURLOPT_QUICK_EXIT` never set; the easy handle created and destroyed inside `Perform`.
- **CPP-28's behaviour-preservation re-derived from the diff**, not trusted from the implementer's report.
- `CurlTransport` holds no mutable state besides the `call_once` flag.
- N and T identical on both architectures.
- No hardcoded version; `/MT` on both architectures.
- Comment dispositions checked **against the target documents themselves**.

**HUM-27 checklist.**

1. Dispatch from a Simulation Setup `on timer`, then poll, then read.
2. The `-2` retry.
3. 8 requests in flight at once, and the 9th rejected.
4. Single discard, and discard-all with `stillRunning`.
5. Cancel timing on a live transfer.
6. Await from a test node.
7. Worker idle exit.
8. **Redeploy check:** after using async, rebuild, recompile, start a new simulation, and confirm `restifyReadVersion` reports the new build.

The run should also note any observed CANoe/VN/VT unload behaviour. It carries forward alongside HUM-14 (sync runtime verification).

**Task IDs.** `CPP-7`, `CPP-8`, `TEST-6`, `REV-6` and `HUM-15` already exist in §12. `CPP-26`, `CPP-27`, `CPP-28`, `TEST-15`, `HUM-26` and `HUM-27` are the next free numbers as of `86ab5f4`. **Re-check them against `main` at fold-in.** An ID is spent only when it is written into §12.

---

## 8. Execution order

1. `build-pipeline-engineer`: fetch, confirm the base, cut the branch.
2. **CPP-26**: status codes and the doc gap.
3. **CPP-28**: the cancel hook. It goes first because the engine depends on it.
4. **CPP-7**: the engine.
5. **CPP-27**: translation layer and `docs/capl-async-surface.md`.
6. **TEST-6 and TEST-15**: coverage. **Before CPP-8, deliberately.**
7. **CPP-8**: the 11 rows, shims, CHANGELOG, README.
8. **REV-6**: full-branch review.
9. **HUM-15**: compile verification in CANoe. **The acceptance gate.**
10. Fold into the master plan: the **last commit** on the branch, before Ready for review.
11. Human: push, PR, merge (`--no-ff`).
12. **HUM-27**: once licensing is resolved.

**Serialization.** No other open PR may touch `src/module/exports.cpp` while this branch is open. If `main` is merged into the branch and brings new export rows, `main`'s rows stay **before** this branch's rows.

---

## 9. Risk register

**R-A — Lock held across a transfer would block realtime callers.** The rule is that the lock is never held across `Perform`. Read's copy is bounded by the caller's buffer, and await releases the lock through its condition variable. Mitigation: a named trap comment plus REV-6.

**R-B — Code still executing after the DLL unloads (host-process crash).** **Eliminated by design 3**, with the curl 8.21.0 join behaviour verified from source. It depends on two invariants (no `CURLOPT_QUICK_EXIT`; the handle inside `Perform`), checked in REV-6 and stated in a trap comment.

**R-C — Joining threads from `DllMain` deadlocks on the loader lock.** Ruled out: nothing thread-related in `DllMain`, and the static destructor only frees memory.

**R-D — Input lifetime across threads.** Dispatch copies all input into slot-owned storage, which is required because `CURLOPT_POSTFIELDS` is not copied by libcurl.

**R-E — Merged before runtime verification; offline real-DLL verification considered and declined.** The 11-row append is permanent. At merge it is backed by review, a compile check in CANoe, and deterministic unit tests of the state machine and concurrency (gated fakes, both architectures). Four behaviours have **no automated evidence at merge**:

- (i) design 3's real unload path: `FreeLibraryAndExitThread`, and the DLL actually unloading after T;
- (ii) OQ18's cancel aborting a live libcurl transfer within about a second;
- (iii) the shims called through the table's function pointers on x86 (the calling-convention class, R2/R-I);
- (iv) CANoe/VN/VT load and unload behaviour.

A local real-DLL smoke harness covering (i)–(iii) without CANoe was proposed and **explicitly declined by the user**, on the grounds that the stage merges before full CANoe verification in any case, as Stage 10 did. **The residual risk is accepted as stated.** All four behaviours are on HUM-27's checklist. The harness remains available as a later, independent addition if HUM-27 is delayed long enough or one of these behaviours fails in the field.

**R-F — Several nodes sharing one DLL.** Largely resolved by N = 8 and id-keyed lookup. What remains: one pool is shared by all nodes, so a greedy node can starve the others. Documented and accepted.

**R-G — State across measurements and reloads.** While the DLL stays loaded, unread responses survive across measurements; the doc recommends discard-all in `on stopMeasurement`. Across reloads, the clock-seeded counter (D10) makes a stale id colliding with a new one negligibly likely (about 1 in 2³²).

**R-H — Concurrency bugs; bitness-dependent races.** Mitigated by one table lock, all transitions under it, deterministic gated tests, and both CI architectures.

**R-I — The largest append in the project (11 rows).** A `parCount` / encoding mismatch is not caught by any compiler and corrupts the CAPL stack, worst on x86. Mitigated by REV-6's row-by-row check and HUM-15.

**R-J — The status-code budget is exhausted.** `-24..-29` are all spent; the next free codes are `-7..-9` (module/glue) and anything below `-29`, which would be a new range decision.

**R-K — The BPE-12 case-sensitivity trap is now live.** `"Sync"` is a case-insensitive substring of `"Async"`. Stage 14's problem, made real by this stage.

**R-L — Permanent API shapes.** Poll's state enumeration, read's two outcomes, and await's timeout semantics are now permanent.

**R-M — Capacity exhaustion by unread responses.** Mitigated by both discard rows and the documentation.

**R-N — Memory ceiling of 8 × each request's cap.** Worst on 32-bit CANoe. Accepted (OQ16).

**R-O — Cancellation cannot interrupt a blocking DNS lookup or revocation fetch.** A discarded transfer can hold its slot and worker until that call returns.

**R-P — Dispatch and discard-all touch the heap on the caller's thread.** The two documented exceptions.

**R-Q — Unobserved runtime environment.** How the VN/VT runtime loads and unloads the DLL is unknown. Design 3 is safe under either behaviour; only the timing of reclamation depends on it.

**R-R — CPP-28 modifies reviewed Stage 9 code.** Behaviour-preservation must be re-derived from the diff, not assumed.

---

## 10. What Stage 11 deliberately does not do

- No response headers. Still deferred from Stage 10's D8.
- No `skipTlsVerification`, and no change to revocation checking.
- No libcurl multi interface.
- No CAPL callbacks and no VIA use in `src/http/`.
- No pin.
- No `DllMain` logic.
- No measurement-start/stop hooks and no node-layer (`VIAModuleApi`) exports.
- No cancel-but-keep-the-slot row.
- No "poll any" row.
- No caching of sync responses.
- No JSON parsing.
- No `examples/*.can` (CPP-11, Stage 12, behind HUM-16).
- No `INFO5`.
- No real-DLL smoke harness (declined; R-E).
- No change to rows 0–7, `exports.def`, `sync-operations.*` or `sync-text-api.*`.

---

## 11. Master-plan fold-in obligations

The last commit on the branch, before Ready for review.

1. **§8 Stage 11 entry:** rewrite it as a completed summary. Pick the disposition of this document: *left in place as the detailed record*, as with Stages 8–10.
2. **Replace the inherited sentence "one active response at a time by deliberate design".** State where it came from: the previous iteration's notes, never re-justified in this project, and overturned by the user's explicit ruling for N-slot concurrency.
3. **§5 export-table snapshot:** add rows 8–18, citing this plan and `docs/capl-async-surface.md`.
4. **§5 fixed-name list:** amend it from nine to **eleven** async names. Record why the two discard rows were added (unread responses consume capacity once N > 1; manual cleanup requirement).
5. **§5 `Status` space:** `-24..-29` spent; the remaining free space and the fact that the HTTP block is now full.
6. **§5 new standing constraints:**
   - design 3's lifecycle invariants, including references held ≥ live workers;
   - the two curl invariants;
   - the rule that only dispatch and discard-all touch the heap on the caller's thread;
   - nothing heavy at load;
   - the lock-never-across-`Perform` rule.
7. **§5 realtime claim:** confirm dispatch, poll, read and single discard as realtime-safe; await (blocks) and discard-all (frees memory) are not.
8. **Stage 9 obligations discharged:** the OQ11 answer on sync caching, stated as answered.
9. **Correct the stale curl version:** the BPE-15 snippet and any 8.22.0 mention become 8.21.0, and the removed override is noted.
10. **R-E recorded as an explicit risk acceptance.** HUM-14 and **HUM-27 recorded as OPEN, never as passed.**
11. **§12 ledger rows** for every ID actually used, **re-checked against `main` for collisions.** This includes any ID that gated this branch's own merge.
12. **§13:** add the curl-source verification as a positive example of "verify against the pinned source, not memory". Note R-K's trap is now live.
13. **Stage 12 note:** `categoryName` for JSON operations is still Stage 12's decision, and the path-syntax window still closes there.

---

## 12. Approval status

**Approved by the user in full, including hand-off to `plan-writer`.**

1. **All eighteen open questions are closed** by explicit user rulings. OQ5 closed through approval of OQ4's definitions, and OQ13 through the OQ10 table sign-off.
2. **HUM-26 is satisfied.**
3. **The complete plan is approved for hand-off to `plan-writer`**, to be persisted verbatim to `docs/work/stage-11-capl-async-exports/plans/plan.md` under slug `stage-11-capl-async-exports`.

The `build-pipeline-engineer` branch cut is the first action after the plan is persisted. **CPP-8 remains human-gated** as the export-contract append, and its human gate is HUM-15.
