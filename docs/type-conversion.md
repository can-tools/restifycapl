# Type conversion

Archival rationale and behavior tables for `src/core/type-conversion.h`.

## Why `To*` and `Parse*` are kept as two separate families

`To*` takes `const nlohmann::json&` in and returns a typed C++ value:
strict, a JSON string arriving where a number was requested is
`TypeMismatch`, never a silent fall-through to `ParseLong`/`ParseDouble`.

`Parse*` takes `std::string_view` in (raw text, e.g. a CAPL-supplied
string) and returns a typed C++ value: a deliberately named lenient escape
hatch, never called internally by the `To*` family.

Keeping the two separate means a caller that reaches for `ToLong` on a
JSON string gets a loud `TypeMismatch` instead of silently accepting `"42"`
as if it were a number.

## Why `ToText` and `ValueToText` both exist

`ToText` is the strict string accessor: JSON string only, every other JSON
type fails. `ValueToText` is the lenient leaf stringifier used when the
caller wants "whatever is there" as text regardless of its JSON type.
Callers need both shapes -- a typed field accessor that fails loudly on
the wrong type, and a generic dumper that always produces something for
any leaf.

## `ValueToText` leaf-stringify behavior (originally written at Stage 8)

| Input node                        | Output                     | Status       |
|------------------------------------|----------------------------|--------------|
| JSON null                          | the text "null"            | Ok           |
| JSON number 42                     | "42"                        | Ok           |
| JSON true                          | "true"                      | Ok           |
| JSON string                        | the string's own contents  | Ok           |
| JSON object or array (non-leaf)    | (out untouched)             | TypeMismatch |

`null -> "null"/Ok` guarantees `ValueToText` always succeeds on a genuine
leaf, the same guarantee `ToText` gives for strings -- no special-cased
failure for null. Container -> `TypeMismatch` is deliberate and loud:
`src/mapping/json-flatten.*` (a later stage) recurses through containers
itself and only ever calls `ValueToText` on leaves it has already
discovered, so a container arriving here means a bug in the caller's
recursion, not a normal runtime case -- it must not be silently serialized
into a nested JSON string that would then look like a flattened "value".
