# CAPL JSON surface

Normative material for the six `CAPL_DLL_INFO4` rows that expose parsed JSON
documents to CAPL, and for the translation layer in
`src/mapping/json-text-api.*` and the document store in
`src/mapping/json-document-store.*` behind them. `src/module/exports.cpp`
points here rather than restating any of it. Key format, entry order, number
text and the limits are defined in `docs/json-flatten.md`; path syntax is
defined in `docs/json-path.md`. `char[]` + `elcount()` usage is shared with
the sync surface and not repeated here -- see `docs/capl-sync-surface.md`.

A script parses JSON text once, receives a `documentId`, and then reads the
flattened entries by index or single values by path. Nothing is parsed again
on a read.

## `categoryName`

The six rows form a fourth group:

| Group | Meaning | Rows |
|---|---|---|
| `Json` | Parsed JSON documents held by the DLL. | rows 19-24 |

## Signature table

| Row | Signature | n |
|---|---|---|
| 19 | `long restifyJsonParse(char json[], dword jsonSize, dword& documentId)` | 3 |
| 20 | `long restifyJsonCountEntries(dword documentId, dword& entryCount)` | 2 |
| 21 | `long restifyJsonReadEntry(dword documentId, dword entryIndex, char key[], dword keySize, char value[], dword valueSize, long& valueType)` | 7 |
| 22 | `long restifyJsonReadValue(dword documentId, char path[], dword pathSize, char value[], dword valueSize, long& valueType)` | 6 |
| 23 | `long restifyJsonDiscardDocument(dword documentId)` | 1 |
| 24 | `long restifyJsonDiscardAllDocuments(dword& discardedCount)` | 1 |

The C++ translation layer (`src/mapping/json-text-api.h`) mirrors this table
one-to-one: `ParseJsonDocument` (row 19), `CountJsonEntries` (row 20),
`ReadJsonEntry` (row 21), `ReadJsonValue` (row 22), `DiscardJsonDocument`
(row 23), `DiscardAllJsonDocuments` (row 24). Each takes an injected
`JsonDocumentStore&` first, then the CAPL parameters as pointer+size pairs.
`exports.cpp`'s shims are expected to be a single forwarding call into these.

## Documents and `documentId`

- A `documentId` is a nonzero `dword`. `0` means "no document".
- The DLL holds at most **8** documents at once, in one pool shared by every
  CAPL node and script that uses the DLL instance. There is no per-script
  reservation.
- Ids count up from a clock-derived start taken at the first parse after the
  DLL loads. A discarded id is rejected with `-31`; it is not handed out again
  until the 32-bit counter has wrapped. An id is only meaningful for the DLL
  instance that issued it: do not keep one across a DLL reload.
- A document never changes after `Parse`. Entry indices and entry order are
  fixed for its whole lifetime, so a read that failed with `-2` can be
  repeated.
- Documents survive until discarded; a measurement stop does not free them.
  Call `restifyJsonDiscardAllDocuments` from `on stopMeasurement` so a new
  measurement does not inherit documents from the one before it.

## Input text

Every `char[]` input is followed by a `dword` size. The text is the bytes up
to the first NUL within that size.

| Input | Status |
|---|---|
| null pointer or size `0` | `InvalidArgument` (`-1`) |
| no NUL found within `size` bytes | `UnterminatedInputText` (`-6`) |

For `Parse`, the size limit of 1 MiB counts the bytes before the NUL.

## Out-parameters

`documentId`, `entryCount`, `valueType` and `discardedCount` are set to `0`
on entry and are overwritten **only** when the operation returns `0`. On any
other status they read `0`, so a script can never mistake a leftover value for
a result. The text buffers (`key`, `value`) follow the rules in the next
section.

## Reading text into buffers, and `-2`

Text buffers follow `CopyToBuffer`: text is never truncated, and the buffer
always holds a complete, NUL-terminated text or an empty one. The caller does
not learn the needed length from the call, so retry with a larger buffer
(doubling is enough; at most one more than the longest text is needed).

| Case | Result |
|---|---|
| buffer pointer null or size `0` | `-1`, nothing is written |
| text plus its NUL does not fit | `-2`, the buffer is set to the empty text; no truncated text is ever written |
| text fits | `0`, the text is copied and NUL-terminated |

`restifyJsonReadEntry` has two buffers. They are handled together:

- If either buffer pointer is null or either size is `0`: `-1`, **neither**
  buffer is written.
- If the key or the value does not fit: `-2`, **both** buffers are set to the
  empty text (also the one that would have fitted) and `valueType` stays `0`.
  Retry with larger buffers for both.
