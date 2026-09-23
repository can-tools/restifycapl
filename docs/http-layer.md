# HTTP layer

Archival rationale for `src/http/http-client.{h,cpp}` (and `sync-operations.*`
once it lands) -- the libcurl-backed transport behind `HttpClient` and the
`HttpTransport` seam it is injected with.

## Why the seam is a pure-virtual interface (Stage 9, CPP-4)

`HttpTransport` has exactly one method, injected into `HttpClient` by
constructor. Three alternatives were considered and rejected:

- **A template policy** would force `HttpClient` to be header-only, dragging
  `curl.h` into a header and breaking the rule that curl only ever appears
  in `http-client.cpp`.
- **`std::function`** works as a seam but gives it no natural place to own
  the `curl_global_init` call-once state or the option policy below.
- **Link-time substitution** (a fake `libcurl.lib` swapped in for tests) is
  not available here: the test executable links the real libcurl, so a
  fake transport has to be an injected object, not a different library.

Virtual dispatch costs nanoseconds against a network round-trip, so the
cost of the indirection is immaterial next to what it buys.

## `CURLcode` -> `Status` mapping (Stage 9, CPP-4)

| `CURLcode` | `Status` |
|---|---|
| `CURLE_OK` | `Ok` |
| `CURLE_OPERATION_TIMEDOUT` | `Timeout` |
| `CURLE_URL_MALFORMAT`, `CURLE_UNSUPPORTED_PROTOCOL` | `InvalidUrl` |
| `CURLE_SSL_CONNECT_ERROR`, `CURLE_PEER_FAILED_VERIFICATION`, `CURLE_SSL_CIPHER`, `CURLE_SSL_CACERT_BADFILE`, `CURLE_SSL_ISSUER_ERROR`, `CURLE_USE_SSL_FAILED`, any other `CURLE_SSL_*` | `TlsError` |
| `CURLE_COULDNT_RESOLVE_HOST`, `CURLE_COULDNT_RESOLVE_PROXY`, `CURLE_COULDNT_CONNECT`, `CURLE_SEND_ERROR`, `CURLE_RECV_ERROR`, `CURLE_GOT_NOTHING`, `CURLE_PARTIAL_FILE`, `CURLE_TOO_MANY_REDIRECTS` | `NetworkError` |
| any other `CURLcode` | `NetworkError` (documented catch-all) |

The size-cap check lives in the write callback (`WriteCallback` in
`http-client.cpp`), which signals a cap breach to libcurl by returning a
short byte count. libcurl reports that as the generic `CURLE_WRITE_ERROR`,
indistinguishable at the mapping layer from any other write failure. The
callback's own context flag therefore takes precedence over this table: if
it recorded a cap breach, the result is `ResponseTooLarge` regardless of
the `CURLcode` libcurl reports. The mapping function carries this as a
named trap in the source; the table above is the reference it points at.

## Fixed libcurl option policy (Stage 9, CPP-4)

| Option | Value | Why it is not negotiable |
|---|---|---|
| `CURLOPT_NOSIGNAL` | `1L` | Required for use from a non-main thread. The async layer (a later stage) will call `Perform` from exactly that context; set now so the seam is thread-safe from birth, not retrofitted. |
| `CURLOPT_SSL_VERIFYPEER` | conditional -- see below | Per-request TLS toggle. |
| `CURLOPT_SSL_VERIFYHOST` | conditional -- see below | Per-request TLS toggle. |
| `CURLOPT_CONNECTTIMEOUT_MS` | from `RequestOptions` | See Timeout policy below. |
| `CURLOPT_TIMEOUT_MS` | from `RequestOptions` | See Timeout policy below. |
| `CURLOPT_FOLLOWLOCATION` | `1L` | Redirects are followed by default. |
| `CURLOPT_MAXREDIRS` | `5L` | Bounded. Exceeding it maps to `NetworkError` via `CURLE_TOO_MANY_REDIRECTS`. |
| `CURLOPT_ACCEPT_ENCODING` | `""` | Enables gzip/deflate transparently. This is the entire reason `zs.lib` (zlib) is in the dependency set at all -- without it zlib is linked and unused. |
| `CURLOPT_USERAGENT` | `"restifycapl"` | No version number embedded. The project's version is never hand-typed anywhere, and the DLL's real version is only reachable from `src/module/` (the Win32 version resource) -- embedding it here would either hardcode a stale literal or drag a level-4 dependency into level 1. Callers may override via a `User-Agent` request header. |

### TLS verification -- the one per-request-conditional option

| `request.options.skipTlsVerification` | `CURLOPT_SSL_VERIFYPEER` | `CURLOPT_SSL_VERIFYHOST` |
|---|---|---|
| `false` -- the default | `1L` | `2L` |
| `true` -- opt-in, per request | `0L` | `0L` |

Both options are set explicitly on every request, in both branches. Never
left at libcurl's implicit default, and never set independently of each
other -- see the `skipTlsVerification` rationale below for why the field
exists and why its default is safe.

### Proxy behavior

libcurl's default honors the `http_proxy`/`https_proxy`/`no_proxy`
environment variables and is left untouched. A CANoe machine sitting behind
a corporate proxy would otherwise produce mystifying `NetworkError`s with
no way to route around it; silently disabling proxy support would produce
the mirror-image mystery for anyone who *does* rely on it.

## Timeout policy (Stage 9, CPP-4)

