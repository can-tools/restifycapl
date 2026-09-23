# Stage 9 — HTTP layer and synchronous operations (logic only)

**Unit of work:** `stage-09-http-sync`
**Branch:** `stage/09-http-sync`, already cut off `main` @ `c77b65d`
**Master plan:** `C:\Workspace\restifycapl\docs\work\capl-rest-dll-rebuild\plans\plan.md` §8, Stage 9 (lines 753–760) — this document finalizes and expands that entry; it does not replace it. It also **supersedes one line of it** (§3, OQ9).
**Structural template:** `C:\Workspace\restifycapl\docs\work\stage-08-core-pure-logic\plans\plan.md`
**Human approval gate: NO** — with two named escalation conditions (§9).
**Status: APPROVED.** All nine open questions answered (§3); R9 resolved (§10). Ready for implementation.

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
| Build | `Makefile` (one change: the `CURL_STATICLIB` define), `scripts/setup-dev-env.ps1`, `.github/workflows/ci.yml`, `.gitignore` |
| Tests | `tests/http/fake-transport.h`, `tests/http/http-client_test.cpp`, `tests/http/sync-operations_test.cpp` (new) |
| Changelog | `CHANGELOG.md` `[Unreleased]` (see §4a) |
| Vector docs | `docs/vector-capl-dll-docs/` — committed on this branch (R9, §8 step 2) |
| Plan fold-in | `docs/work/capl-rest-dll-rebuild/plans/plan.md` (§8 Stage 9 + §12 ledger) |

**Explicitly not in scope**

- Any change to `CAPL_DLL_INFO_LIST4` or `exports.def`. The table diff must be **empty**. Nothing in `src/http/` is exported this stage.
- The async layer, the response store, the ready flag, request IDs — Stage 11 (OQ6).
- JSON parsing of response bodies — Stages 12–13 (OQ5).
- Request-body *building* helpers for CAPL — deferred Stage 17, per `CLAUDE.md` Scope. Stage 9 takes a `std::string` body and sends it.
- Connection pooling / curl share handles / HTTP-2 / multi interface — see §5.5 and R7.
- **Any live-network or integration test, in any form** — deferred to a future unscheduled stage (OQ9, §11(b)).
- Pushing, opening the PR, or merging. **Human-only, and no agent performs any sub-step of it, including "optional" ones.**

### 4a. CHANGELOG — a deliberate departure from Stage 8

Master plan §5: *"Every export-table append gets a `CHANGELOG.md` `[Unreleased]` entry in the same change"* and *"build and CI changes also get an entry"* (BPE-18 exists because the second half slipped). Stage 8 wrote no entry, on the grounds that its Makefile change was an inert include-path addition.

**Stage 9 is different and gets an entry**, for BPE-10: it adds a compile-time define, changes provisioning, and causes libcurl's code to actually enter the shipped DLL for the first time. That is user-visible in a way `/I src` was not. Flagging the divergence from the Stage 8 precedent explicitly rather than letting it look like an inconsistency.

---

## 5. Normative artifacts

These blocks are the specification. They go into the source headers **verbatim** (the enum as code, the tables as header comments) and are what `code-reviewer` checks against.

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

## 6. Design decisions (D1–D15)

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

---

## 7. Tasks

Task IDs are next-free against the master plan's §12 ledger as of `c77b65d` (highest allocated: CPP-18, BPE-29, TEST-12, REV-19, HUM-23). **Per §7.9 condition 2, an ID is spent when it reaches §12 — every new ID below must be written into the ledger in the fold-in commit.**

New IDs: **CPP-19, REV-20**. **No HUM task is minted this stage** — next-free HUM remains **24, unused** (OQ9).

---

### CPP-19 — Extend `src/core/status.h`
**Agent:** `cpp-implementer`
**Files:** `src/core/status.h` (modified)

Append §5.1 verbatim. Tighten the `ParseError = -10` comment to name Stage 12 only (OQ5).

