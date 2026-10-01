# Status codes

Archival rationale for `src/core/status.h`'s `Status` enum -- the single
shared error/result type used across `src/core/` and consumed from
`src/module/exports.cpp`.

## Why one shared enum, owned by its own file

`json-path.h`, `type-conversion.h` and `buffer-copy.h` all need the same
result type. Hosting it inside any one of those headers would create an
arbitrary include dependency between otherwise-independent modules, so it
lives in its own file instead, included by all three (and by
`src/module/exports.cpp`).

## Why 0/-1/-2/-3 are absorbed, not renumbered

`Ok = 0`, `InvalidArgument = -1`, `BufferTooSmall = -2` and
`VersionResourceUnavailable = -3` are pre-existing, already-shipped values
from `restifyReadVersion` (`src/module/exports.cpp`), predating this enum.
They were absorbed into `Status` rather than given new numbers, because
CAPL scripts already depend on these exact values at runtime
(`capl-export-contract`) -- renumbering any of them would silently break
every script built against the DLL's first exported operation.

`VersionResourceUnavailable` is declared here so the numbering space has a
single owner, even though it is unreachable from anything in `src/core/`
itself -- only `src/module/exports.cpp` can produce it (the Win32
version-resource read that yields it has no place in CANoe-unaware code).

## Reserved ranges

- `-7..-9`: future module-local glue codes, currently empty.
- `-30..-39`: the mapping layer (`src/mapping/`). `-30..-34` are assigned;
  `-35..-39` are free.

Each range has exactly one owning layer, so a future addition never has to
guess where the next free number is.

## `-4..-6`

| `Status` | Meaning |
|---|---|
| `MalformedHeaderBlock` | The header block text violates the `Name: Value` grammar -- a line with no colon, an empty name, or an empty value. |
| `UnknownHttpMethod` | The method text did not match any of the six known HTTP verbs. |
| `UnterminatedInputText` | No NUL terminator was found within the caller-stated size bound for an input `char[]` parameter. |

## `-18..-23`

| `Status` | Meaning |
|---|---|
| `NetworkError` | The transfer failed (DNS, connect, send/recv, redirect limit) -- also the catch-all for any unmapped `CURLcode`. |
| `Timeout` | Connect or total timeout expired -- libcurl cannot distinguish the two, so one code covers both. |
| `TlsError` | TLS/Schannel handshake or certificate verification failed, kept distinct from `NetworkError`. |
| `TransportInitFailed` | `curl_global_init` or `curl_easy_init` failed -- process-level, not request-level. |
| `InvalidUrl` | The URL was malformed or used an unsupported scheme, kept distinct from `InvalidArgument` (-1). |
| `ResponseTooLarge` | The response body exceeded the configured cap. |

## `-24..-29`

| `Status` | Meaning |
|---|---|
| `RequestCancelled` | `CURLE_ABORTED_BY_CALLBACK` -- internal only, CAPL never observes this value. |
| `NoFreeRequestSlot` | Dispatch found all 8 async request slots occupied. |
| `RequestNotComplete` | A read was attempted on a live id still `Pending`/`Running`. |
| `UnknownRequestId` | The id was 0, never issued, or already `Consumed`, `Abandoned`, or `Free`. |
| `WaitTimeout` | `await`'s deadline was reached, kept distinct from `Timeout` (-19), which is libcurl's own transfer timeout. |
| `AsyncStartFailed` | Worker thread creation failed. |

## `-30..-34`

| `Status` | Meaning |
|---|---|
| `NoFreeDocumentSlot` | Parsing found no free document slot. |
| `UnknownDocumentId` | The document id is unknown or the document was already discarded. |
| `DocumentTooLarge` | The JSON input is larger than 1 MiB. |
| `NestingTooDeep` | The JSON nesting is deeper than 64 levels. |
| `TooManyEntries` | The document flattens to more than 10,000 entries. |

`ParseError` (`-10`) covers invalid JSON, empty text and invalid UTF-8.
