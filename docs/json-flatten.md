# JSON flattening

Behavior tables for `src/mapping/json-flatten.h`.

`FlattenJson` parses JSON text and returns two things: the parsed document
(for path lookups with `ResolvePath`, see `json-path.md`) and a flat list of
entries, one per leaf, in document order. On any failure the result object is
left unchanged.

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

## Limits

| Limit | Value | Status when exceeded |
|---|---|---|
| Input size | 1 MiB (1048576 bytes) | `DocumentTooLarge` (-32) |
| Nesting depth (root container = 1) | 64 | `NestingTooDeep` (-33) |
| Entries | 10000 | `TooManyEntries` (-34) |

Input of exactly 1048576 bytes, depth 64 and 10000 entries are accepted.

- The size check happens before anything is allocated for the document.
- Depth and entry limits are enforced while the text is scanned, before the
  document is built. Whichever limit is reached first in the text decides the
  status. A text that is both too deep and malformed further on reports
  `NestingTooDeep`.
- While scanning, the entry limit counts leaves as they appear in the text,
  so a repeated key counts once per occurrence even though only the last value
  survives. A text with more than 10000 leaves is rejected even if duplicate
  keys would reduce the final list below the limit.
- Raising a limit later is compatible; lowering one is not.

## Other errors

`ParseError` (-10) is returned for invalid JSON, empty text and invalid UTF-8.
Trailing content after the first JSON value is invalid JSON.

## Cost

Object lookup in the ordered document type is linear, so building an object
with k fields takes about k²/2 field comparisons. The 10000-entry limit bounds
this: one flat object of 10000 fields is the worst case. The text is scanned
twice, once to check the limits and once to build the document.