**Acceptance criteria**
- Every pre-existing value is **byte-identical**: `0`, `-1`, `-2`, `-3`, `-10` … `-17`. `git diff` shows additions and the one comment edit, nothing else.
- The `-24` and `-25..-29` reservations are present and explicit.
- Still includes only `<cstdint>`. Still no `json.hpp`, no CAPL SDK.

**Must land before CPP-4 and CPP-5.**

---

### BPE-10 — Make the libcurl link real
**Agent:** `build-pipeline-engineer`
**Files:** `Makefile`, `.gitignore`, `scripts/setup-dev-env.ps1`, `.github/workflows/ci.yml`, `CHANGELOG.md`

**Scope correction, verified against the tree rather than taken from the master plan's one-line description.** The master plan describes BPE-10 as *"Link `libcurl.lib` and `zs.lib` from the matching `lib/<arch>/`, plus all Windows system libs in `SYSLIBS` (now including `version.lib`)"*. **That half is already fully done** and needs no edit:

- `Makefile` lines 93–94 already read `SYSLIBS := crypt32.lib bcrypt.lib secur32.lib ws2_32.lib normaliz.lib wldap32.lib advapi32.lib version.lib` and `LIBS := libcurl.lib zs.lib $(SYSLIBS)`.
- `lib/x86/` and `lib/x64/` each already contain `libcurl.lib` and `zs.lib`.
- `INCLUDES` (line 83) and `TEST_INCLUDES` (line 177) both already carry `/I include/vendor`, which is all the curl headers need given OQ8's placement.

Because no source references a curl symbol yet, the linker discards both libraries: **Stage 9 is the first time that link line is actually exercised.** What remains open are three gaps not visible in the master plan's description, each found by reading the tree:

**Part A — curl headers are not provisioned anywhere.**
`include/vendor/` contains `gtest/`, `capl-dll-sdk/` and `json.hpp` — **no `curl/`**. `scripts/setup-dev-env.ps1` copies gtest headers (step 7) and downloads `json.hpp` (step 8) but has **no curl-header step at all**. `#include <curl/curl.h>` cannot compile today, locally or in CI. Add a step copying `installed/<triplet>/include/curl/` into `include/vendor/curl/`, in **both** the local script and CI, mirroring the existing gtest-header step.

**Part B — `.gitignore` must gain `include/vendor/curl/`, and the comment above it is about to become false.**
`.gitignore` line 55 is `include/vendor/gtest/` — confirming vcpkg-provisioned headers are ignored. Add `include/vendor/curl/` alongside it. Separately, the comment block at lines 36–42 states *"Vendored SOURCE is still committed (`include/vendor/json.hpp`, `include/vendor/capl-dll-sdk/`) — only compiled output is not."* That was already imprecise (gtest headers are source and are ignored) and a second ignored header tree makes it actively misleading. Correct it to distinguish **hand-vendored, pinned source** (committed: `json.hpp`, `capl-dll-sdk/`) from **vcpkg-provisioned source** (ignored: `gtest/`, `curl/`).

**Part C — `CURL_STATICLIB` is defined nowhere in the repo.**
Verified by search: **zero occurrences.** Without it, `curl.h` declares every entry point `__declspec(dllimport)` and linking against the **static** `libcurl.lib` fails with `LNK2019` on `__imp_curl_easy_init` and friends. Add `/D CURL_STATICLIB` to **both** `CXXFLAGS` (line 87) and `TEST_CXXFLAGS` (line 178) — the test executable compiles `src/http/*.cpp` via `TEST_LOGIC_SRCS` (lines 165–166) and hits the identical wall. Define it once in a shared variable so the two cannot drift.

**This is the only change the `Makefile` needs.** No `LIBS` change, no `INCLUDES` change, no new recipe.

