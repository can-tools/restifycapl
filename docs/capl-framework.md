# CAPL framework (preliminary)

Normative material for the `capl/` folder: a thin CAPL layer over the DLL's
export table, plus two verification nodes. It is not part of the export
contract and is not packaged into a release; take `capl/` from the same
commit or tag as the DLL.

**Status: preliminary.** Wrapper names and parameters may still change; every
such change is recorded in `CHANGELOG.md`. The framework is not yet
append-only like the export table.

## Layout

The layout is fixed.

| Path | What it is |
|---|---|
| `capl/restify-verify-http.can` | Verification node: DLL version at start; sync GET, sync POST, async GET with timer polling, `-2` on a small buffer, apostrophe bodies normalized and POSTed (forms A and B, ambiguous `-10`); releases responses on stop. |
| `capl/restify-verify-json.can` | Verification node: DLL version at start; offline parse, entry count, dump, read by path, `-2` retry, 9th document `-30`, double discard `-31`, apostrophe texts (twin dumps, forms A and B, ambiguous `-10`); releases documents and responses on stop. |
| `capl/includes/includes.cin` | Single master include. The only file with `#pragma library` and the only file that includes the libraries. |
| `capl/includes/libs/restify-common.cin` | Row 1 wrapper and `restLibStatusText`. |
| `capl/includes/libs/restify-sync.cin` | Rows 2-7 wrappers. |
| `capl/includes/libs/restify-async.cin` | Rows 8-18 wrappers. |
| `capl/includes/libs/restify-json.cin` | Rows 19-25 wrappers and `restLibJsonDump`. |
| `capl/includes/dll/win-x64/` | Holds `restifycapl-x64.dll`, copied in by hand. Only `.gitkeep` is tracked. |
| `capl/includes/dll/win-x86/` | Holds `restifycapl-x86.dll`, copied in by hand. Only `.gitkeep` is tracked. |

Libraries include nothing and call no other library file. Every `.can`
contains one `includes` section with only `#include ".\includes\includes.cin"`.
Every CAPL file in `capl/` starts with the line `/*@!Encoding:1250*/` and
stays pure ASCII. `key` is a CAPL keyword; never use it as an identifier.

## One-time setup

1. Copy `build\x64\restifycapl-x64.dll` to `capl\includes\dll\win-x64\`.
2. Copy `restifycapl-x86.dll` to `capl\includes\dll\win-x86\`: from a local x86
   build (`build\x86\`) or from the CI artifact `restifycapl-x86`.
3. Attach `capl/restify-verify-http.can` and `capl/restify-verify-json.can` as
   nodes of a CANoe configuration, in Measurement Setup (or as a test node):
   the sync keys call blocking functions that must not run in Simulation Setup
   (see "Blocking calls").

Close CANoe before overwriting a DLL it has loaded; a loaded DLL cannot be
replaced. Do not place copies in the `capl_includes` fallback folders (below):
a missing DLL in `capl\includes\dll\...` could then silently resolve to a
stale copy there. DLLs are never committed; the repository's `*.dll`
ignore rule covers `capl/includes/dll/`.

## Path resolution

Every `#pragma library` and `#include` path uses the `.\` form, for example
`#include ".\libs\restify-json.cin"` and
`#pragma library(".\dll\win-x64\restifycapl-x64.dll")`. Per the Vector CANoe
19.3 documentation, a relative path in `#pragma library` is resolved against,
in order:

1. the folder of the file containing the command, here `includes.cin`;
2. `<User data folder>\Reusable\capl_includes`;
3. `<Installation directory>\Exec32\capl_includes` or `Exec64\capl_includes`.

The framework uses only the first. Confirmed in CANoe: `.\` include and
pragma paths resolve relative to the file that contains them. No absolute path
is used anywhere.

## Bitness and the library version argument

`#if X64` in `includes.cin` is client-side CAPL selection of the matching DLL.
It is not a `.vmodule`; the DLL deliverable still has no auto-selection.

The optional `version` argument of `#pragma library` is not used. It needs a
`caplDllLibraryVersion` export the DLL does not have and a hand-written
version number, which this project does not keep. The DLL version is shown at
measurement start by both nodes through `restLibReadVersion`.

## Wrapper names

