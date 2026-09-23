# Stage 9 — HTTP layer and synchronous operations (logic only)

**Unit of work:** `stage-09-http-sync`
**Branch:** `stage/09-http-sync`, already cut off `main` @ `c77b65d`
**Master plan:** `C:\Workspace\restifycapl\docs\work\capl-rest-dll-rebuild\plans\plan.md` §8, Stage 9 (lines 753–760) — this document finalizes and expands that entry; it does not replace it. It also **supersedes one line of it** (§3, OQ9).
**Structural template:** `C:\Workspace\restifycapl\docs\work\stage-08-core-pure-logic\plans\plan.md`
**Human approval gate: NO** — with three named escalation conditions (§9).
**Status: APPROVED.** All nine open questions answered (§3); R9 resolved (§10). Comment discipline, agent-definition drivers and `.gitkeep` disposition folded in as D16, D17, §5.0 and tasks BPE-30/BPE-31/BPE-32/REV-21/CPP-20. Ready for implementation.

---

## 1. Goal

Populate `src/http/` with a libcurl-backed HTTP client behind an **injectable transport seam**, plus synchronous REST operations built on it — as pure logic with **zero CANoe knowledge, zero VIA/CAPL SDK includes, and zero export-table rows** — and make the already-written libcurl link line actually work for the first time. Coverage runs offline against a fake transport on both architectures.

Stage 9 adds **zero rows** to `CAPL_DLL_INFO_LIST4` and changes **zero** observable CAPL behavior. Exposing any of this to CAPL is Stage 10.

---

## 2. What the new Vector documentation changes

