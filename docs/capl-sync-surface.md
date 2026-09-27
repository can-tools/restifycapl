# CAPL synchronous REST surface

Normative material for the six `CAPL_DLL_INFO4` rows that expose
`src/http/sync-operations.*` to CAPL, and for the translation layer in
`src/core/input-text.*` / `src/http/sync-text-api.*` that sits between the
raw CAPL parameters and that existing API. `src/module/exports.cpp` and its
`hintText`/trap-comment budgets point here rather than restating any of it.

## `categoryName` scheme

`CAPL_DLL_INFO4`'s `categoryName` groups exported operations for display in
CANoe's function browser. Three groups exist:

| Group | Meaning | Rows |
|---|---|---|
| `Common` | Non-HTTP utility operations. | `restifyReadVersion` |
| `Sync` | Synchronous (blocking) HTTP operations. | `restifyGetSync`, `restifyDeleteSync`, `restifyPostSync`, `restifyPutSync`, `restifyPatchSync`, `restifyRequestSync` |
| `Async` | Reserved for Stage 11's async exports. Nothing uses it yet. | none |

**Case-sensitivity trap:** `"Sync"` is a case-insensitive substring of
`"Async"` -- the last four letters of `Async` are `sync`. Anything that
filters or groups rows by `categoryName` must compare case-sensitively, or
a case-insensitive `"sync"` match will also catch every `Async` row.

## Signature table

`kRefLong = static_cast<char>('L' - 128)`, `kRefDword = static_cast<char>('D' - 128)`
(`type - 128` marks a by-reference CAPL parameter).

| Row | Signature | n |
|---|---|---|
| 2 | `long restifyGetSync(char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char responseBody[], dword responseBodySize, long& httpStatusCode, dword& responseBodyLength)` | 8 |
| 3 | `long restifyDeleteSync(...identical to row 2...)` | 8 |
| 4-6 | `long restifyPostSync(char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char requestBody[], dword requestBodySize, char responseBody[], dword responseBodySize, long& httpStatusCode, dword& responseBodyLength)` -- and `restifyPutSync`, `restifyPatchSync` identically | 10 |
| 7 | `long restifyRequestSync(char method[], dword methodSize, char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char requestBody[], dword requestBodySize, char responseBody[], dword responseBodySize, dword connectTimeoutMs, dword totalTimeoutMs, dword maxResponseBytes, long& httpStatusCode, dword& responseBodyLength)` | 15 |

`restifyRequestSync` has no `restifyHeadSync` counterpart -- `sync-operations`
has no `Head` helper, so HEAD is reached through `restifyRequestSync` with
`method = "HEAD"`.

The C++ translation layer (`src/http/sync-text-api.h`) mirrors this table
one-to-one: `ExecuteRequestSync` (row 7), `ExecuteGetSync`/`ExecuteDeleteSync`
(rows 2-3), `ExecutePostSync`/`ExecutePutSync`/`ExecutePatchSync` (rows 4-6).
Each takes the same `(text, size)` pairs, an injected `HttpClient&`, and
writes `httpStatusCode`/`responseBodyLength` by reference; `exports.cpp`'s
six shims are expected to be a single forwarding call into these.

## Header-block grammar

Request headers are one delimited text block, not a struct array (the CAPL
ABI cannot express a struct) and not parallel arrays (two lengths that could
disagree):

- Empty string -> no headers, `Ok`, output left empty.
- Otherwise split on `\n`; a trailing `\r` on a line is tolerated.
- Each line must be `Name: Value`, split on the **first** `:` only.
- No colon on a line -> `MalformedHeaderBlock`.
- Name and value are trimmed of surrounding whitespace.
- Empty name after trimming -> `MalformedHeaderBlock`.
- Empty value after trimming -> `MalformedHeaderBlock`. Rejected rather than
  passed through: `curl_slist_append("X-Foo:")` means *remove this header*
  in libcurl, the opposite of what an author writing an empty value means.
- Duplicate header names are preserved in order, never deduplicated.
- A block with a trailing `\n` produces an empty final line, which is
  itself subject to the same grammar and therefore also rejected as
  `MalformedHeaderBlock` -- there is no special-case tolerance for a
  trailing newline.

