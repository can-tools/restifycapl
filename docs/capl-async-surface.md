# CAPL asynchronous REST surface

Normative material for the eleven `CAPL_DLL_INFO4` rows that expose
`src/http/async-operations.*` (`AsyncEngine`) to CAPL, and for the
translation layer in `src/http/async-text-api.*` that sits between the raw
CAPL parameters and that engine. `src/module/exports.cpp` and its
`hintText`/trap-comment budgets point here rather than restating any of it.
Header-block grammar, `elcount()` usage and the binary-body caveat are
shared with the sync surface and are not repeated here -- see
`docs/capl-sync-surface.md`.

## `categoryName` scheme

`docs/capl-sync-surface.md` names three `categoryName` groups. The `Async`
group, previously reserved and empty, now holds rows 8-18:

| Group | Meaning | Rows |
|---|---|---|
| `Common` | Non-HTTP utility operations. | `restifyReadVersion` |
| `Sync` | Synchronous (blocking) HTTP operations. | rows 2-7 |
| `Async` | Asynchronous (non-blocking dispatch) HTTP operations and their lifecycle management. | rows 8-18 |

The case-sensitivity trap documented in `docs/capl-sync-surface.md`
(`"Sync"` is a case-insensitive substring of `"Async"`) applies here
unchanged: any code that filters or groups rows by `categoryName` must
compare case-sensitively.

## Signature table

`kRefLong = static_cast<char>('L' - 128)`, `kRefDword = static_cast<char>('D' - 128)`
(`type - 128` marks a by-reference CAPL parameter).

| Row | Signature | n |
|---|---|---|
| 8 | `long restifyGetAsync(char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, dword& requestId)` | 5 |
| 9 | `long restifyDeleteAsync(...identical to row 8...)` | 5 |
| 10 | `long restifyPostAsync(char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char requestBody[], dword requestBodySize, dword& requestId)` | 7 |
| 11 | `long restifyPutAsync(...identical to row 10...)` | 7 |
| 12 | `long restifyPatchAsync(...identical to row 10...)` | 7 |
| 13 | `long restifyRequestAsync(char method[], dword methodSize, char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char requestBody[], dword requestBodySize, dword connectTimeoutMs, dword totalTimeoutMs, dword maxResponseBytes, dword& requestId)` | 12 |
| 14 | `long restifyPollResponse(dword requestId, long& state)` | 2 |
| 15 | `long restifyAwaitResponse(dword requestId, dword waitTimeoutMs)` | 2 |
| 16 | `long restifyReadResponse(dword requestId, char responseBody[], dword responseBodySize, long& requestStatus, long& httpStatusCode, dword& responseBodyLength)` | 6 |
| 17 | `long restifyDiscardResponse(dword requestId)` | 1 |
| 18 | `long restifyDiscardAllResponses(dword& stillRunning)` | 1 |

As with the sync surface, HEAD is reachable only through row 13
(`restifyRequestAsync` with `method = "HEAD"`); there is no
`restifyHeadAsync`.

The C++ translation layer (`src/http/async-text-api.h`) mirrors this table
one-to-one: `DispatchGetAsync`/`DispatchDeleteAsync` (rows 8-9),
`DispatchPostAsync`/`DispatchPutAsync`/`DispatchPatchAsync` (rows 10-12),
`DispatchRequestAsync` (row 13), `PollAsyncResponse` (row 14),
`AwaitAsyncResponse` (row 15), `ReadAsyncResponse` (row 16),
`DiscardAsyncResponse` (row 17), `DiscardAllAsyncResponses` (row 18). Each
dispatch function takes the same `(text, size)` pairs as its sync
counterpart plus an injected `HttpClient&` and `AsyncEngine&`, and writes
`requestId` by reference; the five lifecycle functions take an
`AsyncEngine&` and are a single forwarding call into the matching
`AsyncEngine` method. `exports.cpp`'s eleven shims are expected to be a
single forwarding call into these.

## Dispatch validation (normative)

Every dispatch function (rows 8-13) runs the following checks, in order,
before ever calling `AsyncEngine::Dispatch`. `AsyncEngine::Dispatch` itself
performs no text-level validation -- it assumes a fully-built, valid
`HttpRequest`.

