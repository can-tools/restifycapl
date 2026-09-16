---
name: cpp-testing-conventions
description: GoogleTest conventions for testing CAPL REST DLL logic outside of CANoe. Load this before writing or modifying tests.
---

# C++ testing conventions

## Framework

- GoogleTest, built with `/MT` to match the main project (see
  `msvc-build-conventions`).
- Tests live in `tests/`, with one test file per tested source module,
  mirroring the name in `src/` (e.g. a test file for
  `json-path-resolver.cpp`).

## What can and cannot be tested here

- Testable: pure logic modules — `json-path-resolver`, `type-converters`,
  `struct-mapping`, `json-flatten`, `json-helpers`, `request-builder`, and
  any logic extracted from `sync-rest-operations` / `async-rest-operations`
  that does not require a live CANoe process.
- Not testable here: the CAPL export glue itself
  (`CAPL_DLL_INFO_LIST`, the `extern "C"` wrapper functions) — this can only
  be verified by loading the DLL into a real CANoe instance, which is
  outside the scope of automated unit tests.

## Mocking boundaries

- Mock or fake libcurl-based HTTP calls rather than making real network
  requests in unit tests.
- Do not mock simple internal data structures — only mock true external
  boundaries (network, filesystem, timers).

## Required coverage per module

For each tested function or module, cover at minimum:

- valid/expected input,
- malformed or missing JSON fields,
- type mismatches,
- for the HTTP layer specifically: simulated timeouts and error responses.

Every bug fix should come with a regression test reproducing the original
failure.