Two independent timeouts, both `std::uint32_t` milliseconds on
`RequestOptions`: **connect** (default 5000 ms) and **total** (default
30000 ms). `0` means "use the default" -- there is intentionally no way to
express "no timeout" at all. An unbounded blocking call inside CANoe's
process, on a *high-priority* thread when called from the Simulation Setup
realtime branch, can stall a measurement indefinitely with nothing the CAPL
script can do to recover. The absence of an infinite option is a deliberate
safety property of this API, not an oversight to "fix" later.

A total timeout set below the connect timeout is not treated as an error;
libcurl enforces both independently and the total simply wins in that case.
This is documented rather than validated against, since rejecting it would
add a check for a combination that already behaves predictably.

## Handle lifecycle and threading (Stage 9, CPP-4)

1. `curl_global_init(CURL_GLOBAL_DEFAULT)` runs exactly once, lazily, via
   `std::call_once`, triggered the first time a `CurlTransport` performs a
   request. It is never called from `DllMain` -- doing so runs it under the
   Windows loader lock, a standard way to deadlock a host process at load
   time.
2. `curl_global_cleanup()` is never called. This is a deliberate,
   documented leak: calling it at `DLL_PROCESS_DETACH` means running it
   under the loader lock while other threads may still be in flight, and
   CANoe can load and unload this DLL across measurements. The leak is
   one-time and process-scoped.
3. If `curl_global_init` fails, every subsequent `Perform` call returns
   `TransportInitFailed`. It is not retried.
4. One `CURL*` easy handle is created and destroyed inside every
   `Perform` call. No sharing, no reuse, no `curl_share` handle. The only
   mutable process-wide state is the `call_once` flag, which is what makes
   `Perform` safe to call concurrently from any thread -- the property the
   async layer (a later stage) depends on, at the cost of a fresh
   connection per request. Connection reuse is a deliberate, accepted
   trade against that property, not deferred by oversight.

## `skipTlsVerification` -- why it exists, and its bounds (Stage 9, CPP-4)

`RequestOptions::skipTlsVerification` is a `bool` defaulting to `false`.

- **Why it exists at all:** the capability is wanted for future scenarios
  -- notably testing against a self-signed HTTPS dev server -- and adding
  it later would mean reshaping `RequestOptions` after the async layer has
  already built on its current shape.
- **Why it is not needed today, stated so nobody assumes otherwise:** the
  project's local FastAPI/uvicorn proxy currently serves plain HTTP
  (uvicorn defaults to no TLS), and external servers used so far have
  valid certificates. There is no current caller that sets it to `true`.
- **Why the default is `false`:** a skip-by-default would make every
  request in the project silently unauthenticated against its peer, and
  the failure is invisible -- everything appears to work, including
  against a hostile peer. Safe-by-default was an explicit requirement.
- **Bounds on the mechanism:** it is a per-request option only. There is no
  global switch, no environment variable, no build-time define, and no
  code path that derives it from anything else. `sync-operations` forwards
  it unmodified and contains no logic that could set it.

## What cannot be verified without a live server (Stage 9, CPP-21)

Everything below is exercised only through `FakeTransport` in `tests/http/`,
or through the pure predicates extracted from `CurlTransport::Perform` and
`WriteCallback` (`WouldExceedResponseCap`, `ResolveTransferResult` in
`http-client.cpp`). None of it has been driven by a real `curl_easy_perform`
call over an actual network, and nothing in this repository does that today
-- see the deferred containerized-integration-testing initiative (OQ9).

- **Every row of the `CURLcode` -> `Status` mapping** (`MapCurlCode`,
  above). A unit test of the switch only proves the switch contains what it
  contains; the claim worth testing is that libcurl actually reports each
  of these codes for the matching real-world condition -- timeout,
  malformed/unsupported-scheme URL, DNS failure, connection refused,
  redirect limit -- and only a live transfer establishes that.
- **libcurl's actual error reporting under Schannel specifically.** The TLS
  branch of the mapping table assumes Schannel surfaces the `CURLE_SSL_*`
  family the way this table expects; no self-signed, expired, or
  wrong-host certificate has been driven through a real Schannel handshake
  to confirm it.
- **The real `CURLOPT_SSL_VERIFYPEER`/`VERIFYHOST` wiring.** The values set
  per `skipTlsVerification` (above) are asserted only as the literal
  arguments passed to `curl_easy_setopt`; whether they actually cause
  libcurl/Schannel to accept or reject a given peer certificate is
  unobservable through `FakeTransport`.
- **`ResponseTooLarge` tripping through libcurl's write path.**
  `WouldExceedResponseCap` is directly unit-tested, including the
  unsigned-wraparound boundary, but that only proves the arithmetic; it
  does not prove that `WriteCallback` returning a short byte count actually
  makes `curl_easy_perform` abort the transfer the way this file assumes.
- **`Status::Ok` with each HTTP status class (2xx, 4xx, 5xx) from a real
  server**, confirming transport success and HTTP status stay on separate
  channels.

## Blocking calls are prohibited in CANoe's realtime branch

Vector's own documentation
(`docs/vector-capl-dll-docs/CAPLExportTable.htm.md`, "Note on CAPL DLLs"):

> If functions of this DLL are called in the realtime branch (Simulation
> Setup) the following must be observed: File accesses and other blocking
> calls are prohibited! Dynamic memory management is not recommended. This
> means no `new`, `new[]`, `delete`, `delete[]`, `malloc`, `free`, etc.
> Instead it is better to reserve memory statically before the measurement
> start.

A synchronous HTTP call is precisely a blocking call, and libcurl allocates
freely on every request. Every synchronous operation built on this module
is therefore unsafe in the Simulation Setup realtime branch by Vector's own
rule -- it is safe only from Measurement Setup / test nodes. The async
layer (a later stage) exists specifically because of this constraint, not
merely as a nice-to-have alternative.