**Acceptance criteria**
- `#include <curl/curl.h>` compiles in both the product build and the test build, on both architectures, from a clean tree after running `setup-dev-env.ps1`.
- `CURL_STATICLIB` reaches both compile paths from a **single** definition. No second parallel recipe; both architectures still flow through the one parameterized rule.
- `.gitignore` ignores `include/vendor/curl/`, and its vendored-source comment is accurate for all four trees (`json.hpp`, `capl-dll-sdk/`, `gtest/`, `curl/`).
- A source file referencing a real curl symbol links on x86 and x64 with **zero `LNK4098`** (`/MT` vs `/MD` mismatch). If any dependency turns out to be `/MD`-only: **stop and ask** — do not link it silently (`msvc-build-conventions`).
- `/MT` provenance re-verified with the mandatory **two-part** check: `/DEFAULTLIB:LIBCMT` present **and** `LIBCMTD` absent (master plan §5; a bare substring match passes a debug-CRT lib — this was a live defect once already).
- `lib/<arch>/` still contains only product libs; `lib/gtest/<arch>/` never enters the DLL link line (BPE-17's invariant — **verify against disk, not against the copy step's own report**).
- `CHANGELOG.md` `[Unreleased]` entry (§4a).
- No hardcoded version number anywhere in the diff.

**Sequencing:** Parts A–C land **before** CPP-4 (nothing compiles otherwise). The link verification can only complete **after** CPP-4 references a curl symbol — this is a two-visit task, and the master plan's risk register (§13, nine recorded instances of "static review does not substitute for execution") says plainly that this is where a defect appears. Budget a fix cycle rather than treating red as a setback.

---

### CPP-4 — `src/http/http-client.{h,cpp}`
**Agent:** `cpp-implementer`
**Files:** `src/http/http-client.h`, `src/http/http-client.cpp` (new)

Implement D1–D4, D11, D15, §5.2, §5.3, §5.4, §5.5.

**Acceptance criteria**
- `http-client.h` includes `core/status.h`, `<cstdint>`, `<string>`, `<vector>` — and **nothing else**. No `curl/curl.h`, no `json.hpp`, no CAPL SDK.
- `curl/curl.h` appears in `http-client.cpp` **only** (D2).
- §5.2's mapping table, §5.5's lifecycle rules, and §5.3's option policy (**including both rows of the TLS table**) are present **verbatim** in `http-client.cpp`; §5.4's timeout policy verbatim in `http-client.h`.
- **D15 wiring:** `RequestOptions::skipTlsVerification` exists, is a plain `bool`, and defaults to `false` under every construction path — aggregate init, default member initializer, and any factory helper. `CURLOPT_SSL_VERIFYPEER` **and** `CURLOPT_SSL_VERIFYHOST` are both set explicitly on every request in **both** branches; neither is ever left unset, and they are never set independently of each other. There is no global switch, environment variable, build-time define, or derived value anywhere that can influence this field.
- The `ResponseTooLarge`-beats-`CURLE_WRITE_ERROR` precedence is implemented **and** written as a named editing trap (§5.2).
- `curl_global_init` is **not** called from `DllMain`; `curl_global_cleanup` is **never** called, with the deliberate-leak rationale in the source.
- One easy handle per `Perform`; no process-wide mutable state beyond the `call_once` flag.
- Nothing `noexcept`; every allocation point guarded so no exception escapes `Perform` (D13).
- Every `size_t` → `int32_t`/`uint32_t` conversion explicitly range-checked with **fixed-width types only** — never `size_t`, `ptrdiff_t` or `intptr_t` in a threshold comparison (Stage 8 §3.6; this is the x64-only defect class that also governs the size cap).
- The size cap constant is one named constant, identical on both architectures, with no architecture-conditional anything anywhere in the file.
- `/W4` clean on x86 and x64.

---

### CPP-5 — `src/http/sync-operations.{h,cpp}`
**Agent:** `cpp-implementer`
**Files:** `src/http/sync-operations.h`, `src/http/sync-operations.cpp` (new)

Implement D9, D10, D12, and D8's documentation requirement.