`ParseHeaderBlock` (`src/http/sync-text-api.h`) implements this exactly;
`http-client.cpp` is unmodified by any of it.

## Status codes `-4`, `-5`, `-6`

Full enum-wide rationale: `docs/status-codes.md`. Which of the six exports
can produce each:

| `Status` | Produced by | Condition |
|---|---|---|
| `MalformedHeaderBlock` (`-4`) | All six | The header-block parameter violates the grammar above. |
| `UnknownHttpMethod` (`-5`) | `restifyRequestSync` only | The method text does not case-insensitively match one of the six `HttpMethod` names (`Get`, `Post`, `Put`, `Patch`, `Delete`, `Head`). The five verb-specific rows fix their method in C++ and never parse method text at all. |
| `UnterminatedInputText` (`-6`) | All six | Any input `char[]` parameter has no NUL terminator within the caller-stated size. |

`InvalidArgument` (`-1`) keeps its pre-existing meaning (caller-side
programming error): a null pointer or zero-size input parameter, or a
non-empty body on a `Get`/`Head`/`Delete`-shaped request (caught by
`sync-operations::Request`'s existing check, not duplicated here).

## Write-ordering rule (normative)

`httpStatusCode` and `responseBodyLength` are written if and only if the
request actually reached `HttpClient::Perform` -- i.e. the `Status` returned
by the `sync-operations` call is anything other than `InvalidArgument`. When
that holds, both out-parameters are written **before** the response body is
copied into the caller's buffer, so a subsequent `CopyToBuffer` failure
(`BufferTooSmall`) never erases them. Concretely:

- HTTP 500 from the server -> transport `Status::Ok`, `httpStatusCode` 500,
  `responseBodyLength` set, buffer copy attempted normally.
- A response body that does not fit the caller's buffer -> `BufferTooSmall`
  returned, but `httpStatusCode` and `responseBodyLength` (the exact size
  needed) are already written -- the caller can size a bigger buffer and
  retry without losing the HTTP status.
- A DNS failure or other transport-level error (`-18..-23`) -> that code is
  returned, `httpStatusCode` is `0` (no response was ever received), and
  `responseBodyLength` is `0`.
- A pre-transport rejection (empty URL, or a body on a method that forbids
  one) -> `InvalidArgument`, and neither out-parameter is touched.

`responseBodyLength` is a byte count, not a character count -- relevant
once multi-byte/UTF-8 response bodies are involved.

## `elcount()`

Every input `char[]` parameter is paired with an explicit `dword` size,
placed immediately after it, whose value must come from `elcount(theArray)`
-- the array's real declared capacity, exactly the same meaning
`responseBodySize` already has. `BoundedText` (`src/core/input-text.h`)
scans for a NUL only within that stated bound: found -> the text is
everything before it; not found -> `UnterminatedInputText`.

Every input argument must be a declared `char[]` variable; pass
`elcount()` of that variable as its size. String literals cannot be passed
as `char[]` arguments in CAPL -- declare and fill a variable first. To omit
an optional text parameter (e.g. "no headers", or "no body" on
`restifyRequestSync`), declare a zero-length-content array, e.g.
`char noHeaders[1]; noHeaders[0] = 0;` or `char noHeaders[1] = "";`, and
pass `elcount(noHeaders)` as usual -- not a hand-written literal.

## Binary response bodies are out of scope

`CopyToBuffer` copies the full response body byte-for-byte and
`responseBodyLength` reports its exact size, but a CAPL `char[]` is read by
CAPL as a NUL-terminated C string -- any embedded NUL byte truncates what
the script actually sees, even though the DLL copied everything. This
surface is text/JSON-oriented by design; no guard rejects a binary body,
because doing so would require buffering and inspecting the whole response
before any of this API's normal error paths run, for a case client code can
already detect itself by comparing `responseBodyLength` against
`strlen`-style consumption. Not planned to be closed here -- a caller that
needs true binary transport is out of this surface's scope.
