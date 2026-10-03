# JSON path

Behavior tables for `src/core/json-path.h`.

Paths are JSON Pointers as defined by RFC 6901. The same format is used for
the keys produced by JSON flattening, so every flattened key resolves back to
its value through `ResolvePath`.

The former dot-and-bracket syntax (`data.items[0].name`) is not supported.
Such text does not start with `/` and is rejected with `PathSyntaxError`.

## Syntax

- `""` (empty text) addresses the whole document.
- Any other path starts with `/`; each further `/` starts a new token.
- `/` alone is one token: the empty key `""` at the root.
- In a token, `~1` stands for `/` and `~0` stands for `~`. A `~` followed by
  anything else, or a `~` at the end of a token, is a syntax error.
- Decoding handles each escape once, left to right, so `~01` decodes to `~1`
  (not `/`).
- Tokens are not trimmed; whitespace is part of the key.

## Examples

| Path | Meaning |
|---|---|
| `/data/items/0/name` | key `data`, key `items`, element 0, key `name` |
| `/@odata.context` | key `@odata.context`; `.`, `@`, `[` and `]` need no escaping |
| `/a~1b` | key `a/b` |
| `/m~0n` | key `m~n` |
| `/x/` | the empty key `""` inside the value of `x` |
| `//0` | the empty key `""` at the root, then element 0 (or key `0`) of its value |
| `""` | the whole document |
| `/` | the empty key `""` at the root |

## `ParsePath`

`ParsePath` validates and decodes path text without a document. It returns a
list of decoded text tokens (empty for `""`). Whether a token is a key or an
array index is decided only at resolve time, by the type of the container it
is applied to. `out` is replaced only on `Ok`.

| Case | Status |
|---|---|
| `""` | `Ok`, no tokens |
| Text not starting with `/`, such as `data.items[0].name` or `items` | `PathSyntaxError` |
| `~` followed by a character other than `0` or `1`, such as `/a~2`, or a trailing `~` | `PathSyntaxError` |
| `string_view` whose `data()` is null (default-constructed) | `InvalidArgument` |

## `ResolvePath`

`ResolvePath` parses the path, then walks the document one token at a time.
A syntax error anywhere in the path is reported before the document is
looked at.

| Node the token is applied to | Token | Status |
|---|---|---|
| Object | key present (any text, including `"0"` and `""`) | continue with the value |
| Object | key absent | `PathNotFound` |
| Array | `0` or `[1-9][0-9]*` below the array size | continue with that element |
| Array | `0` or `[1-9][0-9]*` at or above the array size | `IndexOutOfRange` |
| Array | `-` | `IndexOutOfRange` |
| Array | digits only, no leading zero, larger than the `uint32_t` maximum (4294967295) | `IndexOutOfRange` |
| Array | leading zero (`01`), sign (`-1`, `+1`), any other text (`x`), or empty (`""`) | `TypeMismatch` |
| String, number, boolean | any token | `TypeMismatch` |
| `null` | any token | `TypeMismatch` |

An empty `""` path returns `Ok` with the whole document. A `null` or scalar
met while tokens remain is a `TypeMismatch`; `NullValue` is reserved for a
terminal node read by a typed accessor (see `type-conversion.h`'s `To*`
family), not for a traversal failure here.

On `Ok`, `out` points into the caller's document and is valid only while the
document is alive. On any other status `out` is left unchanged.

## Status summary

| Status | Value | Raised for |
|---|---|---|
| `InvalidArgument` | -1 | path `string_view` with null `data()` |
| `PathSyntaxError` | -11 | no leading `/` (non-empty text), bad `~` escape |
| `PathNotFound` | -12 | object key absent |
| `IndexOutOfRange` | -13 | array index at or above size, `-`, number above `uint32_t` |
| `TypeMismatch` | -14 | non-canonical array token, or a scalar/`null` with tokens remaining |