1. `requestId` is set to `0` on entry, before any validation runs.
2. Every input `char[]` parameter is bounds-checked with `BoundedText`
   (`UnterminatedInputText`, `-6`; null pointer or zero size,
   `InvalidArgument`, `-1`) -- in parameter order: method text (row 13
   only), url, headers, body (rows 10-13 only).
3. Row 13 only: the method text is parsed with `ParseMethodText`
   (`UnknownHttpMethod`, `-5`, if it matches none of the six known verbs).
4. The header block is parsed with `ParseHeaderBlock`, the same grammar
   documented in `docs/capl-sync-surface.md` (`MalformedHeaderBlock`,
   `-4`).
5. An empty url is rejected (`InvalidArgument`, `-1`). Unlike the sync
   surface, this check cannot be left to `HttpClient::Perform`, because
   dispatch never calls `Perform` synchronously -- the translation layer
   checks it directly.
6. Row 13 only: a non-empty body combined with a resolved method of `Get`
   or `Delete` is rejected (`InvalidArgument`, `-1`), mirroring
   `sync-operations`'s own forbidden-body rule. Rows 8-9
   (`restifyGetAsync`/`restifyDeleteAsync`) have no body parameter at all,
   so this case cannot arise there.

Only once every check above passes is an `HttpRequest` built and handed to
`AsyncEngine::Dispatch`, which then applies D7's slot-table rules
(`NoFreeRequestSlot`, `-25`; `AsyncStartFailed`, `-29`) and writes
`requestId` only on success. A non-zero `requestId` means the call
returned `Ok`, and nothing else.

## Status codes produced by the async surface

| `Status` | Produced by | Condition |
|---|---|---|
| `InvalidArgument` (`-1`) | Rows 8-13 | Null/zero-size input, empty url, or (row 13 only) a body on a resolved `Get`/`Delete`. |
| `MalformedHeaderBlock` (`-4`) | Rows 8-13 | The header-block parameter violates the grammar. |
| `UnknownHttpMethod` (`-5`) | Row 13 only | The method text matches none of the six `HttpMethod` names. |
| `UnterminatedInputText` (`-6`) | Rows 8-13 | An input `char[]` parameter has no NUL terminator within the caller-stated size. |
| `NoFreeRequestSlot` (`-25`) | Rows 8-13 | All 8 slots are occupied at dispatch time. |
| `RequestNotComplete` (`-26`) | Row 16 | The id is live but still `Pending`/`Running`. |
| `UnknownRequestId` (`-27`) | Rows 14-17 | The id is `0`, was never issued, or the slot is `Consumed`/`Abandoned`/`Free`. |
| `WaitTimeout` (`-28`) | Row 15 | `restifyAwaitResponse`'s deadline was reached. |
| `AsyncStartFailed` (`-29`) | Rows 8-13 | A worker thread could not be created. |

`RequestCancelled` (`-24`) is internal to `AsyncEngine`/`HttpClient` and is
never returned to CAPL -- a cancelled request never reaches `Complete` (see
`docs/status-codes.md`). Once a request is `Complete`, transport-level
failures (`-18..-23`) surface through `requestStatus` at read time, not
through the dispatch or lifecycle return codes above.

## Poll (`restifyPollResponse`, row 14)

Never blocks, allocates, or creates or wakes a worker.

1. `state` is set to `0` on entry.
2. Id matches `Pending` or `Running`: return `0`, `state = 1`.
3. Id matches `Complete`: return `0`, `state = 2`. "Complete" covers a
   failed transport outcome and a response still waiting for a large-enough
   read buffer, not only a successful HTTP response.
4. Otherwise: return `-27`.

## Await (`restifyAwaitResponse`, row 15)

Blocks the calling thread. Not live: `-27`. Already `Complete`: `0`
immediately. Otherwise it waits until the id becomes `Complete`, is
invalidated (discarded, discard-all, or its slot reused under a new id --
`-27`, promptly), or its deadline passes (`-28`). `waitTimeoutMs = 0` means
the request's own resolved `totalTimeoutMs`, measured from the call.

## Read (`restifyReadResponse`, row 16) write order

1. `requestStatus`, `httpStatusCode` and `responseBodyLength` are zeroed on
   entry.
