---
name: capl-export-contract
description: The export contract between the DLL and CANoe/CAPL — the .def file, the CAPL_DLL_INFO_LIST table, and bitness rules. Load this before touching src/capl-rest-dll.cpp, includes/*.h, or the .def file.
---

# CAPL export contract

## What the real contract is

The `.def` file (`capl-rest-dll.def`) is a transport mechanism — it exposes
whatever entry point CANoe needs to reach the description table. It is
**not** the actual API contract.

The actual contract seen by CAPL scripts is the `CAPL_DLL_INFO_LIST` (or
`CAPL_DLL_INFO4`) table defined in `src/capl-rest-dll.cpp`. Each row of this
table defines, for one function:

- the name CAPL sees (which does not have to match the C++ function name),
- the function pointer,
- the return type,
- the number and types of parameters (encoded as a type string),
- category/description metadata.

The first row of the table is a reserved version entry
(`CDLL_VERSION_NAME` / `CDLL_VERSION`) and must always be present.

## Rules

- **Never rename, reorder, or remove an existing entry.** CAPL scripts call
  functions by the name and signature declared in this table. A change here
  is invisible to the compiler but breaks existing CAPL scripts at runtime.
- **Only append new entries** for new functionality. If a function's
  signature must change, add a new entry with a new name rather than
  changing an existing one in place.
- Functions exposed through the table should be declared `extern "C"` to
  avoid C++ name-mangling issues tying the export to a specific compiler
  version.
- Any change to this table, or to `capl-rest-dll.def`, requires a
  `code-reviewer` pass before being considered done.
- If a breaking change to the contract is genuinely required, treat it as a
  major version change and call it out explicitly — do not let it happen as
  a side effect of an unrelated feature.

## Bitness

- CANoe's runtime kernel loads a DLL matching its own bitness exactly: a
  32-bit CANoe configuration will refuse a 64-bit DLL and vice versa
  ("Requested CAPL DLL is invalid").
- Both `build-32b/capl-rest-32b.dll` and `build-64b/capl-rest-64b.dll` must
  always be built and kept behaviorally identical (same exported table,
  same behavior) — only the target architecture differs.
- Clients of this project select the correct DLL manually; there is no
  `.vmodule` auto-selection mechanism in scope for this project.