**Acceptance criteria**
- Depends only on `http-client.h` and `core/status.h`. No curl header, no `json.hpp` (it does not parse — OQ5), no CAPL SDK.
- The header carries the **realtime-branch prohibition (V1) verbatim**, citing `docs/vector-capl-dll-docs/CAPLExportTable.htm.md`, and states that these operations are Measurement-Setup/test-node only.
- **`RequestOptions` is forwarded to `HttpClient` unmodified.** In particular `skipTlsVerification` passes through untouched: `sync-operations` contains **no** assignment to it, no defaulting of it, no verb-specific or URL-scheme-specific override, and no branch that reads it. There must be no path through this module by which TLS verification can be turned off other than the caller having set the field themselves (D15).
- **No response is retained between calls** — no static, no thread-local, no member cache, no singleton. OQ6 defers the entire response-store design to Stage 11, and Stage 9 must not prefigure it.
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
- No Makefile change required: `TEST_CASE_SRCS` already globs `tests/http/*.cpp` (Makefile line 167). Confirm rather than assume.

---

### TEST-5 — `tests/http/http-client_test.cpp`, `tests/http/sync-operations_test.cpp`
**Agent:** `test-engineer`

**Offline only. Zero network. Zero live verification of any kind** (OQ9). No `DISABLED_` live tests, no environment-gated tests, no standalone console program — live integration testing is deferred wholesale to a future stage (§11(b)).

Required coverage, at minimum:

- **Every row of §5.2's mapping table**, driven through the seam — including the catch-all, and including the `ResponseTooLarge`-beats-`CURLE_WRITE_ERROR` precedence.
- **Simulated timeouts and error responses**, explicitly required by `cpp-testing-conventions` for the HTTP layer.
- **HTTP status handling (D5/OQ2):** 200, 201, 204, 400, 401, 404, 500 all yield `Status::Ok` with the code in `statusCode` — asserted individually, since this is the decision Stage 10 freezes into the export contract.
- **Request construction:** each verb sets the right method; headers arrive intact; a POST body arrives byte-identical; default options resolve to the documented defaults when `0` is passed (§5.4).
- **D15 / `skipTlsVerification`, against the fake:**
  - a **default-false regression test** — a default-constructed `RequestOptions` arrives at the transport with `skipTlsVerification == false`. This is the test that catches someone later flipping the default or adding a "convenience" override;
  - **round-trip forwarding of `true`** — a caller setting it reaches the transport unchanged, through every `sync-operations` verb, not just one;
  - a test that no `sync-operations` verb mutates the field in either direction.
  - **Explicitly out of scope, and the test file must say so in a comment:** whether `CURLOPT_SSL_VERIFYPEER`/`CURLOPT_SSL_VERIFYHOST` are *actually* set to the values in §5.3's table. That is real-curl-against-a-real-server behaviour and cannot be observed through the fake. It is deferred to the containerized integration stage (§11(b)). Do not write a test that appears to cover it.
- **D10:** a body on GET/HEAD/DELETE → `InvalidArgument`.
- **D11/D12:** empty URL → `InvalidArgument`.
- **Size cap:** a body one byte over → `ResponseTooLarge`; exactly at the cap → `Ok`. **Both assertions must run in both the x86 and x64 test binaries** — same source, run under `make test ARCH=x86` and `make test ARCH=x64`. This is the architecture-parity check for the cap, the direct analogue of Stage 8's `NumericOverflow` requirement.
- **Thread-safety smoke test:** two concurrent `Perform` calls through the fake complete without a data race (this is the property V2/Stage 11 depends on). If a genuine race test is not practical without a TSan-equivalent, record that explicitly rather than writing a test that only appears to check it.
- Malformed/empty response bodies, and a response with zero headers.

**Acceptance criteria**
- `make test ARCH=x86` and `make test ARCH=x64` both pass.
- **No test makes a network call, and no test in the diff is `DISABLED_`-prefixed or otherwise gated to run only sometimes.** The suite must pass with networking disabled entirely — state how this was confirmed.
- No test is `noexcept`; no test depends on `src/module/`.
- Files use the `_test.cpp` suffix, uniform with `tests/core/` after Stage 8 (D2a/R9 there).

---

### REV-20 — Final review
**Agent:** `code-reviewer`
**Subject:** `git diff main...HEAD` on `stage/09-http-sync`

Checklist, in priority order:

