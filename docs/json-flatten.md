# JSON flattening

Behavior tables for `src/mapping/json-flatten.h`.

`FlattenJson` parses JSON text and returns two things: the parsed document
(for path lookups with `ResolvePath`, see `json-path.md`) and a flat list of
entries, one per leaf, in document order. On any failure the result object is
left unchanged.

## API

- `FlattenJson(text, out)` fills a `FlattenResult`: `out.document` is the
  parsed document (a `JsonValue`) and `out.entries` the flat list. `out` is
  replaced only on `Ok`.
- `DescribeLeaf(node, text, type)` gives the value text and `JsonEntryType` of
  one leaf: a scalar, `null`, `{}` or `[]`. A non-empty object or array returns
  `TypeMismatch`.
- Limit constants: `kMaxJsonInputBytes` (1048576), `kMaxJsonDepth` (64) and
  `kMaxFlatEntries` (10000); see Limits below.

## Key format

Each entry key is a JSON Pointer as defined by RFC 6901, in exactly the syntax
`json-path.md` accepts, so every key resolves back to its own value through
`ResolvePath`.

- Each step down adds `/` followed by the object key or the array index.
- In an object key, `~` is written `~0` and `/` is written `~1`. `~` is
  replaced first, so a key containing `~1` literally becomes `~01`.
- Array indices are decimal, without leading zeros.
- A document whose root is a scalar, `null`, `{}` or `[]` has exactly one
  entry, with the key `""` (empty text).
- An empty object key gives an empty token: `{"x":{"":1}}` has the key `/x/`.

| Document | Keys |
|---|---|
| `{"a":{"b":[10,20]}}` | `/a/b/0`, `/a/b/1` |
| `{"a/b":1,"m~n":2}` | `/a~1b`, `/m~0n` |
| `{"":{"0":1}}` | `//0` |
| `42` | `""` (the empty key) |

## Entry order

- Pre-order, depth first: a container's entries come before its next sibling's.
- Object fields appear in document order; array elements appear by index.
- Indices are stable: the entry at position `i` is the same for the whole
  lifetime of a parsed document.
- CAPL's own associative fields sort keys ascending, so document order is
  visible to a CAPL script only when it walks the entries by index.

## Duplicate object keys

When an object repeats a key, the last value wins and the field keeps the
position of its first occurrence. The value is replaced as a whole, never
merged. Flattening emits one entry set per surviving key.

| Document | Result |
|---|---|
| `{"a":1,"a":2}` | `/a` = `2` |
| `{"a":{"x":1},"b":0,"a":{"y":2}}` | `/a/y` = `2` and `/b` = `0`, in that order; `/a/x` does not exist, and `/a` stays ahead of `/b` |

## Leaves and value types

Only leaves produce entries: strings, numbers, booleans, `null`, and empty
containers. A non-empty object or array produces no entry of its own, only
the entries of its contents.

| Value type | Number | Leaf | Value text |
|---|---|---|---|
| None | 0 | (never emitted; "no value") | |
| String | 1 | JSON string | the string's contents, unquoted and unescaped |
| Number | 2 | JSON number | see below |
| Bool | 3 | `true`, `false` | `true` or `false` |
| Null | 4 | `null` | `null` |
| EmptyObject | 5 | `{}` | `{}` |
| EmptyArray | 6 | `[]` | `[]` |

An empty string is a String entry with empty value text; it is not the same
as `null`.

The numeric values are part of the contract and are never renumbered.

## Number text

Number text comes from `ValueToText` (see `type-conversion.md`), which uses
the JSON library's own locale-independent serializer.

| JSON text | Entry text |
|---|---|
| `42`, `-7` | `42`, `-7` |
| `5.0` | `5.0` (a float with an integer value keeps its `.0`) |
| `1e-5` | `1e-05` |
| `1e300` | `1e+300` |