The wrapper name is the export name with `restify` replaced by `restLib`,
nothing else changed. Each wrapper takes the DLL row's parameters in the same
order, omits every size `dword` that directly follows a `char[]` (supplied
with `elcount()` of that array), keeps reference parameters (`long &`,
`dword &`) and returns the DLL status unchanged. `restLibJsonNormalize(char json[], char normalized[])` takes two arrays. Signatures of the DLL rows:
`docs/capl-sync-surface.md`, `docs/capl-async-surface.md`,
`docs/capl-json-surface.md`.

| Row | DLL function | Wrapper | File in `capl/includes/libs/` |
|---|---|---|---|
| 1 | `restifyReadVersion` | `restLibReadVersion` | `restify-common.cin` |
| 2 | `restifyGetSync` | `restLibGetSync` | `restify-sync.cin` |
| 3 | `restifyDeleteSync` | `restLibDeleteSync` | `restify-sync.cin` |
| 4 | `restifyPostSync` | `restLibPostSync` | `restify-sync.cin` |
| 5 | `restifyPutSync` | `restLibPutSync` | `restify-sync.cin` |
| 6 | `restifyPatchSync` | `restLibPatchSync` | `restify-sync.cin` |
| 7 | `restifyRequestSync` | `restLibRequestSync` | `restify-sync.cin` |
| 8 | `restifyGetAsync` | `restLibGetAsync` | `restify-async.cin` |
| 9 | `restifyDeleteAsync` | `restLibDeleteAsync` | `restify-async.cin` |
| 10 | `restifyPostAsync` | `restLibPostAsync` | `restify-async.cin` |
| 11 | `restifyPutAsync` | `restLibPutAsync` | `restify-async.cin` |
| 12 | `restifyPatchAsync` | `restLibPatchAsync` | `restify-async.cin` |
| 13 | `restifyRequestAsync` | `restLibRequestAsync` | `restify-async.cin` |
| 14 | `restifyPollResponse` | `restLibPollResponse` | `restify-async.cin` |
| 15 | `restifyAwaitResponse` | `restLibAwaitResponse` | `restify-async.cin` |
| 16 | `restifyReadResponse` | `restLibReadResponse` | `restify-async.cin` |
| 17 | `restifyDiscardResponse` | `restLibDiscardResponse` | `restify-async.cin` |
| 18 | `restifyDiscardAllResponses` | `restLibDiscardAllResponses` | `restify-async.cin` |
| 19 | `restifyJsonParse` | `restLibJsonParse` | `restify-json.cin` |
| 20 | `restifyJsonCountEntries` | `restLibJsonCountEntries` | `restify-json.cin` |
| 21 | `restifyJsonReadEntry` | `restLibJsonReadEntry` | `restify-json.cin` |
| 22 | `restifyJsonReadValue` | `restLibJsonReadValue` | `restify-json.cin` |
| 23 | `restifyJsonDiscardDocument` | `restLibJsonDiscardDocument` | `restify-json.cin` |
| 24 | `restifyJsonDiscardAllDocuments` | `restLibJsonDiscardAllDocuments` | `restify-json.cin` |
| 25 | `restifyJsonNormalize` | `restLibJsonNormalize` | `restify-json.cin` |
| - | helper | `restLibStatusText` | `restify-common.cin` |
| - | helper | `restLibJsonDump` | `restify-json.cin` |

## Rules

- Wrappers are thin: no logic, no validation, status returned unchanged.
- Exactly two helpers exist, built only from existing DLL calls and plain
  CAPL, and they only print or release what the DLL returned:
  `restLibStatusText(long status, char text[])` writes the status name for `0`
  and every negative code in `docs/status-codes.md` (an unassigned code gives
  `Unknown(<code>)`); `restLibJsonDump(dword documentId)` prints every entry
  of a parsed document as `key = value (type)` using local buffers of 512
  bytes for the key and 512 bytes for the value. An entry that does not fit
  is reported as status `-2` and skipped.
- Not allowed in the framework: building JSON or request bodies, mapping
  JSON into CAPL structs or keeping a field registry (deferred modules), and
  converting value text into CAPL types (typed accessors, planned separately).
- Every new export-table row gets its 1:1 wrapper in the same change, in the
  file of its `categoryName` group; a new group gets a new file, included from
  `includes.cin`.
- No associative fields, no `const` in `variables`, no version numbers, no
  absolute paths, no other DLL.

## JSON with apostrophes

JSON written in CAPL source is easier to read with apostrophes for strings:
`{'name':'restify'}` means `{"name":"restify"}`. `restLibJsonParse` accepts
both notations. A request body is sent byte for byte, so a body written with
apostrophes goes through `restLibJsonNormalize(json, normalized)` first; pass
the normalized text to the POST/PUT/PATCH/request wrappers (rows 4-7, 10-13).
Rules and status codes: `docs/json-flatten.md` ("Input notation") and
`docs/capl-json-surface.md` (row 25).