1. **Export contract.** `CAPL_DLL_INFO_LIST4[]` diff **empty**. `exports.def` untouched; **no `LIBRARY` line** (`capl-export-contract` flags any added one as a defect). No row renamed, reordered, or removed. `src/module/exports.cpp` should not appear in the diff at all this stage.
2. **`Status` numbering.** Every pre-existing value byte-identical. Additions confined to `-18..-23`. `-24` and `-25..-29` reservations present. Nothing minted from the async range.
3. **`/MT`.** Two-part check (`LIBCMT` present **and** `LIBCMTD` absent), both architectures, product and test binaries. Zero `LNK4098`.
4. **`CURL_STATICLIB`** reaches both compile paths from a single definition; no drift between `CXXFLAGS` and `TEST_CXXFLAGS`. `.gitignore` covers `include/vendor/curl/` and its vendored-source comment is accurate.
5. **D15 / TLS.** `skipTlsVerification` defaults to `false` on every construction path. Both `CURLOPT_SSL_VERIFY*` options set explicitly in both branches, never one without the other. No global/env/define/derived influence on the field. `sync-operations` forwards it unmodified with no branch that reads or writes it. **Treat any path that can disable verification without the immediate caller asking for it as a Must-fix.**
6. **Dependency direction.** `curl/curl.h` in `http-client.cpp` only. No CAPL SDK header anywhere under `src/http/`. No VIA call anywhere under `src/http/` (V2 — this is a correctness check, not a style check). `src/core/` still imports nothing from `src/http/`.
7. **Architecture parity.** No `size_t`/`ptrdiff_t`/`intptr_t` in any threshold comparison. No architecture-conditional code in `src/http/`. Size cap identical on both. Both test legs pass.
8. **Lifecycle.** No `curl_global_init` in `DllMain`. No `curl_global_cleanup` at all, with its rationale in the source. One easy handle per request.
9. **No `noexcept`; nothing throws out of `Perform` or any sync operation**, including on allocation failure. No `at()`, no unguarded `get<T>()`.
10. **§5.2, §5.3, §5.4, §5.5 present verbatim** in the specified files. The `ResponseTooLarge` trap is written as a named trap.
11. **No network call reachable from `make test` — and no live test exists in any form.** Simplified by OQ9: there is no `DISABLED_` test, no environment-gated test and no second binary in scope, so this is now a check that **none of those appear**, rather than a check that a disabled one stays disabled.
12. **No response store.** No static, thread-local, member cache or singleton retaining a response between calls anywhere in `src/http/` (OQ6).
13. **V1's realtime-branch prohibition** is in `sync-operations.h`, cited.
14. **Makefile:** single parameterized rule intact; no second recipe; no hardcoded version number anywhere in the diff. `LIBS` and the include lists unchanged (BPE-10's verified scope).
15. **`CHANGELOG.md` `[Unreleased]`** entry present for BPE-10 (§4a).
16. **Comment discipline** (master plan §6b / `project-docs`): why, not what; multi-line blocks only as named editing traps.

---

## 8. Execution order

1. **CPP-19** — `status.h` append. First; CPP-4 and CPP-5 both need the codes.
2. **Commit `docs/vector-capl-dll-docs/` on this branch** (R9, resolved). **DONE — committed as `879f0ee`.**
3. **BPE-10 Parts A, B, C** — curl headers provisioned, `.gitignore` updated and its comment corrected, `CURL_STATICLIB` defined. Verify a throwaway `#include <curl/curl.h>` compiles on both architectures before proceeding. **Do not skip this verification** — it is the cheapest possible place to find a provisioning gap.
4. **CPP-4** — `http-client.{h,cpp}`.
5. **BPE-10 second visit** — now that real curl symbols are referenced, verify the link on both architectures and re-run the `/MT` two-part check. Expect this to be where something breaks.
6. **CPP-5** — `sync-operations.{h,cpp}`.
7. **TEST-4** — fake transport.
8. **TEST-5** — coverage for both modules.
9. **Verification sweep:** `make test ARCH=x86`, `make test ARCH=x64`, `make all`. All four green, `/W4` clean, before review.
10. **REV-20** — `code-reviewer` on the full branch diff.
11. **Fold-in commit** — last commit on the branch, before the PR is marked Ready (master plan §7.9 condition 2). It must:
    - rewrite master plan §8's Stage 9 entry to the finalized scope, recording that BPE-10 turned out to be **header provisioning + `.gitignore` + one `CURL_STATICLIB` define** — the link-line half it describes was already complete and needed no edit;
    - **delete the superseded sentence** *"Verify as a standalone console program against httpbin.org"* from §8's Stage 9 / TEST-5 entry, and record that live verification is deferred per OQ9;
    - correct the §12 ledger row for **BPE-10**, which currently reads *"Link `libcurl.lib`, `zs.lib` and the system libs"* and overstates the remaining scope;
    - add §12 ledger rows for **CPP-19** and **REV-20**, and mark **CPP-4, CPP-5, BPE-10, TEST-4, TEST-5** DONE. **No HUM row is added; HUM-24 remains unspent.**
    - record in §5 that `-18..-23` are now spent, `-24` and `-25..-29` reserved, and that `ParseError = -10` is now assigned to Stage 12 rather than speculative (OQ5);
    - record the D15 TLS switch as a standing API property, with its safe-by-default requirement, so Stage 10 does not expose it to CAPL by reflex;
    - record the disposition of this working document — **recommendation: left in place as the detailed record**, per §7.10 condition 3, since §5's four normative blocks belong beside the code they specify;
    - record the Stage 10/11/12 obligations this stage creates: the V1 realtime-branch caveat must reach the export-table description text and the `.can` examples, and OQ6's response-store question is Stage 11's to answer from scratch;
    - record §11(b)'s deferred containerized-integration-testing initiative as an unscheduled future stage.
12. **Human-only, no agent action:** push the branch, review/merge the auto-opened PR, confirm CI green on both legs. No agent runs any `git push`, and no agent performs any sub-step of this item, optional or otherwise.

---

## 9. Human approval gate: NO — with two escalation conditions

The gate exists for changes to the export contract, to `/MT`, to what gets published, or to CI's shipping behaviour. Stage 9 touches none of them: zero export-table rows, `/MT` unchanged, no release, and CI's behaviour changes only by gaining a provisioning step. **REV-20 is mandatory regardless** — "no human gate" has never meant "no review".

**Escalate to a human gate immediately if either occurs:**

1. **Any dependency turns up as `/MD`-only**, or the two-part `/MT` check returns anything ambiguous. `msvc-build-conventions` requires stopping and asking; do not link it and carry on.
2. **Anything in `src/module/` appears in the diff.** Stage 9 has no business there. A change to `exports.cpp` in this stage is by definition unintended.

---

## 10. Risks

**R1 — `CURL_STATICLIB` is missing and nobody notices until the link.** The single most likely concrete failure in this stage, and it is already confirmed present in the tree: zero occurrences repo-wide. Symptom is `LNK2019` on `__imp_curl_*`, which reads like a missing library rather than a missing define, and sends people hunting in `LIBS` where nothing is wrong. *Mitigation:* BPE-10 Part C, REV-20 item 4, and the throwaway-compile check at execution step 3.

**R2 — Curl headers are missing from the include path in the *test* build only.** `TEST_INCLUDES` and `INCLUDES` are separate variables (Makefile lines 83 and 177) and have diverged before — BPE-29 had to fix `/I src` in both. Vendoring under `include/vendor/curl/` makes `/I include/vendor` cover both with no new flag, which is precisely why that placement was chosen (OQ8). *Mitigation:* OQ8's placement, REV-20 item 4.

**R3 — Something throws inside CANoe's process.** `src/http/` allocates far more than `src/core/` did — a multi-megabyte response body is a realistic `std::bad_alloc` site, especially on x86 with ~2 GB of user address space. An escaping exception or a `noexcept` that turns it into `std::terminate` takes CANoe down with it. *Mitigation:* D13, the size cap (D4/OQ4), REV-20 item 9.

**R4 — The size cap fires at different inputs on x86 and x64.** Exactly Stage 8's R3 in a new module: a `size_t`-based threshold comparison makes `ResponseTooLarge` architecture-dependent, and a single-architecture test run cannot see it. *Mitigation:* CPP-4's fixed-width-types criterion, TEST-5's both-binaries requirement, REV-20 item 7.

**R5 — A blocking sync call is used from the Simulation Setup realtime branch.** Vector prohibits it outright (V1), it runs on a high-priority thread, and with `0`-means-default timeouts the worst case is a stalled measurement rather than a hang — but it is still a stalled measurement. *Mitigation:* §5.4's no-infinite-timeout property, D8's header documentation, and the Stage 10/12 obligations recorded in the fold-in commit. **This risk is not closed by Stage 9; it is closed by Stage 11.**

**R6 — `curl_global_init` under the loader lock.** A `DllMain`-time init is the conventional-looking thing to write and deadlocks host processes. Symmetrically, a "tidy" `curl_global_cleanup` at `DLL_PROCESS_DETACH` reintroduces it at the other end. *Mitigation:* §5.5 items 1–2 written as named traps in the source, REV-20 item 8.

**R7 — No connection reuse.** One easy handle per request means a fresh TCP+TLS handshake every call. This is a deliberate, named trade for thread-safety and for a seam Stage 11 can use unchanged. It will look like a performance bug to someone later. *Mitigation:* §5.5 item 4 records the rationale in the source; any future change must go through the async layer's locking design, not around it.

**R8 — The seam is designed so that it cannot actually be faked.** The master plan (§8, Stage 9) warns about this specifically: settle it in CPP-4's design "before TEST-4 tries to test around a shape that does not admit a fake". *Mitigation:* D1 fixes the shape before any code is written, and CPP-5's acceptance criteria require that **no** code path needs the real `CurlTransport`.

**R9 — RESOLVED. `docs/vector-capl-dll-docs/` is committed on `stage/09-http-sync` as `879f0ee`.** The directory was untracked while this stage's source comments cite it by repo path (D8). Decision: commit it on this branch, timed alongside the commit that persists this plan. Residual note, accepted rather than eliminated: the `.htm.md` files carry Vector copyright notices and `capl_dll_found_resources.md` is in Polish. This is the same accepted-risk shape as the already-committed Vector SDK headers (master plan §13, *"Vector SDK headers remain committed and remain an accepted risk — public repository, explicit user decision"*).

**R10 — Static review will not catch the provisioning defect.** The master plan's risk register lists **nine** recorded instances where a clean static review passed and execution found the bug — two of them (the shared-install-root triplet collision, the unfiltered lib copy) in this exact `setup-dev-env.ps1` provisioning path, and two more found only by running CI. BPE-10 adds a step to that same script. *Mitigation:* treat the first red run as expected, not as a setback (master plan §13 states this as a planning input); and verify **destination state on disk**, not the copy step's own report (BPE-17's lesson).

**R11 — The TLS switch becomes the default, or gets set somewhere other than the call site. NEW, from D15.** Three concrete shapes, all of which compile, pass review at a glance, and produce a system that silently stops verifying certificates: (a) someone flips the default to `true` to make a local test pass; (b) `sync-operations` grows a "convenience" override — a URL-scheme check, a localhost special case, a verb-specific default; (c) a global switch, environment variable or build-time define is added later so the option can be set "once". The failure is invisible — everything appears to work, including against a hostile peer.

*Mitigation, at four layers deliberately:* the default is `false` and is required to be `false` on every construction path (D15, CPP-4); `sync-operations` is forbidden from reading or writing the field at all (CPP-5); TEST-5 carries a **default-false regression test** specifically aimed at shape (a); and REV-20 item 5 makes any path that can disable verification without the immediate caller asking for it a **Must-fix**. Note honestly what none of these cover: whether the `CURLOPT_SSL_VERIFY*` calls are *actually* correct against a real TLS peer — that is unobservable through the fake and waits on §11(b).

---

## 11. Residual and deferred items

Neither item blocks Stage 9. Both must be carried into the master plan by the fold-in commit (§8 step 11) so the next actor inherits them from the authoritative document rather than from this one.

**(a) The response store — Stage 11's to design from scratch (OQ6).**
Stage 9 builds no store of any kind, and deliberately does not prefigure one. Stage 11 owns the entire design: shared state global within the DLL, per-module synchronization, ready flag cleared once read, request ID correlating a response to the call that produced it, one active response at a time (master plan §8, Stage 11).

Carried forward as an open sub-question Stage 11 must answer explicitly rather than discover: **do synchronous responses also need caching in that store once Stage 12/13's JSON accessors exist?** Stage 10 can export sync operations without a store, because the response body is copied straight into the caller's CAPL `char[]` (V3). But a Stage 12/13 accessor that reads "the last response" needs somewhere for that response to live. Nothing in Stage 9 constrains the answer; recording the question now is what stops it surfacing as a surprise at Stage 12.

**(b) Automated integration / E2E testing against a live containerized server — deferred, unscheduled, unplanned (OQ9).**
Stage 9 performs **no** live verification. The governing principle behind this deferral, recorded because it is broader than this stage: **integration and E2E tests must never be human-only — they must be automated, running both in local development and in CI.** A manually-run `DISABLED_` test and a hand-compiled console program were both rejected on those grounds, not on effort.

The intended mechanism is containerization: a self-hosted test HTTP/HTTPS server spun up in a Docker container and exercised by an automated integration suite in both local dev and CI, with no dependency on httpbin.org or any external network. That would cover what the fake structurally cannot — the real `CurlTransport`, real Schannel, the real static link, and D15's actual `CURLOPT_SSL_VERIFY*` wiring against both a valid-certificate and a self-signed peer (R11's uncovered layer).

Concretely noted so the size of this is not underestimated: **Docker would be a new toolchain dependency — nothing containerized exists in this repository today.** It touches local developer setup (`scripts/setup-dev-env.ps1`), CI (`.github/workflows/ci.yml`), and the "never a second build system" constraint in `msvc-build-conventions`. Likely owner of the tooling decision: **`build-pipeline-engineer`**, with `test-engineer` owning the suite itself. This needs its own dedicated planning pass and has **not** been scheduled or assigned a stage number.

**This supersedes the master plan's Stage 9 line** *"Verify as a standalone console program against httpbin.org"*, which is not followed and is deleted in the fold-in commit.

---

### Files referenced (absolute paths)

- `C:\Workspace\restifycapl\docs\work\capl-rest-dll-rebuild\plans\plan.md` (§8 lines 753–760; §5 lines 78–107; §12 ledger lines 820–918; §13 risks lines 941–996)
- `C:\Workspace\restifycapl\docs\work\stage-08-core-pure-logic\plans\plan.md`
- `C:\Workspace\restifycapl\docs\vector-capl-dll-docs\CAPLExportTable.htm.md` (V1, V2, V3)
- `C:\Workspace\restifycapl\docs\vector-capl-dll-docs\CAPLIncludeWindowsDLL.htm.md` (V1, high-priority thread)
- `C:\Workspace\restifycapl\docs\vector-capl-dll-docs\CAPLIncludeWindowsDLLExample.htm.md`, `CAPLDLLOverview.htm.md`, `CAPLIncludeWindowsDLLSearchSequence.htm.md`, `VModuleFormatCAPLDLL.htm.md`, `capl_dll_canoe_links.md`, `capl_dll_found_resources.md`
- `C:\Workspace\restifycapl\src\core\status.h`
- `C:\Workspace\restifycapl\src\module\exports.cpp`
- `C:\Workspace\restifycapl\Makefile` (LIBS/SYSLIBS lines 93–94 — already correct; CXXFLAGS line 87 and TEST_CXXFLAGS line 178 — the one change; INCLUDES line 83 and TEST_INCLUDES line 177 — already sufficient; TEST_CASE_SRCS line 167; TEST_LOGIC_SRCS lines 165–166)
- `C:\Workspace\restifycapl\.gitignore` (line 55 `include/vendor/gtest/` — the precedent; lines 36–42 — the comment needing correction)
- `C:\Workspace\restifycapl\scripts\setup-dev-env.ps1`
- `C:\Workspace\restifycapl\.github\workflows\ci.yml`
- `C:\Workspace\restifycapl\CHANGELOG.md`

---

**Plan is APPROVED and complete.**
</content>
</invoke>
