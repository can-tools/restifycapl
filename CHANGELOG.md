# Changelog

All notable changes to this project are documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.1.0/).
From now on, release sections are generated from commit messages when a release
is prepared and are not written by hand; the oldest section predates the
generator. Headings are the first 12 characters of
the commit hash the release is based on, and there are no version numbers in files.

## [e5edcea4b23a] - 2026-10-28

### Fixed

- README no longer calls the CHANGELOG sections tag-linked ([#39](https://github.com/can-tools/restifycapl/pull/39))
- CAPL framework setup names the release assets as the DLL source ([#39](https://github.com/can-tools/restifycapl/pull/39))

### Development

- docs/hum-41-readme-and-roadmap: update README.md and close out Stage 14 in master plan ([#19](https://github.com/can-tools/restifycapl/pull/19))
- stage/14-release-pipeline ([#17](https://github.com/can-tools/restifycapl/pull/17))
- fix/auto-pr-canoe-check-checklist: add CANoe check item to PR checklist for exports.cpp branches, drop duplicate exports.cpp sentence ([#20](https://github.com/can-tools/restifycapl/pull/20))
- chore/ci-changelog-parser: add scripts/ci/changelog.ps1 and replace the inline CHANGELOG parser in release.yml ([#21](https://github.com/can-tools/restifycapl/pull/21))
- chore/ci-changelog-cut-next-version: add cut and next-version subcommands to scripts/ci/changelog.ps1 ([#22](https://github.com/can-tools/restifycapl/pull/22))
- chore/ci-dispatch-release-mode: add workflow_dispatch to ci.yml and select the release mode by ref in release.yml ([#23](https://github.com/can-tools/restifycapl/pull/23))
- fix/release-show-version-errexit: capture next-version status under bash -e so a dry run with nothing to release no longer fails ([#25](https://github.com/can-tools/restifycapl/pull/25))
- docs/ci-flow-target-state ([#26](https://github.com/can-tools/restifycapl/pull/26))
- chore/ci-actionlint ([#27](https://github.com/can-tools/restifycapl/pull/27))
- chore/ci-changelog-generate ([#28](https://github.com/can-tools/restifycapl/pull/28))
- chore/ci-feat-branch-prefix ([#30](https://github.com/can-tools/restifycapl/pull/30))
- chore/ci-release-pr-bot ([#31](https://github.com/can-tools/restifycapl/pull/31))
- docs/plan-6c-files ([#33](https://github.com/can-tools/restifycapl/pull/33))
- docs/plan-6c-release-pr ([#34](https://github.com/can-tools/restifycapl/pull/34))
- chore/ci-release-pr-workflow ([#32](https://github.com/can-tools/restifycapl/pull/32))
- docs/plan-6c-fix-recognition ([#35](https://github.com/can-tools/restifycapl/pull/35))
- chore/ci-release-recognition ([#36](https://github.com/can-tools/restifycapl/pull/36))
- docs/plan-6d-tag-release ([#37](https://github.com/can-tools/restifycapl/pull/37))
- chore/ci-tag-release ([#38](https://github.com/can-tools/restifycapl/pull/38))

## [0d0f52240d49] - 2026-10-09

The export table contract covers rows 0–29 in [src/module/exports.cpp](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/src/module/exports.cpp).

Verification status:

- Unit tests pass on both architectures (x86 and x64) outside CANoe.
- Export table checker verifies all 12 structural and semantic checks on [src/module/exports.cpp](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/src/module/exports.cpp).
- CANoe verification: x64 build compiled and verified in CANoe (rows 1–29 recognized, wrapper libraries and verification nodes compile); x86 has not been loaded by CANoe (covered by CI only); no measurement has ever run, and runtime behavior has not been observed in CANoe.

Reference documentation: [docs/capl-sync-surface.md](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/docs/capl-sync-surface.md), [docs/capl-async-surface.md](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/docs/capl-async-surface.md), [docs/capl-json-surface.md](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/docs/capl-json-surface.md), [docs/capl-framework.md](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/docs/capl-framework.md), and [docs/release-process.md](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/docs/release-process.md).

### Added

- `long restifyReadVersion(char buffer[], dword bufferSize)`: writes the DLL's
  build version string into a caller-supplied buffer; returns 0 on success or a
  negative error code (see [src/module/exports.cpp](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/src/module/exports.cpp)).
- Synchronous HTTP operations:
  `long restifyGetSync(char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char responseBody[], dword responseBodySize, long& httpStatusCode, dword& responseBodyLength)` (blocking GET),
  `long restifyDeleteSync(...)` (DELETE),
  `long restifyPostSync(char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char requestBody[], dword requestBodySize, char responseBody[], dword responseBodySize, long& httpStatusCode, dword& responseBodyLength)` (POST),
  `long restifyPutSync(...)` (PUT),
  `long restifyPatchSync(...)` (PATCH), and
  `long restifyRequestSync(char method[], dword methodSize, char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char requestBody[], dword requestBodySize, char responseBody[], dword responseBodySize, dword connectTimeoutMs, dword totalTimeoutMs, dword maxResponseBytes, long& httpStatusCode, dword& responseBodyLength)`
  for general-purpose blocking requests covering any HTTP method, explicit timeouts and response body cap (see [docs/capl-sync-surface.md](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/docs/capl-sync-surface.md)).
- Asynchronous HTTP operations: non-blocking request dispatch helpers
  `long restifyGetAsync(char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, dword& requestId)`,
  `long restifyDeleteAsync(...)`,
  `long restifyPostAsync(char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char requestBody[], dword requestBodySize, dword& requestId)`,
  `long restifyPutAsync(...)`,
  `long restifyPatchAsync(...)`, and
  `long restifyRequestAsync(char method[], dword methodSize, char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char requestBody[], dword requestBodySize, dword connectTimeoutMs, dword totalTimeoutMs, dword maxResponseBytes, dword& requestId)`;
  plus response polling and lifecycle management
  `long restifyPollResponse(dword requestId, long& state)`,
  `long restifyAwaitResponse(dword requestId, dword waitTimeoutMs)`,
  `long restifyReadResponse(dword requestId, char responseBody[], dword responseBodySize, long& requestStatus, long& httpStatusCode, dword& responseBodyLength)`,
  `long restifyDiscardResponse(dword requestId)`, and
  `long restifyDiscardAllResponses(dword& stillRunning)`
  (see [docs/capl-async-surface.md](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/docs/capl-async-surface.md)).
- JSON parsing, normalization and typed path accessors:
  `long restifyJsonParse(char json[], dword jsonSize, dword& documentId)` (parses JSON text into a flattened document; strings may use apostrophe notation),
  `long restifyJsonCountEntries(dword documentId, dword& entryCount)`,
  `long restifyJsonReadEntry(dword documentId, dword entryIndex, char key[], dword keySize, char value[], dword valueSize, long& valueType)`,
  `long restifyJsonReadValue(dword documentId, char path[], dword pathSize, char value[], dword valueSize, long& valueType)`,
  `long restifyJsonReadLong(dword documentId, char path[], dword pathSize, long& value)`,
  `long restifyJsonReadDouble(dword documentId, char path[], dword pathSize, float& value)`,
  `long restifyJsonReadBool(dword documentId, char path[], dword pathSize, long& value)`,
  `long restifyJsonCountElements(dword documentId, char path[], dword pathSize, dword& elementCount)`,
  `long restifyJsonDiscardDocument(dword documentId)`,
  `long restifyJsonDiscardAllDocuments(dword& discardedCount)`, and
  `long restifyJsonNormalize(char json[], dword jsonSize, char normalized[], dword normalizedSize)`
  (see [docs/capl-json-surface.md](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/docs/capl-json-surface.md)).
- Preliminary CAPL framework in
  [capl/](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/capl/)
  (include entry point, `restLib` wrapper libraries, verification nodes); see
  [docs/capl-framework.md](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/docs/capl-framework.md).
- Tagged releases: pushing a `vX.Y.Z` tag builds both DLLs and publishes
  `restifycapl-x86.dll`, `restifycapl-x64.dll` and a `SHA256SUMS` file as a
  GitHub release, with the tag's changelog section as the release notes and a
  build-provenance attestation for the DLLs. The release is created as a draft,
  checked, and only then made public. Running the `Release` workflow manually
  performs a dry run that builds and checks everything without publishing. See
  [docs/release-process.md](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/docs/release-process.md).
- [scripts/setup-dev-env.ps1](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/scripts/setup-dev-env.ps1):
  development environment bootstrap script that provisions MSVC Build Tools,
  `make`, vcpkg-built libcurl (x86 and x64, static, SChannel), and the pinned
  `nlohmann/json` single header. Stores per-architecture MSVC environment
  variables (`RESTIFY_MSVC_X64_*`, `RESTIFY_MSVC_X86_*`, user scope) so
  `make build-x64` and `make build-x86` work from any new shell; bare `make`
  prints the target list instead of building. See
  [docs/development-environment.md](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/docs/development-environment.md).
- Statically linked runtime: `/MT` CRT for the DLL and all static dependencies
  (libcurl, zlib, GoogleTest), linking static libcurl with the Windows
  SChannel TLS backend.

### Development

- CI pipeline in
  [.github/workflows/ci.yml](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/.github/workflows/ci.yml):
  runs on `windows-latest` calling a reusable per-architecture pipeline
  ([.github/workflows/arch-pipeline.yml](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/.github/workflows/arch-pipeline.yml))
  once for x86 and once for x64, running `make build-<arch>` and `make test ARCH=<arch>`
  in parallel with shared dependency provisioning in
  [.github/actions/provision](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/.github/actions/provision),
  dependency caching, static `/MT` CRT provenance checks (`dumpbin /directives`),
  and uploading both DLLs as workflow artifacts. Triggers are filtered to `main`
  and working-branch prefixes (`stage/**`, `chore/**`, `fix/**`, `docs/**`).
- Tag-driven release workflow in
  [.github/workflows/release.yml](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/.github/workflows/release.yml):
  validates the tag and `CHANGELOG.md` heading hash, calls `arch-pipeline.yml` for
  x86 and x64 with tag-derived version resources, verifies export table integrity,
  assembles release notes and `SHA256SUMS`, attests build provenance via GitHub
  Attestations, and creates a draft release that is verified before publication.
  Includes a manual dry-run mode via `workflow_dispatch`.
- [scripts/list-export-table.ps1](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/scripts/list-export-table.ps1)
  and
  [.github/actions/export-table](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/.github/actions/export-table):
  validation of the CAPL export table in `src/module/exports.cpp` (12 checks)
  that also renders the table as markdown for CI job summaries and release notes.
  Test fixtures in `tests/export-table/` pin expected check results.
- [.github/workflows/auto-pr.yml](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/.github/workflows/auto-pr.yml):
  workflow that opens a draft PR into `main` on a push to a `stage/`, `chore/`,
  `fix/`, or `docs/` branch, guarded against no-op reruns and branches with no
  commits ahead of `main`, with PR body derived from branch compare data.
- [.claude/skills/stage-branch/SKILL.md](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/.claude/skills/stage-branch/SKILL.md):
  branch-naming convention and create-and-push procedure for starting new work.
- [Makefile](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/Makefile):
  `SYSLIBS` includes `version.lib` for `GetFileVersionInfo` version queries and
  `iphlpapi.lib` for libcurl's `if_nametoindex` reference, applying identically
  to both architectures via the shared parameterized build rule.
- [scripts/setup-dev-env.ps1](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/scripts/setup-dev-env.ps1)
  detects PowerShell 7 (`pwsh`), required by `scripts/list-export-table.ps1`,
  and installs it via winget or Chocolatey when missing, warning with the
  manual install command otherwise.
- [.gitattributes](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/.gitattributes)
  and
  [.editorconfig](https://github.com/can-tools/restifycapl/blob/0d0f52240d4976240efcd619384e770c09abda2c/.editorconfig)
  enforce line-ending policy: CRLF in the working tree, LF for shell scripts,
  and vendored `include/vendor/json.hpp` untouched.
- Every action `uses:` in workflows and composite actions is pinned to a full
  40-character commit SHA with a trailing version comment.

[e5edcea4b23a]: https://github.com/can-tools/restifycapl/commit/e5edcea4b23a78c3e940d47b5b5db62d4b6bf802
[0d0f52240d49]: https://github.com/can-tools/restifycapl/commit/0d0f52240d4976240efcd619384e770c09abda2c