- Otherwise: `0`, both buffers hold their NUL-terminated text.

On every other status (`-31`, `-13`, and in `restifyJsonReadValue` the path
statuses) the buffers are not touched.

## `valueType`

`valueType` is a `long` returned by `restifyJsonReadEntry` and
`restifyJsonReadValue`. The numbers are part of the contract and are never
renumbered.

| Value | Name | Value text |
|---|---|---|
| `0` | None | no value (what the out-parameter holds on any status other than `0`) |
| `1` | String | the string's contents, unquoted and unescaped |
| `2` | Number | the number text, see `docs/json-flatten.md` |
| `3` | Bool | `true` or `false` |
| `4` | Null | `null` |
| `5` | EmptyObject | `{}` |
| `6` | EmptyArray | `[]` |

An empty string is `valueType` `1` with empty value text; it is not `null`.

## `restifyJsonParse` (row 19)

Parses `json`, flattens it and stores it in a free slot.

1. `documentId` is set to `0` on entry.
2. The text is checked as in "Input text": `-1`, `-6`.
3. The text is parsed and flattened (`docs/json-flatten.md`). Failures:
   `-10` (invalid JSON, empty text, invalid UTF-8), `-32`, `-33`, `-34` (see
   "Limits").
4. Only a text that would otherwise succeed is checked against the slot pool:
   all 8 slots occupied gives `-30`. A malformed text therefore reports its own
   error even when the pool is full.
5. On success `documentId` is set to the new nonzero id and `0` is returned.

| `Status` | Condition |
|---|---|
| `Ok` (`0`) | Document stored; `documentId` written. |
| `InvalidArgument` (`-1`) | `json` null or `jsonSize` `0`. |
| `UnterminatedInputText` (`-6`) | No NUL within `jsonSize`. |
| `ParseError` (`-10`) | Invalid JSON, empty text, or invalid UTF-8. |
| `NoFreeDocumentSlot` (`-30`) | All 8 slots are occupied. |
| `DocumentTooLarge` (`-32`) | Input larger than 1 MiB. |
| `NestingTooDeep` (`-33`) | Nesting deeper than 64. |
| `TooManyEntries` (`-34`) | More than 10,000 entries. |
| `InternalError` (`-35`) | Unexpected failure inside the DLL, see "Unexpected internal failure". |

## `restifyJsonCountEntries` (row 20)

`entryCount` is set to `0` on entry; on `0` it is the number of entries of the
document. Valid `entryIndex` values for `restifyJsonReadEntry` are
`0 .. entryCount - 1`. A document always has at least one entry.

| `Status` | Condition |
|---|---|
| `Ok` (`0`) | `entryCount` written. |
| `UnknownDocumentId` (`-31`) | The id is `0`, was never issued, or was discarded. |
| `InternalError` (`-35`) | Unexpected failure inside the DLL, see "Unexpected internal failure". |

## `restifyJsonReadEntry` (row 21)

Copies the key and the value text of entry number `entryIndex`, and its type.
Entries are listed in document order; the key is the JSON Pointer of the leaf
(`docs/json-flatten.md`).

Checks run in this order, and the first failure decides the status:

1. `valueType` is set to `0` on entry.
2. Unknown id: `-31`.
3. `entryIndex >= entryCount`: `-13`.
4. Buffers: `-1` or `-2` as in "Reading text into buffers, and `-2`".
5. Otherwise `0`: both buffers are written, `valueType` is set.

| `Status` | Condition |
|---|---|
| `Ok` (`0`) | Key, value and `valueType` written. |
| `InvalidArgument` (`-1`) | A buffer pointer is null or a buffer size is `0`. |
| `BufferTooSmall` (`-2`) | The key or the value does not fit; both buffers set to the empty text. |
| `IndexOutOfRange` (`-13`) | `entryIndex` is not below `entryCount`. |
| `UnknownDocumentId` (`-31`) | The id is `0`, was never issued, or was discarded. |
| `InternalError` (`-35`) | Unexpected failure inside the DLL, see "Unexpected internal failure". |

## `restifyJsonReadValue` (row 22)

Reads one value by path. `path` is a JSON Pointer (`docs/json-path.md`); every
key listed by `restifyJsonReadEntry` resolves to its own value here, and `""`
(the empty text) addresses the whole document.

Checks run in this order, and the first failure decides the status:

