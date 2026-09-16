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
- Dependencies: libcurl, zlib, plus Windows system libs (crypt32, bcrypt,
  secur32, ws2_32, normaliz, wldap32, advapi32).
- Tests: GoogleTest, built with `/MT` to match the main project.

## Build

- `make build32` — builds `build-32b/capl-rest-32b.dll` (`/MACHINE:X86`).
- `make build64` — builds `build-64b/capl-rest-64b.dll` (`/MACHINE:X64`).
- `make test` — builds and runs the GoogleTest suite (outside CANoe).
- Dependencies live in `lib-32b/` and `lib-64b/`, matched to `/MT`.

## Directory layout

```
include/   public headers (CAPL export declarations)
src/       implementation (.cpp)
lib-32b/   x86 static dependencies (.lib), built with /MT
lib-64b/   x64 static dependencies (.lib), built with /MT
build-32b/ x86 build output
build-64b/ x64 build output
tests/     GoogleTest unit tests, mirrors src/ module names
docs/      project documentation
```

## Non-negotiable constraints

- **Export contract**: the real contract with CANoe is the
  `CAPL_DLL_INFO_LIST` table in `src/capl-rest-dll.cpp`, not just the `.def`
  file. Never rename, reorder, or remove an existing entry — see the
  `capl-export-contract` skill before touching this file.
- **Runtime library**: `/MT` is mandatory for the project and every static
  dependency (libcurl, zlib, GoogleTest). Never mix `/MT` and `/MD` in the
  same link — see the `msvc-build-conventions` skill.
- **Bitness**: x86 and x64 builds must both exist and be tested. Clients
  select the correct DLL manually; there is no `.vmodule` auto-selection.
- **Tests run outside CANoe**: logic that can be unit-tested must be
  reachable without going through the CAPL export glue.

## Agents

See `.claude/agents/`: `planner`, `plan-writer`, `cpp-implementer`,
`build-pipeline-engineer`, `test-engineer`, `code-reviewer`. Start new
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
