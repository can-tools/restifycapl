# JSON path

Archival rationale and behavior tables for `src/core/json-path.h`.

## Interim syntax caveat (originally written at Stage 8)

`data.items[0].name` (dot for object keys, brackets for array indices) is
explicitly interim, not the target architecture. Struct mapping
(currently deferred, see `CLAUDE.md`'s Scope section) is the intended
eventual successor for structured access. This is a named, stated caveat
-- not a silent permanent contract -- so a future struct-mapping stage is
free to supersede this syntax without archaeology.

## `ParsePath` edge cases

| Case                                             | Resolution        |
|---------------------------------------------------|-------------------|
| Empty path ""                                      | PathSyntaxError   |
| Leading dot, ".items[0]"                            | PathSyntaxError   |
| Top-level index on a root array, "[0].name"          | Valid -- Ok       |
| Negative or non-numeric index, "[-1]", "[x]"          | PathSyntaxError   |
| Index too large for uint32_t                          | PathSyntaxError   |
| Unclosed bracket "items[0", empty brackets "[]",        | PathSyntaxError   |
| double dot "..", trailing "."                            |                   |
| Key containing "." or "["                                  | Unreachable via this syntax -- a documented limitation of the interim syntax, not an error path |
| Whitespace                                                    | Not trimmed; part of the key |

## `ResolvePath` segment-kind resolution rule

| Segment kind | Container kind at that point                         | Result          |
|--------------|--------------------------------------------------------|-----------------|
| [n]          | array, n >= size                                        | IndexOutOfRange |
| .key         | object, key absent                                      | PathNotFound    |
| [n]          | object (not an array)                                    | TypeMismatch    |
| .key         | array (not an object)                                     | TypeMismatch    |
| either       | scalar (string/number/bool) where a container was expected | TypeMismatch    |

A JSON null encountered mid-path where a container was expected falls
under the last row (TypeMismatch) -- NullValue is reserved for a terminal
node read by a typed accessor (see `type-conversion.h`'s `To*` family), not
for a traversal failure here.