1. `valueType` is set to `0` on entry.
2. The path text is checked: `-1` (null pointer or size `0`), `-6` (no NUL).
3. Unknown id: `-31`.
4. The path is resolved against the document: `-11`, `-12`, `-13`, `-14`.
5. The node must be a leaf. A scalar or `null` gives its value text and type;
   an **empty** `{}` or `[]` gives the text `{}` or `[]` with `valueType` `5`
   or `6`; a **non-empty** object or array gives `-14`.
6. Buffer: `-1` or `-2` as in "Reading text into buffers, and `-2`".
7. Otherwise `0`: `value` and `valueType` are written.

| `Status` | Condition |
|---|---|
| `Ok` (`0`) | Value text and `valueType` written. |
| `InvalidArgument` (`-1`) | `path` null or `pathSize` `0`, or `value` null or `valueSize` `0`. |
| `BufferTooSmall` (`-2`) | The value text does not fit; `value` set to the empty text. |
| `UnterminatedInputText` (`-6`) | No NUL within `pathSize`. |
| `PathSyntaxError` (`-11`) | Path does not start with `/` (and is not empty), or has a bad `~` escape. |
| `PathNotFound` (`-12`) | An object has no such key. |
| `IndexOutOfRange` (`-13`) | An array index is at or above the array size, is `-`, or is above the `uint32` maximum. |
| `TypeMismatch` (`-14`) | Non-canonical array token, a scalar or `null` met with path tokens left, or the path ends on a non-empty object or array. |
| `UnknownDocumentId` (`-31`) | The id is `0`, was never issued, or was discarded. |
| `InternalError` (`-35`) | Unexpected failure inside the DLL, see "Unexpected internal failure". |

Path rules are in `docs/json-path.md`; the old dot-and-bracket syntax is not
accepted and gives `-11`.

## `restifyJsonDiscardDocument` (row 23)

Frees the document and its slot. Returns `0`, or `-31` if the id is `0`, was
never issued, or was already discarded. A second discard of the same id
therefore returns `-31`.

| `Status` | Condition |
|---|---|
| `Ok` (`0`) | Document freed. |
| `UnknownDocumentId` (`-31`) | The id is `0`, was never issued, or was discarded. |
| `InternalError` (`-35`) | Unexpected failure inside the DLL, see "Unexpected internal failure". |

## `restifyJsonDiscardAllDocuments` (row 24)

Frees every document. Returns `0` unless something unexpected fails;
`discardedCount` is the number of documents that were freed (`0` when none
were held).

| `Status` | Condition |
|---|---|
| `Ok` (`0`) | All documents freed; `discardedCount` written. |
| `InternalError` (`-35`) | Unexpected failure inside the DLL, see "Unexpected internal failure". |

## Limits

| Limit | Value | Status when exceeded |
|---|---|---|
| Input size, counted up to the NUL | 1 MiB (1048576 bytes) | `DocumentTooLarge` (`-32`) |
| Nesting depth (root container = 1) | 64 | `NestingTooDeep` (`-33`) |
| Entries | 10000 | `TooManyEntries` (`-34`) |
| Documents held at once | 8 | `NoFreeDocumentSlot` (`-30`) |

Input of exactly 1048576 bytes, depth 64 and 10000 entries are accepted.
Details on when each limit is checked are in `docs/json-flatten.md`. Raising a
limit in a later release is compatible; lowering one is not.

## Unexpected internal failure

No exception crosses into CANoe. If something unexpected fails inside the DLL
(for example, memory runs out), the operation returns `InternalError`
(`-35`) and leaves its out-parameters at 0; the same happens for all six
operations. It is distinct from `InvalidArgument` (`-1`), which always means
the caller passed a bad argument.

## Realtime-safety summary

| Row | From Simulation Setup | Notes |
|---|---|---|
| 19 (`restifyJsonParse`) | Allowed | Allocates memory; its time grows with the size of the document, bounded by the limits above. |
| 20 (`restifyJsonCountEntries`) | Allowed | Short lock; no allocation. |
| 21 (`restifyJsonReadEntry`) | Allowed | Short lock and a copy into the caller's buffer. |
| 22 (`restifyJsonReadValue`) | Allowed | Short lock and a copy into the caller's buffer; resolving the path allocates a little. |
| 23 (`restifyJsonDiscardDocument`) | Allowed | Frees memory, after the lock is released. |
| 24 (`restifyJsonDiscardAllDocuments`) | Allowed | Frees memory, after the lock is released. |

## Risk

Vector advises against dynamic memory allocation on the Simulation Setup
realtime thread. These operations may be called from there, but doing so may
disturb simulation timing, most noticeably for `restifyJsonParse` with a large
document. The DLL cannot detect the context it is called from and returns no
error for it.