Integers within the signed or unsigned 64-bit range keep all their digits.
A number beyond 64 bits is held as a double and loses precision, so its text
may differ from the digits in the input. A number too large even for a
double is invalid JSON for this purpose (`ParseError`).

## Input text rules

These follow the JSON library's (nlohmann/json 3.11.3) scanner.

- **BOM.** A UTF-8 byte order mark (`EF BB BF`) at the very start of the text
  is accepted and skipped. It counts towards the 1 MiB input limit. A partial
  BOM (`EF BB` or a lone `EF` not followed by `BB BF`) is `ParseError`, as is
  a text that is only a BOM.
- **Comments** (`//` and `/* */`) are not allowed and give `ParseError`.
- **`-0`** is read as the integer `0`, so its entry text is `0`.
- **NUL bytes.** A NUL byte anywhere in the input text is `ParseError`; this
  check runs before the scan, so it wins even if a limit (`-33`, `-34`,
  `-36`) would be reached earlier in the text. A
  `\u0000` escape in an object key is `ParseError` too, so no entry key ever
  contains a NUL and every key can be read back by path. A `\u0000` escape in
  a string value is accepted and the value text then contains a NUL byte. Value
  text is copied into CAPL buffers with its full length (the `-2` size check
  counts the NUL byte and what follows it), but a CAPL script reads it as
  NUL-terminated text, so it sees only the part before the first NUL.

## Limits

| Limit | Value | Status when exceeded |
|---|---|---|
| Input size (`kMaxJsonInputBytes`) | 1 MiB (1048576 bytes) | `DocumentTooLarge` (-32) |
| Nesting depth (`kMaxJsonDepth`; root container = 1) | 64 | `NestingTooDeep` (-33) |
| Entries (`kMaxFlatEntries`) | 10000 | `TooManyEntries` (-34) |
| Key text (`kMaxFlatKeyBytes`; sum of all entry keys in bytes, after escaping, no NULs) | 4 MiB (4194304 bytes) | `KeyTextTooLarge` (-36) |

Input of exactly 1048576 bytes, depth 64, 10000 entries and 4194304 bytes of
key text are accepted.

- The size check happens before anything is allocated for the document.
- Depth and entry limits are enforced while the text is scanned, before the
  document is built. Whichever limit is reached first in the text decides the
  status. A text that is both too deep and malformed further on reports
  `NestingTooDeep`.
- While scanning, the entry limit counts leaves as they appear in the text,
  so a repeated key counts once per occurrence even though only the last value
  survives. A text with more than 10000 leaves is rejected even if duplicate
  keys would reduce the final list below the limit.
- The key text limit is counted while scanning, per occurrence in the text
  (before duplicate keys collapse). It sums the byte length of every entry key
  as `ReadEntry` returns it: `~` and `/` in an object key count 2 bytes, an
  array index counts its decimal digits, the root key `""` counts 0, and an
  empty container counts its own key. If one leaf crosses both the entry limit
  and the key text limit, `TooManyEntries` is reported, not `KeyTextTooLarge`.
- The key text limit exists because every entry key repeats its full path, so a
  deep or wide document can need far more key text than its input size.
- Raising a limit later is compatible; lowering one is not.

## Other errors

`ParseError` (-10) is returned for invalid JSON, empty text, invalid UTF-8, a
NUL byte in the text and a NUL in an object key. Trailing content after the
first JSON value is invalid JSON.

FlattenJson reports every input-related failure as a status. It can still
throw `std::bad_alloc` when the process cannot supply memory. The limits bound
what one document needs, so this means the host is short of memory, not that
the input was too large. The text layer turns it into `InternalError` (-35).

## Cost

Object lookup in the ordered document type is linear, so building an object
with k fields takes about k²/2 field comparisons. The 10000-entry limit bounds
this: one flat object of 10000 fields is the worst case. The text is scanned
twice, once to check the limits and once to build the document.

Memory: the key text of one document is bounded to 4 MiB, so 8 held documents
need at most 32 MiB of key text.