`C:\Workspace\restifycapl\docs\vector-capl-dll-docs\` is authoritative reference material for this stage. Three findings are load-bearing and are cited throughout this plan — they are not background colour, they change the design.

**V1 — Blocking calls are prohibited in the realtime branch.**
`docs\vector-capl-dll-docs\CAPLExportTable.htm.md` ("Note on CAPL DLLs"):

> If functions of this DLL are called in the realtime branch (Simulation Setup) the following must be observed: File accesses and other blocking calls are prohibited! Dynamic memory management is not recommended. This means no `new`, `new[]`, `delete`, `delete[]`, `malloc`, `free`, etc. Instead it is better to reserve memory statically before the measurement start.

And `docs\vector-capl-dll-docs\CAPLIncludeWindowsDLL.htm.md`:

> Whenever functions of a DLL are called in the realtime area (Simulation Setup), they run in a high-priority thread. This can affect measurement.

A synchronous HTTP call is precisely a blocking call, and libcurl allocates freely. **Every synchronous operation this stage produces is therefore unsafe in the Simulation Setup realtime branch, by Vector's own rule** — it is safe only from Measurement Setup / test nodes. This is not a caveat we can discover at Stage 10; it must be written into `sync-operations.h`'s header now, carried into the export-table description text at Stage 10, and into `examples/*.can` at Stage 12. It also converts Stage 11's async layer from "a nice-to-have" into "the only conforming way to call REST from a simulation node".

**V2 — The async design is mandated, not chosen.**
Same file, "CAPL Callback Functions" note:

> It is not permitted to call callback functions on additional threads used in the CAPL DLL. CAPL has no way of securing access to variables shared between threads. […] To fetch data from an additional thread used in the DLL, you must use a timer in CAPL, in whose `on timer` routine it is checked whether data is available and copied if necessary. This operation must be secured in the DLL using the usual means of C/C++ (e.g. a `std::mutex`).

This independently confirms the master plan's Stage 11 design (ready flag + poll from `on timer` + per-module mutex) and **rules out** a VIA-callback-driven async layer. The consequence for Stage 9: the HTTP core must be (a) callable from a worker thread that is not CANoe's, and (b) absolutely free of any VIA/CAPL interaction, so Stage 11 can run it on a background thread without violating this rule. That is a hard design constraint on CPP-4/CPP-5, not merely the existing layering convention restated.

**V3 — Parameter/return type inventory confirms the Stage 10 shape.**
`CAPLExportTable.htm.md`'s type table has no string return type; `C` (char) and `B` (byte) are legal **only for arrays**. So a CAPL-visible operation can only deliver a response body as a `char[]` out-array plus an `L` (int32) status return — exactly the `restifyGetVersion` shape §5 of the master plan already fixed, and exactly what `Status` being `int32_t` already serves. Nothing in Stage 9 needs to change for this; it confirms that `sync-operations` should hand back `std::string` bodies that Stage 10 copies out via the existing `CopyToBuffer` (`src/core/buffer-copy.h`), rather than inventing a second buffer discipline.

Two further notes, recorded so nobody re-derives them: `CAPLIncludeWindowsDLLSearchSequence.htm.md` describes a PATH/Exec32/Exec64 search order that matters for *dependent* DLLs — our `/MT` + fully-static-libcurl build has no dependent DLLs beyond system ones, so this is a non-issue for us and is a positive argument for keeping the static link. And `CAPLIncludeWindowsDLLExample.htm.md` shows Vector's own sample using `CAPL_DLL_INFO3` (10-parameter limit); we are on `CAPL_DLL_INFO4` (64 parameters), which is the right table for Stage 10's multi-parameter REST calls.

---

## 3. Open questions — ALL RESOLVED

**OQ1 — Does `HttpResponse` capture response headers? — RESOLVED: YES, as recommended.**
`HttpResponse` collects response headers internally via curl's header callback. Not exposed to CAPL in Stage 9 or Stage 10. The project's anti-pre-emption rule is about the **export contract** (expensive to change) and deferred **modules** — an internal struct field is neither. Retrofitting header capture after the async layer has frozen its response-state shape (Stage 11) is the expensive ordering.

**OQ2 — Is an HTTP 4xx/5xx an error `Status`? — RESOLVED: NO, as recommended.**
`Status::Ok` plus a `statusCode` out-param. The transport succeeded; the HTTP status is data. See D5. This is the most consequential decision in the stage, because Stage 10 exports it and it can never be changed afterward. Explicit alternative rejected: minting `HttpError` for `>= 400`.

**OQ3 — Is there a TLS-verification-off knob? — RESOLVED: YES. ⚠ DEVIATION from the recommendation.**
The recommendation was to defer it; the user explicitly wants the capability available now rather than discovering the need later. Resolution: a `skipTlsVerification` **bool field on `RequestOptions`, defaulting to `false`** — verification ON by default. The safe default was insisted upon after the security concern of a skip-by-default was raised. See **D15** and §5.3.

Rationale recorded so it is not re-litigated: the user's local FastAPI/uvicorn proxy currently runs plain HTTP (uvicorn defaults to no TLS) and external servers have valid certificates, so the switch is **not needed today**. It exists for future scenarios — e.g. testing against a self-signed HTTPS dev server — without being a footgun.

**OQ4 — Response body size cap — RESOLVED as recommended.**
Per-request override via `maxResponseBytes`, defaulting to a single named constant of **8 MiB**, identical on both architectures. The cap exists because the DLL lives inside CANoe's process: on x86 the host has ~2 GB of user address space and an unbounded response can kill the measurement. The constant must be literally the same number on x86 and x64 or `ResponseTooLarge` fires at different inputs per architecture — the same parity-break class as Stage 8's `NumericOverflow`.

**OQ5 — Does `sync-operations` parse JSON? — RESOLVED: NO, as recommended.**
It returns the raw body as `std::string`. `src/http/` is levels 1+3; JSON is `src/mapping/`, Stages 12–13. `ParseError = -10` stays unclaimed by Stage 9, and its comment in `src/core/status.h` is tightened to name Stage 12 only — **resolving the master plan's open speculation** at `status.h` line 29 and §5.

**OQ6 — Response store: Stage 9 or Stage 11? — RESOLVED: Stage 11, as recommended, and strengthened.**
Stage 9 builds **no response store at all** — not for sync, and not preparatory for async. Operations fill a caller-provided `HttpResponse` value; nothing is retained between calls. The full response-store design is **entirely Stage 11's problem to solve from scratch**, including the sub-question of whether synchronous responses will also need caching there once Stage 12/13 JSON accessors exist. Stage 9 must not shape or architecturally hint at that design. Recorded as a confirmed deliberate deferral in §11.

**OQ7a — `InvalidUrl` distinct, or reuse `InvalidArgument`? — RESOLVED: distinct, as recommended.**
`InvalidUrl = -22`. `-1` already means "null buffer / zero size" to shipped CAPL scripts; merging would make CAPL-side diagnosis ambiguous forever.

**OQ7b — Catch-all name — RESOLVED: `NetworkError`, as recommended.**
CAPL-friendly name; its catch-all role is stated in its comment. Alternative `TransportError` rejected.

**OQ8 — Where do curl headers live, and are they tracked? — RESOLVED as recommended, with the precedent now verified rather than assumed.**
`include/vendor/curl/`, provisioned by **both** `scripts/setup-dev-env.ps1` and CI. On the tracked-vs-ignored question: `.gitignore` line 55 already reads `include/vendor/gtest/`, so the established precedent for **vcpkg-provisioned headers is ignored, not committed**. `include/vendor/curl/` therefore gets the same treatment — see BPE-10, which also has to correct the now-incomplete comment sitting above it.

This placement means `/I include/vendor` — already present in **both** `INCLUDES` (Makefile line 83) and `TEST_INCLUDES` (line 177) — covers `#include <curl/curl.h>` with **no new include flag at all**, keeping the two lists identical in shape.

**OQ9 — TEST-5's live verification — RESOLVED: deferred entirely to a future stage. ⚠ DEVIATION from the recommendation AND from the master plan's literal text.**
Both presented options were rejected — the `DISABLED_`-test HUM task and the standalone console program — on the principle that **integration/E2E tests must never be human-only; they must be automated, both locally and in CI.**

Direction: real integration testing moves to a **separate, not-yet-planned future stage**, built on containerization (Docker) — a self-hosted local test HTTP/HTTPS server spun up in a container and exercised by an automated integration suite running in both local dev and CI, with no dependency on httpbin.org or any external network. That is a substantial new decision (Docker becomes a new toolchain dependency; nothing containerized exists in the repo today) and needs its own dedicated planning pass. It is **not** resolved here.

**For Stage 9 specifically:** TEST-5 stays exactly as originally scoped — offline-only, against `FakeTransport`, zero network, zero live verification of any kind. **No HUM-24 is minted. No standalone console program task is created.** This **supersedes** the master plan's Stage 9 line *"Verify as a standalone console program against httpbin.org"*; that literal instruction is not followed, and the fold-in commit deletes it. See §11(b).

---

## 4. Scope

**In scope**

| Area | Files |
|---|---|
| Status codes | `src/core/status.h` (modified — append only) |
| HTTP client + seam | `src/http/http-client.h`, `src/http/http-client.cpp` (new) |
| Sync operations | `src/http/sync-operations.h`, `src/http/sync-operations.cpp` (new) |
| HTTP-layer rationale | `docs/http-layer.md` (new, topic-scoped) |
| Build | `Makefile` (one change: the `CURL_STATICLIB` define), `scripts/setup-dev-env.ps1`, `.github/workflows/ci.yml`, `.github/workflows/auto-pr.yml`, `.gitignore` |
| Tests | `tests/http/fake-transport.h`, `tests/http/http-client_test.cpp`, `tests/http/sync-operations_test.cpp` (new) |
| Comment-discipline rule | `.claude/skills/project-docs/SKILL.md`; `.claude/agents/{cpp-implementer,build-pipeline-engineer,test-engineer,code-reviewer,planner}.md` |
| Directory placeholders | `.gitkeep` files under `src/`, `tests/`, `examples/`, `lib/gtest/`; `.claude/skills/msvc-build-conventions/SKILL.md` |
| Comment fixes | `src/core/*`, `src/module/exports.cpp`, `tests/core/*` (comments only, zero behavioural change) |
| Changelog | `CHANGELOG.md` `[Unreleased]` (see §4a) |
| Vector docs | `docs/vector-capl-dll-docs/` — committed on this branch (R9, §8 step 6) |
| Plan fold-in | `docs/work/capl-rest-dll-rebuild/plans/plan.md` (§8 Stage 9 + §12 ledger) |

**Explicitly not in scope**

- Any change to `CAPL_DLL_INFO_LIST4` or `exports.def`. The table diff must be **empty**. Nothing in `src/http/` is exported this stage. `exports.cpp` is touched for comments only.
- The async layer, the response store, the ready flag, request IDs — Stage 11 (OQ6).
- JSON parsing of response bodies — Stages 12–13 (OQ5).
- Request-body *building* helpers for CAPL — deferred Stage 17, per `CLAUDE.md` Scope. Stage 9 takes a `std::string` body and sends it.
- Connection pooling / curl share handles / HTTP-2 / multi interface — see §5.5 and R7.
- **Any live-network or integration test, in any form** — deferred to a future unscheduled stage (OQ9, §11(b)).
- **Everything under `docs/` for comment-discipline purposes**, and everything about `scripts/setup-dev-env.ps1` **other than** its comments and message text — its provisioning logic, vcpkg pinning, allow-list filtering and `/MT` checks stay deferred to BPE-13 at Stage 15. The carve-out pulled forward into this stage is deliberately narrow.
- Any behavioural change whatsoever in CPP-20 or BPE-32. Both are text-only.
- Pushing, opening the PR, or merging. **Human-only, and no agent performs any sub-step of it, including "optional" ones.**

### 4a. CHANGELOG — a deliberate departure from Stage 8

Master plan §5: *"Every export-table append gets a `CHANGELOG.md` `[Unreleased]` entry in the same change"* and *"build and CI changes also get an entry"* (BPE-18 exists because the second half slipped). Stage 8 wrote no entry, on the grounds that its Makefile change was an inert include-path addition.

**Stage 9 is different and gets an entry**, for BPE-10: it adds a compile-time define, changes provisioning, and causes libcurl's code to actually enter the shipped DLL for the first time. That is user-visible in a way `/I src` was not. Flagging the divergence from the Stage 8 precedent explicitly rather than letting it look like an inconsistency.

---

## 5. Normative artifacts

These blocks are the specification, and `code-reviewer` checks against them. **They are not all destined for source comments.** An earlier draft of this plan said each block should be written "verbatim into" a header, and that instruction is withdrawn: applied literally it would have produced two new files in the 70%-comment range and put one rationale in two files, which the project's comment rule bans outright. §5.0 assigns each block a single home. §5.1–§5.5 below remain the authoritative text of each block; §5.0 decides where it lives.

### 5.0 Disposition of the normative blocks

**The measured baseline this exists to prevent.** Comment lines against total lines, across `src/` as it stands:

| File | Comment lines | Total | Ratio |
|---|---|---|---|
| `src/core/type-conversion.h` | 63 | 85 | **74%** |
| `src/core/json-path.h` | 57 | 78 | **73%** |
| `src/core/buffer-copy.h` | 13 | 21 | **62%** |
| `src/core/status.h` | 19 | 37 | **51%** |
| `src/module/exports.cpp` | 92 | 194 | **47%** |
| `src/core/json-path.cpp` | 19 | 139 | 14% |
| `src/core/type-conversion.cpp` | 13 | 183 | 7% |
| `src/core/buffer-copy.cpp` | 2 | 20 | 10% |

The `.cpp` files are healthy; the regression is **entirely in headers**, and the three worst are exactly the files a previous stage plan told an implementer to fill with normative tables verbatim. `exports.cpp` at 47% is a large improvement on the ~150-comment-lines-to-100-code-lines state recorded when the comment rule was first installed, so this is a new instance with a different cause, not a relapse of the fixed problem. The cause is the planning instruction, which is why the fix is a disposition table rather than a cleanup task.

**A new topic-scoped document, `docs/http-layer.md`**, is the home for evicted HTTP-layer rationale. Topic-scoped, never stage-scoped; each section takes a one-line breadcrumb. `cpp-implementer` authors it as a deliverable of CPP-4, so it lands in the same change as the code it explains.

| Block | Disposition | Form in source |
|---|---|---|
| §5.1 `Status` additions | stays in `status.h` | one-line-per-value comments only. `status.h`'s existing 18-line file header is **not** extended — a file-header block summarising a file is a banned category. |
| §5.2 `CURLcode`→`Status` table | → `docs/http-layer.md` | **none.** The `switch` *is* the table; a comment copy restates the code, and two copies drift apart. |
| §5.2 `ResponseTooLarge` beats `CURLE_WRITE_ERROR` | **inline** | one tier-3 block, ≤10 lines, at the mapping function. Names a specific bug a future editor would otherwise reintroduce. |
| §5.3 option policy (9 rows + the TLS table) | → `docs/http-layer.md` | three tier-2 one-liners, each at its own `curl_easy_setopt` call: `NOSIGNAL` (required for use from a non-main thread), `ACCEPT_ENCODING` (this is why zlib is a dependency at all), `MAXREDIRS` (bounded on purpose). Nothing else. |
| §5.3 TLS pair | **inline** | one tier-3 block, ≤10 lines: set both `VERIFYPEER` and `VERIFYHOST` or neither — setting one without the other is the classic half-disabled-TLS bug. |
| §5.4 timeout policy | → `docs/http-layer.md` | one tier-3 block, ≤10 lines, at the field: `0` means default, there is **intentionally** no way to express "no timeout", and an unbounded blocking call stalls the measurement with no CAPL-side recovery. |
| §5.5 items 1–2 (`curl_global_init` placement; never `curl_global_cleanup`) | **inline** | one tier-3 block, ≤10 lines, covering both. Loader-lock deadlock is a named bug. |
| §5.5 items 3–4 (handle per request, no reuse) | → `docs/http-layer.md` | one tier-2 line at handle creation: a fresh handle per request, required for thread-safe concurrent use. |
| D15's four rationale bullets | → `docs/http-layer.md` | **one** tier-3 block, ≤10 lines, at the `skipTlsVerification` field in `http-client.h` **only**. Putting this rationale in both `.h` and `.cpp` would be the banned "same rationale in two files". |
| D8 realtime-branch prohibition | → `docs/http-layer.md`, with the Vector quotation and citation | one ≤10-line note in `sync-operations.h` pointing to `docs/http-layer.md`. A `docs/` path is not a task identifier; this pointer form remains permitted. |

**Ceiling.** Comment lines must not exceed **35%** of total lines in either new `src/http/` file. This is a tripwire, not a target — the existing `.cpp` files sit at 7–14%, so it is not a tight budget. If meeting it would require deleting a genuine one-line WHY, stop and raise it: removing a real explanation to hit a number is a worse outcome than the bloat.

### 5.1 `src/core/status.h` — appended block

Appended **after** `NotIntegral = -17`. Existing values are untouched; this is an append, never a renumber (master plan §5).

```cpp
  // -18..-24 -- HTTP/transport layer (src/http/, Stage 9).
  NetworkError    = -18,  // transfer failed: DNS, connect, send/recv, redirect
                          // limit. Also the CATCH-ALL for any CURLcode not
                          // explicitly mapped in http-client.cpp's table.
  Timeout         = -19,  // connect or total timeout expired. libcurl reports
                          // both as CURLE_OPERATION_TIMEDOUT and cannot
                          // distinguish them; one code covers both deliberately.
  TlsError        = -20,  // TLS/Schannel handshake or certificate verification
                          // failure. Distinct from NetworkError so a CAPL
                          // script can tell "server unreachable" from
                          // "server reachable, certificate rejected".
  TransportInitFailed = -21,  // curl_global_init or curl_easy_init failed.
                              // Process-level, not request-level.
  InvalidUrl      = -22,  // malformed or unsupported-scheme URL. Kept distinct
                          // from InvalidArgument (-1), which already means
                          // "null buffer / zero size" to shipped CAPL scripts.
  ResponseTooLarge = -23, // response body exceeded the configured cap. See
                          // http-client.h -- the cap is identical on x86 and
                          // x64 by requirement, not by coincidence.
  // -24 reserved, currently empty, for the HTTP layer.
  // -25..-29 reserved for the ASYNC layer (Stage 11): likely RequestPending,
  //          NoActiveRequest, RequestIdMismatch. Do not mint from this range
  //          in Stage 9.
```

The existing comment on `ParseError = -10` ("likely Stage 9 (sync-operations) or Stage 12 (json-flatten)") is tightened to name **Stage 12 only** (OQ5).

### 5.2 `CURLcode` → `Status` mapping (verbatim into `http-client.cpp`)

| `CURLcode` | `Status` |
|---|---|
| `CURLE_OK` | `Ok` |
| `CURLE_OPERATION_TIMEDOUT` | `Timeout` |
| `CURLE_URL_MALFORMAT`, `CURLE_UNSUPPORTED_PROTOCOL` | `InvalidUrl` |
| `CURLE_SSL_CONNECT_ERROR`, `CURLE_PEER_FAILED_VERIFICATION`, `CURLE_SSL_CIPHER`, `CURLE_SSL_CACERT_BADFILE`, `CURLE_SSL_ISSUER_ERROR`, `CURLE_USE_SSL_FAILED`, any other `CURLE_SSL_*` | `TlsError` |
| `CURLE_COULDNT_RESOLVE_HOST`, `CURLE_COULDNT_RESOLVE_PROXY`, `CURLE_COULDNT_CONNECT`, `CURLE_SEND_ERROR`, `CURLE_RECV_ERROR`, `CURLE_GOT_NOTHING`, `CURLE_PARTIAL_FILE`, `CURLE_TOO_MANY_REDIRECTS` | `NetworkError` |
| **any other `CURLcode`** | `NetworkError` (documented catch-all) |

**Named editing trap, and it must be written as one in the source:** the size-cap check lives in the write callback, which signals a cap breach by returning a short byte count. libcurl converts that into the generic `CURLE_WRITE_ERROR`. **The cap flag in the callback's context object therefore takes precedence over this table** — if it is set, the result is `ResponseTooLarge`, regardless of the `CURLcode`. Mapping `CURLE_WRITE_ERROR` through the catch-all would report a body-too-large as a network failure, which is wrong and is exactly the sort of thing that survives review.

### 5.3 Fixed libcurl option policy (verbatim into `http-client.cpp`)

| Option | Value | Why it is not negotiable |
|---|---|---|
| `CURLOPT_NOSIGNAL` | `1L` | Required for use from a non-main thread. Stage 11 will do exactly that (V2). Set now so the seam is thread-safe from birth, not retrofitted. |
| `CURLOPT_SSL_VERIFYPEER` | **conditional — see below** | D15 / OQ3. |
| `CURLOPT_SSL_VERIFYHOST` | **conditional — see below** | D15 / OQ3. |
| `CURLOPT_CONNECTTIMEOUT_MS` | from options | §5.4. |
| `CURLOPT_TIMEOUT_MS` | from options | §5.4. |
| `CURLOPT_FOLLOWLOCATION` | `1L` | |
| `CURLOPT_MAXREDIRS` | `5L` | Bounded. Exceeding → `NetworkError`. |
| `CURLOPT_ACCEPT_ENCODING` | `""` | Enables gzip/deflate transparently. **This is the reason `zs.lib` (zlib) is in the dependency set at all** — without it zlib is linked and unused. Record that in the comment. |
| `CURLOPT_USERAGENT` | `"restifycapl"` | **No version number.** Master plan §5: no version is ever typed by hand. The DLL's real version is only reachable from `src/module/` (Win32 version resource); embedding it here would either hardcode a literal or drag a level-4 dependency into level 1. Callers may override via a `User-Agent` request header. |

**TLS verification is the one per-request-conditional option (D15).** It is not hardcoded:

| `request.options.skipTlsVerification` | `CURLOPT_SSL_VERIFYPEER` | `CURLOPT_SSL_VERIFYHOST` |
|---|---|---|
| `false` — **the default** | `1L` | `2L` |
| `true` — opt-in, per request | `0L` | `0L` |

Both options are set **explicitly on every request, in both branches**. Never leave either at libcurl's implicit default and never set only one of the pair — a handle is freshly created per request (§5.5), so an unset option is a silent dependency on library defaults, and setting `VERIFYPEER` without `VERIFYHOST` is the classic half-disabled-TLS bug.

Proxy behaviour: libcurl's default honours `http_proxy`/`https_proxy`/`no_proxy` environment variables. **Leave the default, and document it** — a CANoe machine behind a corporate proxy will otherwise produce mystifying `NetworkError`s, and silently disabling proxy support would produce the mirror-image mystery.

### 5.4 Timeout policy (verbatim into `http-client.h`)

- Two independent timeouts, both `std::uint32_t` milliseconds: **connect** (default **5 000 ms**) and **total** (default **30 000 ms**).
- **`0` means "use the default". There is no way to express "no timeout".** A blocking call with no upper bound inside CANoe's process — on a *high-priority* thread in the realtime branch (V1) — can stall a measurement indefinitely with no way for the CAPL script to recover. The absence of an infinite option is a deliberate safety property of the API, and must be stated as such in the header so a future contributor does not "fix" it.
- A total timeout below the connect timeout is not an error; libcurl enforces both independently and the total wins. Document rather than validate.

### 5.5 Handle lifecycle and threading (verbatim into `http-client.cpp`)

1. `curl_global_init(CURL_GLOBAL_DEFAULT)` is called **exactly once, lazily, via `std::call_once`, on first transport construction**. It is **never** called from `DllMain` — doing so runs it under the Windows loader lock, which is unsafe and is the standard way to deadlock a host process at load time.
2. **`curl_global_cleanup()` is never called.** This is a deliberate, documented leak. Calling it at `DLL_PROCESS_DETACH` means running it under the loader lock with other threads possibly still in flight; CANoe may also load and unload the DLL across measurements. The leak is one-time and process-scoped. Write this as a named trap so it is not "fixed" later.
3. If `curl_global_init` fails, every subsequent `Perform` returns `TransportInitFailed`. It is not retried.
4. **One `CURL*` easy handle per request**, created and destroyed inside `Perform`. No sharing, no reuse, no `curl_share` handle. The only mutable process-wide state is the `call_once` flag. This makes `Perform` safe to call concurrently from any thread — the property Stage 11 depends on (V2) — at the cost of a fresh connection per request. Connection reuse is deliberately deferred; see R7.

---

## 6. Design decisions (D1–D17)

**D1 — The seam is a pure-virtual interface with one method, injected by constructor.**

```cpp
class HttpTransport {
 public:
  virtual ~HttpTransport();
  virtual Status Perform(const HttpRequest& request, HttpResponse& response) = 0;
};
```

Production implementation `CurlTransport` and the default-transport accessor live in `http-client.cpp`. `HttpClient` takes a `HttpTransport&` (or a pointer, implementer's choice, uniformly) and defaults to the process-wide `CurlTransport`.

Rationale, all three alternatives stated so this is not relitigated: a **template policy** would force `HttpClient` to be header-only and drag `curl.h` into a header, breaking D2 and dependency direction; **`std::function`** works but gives the seam no place to own the `call_once` init and the option policy; **link-time substitution is impossible here** — the master plan already establishes (§8, Stage 9) that `TEST_LIBS` is `gtest.lib gtest_main.lib $(LIBS)`, so the test executable links the **real** libcurl and a fake `libcurl.lib` cannot be swapped in. Virtual dispatch costs nanoseconds against a network round-trip.

**D2 — `curl/curl.h` appears in exactly one file: `src/http/http-client.cpp`.** Never in any header under `src/`, never in `tests/`. Consequence: `sync-operations.*` and every test compile without curl headers on the include path, and `HttpRequest`/`HttpResponse` stay plain C++ value types.

**D3 — Filing stays exactly as the master plan names it.** `src/http/http-client.{h,cpp}` and `src/http/sync-operations.{h,cpp}`. No `curl-transport.cpp`, no extra module invented. `CurlTransport` is a file-local implementation detail of `http-client.cpp` (anonymous namespace where possible), reachable only through the default-transport accessor. The fake is test-only: `tests/http/fake-transport.h`.

**D4 — Value types.**

```cpp
enum class HttpMethod { Get, Post, Put, Patch, Delete, Head };
struct HttpHeader   { std::string name; std::string value; };
struct RequestOptions {
  std::uint32_t connectTimeoutMs;   // 0 = default (5000)
  std::uint32_t totalTimeoutMs;     // 0 = default (30000)
  std::uint32_t maxResponseBytes;   // 0 = default (8 MiB)
  bool          skipTlsVerification;  // default false -- see D15. NEVER
                                      // default this to true, and never
                                      // derive it from anything.
};
struct HttpRequest  { HttpMethod method; std::string url;
                      std::vector<HttpHeader> headers; std::string body;
                      RequestOptions options; };
struct HttpResponse { std::int32_t statusCode;          // 0 if no response
                      std::string body;
                      std::vector<HttpHeader> headers;  // OQ1
                    };
```

`statusCode` is `int32_t`: libcurl's `CURLINFO_RESPONSE_CODE` yields a `long`, which is 32-bit on both Windows targets — assert it the way `exports.cpp` already asserts `sizeof(unsigned long)`, rather than assuming it.

**D5 — HTTP `>= 400` is `Status::Ok` plus `statusCode`** (OQ2). The transport-level `Status` answers "did we get a response"; the HTTP status answers "what did the server say". Two different questions, two different channels.

**D6 — `Status` extension per §5.1** (OQ7). Append-only, block-reserved, with `-25..-29` explicitly fenced off for Stage 11 so the async layer does not have to negotiate for numbers later.

**D7 — Nothing under `src/http/` includes a CAPL SDK header, touches VIA, or knows CANoe exists.** Already the project rule; restated because V2 makes it a correctness requirement rather than a tidiness one — Stage 11 will run this code on a background thread, where any VIA interaction is *forbidden by Vector*, not merely ugly.

**D8 — The realtime-branch prohibition (V1) is documented in `sync-operations.h`'s header**, citing `docs/vector-capl-dll-docs/CAPLExportTable.htm.md`, and is carried forward as a requirement on Stage 10's export-table description text and Stage 12's `.can` examples. Recording it here in the plan is not sufficient — it must live where the next person reads it.

**D9 — `sync-operations` surface.** One generic executor plus thin verb helpers:

```cpp
Status Request(HttpClient& client, const HttpRequest& request, HttpResponse& response);
Status Get   (HttpClient& client, const std::string& url, const std::vector<HttpHeader>&,
              const RequestOptions&, HttpResponse& response);
Status Post  (HttpClient& client, const std::string& url, const std::vector<HttpHeader>&,
              const std::string& body, const RequestOptions&, HttpResponse& response);
Status Put   (...);  Status Patch (...);  Status Delete(...);
```

The implementer may adjust parameter ordering or fold the options into a builder, provided it is uniform across all of them and the `Status`/out-param direction is unchanged. **No `restify*` names in this layer** — the CAPL-visible naming convention (`restify<VerbNoun>`, master plan §5, fixed at Stage 5) belongs to `src/module/exports.cpp` and Stage 10.

**D10 — A non-empty body on `Get`, `Head` or `Delete` returns `InvalidArgument`.** Legal in HTTP, widely mishandled by intermediaries, and ambiguous at the CAPL boundary. Deliberate strictness; documented as such so it reads as a decision rather than an oversight.

**D11 — URL validation is libcurl's, not ours.** No hand-rolled URL parser. An empty URL is `InvalidArgument` (a caller bug); anything else that libcurl rejects becomes `InvalidUrl` per §5.2. Writing a second URL validator is how the two disagree.

**D12 — `InvalidArgument` is reserved for caller-side programming errors** (empty URL, body on GET, out-of-range enum). Everything network-facing gets a code from the `-18..-23` block. Keeping that line clean is what makes CAPL-side diagnosis possible.

**D13 — Nothing is `noexcept`, nothing throws at the boundary, no `at()`, no unguarded `get<T>()`.** Carried forward verbatim from Stage 8 §3.4/§3.5. Marking these `noexcept` converts a `std::bad_alloc` — entirely plausible when accumulating a multi-megabyte response body — into `std::terminate` **inside CANoe's host process**. `src/http/` allocates far more than `src/core/` did, so this constraint is sharper here than it was in Stage 8, not softer. Every `std::string`/`std::vector` growth point must be inside a `try`/`catch(...)` that converts to a `Status` before returning.

**D14 — Branch and slug.** `stage/09-http-sync`, already cut off `main` @ `c77b65d`. Slug `stage-09-http-sync`. Pushing stays manual and human-only.

**D15 — TLS verification is per-request-switchable, safe by default** (OQ3 — deviation from the original recommendation, which was to defer it).

`RequestOptions::skipTlsVerification`, a `bool` defaulting to `false`. When `false` (the default), `CurlTransport::Perform` sets `CURLOPT_SSL_VERIFYPEER=1L` and `CURLOPT_SSL_VERIFYHOST=2L` — exactly the behaviour the original fixed policy specified. When `true`, it sets both to `0L`. Both are set explicitly in both branches (§5.3).

Rationale, to be recorded in `http-client.h` at the field and in `http-client.cpp` at the wiring:

- **Why it exists at all:** the capability is wanted for future scenarios — notably testing against a self-signed HTTPS dev server — and adding it later would mean touching a shape the async layer has already built on.
- **Why it is not needed today, stated plainly so nobody assumes otherwise:** the local FastAPI/uvicorn proxy currently serves plain HTTP (uvicorn defaults to no TLS), and external servers have valid certificates. There is no current caller that sets it to `true`.
- **Why the default is `false`:** a skip-by-default would make every request in the project silently unauthenticated against its peer, and the failure is invisible — it looks like everything works. Safe-by-default was an explicit requirement, not a preference.
- **Bounds on the mechanism:** it is a per-request option. There is no global switch, no environment variable, no build-time define, and no code path that derives it from anything. `sync-operations` forwards it unmodified and must contain no logic that could set it (D9, and CPP-5's acceptance criteria).

**D16 — Comments in this stage carry no plan, stage, or task identifiers, and normative prose lives in `docs/`, not in headers.**

Two halves, both binding on every task below.

*Half one — the identifier ban.* No inline comment in this stage's output names a plan section, stage number, or task ID (`plan.md §7.5`, `Stage 5`, `CPP-16`, `REV-3`, `D15`, `OQ3`), **nor a bare criterion or section number that only resolves against a plan document**. A comment must stand on its own and explain the current *why* without depending on an external tracker. Material worth recording at length goes to `docs/<topic>.md`, and the comment points there.

This **reverses** a rule the project previously held: `project-docs` currently instructs writing `see plan.md §7.5` rather than restating a section, and every existing instance in the tree was written in compliance with it. The rule is changed first, in BPE-30, precisely so the audit that follows does not keep re-manufacturing what it removes.

*Three categories are exempt from mechanical flagging, not from the rule:* program output read by a user at runtime (error messages, log lines, `echo`/`Write-Host` text); PowerShell comment-based help (`<# .SYNOPSIS … #>`), which `Get-Help` consumes; and generated artefacts such as auto-filled PR bodies. A grep cannot distinguish these from developer-facing comments, and auto-editing them breaks working features. **Each hit goes to human judgment under the same principle: the citation never survives; what varies is only whether there is substance worth inlining in its place.** The settled instances are listed in BPE-32.

*Half two — disposition over duplication.* A plan does not instruct that prose be reproduced verbatim in source. Where a plan specifies a source comment it gives the tier and a line budget; normative tables stay in the plan or move to `docs/<topic>.md`, and only named traps go inline. §5.0 is the worked application for this stage.

*Why this is a design decision and not a cleanup chore.* The 74%/73% headers in §5.0's table were produced by a compliant implementer following a plan instruction exactly. The defect was in the instruction, so the fix has to reach the instruction — which is BPE-30 Parts C and D, amending the agent definitions that carry it forward.

**D17 — A `.gitkeep` is removed in the same change that adds the first real tracked file to its directory.**

Never swept separately, never left behind "to clean up later," and never removed from a directory that is still empty. Standing practice, not a one-off: the rule's substance lives in `msvc-build-conventions`'s **Directory conventions** section — that skill already owns directory semantics (`lib/x86/`, `lib/gtest/`, `build/`, `include/vendor/`) and already loads on build-related edits. `build-pipeline-engineer` carries a one-line pointer, matching where the `lib/`-directory invariant already sits. No substance is duplicated.

---

## 7. Tasks

Task IDs are next-free against the master plan's §12 ledger as of `c77b65d`, accounting for the IDs this plan itself spends (highest previously allocated: CPP-18, BPE-29, TEST-12, REV-19, HUM-23). **Per §7.9 condition 2, an ID is spent when it reaches §12 — every new ID below must be written into the ledger in the fold-in commit.**

New IDs: **CPP-19, CPP-20, BPE-30, BPE-31, BPE-32, REV-20, REV-21.** **No HUM task is minted this stage** — next-free HUM remains **24, unused** (OQ9).

**Tool-boundary constraint governing the comment-discipline tasks.** `code-reviewer`'s tools are `Read`, `Glob`, `Grep`, `Bash` — it has **no `Edit` or `Write`**. It audits; it cannot fix. That is why REV-21 is an audit that routes findings to three separate fix tasks (CPP-20, BPE-32, BPE-30) rather than a single review-and-repair task.

---

### BPE-30 — Correct the comment rule and the agent-definition drivers
**Agent:** `build-pipeline-engineer`
**Files:** `.claude/skills/project-docs/SKILL.md`, `.claude/agents/cpp-implementer.md`, `.claude/agents/build-pipeline-engineer.md`, `.claude/agents/test-engineer.md`, `.claude/agents/code-reviewer.md`, `.claude/agents/planner.md`

**This task is not movable.** BPE-30 stays inside Stage 9 unconditionally. If R12 fires and any part of this work is split into a later stage, **BPE-30 does not go with it** — it is the precondition for every other task here being correct, and deferring it means CPP-4 and CPP-5 write new source under a rule the project has already decided is wrong.

**Ownership, stated rather than assumed.** `.claude/**` falls outside every agent's declared file charter. Precedent resolves it: `build-pipeline-engineer` installed the original comment-discipline pointer bullets into these same four agent files. Same owner, same files, same rule. A direct main-session edit is equally acceptable; nothing else in this plan changes if that is preferred.

**Part A — the rule contradiction.** `project-docs` currently carries, in its **Never write** list, a bullet reading *"restatement of a `plan.md` section — point to it instead (`see plan.md §7.5`), never repeat it"*. That bullet instructs exactly what D16 now bans. Replace it with:

> - **In an inline comment:** any reference to a plan section, stage number, or task ID (`plan.md §7.5`, `Stage 5`, `CPP-16`, `REV-3`, `D15`, `OQ3`), including bare criterion or section *numbers* that only resolve against a plan document. A comment must stand on its own and explain the current *why* without depending on an external tracker. Material worth recording at length goes in `docs/<topic>.md` — point **there**.
>
> **Three categories are exempt from mechanical flagging, not from the rule:** program output read by a user at runtime (error messages, log lines, `echo`/`Write-Host` text); PowerShell comment-based help (`<# .SYNOPSIS … #>`), which `Get-Help` consumes; and generated artefacts such as auto-filled PR bodies. A sweep must not auto-file these as Must-fix — it cannot distinguish them from developer-facing comments, and a wrong edit breaks a working feature. **Each hit goes to human judgment, under the same principle: the citation never survives; what varies is only whether there is substance worth inlining in its place.**

Add the three worked patterns from BPE-32's table to the skill as the reference precedent — *delete the whole sentence* (nothing to inline), *inline the substance* (the citation stood in for real content), and *drop the prefix only* (the rest already stands alone) — so the next person applying this has examples rather than a judgment call from scratch. Add the corresponding row to the **Where explanatory material goes instead** table.

**Part B — the protected-comments list.** Update both copies (the skill's list and master plan §6b's) so protection is understood to cover **substance, not identifiers**: `exports.cpp`'s CAPL naming-convention statement stays and loses its `(Stage 5)`. Record one standing exception honestly rather than leaving a silent violation: the `Makefile`'s `make -n` trap runs roughly 19 lines against tier 3's ≤10-line limit and is protected anyway — a named, bounded exception to the tier the same document defines.

**Part C — the "match existing style" driver.** `cpp-implementer.md`'s workflow step 1 instructs reading existing modules and matching their style. The existing modules are the 74%- and 73%-comment headers in §5.0's table, so the instruction propagates the defect forward on its own. Scope it: "style" covers naming, layout and error-handling conventions and **explicitly excludes comment volume and comment style**, with a one-line note that some existing headers are known to be over-commented and are not the reference. Do not delete the instruction — matching error-handling conventions is genuinely wanted.

**Part D — the planner gap.** All four coding agents carry a comment-discipline pointer; `planner` and `plan-writer` carry none, while `planner` is the agent that writes "verbatim into the header" instructions in the first place. **The agent that authors the instruction is the only one never told what a comment may contain.** Add a bullet to `planner.md` stating D16's half two: plan documents do not instruct that prose be reproduced verbatim in source; a plan specifying source comments gives the tier and a line budget; normative tables belong in the plan or in `docs/<topic>.md`, with only named traps inline. For `plan-writer.md`, either add the same or record explicitly why it is exempt — **recommendation: exempt, and say so**, since it copies an approved document verbatim and makes no authorial choice.

**Part E — collapse duplicated substance.** §6b specifies "one pointer bullet and no substance", but all four coding agents carry a roughly four-line bullet restating *"WHY-only, minimal. Design rationale, rejected options and bug narratives go in documentation, never inline."* That is duplicated substance in four files — the shape the rule itself warns about. Reduce each to a genuine one-line pointer. Do not let this expand into a rewrite of those files.

**Acceptance criteria**
- No file under `.claude/` instructs, or shows by example, writing a plan/stage/task identifier into a source **comment**. `code-reviewer.md`'s two checklist lines referring to "a comment restating `plan.md`" are updated to match the new rule rather than left describing the old one.
- The rule states that the three categories are exempt from *mechanical flagging*, **not** immune from editing, and carries the three worked patterns.
- The ban explicitly covers bare criterion and section numbers, not only `§`-prefixed citations.
- The rule's full substance lives in **exactly one** file (`project-docs/SKILL.md`); the five agent files carry pointers only.
- `project-docs`'s `description:` frontmatter is **re-verified unchanged and still fires** on `src/`, `tests/`, `Makefile` and `.github/workflows/` edits. This is the load-bearing detail: a rule whose skill does not load on the file it governs does not exist. Do not edit the body without re-reading it.
- `description:` also covers `scripts/`, or it is confirmed in writing that the existing scoping already fires there.
- `cpp-implementer.md` no longer directs an implementer to reproduce the existing headers' comment style; `planner.md` carries the constraint.
- Master plan §6b and the skill do not disagree after the edit.
- **Must land before REV-21, CPP-4 and CPP-5.**

---

### REV-21 — Comment-discipline audit
**Agent:** `code-reviewer`
**Subject:** `src/core/`, `src/module/`, `tests/core/`, `Makefile`, `.github/workflows/ci.yml`, `.github/workflows/auto-pr.yml`, `scripts/setup-dev-env.ps1`, `.gitignore` — plus §5.0 of this plan and the CPP-4/CPP-5/CPP-19/BPE-10 task definitions, read as a specification **before** they are implemented.

**Audit only. Produces findings and a disposition list; performs no edits.**

**Scope fence.** `scripts/setup-dev-env.ps1` is **in** scope. Everything under `docs/` is **out**, deferred to BPE-13 at Stage 15 — report observations there as a one-line pointer, do not fix or enumerate them.

**The `setup-dev-env.ps1` carve-out is narrow, and the report must say so.** It covers **comment discipline only** — identifier references, comment volume, banned categories. It is **not** a re-audit of the script's provisioning logic, vcpkg pinning, allow-list filtering or `/MT` checks; those stay deferred. A logic defect noticed in passing is a one-line note for BPE-13, not an action.

**Checklist**
1. **Comment-volume regression.** Report the comment-line/total-line ratio per file. §5.0's table, and the ~17% measured for `setup-dev-env.ps1` (241 `#` lines of 1435), are baselines to **confirm or correct**, not to trust.
2. **Banned-list sweep** against `project-docs`'s three tiers: file-header blocks summarising what a file does; multi-paragraph rationale; rejected-options discussion; commit or PR archaeology; the same rationale in two files. `type-conversion.h`, `json-path.h`, `buffer-copy.h` and `status.h` are the named suspects.
3. **Identifier sweep, comments only.** Known `src/` violations to confirm or correct — 18 across seven of eight files: `status.h:4-5`, `:29`; `type-conversion.h:2`, `:4`, `:22`, `:68`; `type-conversion.cpp:56`, `:134`; `json-path.h:2`, `:40`, `:58`; `json-path.cpp:20`; `buffer-copy.h:3`, `:5`; `buffer-copy.cpp:12`; `exports.cpp:6`, `:36`, `:148`. Plus `Makefile`, `ci.yml`, `auto-pr.yml` and the `.ps1`. Three constraints on how this is reported:
   - A hit in one of D16's three exempt categories is **not auto-filed as Must-fix.** It goes to a separate **"requires human judgment"** section with proposed replacement text.
   - **The eight instances listed in BPE-32 are already settled.** Do not re-discover, re-litigate or re-propose them — reference the resolution and confirm BPE-32 applied it. Re-opening a settled decision is itself a finding against this task.
   - Include **bare criterion and section numbers**; a `§`-only regex misses them, and one such instance survived two earlier passes.
   - Report **per-file triage counts** — raw matches, how many triaged out, and why. A bare grep count is not an audit.
4. **Tier-3 over-length.** Every multi-line block that is genuinely a trap but exceeds 10 lines, distinguished from blocks that are not traps at all.
5. **The Stage 9 task definitions, pre-implementation.** Read §5.0's disposition table and CPP-4/CPP-5's acceptance criteria, and answer one question directly: *would an implementer following these literally produce a header in the 70% range?* **Yes or no, not a hedge.** This is the check that prevents the defect recurring rather than being cleaned up afterwards.
6. **Disposition list.** Every block proposed for removal carries exactly one of `captured → <file>#<section>`, `already covered → <file>#<section>`, or `dropped as redundant → <reason>`, each verified against the **actual target file**. A missing or false disposition is **Must-fix**, not Nice-to-have — filing this category as a nice-to-have is precisely how the original comment-bloat task stalled once already.
7. **Route every finding to its fix task:** `src/` and `tests/` → CPP-20; `Makefile`, `ci.yml`, `auto-pr.yml`, `.gitignore`, `setup-dev-env.ps1` → BPE-32; `.claude/**` → BPE-30 (which should be empty if BPE-30 landed correctly — a non-empty result there is itself a finding).

**Acceptance criteria**
- Every finding carries a file, a line range, a tier verdict, a disposition and an owning fix task.
- Severity-ranked on the agent's existing three tiers; the identifier sweep ranks **Should-fix or higher**.
- Protected comments (which stay, minus identifiers) are distinguished from removable ones; nothing on the protected list is proposed for stripping.
- Exempt-category hits appear under human judgment with proposed text, never as automatic Must-fix; the eight settled instances are referenced, not re-opened.
- **Runs after BPE-30**, so it audits against the corrected rule rather than the contradictory one.

---

### CPP-20 — Apply the audit's `src/` and `tests/` fixes
**Agent:** `cpp-implementer`
**Files:** `src/core/status.h`, `src/core/type-conversion.{h,cpp}`, `src/core/json-path.{h,cpp}`, `src/core/buffer-copy.{h,cpp}`, `src/module/exports.cpp`, `tests/core/*_test.cpp` as REV-21 directs; plus `docs/<topic>.md` for anything evicted with a `captured →` disposition.

Execute REV-21's disposition list for everything under `src/` and `tests/`. Findings outside those trees belong to BPE-32 or BPE-30.

**Acceptance criteria**
- **Zero behavioural change.** Comments only; `git diff` shows no change to any executable line. `make test ARCH=x86` and `make test ARCH=x64` both still pass, 74/74.
- **`CAPL_DLL_INFO_LIST4[]` diff is empty** and `exports.def` is untouched. `exports.cpp` is in scope for comment edits only, and the table is not a comment. Load `capl-export-contract` before touching that file.
- `exports.cpp`'s CAPL naming-convention statement is **retained** — master plan §5 requires it to live in that file — with `(Stage 5)` removed.
- No comment anywhere under `src/` or `tests/` names a plan section, stage, task ID, or bare plan criterion number.
- Every `captured →` disposition is honoured: the material exists in its named target file **before** this task is called done.
- The four worst headers land materially below their §5.0 ratios, with the reduction coming from removing **banned categories** — not from deleting genuine tier-2 one-line WHYs. Removing a real explanation to hit a number is a worse outcome than the bloat.

---

### BPE-32 — Apply the audit's build-file and script fixes
**Agent:** `build-pipeline-engineer`
**Files:** `Makefile`, `.github/workflows/ci.yml`, `.github/workflows/auto-pr.yml`, `.gitignore`, `scripts/setup-dev-env.ps1`

**Eight settled edits, applied regardless of what REV-21 reports.** These are decided; apply the text as given rather than redesigning it. Each demonstrates one of D16's three patterns.

| # | Location | Pattern | Edit |
|---|---|---|---|
| 1 | `setup-dev-env.ps1` lines 11–12, inside the `<# … #>` `.DESCRIPTION` | delete whole sentence | Delete `See docs/work/capl-rest-dll-rebuild/plans/plan.md (Stage 2, task BPE-1) for the authoritative spec.` The three preceding sentences already say what the script does; someone running `-?` gains nothing from knowing which task specified it. Nothing to inline. |
| 2 | `auto-pr.yml` line 121 | inline the substance | `echo '- [ ] Plan folded into `plan.md`, incl. any renumbering (§7.8 criterion 6)'` → `echo '- [ ] Plan folded into `plan.md`, incl. any task-ID/section renumbering (last commit before marking Ready for review)'` |
| 3 | `auto-pr.yml` lines 129–131 | drop the prefix only | Remove `Per plan.md §7.10:` and reflow: `echo '⚠️ This branch touches `src/module/exports.cpp`. Never have two open PRs that'` / `echo 'both touch this file, and verify the human gate against the CI artifact from'` / `echo 'this PR before merge.'` |
| 4 | `auto-pr.yml` lines 111–113 | inline the substance | Replace the plan-only notice with: `echo 'Plan-only branch: the `code-reviewer`, human-gate and `CHANGELOG.md` items'` / `echo 'below do not apply. CI green and up-to-date-with-`main` still apply.'` This also removes a dependency on §7.8's *numbering* — "criteria 2, 3, 4 / 1, 5" carries no `§` and no task ID, and would silently falsify the generated PR body if §7.8 were ever renumbered. |
| 5 | `setup-dev-env.ps1` lines 1408–1412, CAPL SDK headers FAIL message | inline the substance | `…from a Vector CANoe/CANalyzer installation. Install Vector CANoe/CANalyzer and build the official "Example of a Windows DLL for CAPL" sample to obtain them, then place all three at include/vendor/capl-dll-sdk/.` The instructions survive; the Stage 3 / HUM-10 / HUM-11 pointer does not. |
| 6 | `setup-dev-env.ps1` line 740 | delete, nothing to inline | `errors (see BPE-25).` → `errors.` The surrounding sentences already state the failure and the fix in full. |
| 7 | `ci.yml` line 132 (error-message string) | drop the prefix only | `"See BPE-25 and docs/development-environment.md's 'Pinning the vcpkg tool itself' section."` → `"See docs/development-environment.md's 'Pinning the vcpkg tool itself' section."` The `docs/` pointer is explicitly permitted and stays. |
| 8 | `ci.yml` line 121 (a genuine YAML comment) | drop the prefix only | `# commit (BPE-25; docs/development-environment.md#pinning-the-vcpkg-tool-itself).` → drop `BPE-25; `, keep the anchor. |

Edits 6, 7 and 8 touch three copies of the same baseline/tool-pin rationale in three files — a duplication already mitigated by `docs/development-environment.md` being its single substantive home. Confirm all three still point at a section that exists.

Then apply REV-21's remaining dispositions for these five files.

**Ownership.** BPE-32 owns `setup-dev-env.ps1`, both workflows, `Makefile` and `.gitignore`. BPE-30 owns only `.claude/**`.

**Acceptance criteria**
- **Zero behavioural change.** Text only. `make all`, `make test ARCH=x86` and `make test ARCH=x64` green; CI green on both legs.
- **`scripts/setup-dev-env.ps1` still runs end to end**, still reporting its existing 19 OK / 0 WARN / 0 FAIL bar on both architectures. **Run it — do not reason about it.** This script has four recorded instances of static review missing what execution caught.
- **`setup-dev-env.ps1 -?` still produces its full comment-based help** — `.SYNOPSIS`, `.DESCRIPTION` and every `.PARAMETER` intact, the `<# … #>` block still well-formed after edit 1.
- **`auto-pr.yml` still parses and still produces a correct PR body.** All three echo edits sit inside a `run: |` block scalar, so YAML does not reinterpret them and backticks inside single quotes stay literal — but **verify by triggering the workflow, not by reading the diff.** This workflow's own last defect was a regex that truncated multi-segment task IDs, found only by running it.
- After edit 4, the plan-only notice still matches the checklist items printed below it — the rewrite removes the cross-reference, so the two must now agree by wording rather than by index.
- The protected `Makefile` traps (`/SUBSYSTEM:CONSOLE`, `make -n`) are retained, trimmed at most, never removed.
- Every disposition honoured against its target file.

---

### CPP-19 — Extend `src/core/status.h`
**Agent:** `cpp-implementer`
**Files:** `src/core/status.h` (modified)

Append §5.1's block. Tighten the `ParseError = -10` comment per OQ5.

**Acceptance criteria**
- Every pre-existing value is **byte-identical**: `0`, `-1`, `-2`, `-3`, `-10` … `-17`. `git diff` shows additions and the one comment edit, nothing else.
- The `-24` and `-25..-29` reservations are present and explicit.
- Still includes only `<cstdint>`. No `json.hpp`, no CAPL SDK.
- The appended block carries **one-line comments only**, and `status.h`'s existing 18-line file header is **not extended** — it is a banned file-header block, and REV-21 will already have flagged it.
- The `ParseError` comment currently reads *"no Stage 8 caller yet — forward reservation; likely Stage 9 (sync-operations) or Stage 12 (json-flatten)"*. The rewrite drops **all three** stage references, not merely narrows them — for example *"reserved for JSON parse failures; no caller yet."* Narrowing rather than removing would leave a D16 violation in a file this task is already editing.

**Must land before CPP-4 and CPP-5.**

---

### BPE-10 — Make the libcurl link real
**Agent:** `build-pipeline-engineer`
**Files:** `Makefile`, `.gitignore`, `scripts/setup-dev-env.ps1`, `.github/workflows/ci.yml`, `CHANGELOG.md`

**Scope correction, verified against the tree rather than taken from the master plan's one-line description.** The master plan describes BPE-10 as *"Link `libcurl.lib` and `zs.lib` from the matching `lib/<arch>/`, plus all Windows system libs in `SYSLIBS`"*. **That half is already fully done** and needs no edit:

- `Makefile` already reads `SYSLIBS := crypt32.lib bcrypt.lib secur32.lib ws2_32.lib normaliz.lib wldap32.lib advapi32.lib version.lib` and `LIBS := libcurl.lib zs.lib $(SYSLIBS)`.
- `lib/x86/` and `lib/x64/` each already contain `libcurl.lib` and `zs.lib`.
- `INCLUDES` and `TEST_INCLUDES` both already carry `/I include/vendor`, which is all the curl headers need given the placement below.

Because no source references a curl symbol yet, the linker discards both libraries: **Stage 9 is the first time that link line is actually exercised.** Three gaps remain, none visible in the master plan's description:

**Part A — curl headers are provisioned nowhere.** `include/vendor/` holds `gtest/`, `capl-dll-sdk/` and `json.hpp` — **no `curl/`**. `setup-dev-env.ps1` copies gtest headers and downloads `json.hpp` but has no curl-header step at all, so `#include <curl/curl.h>` cannot compile today, locally or in CI. Add a step copying `installed/<triplet>/include/curl/` into `include/vendor/curl/`, in **both** the local script and CI, mirroring the existing gtest-header step.

**Part B — `.gitignore` must gain `include/vendor/curl/`, and its neighbouring comment is about to become false.** `.gitignore` already ignores `include/vendor/gtest/`, confirming that vcpkg-provisioned headers are ignored rather than committed; add `include/vendor/curl/` alongside it. Separately, the comment block above states *"Vendored SOURCE is still committed (`include/vendor/json.hpp`, `include/vendor/capl-dll-sdk/`) — only compiled output is not."* That was already imprecise — gtest headers are source and are ignored — and a second ignored header tree makes it actively misleading. Correct it to distinguish **hand-vendored, pinned source** (committed: `json.hpp`, `capl-dll-sdk/`) from **vcpkg-provisioned source** (ignored: `gtest/`, `curl/`).

**Part C — `CURL_STATICLIB` is defined nowhere in the repo.** Verified by search: zero occurrences. Without it, `curl.h` declares every entry point `__declspec(dllimport)` and linking against the **static** `libcurl.lib` fails with `LNK2019` on `__imp_curl_easy_init` and friends. Add `/D CURL_STATICLIB` to **both** `CXXFLAGS` and `TEST_CXXFLAGS` — the test executable compiles `src/http/*.cpp` and hits the identical wall. Define it once in a shared variable so the two cannot drift. **This is the only change the `Makefile` needs.**

**Acceptance criteria**
- `#include <curl/curl.h>` compiles in both the product build and the test build, on both architectures, from a clean tree after running `setup-dev-env.ps1`.
- `CURL_STATICLIB` reaches both compile paths from a **single** definition. No second parallel recipe; both architectures still flow through the one parameterized rule.
- `.gitignore` ignores `include/vendor/curl/`, and its vendored-source comment is accurate for all four trees.
- A source file referencing a real curl symbol links on x86 and x64 with **zero `LNK4098`**. If any dependency turns out to be `/MD`-only: **stop and ask** — do not link it silently.
- `/MT` provenance re-verified with the mandatory **two-part** check: `/DEFAULTLIB:LIBCMT` present **and** `LIBCMTD` absent. A bare substring match passes a debug-CRT lib; this was a live defect once already.
- `lib/<arch>/` still contains only product libs; `lib/gtest/<arch>/` never enters the DLL link line — verify against disk, not against the copy step's own report.
- `CHANGELOG.md` `[Unreleased]` entry (§4a), describing the user-visible fact and carrying no stage or task identifier. The `.gitignore` comment correction carries none either.
- No hardcoded version number anywhere in the diff.

**Sequencing.** Parts A–C land **before** CPP-4 — nothing compiles otherwise. Link verification can only complete **after** CPP-4 references a curl symbol, so this is a two-visit task. The master plan's risk register records nine instances of a clean static review missing what execution caught; budget a fix cycle rather than treating red as a setback.

---

### CPP-4 — `src/http/http-client.{h,cpp}` and `docs/http-layer.md`
**Agent:** `cpp-implementer`
**Files:** `src/http/http-client.h`, `src/http/http-client.cpp`, `docs/http-layer.md` (all new)

Implement D1–D4, D11, D15, D16, and §5.2–§5.5 **as dispositioned by §5.0**.

**Acceptance criteria**
- `http-client.h` includes `core/status.h`, `<cstdint>`, `<string>`, `<vector>` — and **nothing else**. No `curl/curl.h`, no `json.hpp`, no CAPL SDK.
- `curl/curl.h` appears in `http-client.cpp` **only** (D2).
- **§5.0's dispositions are followed exactly.** Each block marked *inline* is a single tier-3 block of **≤10 lines** whose first line names the trap. Each block marked → `docs/http-layer.md` appears there and **not** as a source comment.
- **`docs/http-layer.md` is created in this same change**, topic-scoped rather than stage-scoped, carrying the evicted material with one-line breadcrumbs.
- **Comment lines ≤ 35% of total lines** in both files (§5.0). A tripwire, not a target: if meeting it would require deleting a genuine one-line WHY, stop and raise it.
- No comment in either file names a plan section, stage, task ID, or bare plan criterion number (D16).
- **D15 wiring:** `RequestOptions::skipTlsVerification` exists, is a plain `bool`, and defaults to `false` under every construction path. `CURLOPT_SSL_VERIFYPEER` **and** `CURLOPT_SSL_VERIFYHOST` are both set explicitly on every request in **both** branches; neither is left unset, and they are never set independently. No global switch, environment variable, build-time define or derived value can influence the field. Its rationale appears in `docs/http-layer.md` and in **one** source location — never both files.
- The `ResponseTooLarge`-beats-`CURLE_WRITE_ERROR` precedence is implemented **and** written as a named trap.
- `curl_global_init` is **not** called from `DllMain`; `curl_global_cleanup` is **never** called, with the deliberate-leak rationale recorded per §5.0.
- One easy handle per `Perform`; no process-wide mutable state beyond the `call_once` flag.
- Nothing `noexcept`; every allocation point guarded so no exception escapes `Perform` (D13).
- Every `size_t` → `int32_t`/`uint32_t` conversion explicitly range-checked with **fixed-width types only** — never `size_t`, `ptrdiff_t` or `intptr_t` in a threshold comparison.
- The size-cap constant is one named constant, identical on both architectures, with no architecture-conditional code anywhere in the file.
- `/W4` clean on x86 and x64.

---

### CPP-5 — `src/http/sync-operations.{h,cpp}`
**Agent:** `cpp-implementer`
**Files:** `src/http/sync-operations.h`, `src/http/sync-operations.cpp` (new)

Implement D9, D10, D12, D16, and D8 as dispositioned by §5.0.

**Acceptance criteria**
- Depends only on `http-client.h` and `core/status.h`. No curl header, no `json.hpp` (it does not parse — OQ5), no CAPL SDK.
- The realtime-branch prohibition appears as a **≤10-line note** stating the constraint and pointing to `docs/http-layer.md`, which carries the Vector quotation and citation (§5.0). A `docs/` path is not a task identifier; this pointer is permitted.
- **Comment lines ≤ 35% of total lines**; no plan, stage, task or bare criterion identifiers (D16).
- **`RequestOptions` is forwarded to `HttpClient` unmodified.** `skipTlsVerification` passes through untouched: no assignment to it, no defaulting, no verb-specific or URL-scheme-specific override, no branch that reads it. There must be no path through this module by which TLS verification can be turned off other than the caller having set the field.
- **No response is retained between calls** — no static, no thread-local, no member cache, no singleton. The entire response-store design belongs to Stage 11 (OQ6) and must not be prefigured here.
- `InvalidArgument` is returned only for caller-side programming errors (D12).
- Nothing `noexcept`; nothing throws; `/W4` clean on both architectures.

---

### TEST-4 — The fake transport
**Agent:** `test-engineer`
**Files:** `tests/http/fake-transport.h` (new)

A `FakeTransport : public HttpTransport` that:
- returns a scripted `Status` and `HttpResponse` (queue or single, implementer's choice, used uniformly);
- **records the last request it received** — URL, method, headers, body, and the **full resolved `RequestOptions` including `skipTlsVerification`** — so tests can assert what `sync-operations` actually built, not merely what it returned;
- can simulate `Timeout`, `NetworkError`, `TlsError`, `ResponseTooLarge` and any HTTP status code without a socket.

**Acceptance criteria**
- Lives under `tests/`, never under `src/`. Never linked into the product DLL.
- Makes **no network call**, under any configuration.
- No Makefile change required: `TEST_CASE_SRCS` already globs `tests/http/*.cpp`. Confirm rather than assume.

---

### TEST-5 — `tests/http/http-client_test.cpp`, `tests/http/sync-operations_test.cpp`
**Agent:** `test-engineer`

**Offline only. Zero network. Zero live verification of any kind** (OQ9). No `DISABLED_` live tests, no environment-gated tests, no standalone console program — live integration testing is deferred wholesale to a future stage (§11(b)).

Required coverage, at minimum:

- **Every row of §5.2's mapping table**, driven through the seam — including the catch-all and the `ResponseTooLarge`-beats-`CURLE_WRITE_ERROR` precedence.
- **Simulated timeouts and error responses**, required by `cpp-testing-conventions` for the HTTP layer.
- **HTTP status handling (D5/OQ2):** 200, 201, 204, 400, 401, 404, 500 all yield `Status::Ok` with the code in `statusCode` — asserted individually, since this is the decision Stage 10 freezes into the export contract.
- **Request construction:** each verb sets the right method; headers arrive intact; a POST body arrives byte-identical; default options resolve to the documented defaults when `0` is passed (§5.4).
- **D15 / `skipTlsVerification`, against the fake:** a **default-false regression test** (a default-constructed `RequestOptions` arrives with `skipTlsVerification == false` — this is the test that catches someone later flipping the default); **round-trip forwarding of `true`** through every `sync-operations` verb, not just one; and a test that no verb mutates the field in either direction. **Explicitly out of scope, and the test file must say so:** whether `CURLOPT_SSL_VERIFYPEER`/`VERIFYHOST` are *actually* set to §5.3's values — that is real-curl-against-a-real-server behaviour, unobservable through the fake, and deferred to §11(b). Do not write a test that appears to cover it.
- **D10:** a body on GET/HEAD/DELETE → `InvalidArgument`. **D11/D12:** empty URL → `InvalidArgument`.
- **Size cap:** one byte over → `ResponseTooLarge`; exactly at the cap → `Ok`. **Both assertions must run in both the x86 and x64 test binaries** — the architecture-parity check for the cap.
- **Thread-safety smoke test:** two concurrent `Perform` calls through the fake complete without a data race. If a genuine race test is impractical without a TSan equivalent, record that explicitly rather than writing a test that only appears to check it.
- Malformed and empty response bodies; a response with zero headers.

**Acceptance criteria**
- `make test ARCH=x86` and `make test ARCH=x64` both pass.
- **No test makes a network call, and no test in the diff is `DISABLED_`-prefixed or otherwise gated to run only sometimes.** The suite must pass with networking disabled entirely — state how this was confirmed.
- No test is `noexcept`; no test depends on `src/module/`.
- Files use the `_test.cpp` suffix, uniform with `tests/core/`.
- Comments obey D16: no plan, stage, task or bare criterion identifiers.

---

### CPP-21 — Extract the response-cap predicate and the transfer-result precedence
**Agent:** `cpp-implementer`
**Files:** `src/http/http-client.h`, `src/http/http-client.cpp`, `docs/http-layer.md`

Two pure helpers currently sit in an anonymous namespace in `http-client.cpp`, reachable only through `CurlTransport::Perform` and therefore only during a live network transfer. Neither depends on a libcurl type; both are trapped by placement rather than by design. Extract both to external linkage so they can be tested directly.

```cpp
bool   WouldExceedResponseCap(std::size_t currentSize, std::size_t incoming,
                              std::uint32_t capBytes);
Status ResolveTransferResult (bool capExceeded, Status mappedStatus);
```

`WriteCallback` calls the first in place of its inline comparison. `Perform` calls the second in place of its two sequential early returns, computing the mapped status eagerly — `MapCurlCode` is a pure switch with no side effects, so evaluating it unconditionally is behaviour-preserving.

**The hazard, and the only condition under which this is safe.** This restructures early-return control flow in code that has already been written and reviewed — the same shape as the `CopyOwnVersionString` rewiring in the core-logic stage, where collapsing guards would have compiled cleanly, passed tests, and been wrong at runtime. That change was made safe by re-deriving its behaviour from the diff independently rather than trusting the implementer's own report, and this one carries the identical requirement.

**Acceptance criteria**
- **Behaviour-preserving, verified by re-deriving it from the diff — not by trusting this task's own report.** The cap must trip on exactly the same inputs as before, and `Perform` must return the same `Status` for every input class it can encounter, with the cap breach still taking precedence over any mapped transfer error.
- No libcurl type appears in `http-client.h`. `curl/curl.h` still appears in `http-client.cpp` only; the one-translation-unit rule is untouched.
- The widening rationale currently carried above the cap comparison — why `capBytes` is widened up to `size_t` rather than `body->size()` narrowed down to it — moves with the predicate, stays ≤10 lines, and carries no plan, stage, or task identifiers.
- The trap comment above the `CURLcode` mapping still states the precedence trap accurately after the extraction.
- No new file, no Makefile change, no new dependency. The test binary already compiles `src/http/*.cpp`.
- `docs/http-layer.md` gains a **"What cannot be verified without a live server"** section naming the `CURLcode`→`Status` mapping, libcurl's actual error reporting under Schannel, and the real `CURLOPT_SSL_VERIFY*` wiring.
- `/W4` clean on x86 and x64; `make all` green.

---

### TEST-13 — Direct coverage for the extracted predicate and precedence
**Agent:** `test-engineer`
**Files:** `tests/http/http-client_test.cpp` (extend)

`WouldExceedResponseCap`:
- exactly at the cap → does not exceed; one byte over → exceeds;
- empty incoming into a full buffer; `capBytes == 0`; `currentSize` already at the cap with non-zero incoming;
- **the wrap case explicitly** — `incoming` larger than `capBytes`, proving the `incoming > cap` guard prevents `cap - incoming` from wrapping. This single assertion is what the whole extraction exists for.

`ResolveTransferResult`:
- cap exceeded beats a mapped transfer error; cap exceeded beats `Status::Ok`; not exceeded passes the mapped status through unchanged.

**Acceptance criteria**
- **Every cap assertion runs in both the x86 and x64 test binaries** — `make test ARCH=x86` and `make test ARCH=x64`. This is what closes the architecture-parity risk the cap carries; a single-architecture run cannot see a width-dependent threshold.
- No network call; no `DISABLED_` test; nothing `noexcept`; no dependency on `src/module/`.
- Comments carry no plan, stage, or task identifiers.

---

### BPE-31 — Remove dead `.gitkeep` files and install the standing rule
**Agent:** `build-pipeline-engineer`
**Files:** the `.gitkeep` files below; `.claude/skills/msvc-build-conventions/SKILL.md`; `.claude/agents/build-pipeline-engineer.md`

Nine `.gitkeep` files exist. Three buckets, deliberately not one sweep.

**Bucket A — remove now** (directory already holds tracked real files):
- `src/core/.gitkeep` — `status.h`, `type-conversion.{h,cpp}`, `json-path.{h,cpp}`, `buffer-copy.{h,cpp}` all landed in Stage 8.
- `tests/mapping/.gitkeep` — **verify first.** If `tests/mapping/` is still empty (Stage 12's TEST-7 populates it), it belongs in Bucket C. Listed here only so the check is not skipped.

**Bucket B — remove as the last content change of Stage 9**, after CPP-4/CPP-5/TEST-4/TEST-5 have landed real files:
- `src/http/.gitkeep`, `tests/http/.gitkeep`

Removing these earlier would leave the directories briefly untracked-and-empty mid-stage — the one state `.gitkeep` exists to prevent.

**Bucket C — leave in place, with the reason recorded** so the next sweep does not re-litigate:
- `src/registry/.gitkeep`, `src/mapping/.gitkeep` — both deferred per `CLAUDE.md` Scope (registry conditional/Stage 16; mapping Stage 12).
- `examples/.gitkeep` — verified: `examples/` contains nothing else. Stage 12 CPP-11 populates it.
- `lib/gtest/x86/.gitkeep`, `lib/gtest/x64/.gitkeep` — **decide, don't sweep.** `.gitignore` ignores `lib/gtest/` entirely, so these can only be tracked if force-added; siblings `lib/x86/` and `lib/x64/` are ignored identically and carry **no** `.gitkeep`; and `setup-dev-env.ps1` creates all four directories regardless. **Run `git ls-files lib/gtest` first.** If untracked, they are inert local files — delete them and note it. If tracked, they are inconsistent with their own siblings — remove them and let the script own directory creation, as it already does for `lib/x86`/`lib/x64`.

**Install D17 as standing practice.** Substance in **one** place — `msvc-build-conventions`'s existing **Directory conventions** section, which already owns directory semantics and already loads on build-related edits. One **pointer line** in `build-pipeline-engineer.md`, which already owns the `lib/`-directory invariant. No substance duplicated.

**Acceptance criteria**
- Every `.gitkeep` is either removed or has a one-line recorded reason for staying. Nothing is left in an undecided state.
- Tracked-vs-untracked status is established with `git ls-files` **before** any deletion — not inferred from `.gitignore` or from the file being on disk.
- After Buckets A and B, `git status` shows no newly-untracked-and-empty directory, and a fresh clone followed by `scripts/setup-dev-env.ps1` still produces a tree that builds both architectures.
- `make clean && make all && make test ARCH=x86 && make test ARCH=x64` green afterwards.
- No `.gitignore` change is needed; if one appears to be, stop and flag it.
- D17's rule exists in exactly one place, with a single pointer line in the agent file. Adding it twice would violate the rule BPE-30 installs earlier in this same stage.
- No edit collision with BPE-30: both touch `build-pipeline-engineer.md`, but they are serialized (§8 steps 1 and 12). BPE-31 **re-reads** that file rather than working from a remembered version.

---

### REV-20 — Final review
**Agent:** `code-reviewer`
**Subject:** `git diff main...HEAD` on `stage/09-http-sync`

Checklist, in priority order:

1. **Export contract.** `CAPL_DLL_INFO_LIST4[]` diff **empty**. `exports.def` untouched; **no `LIBRARY` line**. No row renamed, reordered, or removed. `src/module/exports.cpp` appears in the diff for comment edits only.
2. **`Status` numbering.** Every pre-existing value byte-identical. Additions confined to `-18..-23`. The `-24` and `-25..-29` reservations present. Nothing minted from the async range.
3. **`/MT`.** Two-part check (`LIBCMT` present **and** `LIBCMTD` absent), both architectures, product and test binaries. Zero `LNK4098`.
4. **`CURL_STATICLIB`** reaches both compile paths from a single definition; no drift between `CXXFLAGS` and `TEST_CXXFLAGS`. `.gitignore` covers `include/vendor/curl/` and its vendored-source comment is accurate.
5. **D15 / TLS.** `skipTlsVerification` defaults to `false` on every construction path. Both `CURLOPT_SSL_VERIFY*` options set explicitly in both branches, never one without the other. No global, environment, define or derived influence. `sync-operations` forwards it unmodified with no branch that reads or writes it. **Any path that can disable verification without the immediate caller asking for it is a Must-fix.**
6. **Dependency direction.** `curl/curl.h` in `http-client.cpp` only. No CAPL SDK header anywhere under `src/http/`. No VIA call anywhere under `src/http/` — a correctness check, not a style check. `src/core/` still imports nothing from `src/http/`.
7. **Architecture parity.** No `size_t`/`ptrdiff_t`/`intptr_t` in any threshold comparison. No architecture-conditional code in `src/http/`. Size cap identical on both. Both test legs pass.
8. **Lifecycle.** No `curl_global_init` in `DllMain`. No `curl_global_cleanup` at all. One easy handle per request.
9. **No `noexcept`; nothing throws** out of `Perform` or any sync operation, including on allocation failure. No `at()`, no unguarded `get<T>()`.
10. **§5.0's dispositions honoured.** Each *inline* block is one tier-3 block ≤10 lines naming its trap; each evicted block is actually present in `docs/http-layer.md`; `docs/http-layer.md` exists, is topic-scoped, and carries breadcrumbs.
11. **No network call reachable from `make test` — and no live test exists in any form.** No `DISABLED_` test, no environment-gated test, no second binary.
12. **No response store.** No static, thread-local, member cache or singleton retaining a response between calls anywhere in `src/http/` (OQ6).
13. **Realtime-branch prohibition** present in `sync-operations.h` in §5.0's form.
14. **Makefile:** single parameterized rule intact; no second recipe; no hardcoded version number. `LIBS` and both include lists unchanged.
15. **`CHANGELOG.md` `[Unreleased]`** entry present for BPE-10 (§4a).
16. **Comment discipline, against D16 — not the superseded rule.** No plan, stage, task ID or bare plan criterion number in any comment in the diff. Every multi-line block ≤10 lines and naming its trap. The 35% ceiling met in both new `src/http/` files. Check the **composition** of any reduction, not just the ratio: a drop achieved by deleting genuine one-line WHYs is a finding, not a pass.
17. **REV-21's findings closed.** CPP-20, BPE-32 and BPE-30 landed; each disposition verified against its target file; BPE-32's eight settled edits applied as specified; `setup-dev-env.ps1 -?` and the `auto-pr.yml` PR body both verified by execution, not by inspection.
18. **Testability boundary recorded, not silently dropped.** The extracted cap predicate and transfer-result precedence are directly unit-tested, and every cap assertion runs in both architectures' binaries. Everything deliberately left to a live server is named in `docs/http-layer.md` and in the `tests/http/` files, with no plan, stage or task identifiers in those comments. **A gap that is neither closed nor recorded is a Must-fix** — an untested path nobody wrote down is indistinguishable from one nobody noticed.

---

## 8. Execution order

1. **BPE-30** — correct the comment rule and both agent-definition drivers. **First, and not movable.** Everything downstream is then written under the corrected rule, and the "match existing style" driver is defused before it can fire on `src/http/`.
2. **REV-21** — audit existing code and this plan's own task definitions against the corrected rule. Produces the disposition list; checklist item 5's yes/no gates step 8.
3. **CPP-20** — apply REV-21's `src/` and `tests/` dispositions. **This lands before CPP-4**, so that when the implementer reads existing modules for style, the examples it finds are the fixed ones.
4. **BPE-32** — apply the eight settled edits and REV-21's remaining build-file and script dispositions. Run `setup-dev-env.ps1` end to end, confirm `-?` help survives, and **trigger `auto-pr.yml`** to verify the PR body still renders. Independent of CPP-20; either order, or in parallel.
5. **CPP-19** — `status.h` append, under the corrected rule.
6. **Commit `docs/vector-capl-dll-docs/`** on this branch (R9). The directory is currently untracked while `docs/http-layer.md` and `sync-operations.h` are about to reference its material — a citation to a path not in the repository is dead on arrival for everyone but its author. Time this alongside the commit that persists this plan. **`plan-writer` has `Write`/`Read`/`Glob` and no `Bash`, so it cannot make the commit itself; the main session performs it.**
7. **BPE-10 Parts A, B, C** — curl headers provisioned, `.gitignore` updated and its comment corrected, `CURL_STATICLIB` defined. Verify a throwaway `#include <curl/curl.h>` compiles on both architectures before proceeding. **Do not skip this** — it is the cheapest possible place to find a provisioning gap.
8. **CPP-4** — `http-client.{h,cpp}` **plus `docs/http-layer.md`**, per §5.0.
9. **BPE-10 second visit** — now that real curl symbols are referenced, verify the link on both architectures and re-run the `/MT` two-part check. Expect this to be where something breaks.
10. **CPP-5** — `sync-operations.{h,cpp}`.
11. **TEST-4**, then **TEST-5** — fake transport, then offline coverage for both modules. Where a behaviour is reachable only through a live transfer, record it in the test file rather than asserting it against the fake: a test that tells the fake what to return and then checks the fake returned it proves nothing and reads as coverage.
12. **CPP-21**, then **TEST-13** — extract the cap predicate and the transfer-result precedence, then cover them directly. Sequenced after TEST-5 deliberately, so the extraction lands as a small, separately reviewable diff rather than tangled into the test commit.
13. **BPE-31** — `.gitkeep` Bucket A (verify `tests/mapping/` first), then Bucket B now that `src/http/` and `tests/http/` hold real files, then record Bucket C's reasons and install D17. **Last content change of the stage.**
14. **Verification sweep** — `make test ARCH=x86`, `make test ARCH=x64`, `make all`. All green, `/W4` clean, before review.
15. **REV-20** — `code-reviewer` on the full branch diff.
16. **Fold-in commit** — the last commit on the branch, before the PR is marked Ready. It must:
    - rewrite master plan §8's Stage 9 entry to the finalized scope, recording that BPE-10 turned out to be header provisioning, a `.gitignore` update and one `CURL_STATICLIB` define — the link-line half it describes was already complete and needed no edit;
    - **delete the superseded sentence** *"Verify as a standalone console program against httpbin.org"* from the Stage 9 / TEST-5 entry, recording that live verification is deferred per OQ9;
    - correct the §12 ledger row for **BPE-10**, which currently reads *"Link `libcurl.lib`, `zs.lib` and the system libs"* and overstates the remaining scope;
    - add §12 ledger rows for **CPP-19, CPP-20, CPP-21, BPE-30, BPE-31, BPE-32, TEST-13, REV-20, REV-21**, and mark **CPP-4, CPP-5, BPE-10, TEST-4, TEST-5** DONE. No HUM row is added; HUM-24 remains unspent;
    - record in §5 that `-18..-23` are now spent, `-24` and `-25..-29` reserved, and that `ParseError = -10` is assigned to Stage 12 rather than speculative;
    - record D15's TLS switch as a standing API property with its safe-by-default requirement, so Stage 10 does not expose it to CAPL by reflex;
    - **record that R4's mitigation was completed by extraction rather than deferred** — the cap's architecture-parity risk is closed by a direct unit test running in both binaries, not merely claimed by a plan that assigned it to a test which could not reach the code. Record alongside it that §11(b) now carries concrete required coverage originating from a real finding made while writing the tests, not a general aspiration;
    - amend §6b to record that the comment rule now bans plan, stage and task identifiers **in comments**, with three categories exempt from mechanical flagging but **not** from editing; that `planner` now carries the constraint; and that this is a deliberate trade against the traceability those citations provided;
    - record that §6b's Stage-15/BPE-13 deferral of `setup-dev-env.ps1` is **narrowed** — comment discipline pulled forward into Stage 9, while provisioning logic, pinning, allow-list and `/MT` checks stay deferred, and `docs/` is untouched;
    - record D17's `.gitkeep` rule as living in `msvc-build-conventions`, and `docs/http-layer.md` as the topic home for HTTP-layer rationale **and for the testability boundary**;
    - record the disposition of this working document — **recommendation: left in place as the detailed record**, since §5's normative blocks belong beside the code they specify;
    - record the obligations this stage creates for later ones: the realtime-branch caveat must reach Stage 10's export-table description text and Stage 12's `.can` examples, and OQ6's response-store question is Stage 11's to answer from scratch;
    - record §11(b)'s deferred containerized-integration-testing initiative as an unscheduled future stage, now carrying a named required-coverage list.
17. **Human-only, no agent action:** push the branch, review and merge the auto-opened PR, confirm CI green on both legs. No agent runs any `git push`, and no agent performs any sub-step of this item, optional or otherwise.

---

## 9. Human approval gate: NO — with three escalation conditions

The gate exists for changes to the export contract, to `/MT`, to what gets published, or to CI's shipping behaviour. Stage 9 touches none of them: zero export-table rows, `/MT` unchanged, no release, and CI's behaviour changes only by gaining a provisioning step. **REV-20 and REV-21 are both mandatory regardless** — "no human gate" has never meant "no review".

**The comment-discipline tasks do not trip the gate, and the reasoning is recorded rather than assumed.** CPP-20 and BPE-32 are text-only with zero-behavioural-change criteria. BPE-32 edits `ci.yml` and `auto-pr.yml`, but only their comments and message strings — nothing that changes what is built, tested or shipped. BPE-30 and BPE-31 edit `.claude/**` and placeholder files, which are not build inputs. No artifact reaching a user differs because of any of them.

**Escalate to a human gate immediately if any of these occurs:**

1. **Any dependency turns up as `/MD`-only**, or the two-part `/MT` check returns anything ambiguous. Stop and ask; do not link it and carry on.
2. **Anything in `src/module/` appears in the diff other than a comment change.** Stage 9 has no business altering that file's behaviour, and CPP-20's scope there is comments only.
3. **CPP-20 or BPE-32 turns out to require a behavioural change** to honour a disposition. Both are defined as text-only; if honouring a finding needs real code or workflow logic to move, that is a different task with a different risk profile, and it stops here rather than being absorbed.

---

## 10. Risks

**R1 — `CURL_STATICLIB` is missing and nobody notices until the link.** The single most likely concrete failure in this stage, and it is already confirmed present in the tree: zero occurrences repo-wide. Symptom is `LNK2019` on `__imp_curl_*`, which reads like a missing library rather than a missing define, and sends people hunting in `LIBS` where nothing is wrong. *Mitigation:* BPE-10 Part C, REV-20 item 4, and the throwaway-compile check at execution step 3.

**R2 — Curl headers are missing from the include path in the *test* build only.** `TEST_INCLUDES` and `INCLUDES` are separate variables (Makefile lines 83 and 177) and have diverged before — BPE-29 had to fix `/I src` in both. Vendoring under `include/vendor/curl/` makes `/I include/vendor` cover both with no new flag, which is precisely why that placement was chosen (OQ8). *Mitigation:* OQ8's placement, REV-20 item 4.

**R3 — Something throws inside CANoe's process.** `src/http/` allocates far more than `src/core/` did — a multi-megabyte response body is a realistic `std::bad_alloc` site, especially on x86 with ~2 GB of user address space. An escaping exception or a `noexcept` that turns it into `std::terminate` takes CANoe down with it. *Mitigation:* D13, the size cap (D4/OQ4), REV-20 item 9.

**R4 — The size cap fires at different inputs on x86 and x64.** A `size_t`-based threshold comparison makes `ResponseTooLarge` architecture-dependent, and a single-architecture test run cannot see it. The cap check guards an unsigned subtraction — `currentSize > cap - incoming`, safe only because `incoming > cap` is tested first — which is the defect class review reads past and execution catches.

*Mitigation, and note the shape it had to take.* CPP-4's fixed-width-types criterion and REV-20 item 7 cover the static half. The behavioural half was originally assigned to TEST-5, and **that assignment turned out to be unsatisfiable**: the comparison lived in an anonymous-namespace callback reachable only through a real network transfer, so the fake transport could not reach it, and asserting it against the fake would have proved only that the fake returned what it was told. CPP-21 extracts the comparison as a pure predicate with no libcurl dependency, and TEST-13 exercises it directly — including the wrap case — **in both the x86 and x64 binaries**. The risk is closed by that test, not by the plan having named one.

**R5 — A blocking sync call is used from the Simulation Setup realtime branch.** Vector prohibits it outright (V1), it runs on a high-priority thread, and with `0`-means-default timeouts the worst case is a stalled measurement rather than a hang — but it is still a stalled measurement. *Mitigation:* §5.4's no-infinite-timeout property, D8's header documentation, and the Stage 10/12 obligations recorded in the fold-in commit. **This risk is not closed by Stage 9; it is closed by Stage 11.**

**R6 — `curl_global_init` under the loader lock.** A `DllMain`-time init is the conventional-looking thing to write and deadlocks host processes. Symmetrically, a "tidy" `curl_global_cleanup` at `DLL_PROCESS_DETACH` reintroduces it at the other end. *Mitigation:* §5.5 items 1–2 written as named traps in the source, REV-20 item 8.

**R7 — No connection reuse.** One easy handle per request means a fresh TCP+TLS handshake every call. This is a deliberate, named trade for thread-safety and for a seam Stage 11 can use unchanged. It will look like a performance bug to someone later. *Mitigation:* §5.5 item 4 records the rationale in the source; any future change must go through the async layer's locking design, not around it.

**R8 — The seam is designed so that it cannot actually be faked.** The master plan (§8, Stage 9) warns about this specifically: settle it in CPP-4's design "before TEST-4 tries to test around a shape that does not admit a fake". *Mitigation:* D1 fixes the shape before any code is written, and CPP-5's acceptance criteria require that **no** code path needs the real `CurlTransport`.

**R9 — RESOLVED. `docs/vector-capl-dll-docs/` is committed on `stage/09-http-sync` as `879f0ee`.** The directory was untracked while this stage's source comments cite it by repo path (D8). Decision: commit it on this branch, timed alongside the commit that persists this plan. Residual note, accepted rather than eliminated: the `.htm.md` files carry Vector copyright notices and `capl_dll_found_resources.md` is in Polish. This is the same accepted-risk shape as the already-committed Vector SDK headers (master plan §13, *"Vector SDK headers remain committed and remain an accepted risk — public repository, explicit user decision"*).

**R10 — Static review will not catch the provisioning defect.** The master plan's risk register lists **nine** recorded instances where a clean static review passed and execution found the bug — two of them (the shared-install-root triplet collision, the unfiltered lib copy) in this exact `setup-dev-env.ps1` provisioning path, and two more found only by running CI. BPE-10 adds a step to that same script. *Mitigation:* treat the first red run as expected, not as a setback (master plan §13 states this as a planning input); and verify **destination state on disk**, not the copy step's own report (BPE-17's lesson).

**R11 — The TLS switch becomes the default, or gets set somewhere other than the call site. NEW, from D15.** Three concrete shapes, all of which compile, pass review at a glance, and produce a system that silently stops verifying certificates: (a) someone flips the default to `true` to make a local test pass; (b) `sync-operations` grows a "convenience" override — a URL-scheme check, a localhost special case, a verb-specific default; (c) a global switch, environment variable or build-time define is added later so the option can be set "once". The failure is invisible — everything appears to work, including against a hostile peer.

*Mitigation, at four layers deliberately:* the default is `false` and is required to be `false` on every construction path (D15, CPP-4); `sync-operations` is forbidden from reading or writing the field at all (CPP-5); TEST-5 carries a **default-false regression test** specifically aimed at shape (a); and REV-20 item 5 makes any path that can disable verification without the immediate caller asking for it a **Must-fix**. Note honestly what none of these cover: whether the `CURLOPT_SSL_VERIFY*` calls are *actually* correct against a real TLS peer — that is unobservable through the fake and waits on §11(b).

**R12 — This stage grows past what one stage should hold.** Twelve tasks across five file classes, one of them an open-ended audit. The `setup-dev-env.ps1` carve-out sounds like the largest expansion and, measured, is close to the smallest: 1435 lines at ~17% comments — already in the healthy band — with 13 identifier hits, most of them either triaged out as non-comments or already settled in BPE-32's table. What genuinely raises risk is the **fifth file class**: BPE-32 spans `Makefile`, two workflows, `.gitignore` and a 1435-line PowerShell script that has four recorded instances of static review missing what execution caught — hence its run-it-don't-reason-about-it criteria.

*Containment, structural rather than aspirational:* `docs/` stays out of scope; the `.ps1` carve-out is comment discipline only, explicitly not a provisioning-logic audit; CPP-20 and BPE-32 are text-only with zero-behavioural-change criteria; BPE-31 is three enumerated buckets plus a one-line rule, not a sweep. **If R12 fires, BPE-30 does not go with the split** — deferring it would leave CPP-4 and CPP-5 writing new source under a rule already decided to be wrong, guaranteeing a second cleanup cycle. Splitting candidates, in order: BPE-32's `.ps1` half, then BPE-31's Bucket C decisions. Never BPE-30, and never REV-21 checklist item 5.

**R13 — The 35% comment ceiling gets gamed.** A hard number invites deleting real one-line WHYs to reach it. *Mitigation:* CPP-4's and CPP-20's criteria state that removing a genuine explanation to hit a number is a worse outcome than the bloat, and REV-20 item 16 checks the composition of any reduction rather than the ratio alone.

**R14 — BPE-30 edits the rule while REV-21 audits against it.** If they overlap, the audit measures a moving target. *Mitigation:* strict sequencing — §8 step 1 fully complete before step 2. Worth stating explicitly, because "run them in parallel, they touch different files" is the tempting and wrong call.

**R15 — The identifier ban removes traceability that was deliberately installed.** Stage 8 put `plan.md §6 CPP-3` into `json-path.h` on purpose, and the master plan's "documentation that plausibly resembles the truth" risk is about exactly this class of link going stale. The new rule removes the link; `docs/http-layer.md` and the `docs/work/<slug>/plans/` documents are where traceability now lives. **This is a real trade, chosen deliberately, not an oversight** — the fold-in commit records it in §6b as a decision, so a future contributor finding a bare comment where a citation used to stand can learn why.

**R16 — A mechanical identifier sweep files confident, wrong findings — in both directions.** A naive grep across the widened scope hits PowerShell comment-based help (breaking `setup-dev-env.ps1 -?`), a user-facing error message that tells an operator how to fix a real failure, and `auto-pr.yml`'s generated PR-body text. All three look like textbook violations. But a blanket exemption fails the other way: reading those same instances closely showed seven citations that *should* be edited, five of which two earlier passes had missed. *Mitigation:* D16 routes these three categories to human judgment rather than either auto-flagging or exempting them; the rule carries three worked patterns so the next person has precedent; REV-21 reports them under human judgment with proposed text and per-file triage counts rather than raw match counts; BPE-32's eight settled edits are pre-decided so they are neither re-litigated nor missed; and BPE-32 verifies all three affected features by **running them**.

**R17 — Bare criterion numbers are invisible to the obvious regex.** `auto-pr.yml`'s plan-only notice reads *"criteria 2, 3 and 4 do not apply"* — a dependency on master plan §7.8's numbering with no `§`, no task ID and no `plan.md` in sight. It survived two earlier passes of this very analysis. Renumbering §7.8 would silently falsify every generated PR body, with nothing to catch it. *Mitigation:* D16 and BPE-30 Part A name bare criterion and section numbers explicitly; REV-21 item 3 includes them; and BPE-32 edit 4 removes the cross-reference in favour of wording that matches the checklist printed eight lines below it, so the two agree by content rather than by index.

---

## 11. Residual and deferred items

Neither item blocks Stage 9. Both must be carried into the master plan by the fold-in commit (§8 step 11) so the next actor inherits them from the authoritative document rather than from this one.

**(a) The response store — Stage 11's to design from scratch (OQ6).**
Stage 9 builds no store of any kind, and deliberately does not prefigure one. Stage 11 owns the entire design: shared state global within the DLL, per-module synchronization, ready flag cleared once read, request ID correlating a response to the call that produced it, one active response at a time (master plan §8, Stage 11).

Carried forward as an open sub-question Stage 11 must answer explicitly rather than discover: **do synchronous responses also need caching in that store once Stage 12/13's JSON accessors exist?** Stage 10 can export sync operations without a store, because the response body is copied straight into the caller's CAPL `char[]` (V3). But a Stage 12/13 accessor that reads "the last response" needs somewhere for that response to live. Nothing in Stage 9 constrains the answer; recording the question now is what stops it surfacing as a surprise at Stage 12.

**(b) Automated integration / E2E testing against a live containerized server — deferred, unscheduled, unplanned (OQ9).**
Stage 9 performs **no** live verification. The governing principle behind this deferral, recorded because it is broader than this stage: **integration and E2E tests must never be human-only — they must be automated, running both in local development and in CI.** A manually-run `DISABLED_` test and a hand-compiled console program were both rejected on those grounds, not on effort.

The intended mechanism is containerization: a self-hosted test HTTP/HTTPS server spun up in a Docker container and exercised by an automated integration suite in both local dev and CI, with no dependency on httpbin.org or any external network.

Concretely noted so the size of this is not underestimated: **Docker would be a new toolchain dependency — nothing containerized exists in this repository today.** It touches local developer setup (`scripts/setup-dev-env.ps1`), CI (`.github/workflows/ci.yml`), and the "never a second build system" constraint in `msvc-build-conventions`. Likely owner of the tooling decision: **`build-pipeline-engineer`**, with `test-engineer` owning the suite itself. This needs its own dedicated planning pass and has **not** been scheduled or assigned a stage number.

**Required coverage, carried forward from Stage 9's testability boundary.** These were identified while writing Stage 9's tests — not guessed at in advance — and are deliberately unverified until this stage exists. They are the concrete exit criteria for it:

- **Every row of the `CURLcode`→`Status` mapping, driven by a real server condition** rather than a synthesised code: timeout (slow endpoint), TLS failure (self-signed, expired, and wrong-host certificates, **under Schannel specifically**), network failure (connection refused, DNS failure, redirect limit exceeded), malformed and unsupported-scheme URLs, and the documented catch-all. *Why this cannot be done sooner:* a direct unit test of the mapping asserts only that the switch contains what the switch contains. The claim worth testing is that libcurl actually reports those codes in this configuration, and only a live transfer establishes it.
- **`ResponseTooLarge` through a real oversized transfer**, confirming the cap trips through libcurl's write path and not merely in the extracted predicate. CPP-21/TEST-13 prove the arithmetic; only a live transfer proves the wiring.
- **`Status::Ok` with each HTTP status class** (2xx, 4xx, 5xx) from a real server, confirming that transport success and HTTP status remain separate channels — the decision Stage 10 freezes into the export contract.
- **`skipTlsVerification` actually acting:** `false` **rejects** a self-signed certificate; `true` **accepts** it. This is the only proof that `CURLOPT_SSL_VERIFYPEER`/`VERIFYHOST` are wired to the values §5.3 specifies. The fake cannot observe it, and the default-false unit test proves only that the flag travels, not that it does anything.
- **Both architectures.**

**This supersedes the master plan's Stage 9 line** *"Verify as a standalone console program against httpbin.org"*, which is not followed and is deleted in the fold-in commit.

---

### Files referenced (absolute paths)

- `C:\Workspace\restifycapl\docs\work\capl-rest-dll-rebuild\plans\plan.md` — §5 standing constraints; §6b comment discipline; §8 Stage 9; §12 ledger; §13 risks
- `C:\Workspace\restifycapl\docs\work\stage-08-core-pure-logic\plans\plan.md`
- `C:\Workspace\restifycapl\docs\vector-capl-dll-docs\CAPLExportTable.htm.md` (V1, V2, V3), `CAPLIncludeWindowsDLL.htm.md` (V1), `CAPLIncludeWindowsDLLExample.htm.md`, `CAPLDLLOverview.htm.md`, `CAPLIncludeWindowsDLLSearchSequence.htm.md`, `VModuleFormatCAPLDLL.htm.md`, `capl_dll_canoe_links.md`, `capl_dll_found_resources.md`
- `C:\Workspace\restifycapl\src\core\status.h`, `type-conversion.{h,cpp}`, `json-path.{h,cpp}`, `buffer-copy.{h,cpp}`
- `C:\Workspace\restifycapl\src\module\exports.cpp`
- `C:\Workspace\restifycapl\Makefile` — `LIBS`/`SYSLIBS` already correct; `CXXFLAGS` and `TEST_CXXFLAGS` are the one change; `INCLUDES`/`TEST_INCLUDES` already sufficient; `TEST_CASE_SRCS`, `TEST_LOGIC_SRCS`
- `C:\Workspace\restifycapl\.gitignore` — `include/vendor/gtest/` is the precedent for curl headers; the vendored-source comment needs correcting
- `C:\Workspace\restifycapl\scripts\setup-dev-env.ps1`
- `C:\Workspace\restifycapl\.github\workflows\ci.yml`, `C:\Workspace\restifycapl\.github\workflows\auto-pr.yml`
- `C:\Workspace\restifycapl\CHANGELOG.md`
- `C:\Workspace\restifycapl\.claude\skills\project-docs\SKILL.md`, `.claude\skills\msvc-build-conventions\SKILL.md`
- `C:\Workspace\restifycapl\.claude\agents\` — `cpp-implementer.md`, `build-pipeline-engineer.md`, `test-engineer.md`, `code-reviewer.md`, `planner.md`, `plan-writer.md`

---

**Plan complete and approved.** The next action is implementation from §8 step 1. The main session commits `docs/vector-capl-dll-docs/` at §8 step 6, since `plan-writer` has no `Bash`.