**The ambiguity rule.** An apostrophe inside a value written in apostrophes,
e.g. `'it's'`, is ambiguous and gives `-10`. Write that value in double quotes
(CAPL source `\"it's\"`), or escape the apostrophe as `\'`, which in CAPL
source must be written `\\'`, because the CAPL compiler turns `\'` in a string
into a plain `'`. That CAPL turns `\\` into a single `\` is not yet verified
(see "Verified in CANoe").

| Form | CAPL source | Status |
|---|---|---|
| A: value in double quotes | `{'name':'restify','note':\"it's ok\"}` | not yet verified at runtime |
| B: escaped apostrophe | `{'name':'restify','note':'it\\'s ok'}` | not yet verified at runtime (how CAPL treats `\\` and `\'`); if CAPL passes both backslashes the text gives `-10` and form B is not usable from CAPL |

## Blocking calls

Sync operations (`restLibGetSync` and the other rows 2-7) and
`restLibAwaitResponse` block the calling thread until the request ends. Use
them from Measurement Setup or a test node. The two verification nodes are
meant for that use. Details: `docs/capl-sync-surface.md`,
`docs/capl-async-surface.md`. The JSON rows may be called from any context;
the risk of doing so from Simulation Setup is described in
`docs/capl-json-surface.md`.

## Verification nodes

Keys of `restify-verify-http.can`:

| Key | Action |
|---|---|
| `1` | Sync GET of `https://httpbin.org/json`. |
| `2` | Sync POST of a small JSON body to `https://httpbin.org/post`. |
| `3` | Async GET, polled by a timer, then read; a response left behind by a failed read is discarded. |
| `4` | Sync GET into a deliberately small buffer, expecting `-2` with the needed length reported. |
| `5` | Normalize the form A body, print it, sync POST it to `https://httpbin.org/post`. |
| `6` | Same for the form B body. |
| `7` | Normalize the ambiguous `{'note':'it's'}`, expecting `-10` and a hint to use form A or B. |

Keys of `restify-verify-json.can`:

| Key | Action |
|---|---|
| `1` | Parse the JSON text held in a variable, count entries, dump them. |
| `2` | Read `/list/1` from the open document. |
| `3` | Sync GET of `https://httpbin.org/json`, then parse, count and dump the response. |
| `4` | Read `/name` into a 4-byte buffer (`-2`), then retry with a larger one. |
| `5` | Discard all documents, then open nine; the ninth gives `-30`. |
| `6` | Parse a document and discard its id twice; the second discard gives `-31`. |
| `7` | Discard all documents. |
| `8` | Parse `{'name':'restify','value':1}` and its double-quoted twin, dump both, discard both. |
| `9` | Parse forms A and B, read `/note` (expected `it's ok`), discard both. |
| `0` | Parse the ambiguous `{'note':'it's'}`, expecting `-10`. |

Both nodes release everything on `on stopMeasurement`. Keys `5` and `6` need
free document slots; run key `7` after key `5`.

## Verified in CANoe

The CANoe compile on x64 is confirmed at commit 5a32872: CANoe recognizes
rows 19-25, and `includes.cin`, the four libraries and both nodes compile.
Confirmed items:

- `.\` include and pragma paths resolve relative to the file containing them.
- There are no name clashes and no CAPL keyword is used as an identifier;
  `key` is a CAPL keyword.

Not yet verified, because each needs a running measurement and is pending a
CANoe licence:

1. The `#if X64` branch loads the x64 DLL from `capl\includes\dll\win-x64\`.
2. User-defined functions in a `.cin` accept reference parameters
   (`long &`, `dword &`).
3. `elcount()` on an array parameter inside a function yields the caller's
   array size.
4. Both nodes print the DLL version at measurement start.
5. Form B: the CAPL source `'it\\'s ok'` reaches the DLL as `'it\'s ok'`.
6. All runtime behaviour, including CAPL string-literal initializers with
   escaped quotes (`\"`) as used for the JSON text, large local arrays inside
   a function (`restLibJsonDump`), passing a global by reference to a wrapper,
   and the length at which `write()` cuts a long response body.

32-bit CANoe is not checked: no 32-bit configuration is available, and the
x86 DLL is covered by CI only. CI cannot compile CAPL.