2. Id not live: `-27`. Id live but still `Pending`/`Running`: `-26`.
3. Id `Complete`: all three out-parameters are written **before** the copy
   into the caller's buffer is attempted, so a too-small buffer never
   erases them.
4. The copy follows `CopyToBuffer`'s rules exactly -- it never truncates:
   - null buffer or zero size: `-1`, nothing written to the buffer;
   - buffer too small: `-2`, the buffer is left empty, the slot stays
     `Complete` so a retry with a bigger buffer (at least
     `responseBodyLength + 1`) can succeed;
   - fits: `0`, and the slot moves to `Consumed` -- a second read of the
     same id then returns `-27`.

`requestStatus` can only be `0` or a code in `-18..-23`; an HTTP 4xx/5xx
response is `requestStatus == 0` with the code in `httpStatusCode`, exactly
as for the sync surface. Binary response bodies are out of scope for the
same reason given in `docs/capl-sync-surface.md`.

## Single discard (`restifyDiscardResponse`, row 17)

Id matches a live slot: returns `0`. A `Pending` or `Complete` slot is
freed immediately (`Consumed`); a `Running` slot is marked `Abandoned` and
its cooperative cancel flag is set -- its worker drops the result once
`Perform` returns and frees the slot itself. Otherwise: `-27`. Realtime-safe:
it never allocates or frees on the caller's thread.

## Discard-all (`restifyDiscardAllResponses`, row 18)

Applies single-discard's rule to every slot at once, reports the number of
slots that were `Running` (and are now `Abandoned`) in `stillRunning`, wakes
idle workers so they can exit, and returns `0`. **It frees memory on the
caller's thread** -- unlike every other row in this table, it is not
realtime-safe. Call it from `on stopMeasurement` or from a test node, not
from a Simulation Setup node.

## Realtime-safety summary

| Row | Safe from Simulation Setup | Notes |
|---|---|---|
| 8-13 (dispatch) | Yes | Returns immediately with a `requestId`; the request itself runs on a worker thread. |
| 14 (`restifyPollResponse`) | Yes | Never blocks or touches the heap. |
| 16 (`restifyReadResponse`) | Yes | Bounded copy under the table lock; never blocks. |
| 17 (`restifyDiscardResponse`) | Yes | Never blocks or touches the heap. |
| 15 (`restifyAwaitResponse`) | **No** | Blocks the calling thread until completion, timeout, or invalidation -- Measurement Setup or a test node only, same caveat as any blocking sync call. |
| 18 (`restifyDiscardAllResponses`) | **No** | Frees memory on the caller's thread -- Measurement Setup or a test node only. |

This split is the reason the async surface exists at all: rows 8-13, 14,
16 and 17 are the conforming way to reach REST from CANoe's realtime
branch, where a blocking call is not acceptable.

## `requestId` lifetime and redeploy

A `requestId` is only meaningful for the DLL instance that issued it. A
CAPL script must not persist a `requestId` across a DLL reload (e.g. a
rebuild-and-restart-measurement cycle): the id counter reseeds from a
high-resolution clock (`QueryPerformanceCounter`) on the first dispatch
after each load, so a stale id kept from a previous load could
coincidentally collide with a live id in the new load -- astronomically
unlikely given the seeding, but not impossible, and not a case any lifecycle
row can detect from the id's value alone.

## Single shared slot pool

All 8 request slots are one pool for the whole DLL instance, not
partitioned per CAPL node or script. Vector's own documentation allows
several CAPL nodes to share one loaded DLL; if they do, they all draw from
the same 8 slots and can starve each other under load. There is no
per-script or per-node reservation, and none is planned.

## Across a measurement stop/restart

In-flight or unread async requests do not survive a measurement
stop/restart within the same DLL load in any way a CAPL script can rely on.
`restifyDiscardAllResponses` exists specifically to be called from
`on stopMeasurement`, so a new measurement never inherits stale slots from
the one before it.

## Transport, TLS and outbound network

Async requests run over the same libcurl/Schannel transport as sync
requests, on a worker thread rather than the caller's thread -- same TLS
verification defaults, same absence of a CAPL-reachable
`skipTlsVerification`, same outbound-network prerequisite (a reachable
server, proxy environment variables honored). See `docs/http-layer.md` for
the full transport contract; nothing about it changes for the async surface
beyond which thread calls `Perform`.
