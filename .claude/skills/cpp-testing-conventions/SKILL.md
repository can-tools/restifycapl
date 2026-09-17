---
name: cpp-testing-conventions
description: GoogleTest conventions for testing CAPL REST DLL logic outside of CANoe. Load this before writing or modifying tests.
---

# C++ testing conventions

## Framework

- GoogleTest, built with `/MT` to match the main project (see
  `msvc-build-conventions`).
- Tests mirror the layered `src/` structure — `tests/core/`, `tests/http/`,
  `tests/mapping/` — with one test file per tested source module.

## What can and cannot be tested here

- Testable: pure logic modules — `type-conversion`, `json-path`,
  `json-flatten`, `json-accessors`, and any logic extracted from
  `sync-operations` / `async-operations` that does not require a live CANoe
  process. `struct-registry` / `struct-mapping` are deferred and out of
  scope unless explicitly reactivated.
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
- for the HTTP layer specifically: simulated timeouts and error responses,
- for the async layer specifically: ready-flag-cleared-after-read, and
  request-ID correlation across consecutive requests.

Every bug fix should come with a regression test reproducing the original
failure.
