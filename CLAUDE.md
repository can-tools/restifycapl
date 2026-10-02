# CAPL REST DLL

Native Windows DLL plugin for Vector CANoe. Exposes synchronous and
asynchronous REST/HTTP operations, JSON parsing, and struct mapping to CAPL
scripts running inside CANoe.

## Tech stack

- C++17, compiled with MSVC (`cl.exe`, `rc.exe`), no CMake.
- Statically linked runtime: `/MT` everywhere (project and all dependencies).
- Two architecture targets: x86 and x64. Both must exist and stay behaviorally
  identical except for architecture flags — CANoe's runtime kernel only loads
  a DLL matching its own bitness.
- Dependencies: libcurl (vcpkg static triplets, Schannel TLS backend), zlib
  (transitive), nlohmann/json v3.11.3 (header-only), plus Windows system
  libs (crypt32, bcrypt, secur32, ws2_32, normaliz, wldap32, advapi32).
- Toolchain: Visual Studio **Build Tools** (no IDE required); development
  in VS Code.
- Tests: GoogleTest, built with `/MT` to match the main project.

## Scope

In scope now: sync + async REST/HTTP, JSON flattening, typed JSON path
accessors. Deferred until a demonstrated need: struct registry / JSON→struct
mapping, and CAPL-side request-body building. Do not build the deferred
modules pre-emptively.

Every new export-table row gets its 1:1 restLib wrapper (name: restify ->
restLib) in capl/includes/libs/ in the same change.

## Build

- `make` (no target) or `make help` — prints the available targets; builds
  nothing.
- `make all` — builds x86, then x64. Works in CI, where each matrix leg has
  its own MSVC environment, and locally from any shell once
  `scripts/setup-dev-env.ps1` has stored the build environment (see below).
  Without those stored variables, one shell only ever has one architecture's
  `cl.exe`/`link.exe` on `PATH`, so `make all` in such a shell fails at link
  time (`LNK1112`) at the architecture the shell does not target — run
  `make build-x86` and `make build-x64` in separately activated shells then.
- Local build environment: `scripts/setup-dev-env.ps1` stores each
  architecture's MSVC environment as user environment variables
  (`RESTIFY_MSVC_X64_*`, `RESTIFY_MSVC_X86_*`). The Makefile uses the set
  matching `ARCH` when present and the shell's own environment otherwise
  (CI). Open a new window after running the script; re-run it after a
  Visual Studio update.
- `make build-x86` — builds `build/x86/restifycapl-x86.dll` (`/MACHINE:X86`).
- `make build-x64` — builds `build/x64/restifycapl-x64.dll` (`/MACHINE:X64`).
- `make test` — builds and runs the GoogleTest suite (outside CANoe) for
  one architecture (default x64; `ARCH=x86` for the other). Local
  verification defaults to x64; CI runs both.
- `make clean` — removes all build output and intermediate files.
- Dependencies live in `lib/x86/` and `lib/x64/`, matched to `/MT`.
- Both architecture targets are thin wrappers over a single parameterized
  rule, so compiler and linker flags cannot drift between x86 and x64.
- First-time local setup: `scripts/setup-dev-env.ps1`. It cannot install
  CANoe — that step is manual. It also stores the per-architecture build
  environment.

## Directory layout

```
include/vendor/         third-party headers, never edited by hand
  json.hpp                nlohmann/json, pinned v3.11.3
  capl-dll-sdk/           Vector SDK headers (cdll.h, VIA.h, VIA_CDLL.h)
src/core/               LEVEL 0: pure logic, zero I/O, no CANoe knowledge
src/http/               LEVEL 1+3: http-client, sync-operations, async-operations
src/registry/           LEVEL 1: struct-registry (conditional, not yet in scope)
src/mapping/            LEVEL 2: json-flatten, json-accessors, struct-mapping
src/module/             LEVEL 4: the ONLY place that knows about CANoe/CAPL
                          exports.cpp, exports.def, version.rc
lib/x86/, lib/x64/      static dependencies (.lib), built with /MT
build/x86/, build/x64/  build output (gitignored)
tests/                  GoogleTest unit tests, run without CANoe
examples/               .can examples showing usage from the CAPL side
capl/                   CAPL framework (preliminary): *.can verification nodes,
                          each including only includes/includes.cin
  includes/includes.cin   single master include: #pragma library (x64/x86) and
                          #include of libs/
  includes/libs/          restLib wrapper libraries, one .cin per export category
  includes/dll/win-x64/   restifycapl-x64.dll copied in by the user; never
                          committed (.gitkeep only)
  includes/dll/win-x86/   restifycapl-x86.dll copied in by the user; never
                          committed (.gitkeep only)
scripts/                setup-dev-env.ps1 — environment bootstrap
docs/                   project documentation
```

Folders map 1:1 onto dependency levels. `src/core/` must not import from
`src/http/`, `src/registry/`, or `src/mapping/`. Only `src/module/` may
include the CAPL SDK headers.

## Non-negotiable constraints

- **Export contract**: the real contract with CANoe is the
  `CAPL_DLL_INFO_LIST` / `CAPL_DLL_INFO4` table in `src/module/exports.cpp`,
  not just `src/module/exports.def`. Never rename, reorder, or remove an
  existing entry — see the `capl-export-contract` skill before touching
  this file.
- **Runtime library**: `/MT` is mandatory for the project and every static
  dependency (libcurl, zlib, GoogleTest). Never mix `/MT` and `/MD` in the
  same link — see the `msvc-build-conventions` skill.
- **Bitness**: x86 and x64 builds must both exist and be tested. Clients
  select the correct DLL manually; there is no `.vmodule` auto-selection.
- **Tests run outside CANoe**: logic that can be unit-tested must be
  reachable without going through the CAPL export glue.
- **Versioning**: the single source of truth is the Git tag (`vX.Y.Z`) used
  for a release — never edit a version number by hand in any file. See the
  `msvc-build-conventions` skill for the full mechanism.

## Agents

See `.claude/agents/`: `planner`, `plan-writer`, `cpp-implementer`,
`build-pipeline-engineer`, `test-engineer`, `code-reviewer`, `docs-writer`
(sole owner of README.md). Start new
features or non-trivial changes with `planner` before implementation.
Delegate build/CI work, testing, and export-contract-sensitive review to
the matching agent instead of doing it inline in the main session.

## Planning documents

Project-specific planning material (requirements, notes, decisions) is
kept in `docs/planning/` and intended primarily for the `planner` agent.
This is a convention, not an enforced access boundary — Claude Code's
permission rules are global, not per-subagent. Do not put anything there
that must never be visible to other agents in the same session.

## Saving plans

`planner` never writes files itself. Once a plan is finished and you
approve it, `planner` delegates persisting it to `plan-writer` (which has
`Write` and the `save-plan` skill) via the `Agent` tool. `plan-writer`
saves the plan verbatim to `docs/work/<slug>/plans/plan.md`. This works in
a single window/session, including when `planner` is run standalone via
`claude --agent planner` — `planner` still never touches disk itself, it
only asks `plan-writer` to.

## Relaying subagent output

When relaying the output of any subagent (especially `planner` and
`code-reviewer`) to the user, always show the full text verbatim. Never
summarize, shorten, or paraphrase a subagent's response on your own — the
user needs the complete plan or review, not your interpretation of it. If
the output is long, show it in full anyway rather than trimming it.
