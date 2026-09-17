<!-- Produced by: planner agent, 2026-09-17 -->

# Plan (FINAL, v4): Build the CAPL REST DLL (restifycapl) from zero

Supersedes v1–v3. All questions resolved; nothing is blocking.

---

## 0. Current state, verified

**No code yet.** `src/`, `lib/`, `tests/`, `scripts/`, `examples/`, `Makefile` do not exist. Tracked: `CLAUDE.md`, `README.md`, `.gitignore`, `.claude/`, `docs/`, `include/vendor/`.

**`.gitignore`: fixed by the user.** Line 35 is `#*.lib`, line 51 is `# Makefile`. Commenting rather than deleting is the better choice — it leaves a visible trace of why a standard-looking line was disabled. Remaining work is cosmetic: drop the dead `build-*/` (line 45, a leftover from the abandoned `build-32b/` naming) and annotate lines 35 and 51 with *why* they're off, or a future tidy-up of the "CMake generated files" block will silently re-break `Makefile` tracking.

**SDK headers: present and tracked, but flat.** At `include/vendor/cdll.h`, `VIA.h`, `VIA_CDLL.h`; they belong in `include/vendor/capl-dll-sdk/`. `json.hpp` is about to land in `include/vendor/` too, and mixing Vector's three licensed headers flat beside nlohmann's one destroys the "what's foreign, and whose" separation that is the entire justification for `vendor/` in 04-FLOW section 3.3. It also keeps the licensing boundary confined to one directory.

**Struct mapping and request building remain conditional** per 04-FLOW section 4 (Step 6 "only if actually needed", Step 7 "(optional)").

---

## 1. Goal

Build the CAPL REST DLL (`restifycapl`) from the cloned repository to a working, dual-architecture (x86 + x64) native CANoe plugin: synchronous and asynchronous REST/HTTP for CAPL scripts plus JSON flattening and typed path accessors, with struct mapping and CAPL-side request building deferred until a real need appears. Local environment setup is scripted wherever possible. The CANoe ABI is proven on one trivial operation before any REST logic exists. All business logic is unit-tested outside CANoe. The export table stays a thin, append-only glue layer. Every version number derives from the Git tag.

---

## 2. Decisions — all resolved

| # | Decision |
|---|---|
| **Layout** | 04-FLOW section 3.3 authoritative: `src/core/`, `src/http/`, `src/registry/`, `src/mapping/`, `src/module/`; `lib/x86/`, `lib/x64/`; `build/x86/`, `build/x64/`; `include/vendor/`; `tests/`; `examples/`; plus `scripts/`. Export table at `src/module/exports.cpp` with `exports.def` and `version.rc` beside it. Headers sit next to their `.cpp`, so `include/` holds only `vendor/`. |
| **CANoe verification** | Manual CANoe verification after a green build + `tests/` suite, before release. |
| **SDK headers** | Committed to the repository. **The repo is public and the user has decided to keep them there anyway — an explicit, conscious call.** Consequence: CI compiles `src/module/` and links both DLLs; no split-responsibility CI design. |
| **Scope** | 04-FLOW section 4 exactly. Steps 0–5 and 8 in scope; Steps 6 and 7 conditional. |
| **libcurl** | vcpkg, `curl[schannel]:x86-windows-static` + `:x64-windows-static`. `/MT` by default. zlib transitive. Manual source build is fallback only. |
| **JSON** | nlohmann/json `json.hpp` pinned to **v3.11.3**, from the Releases page (amalgamated single file), SHA-256 verified. Not `git clone`. |
| **Release policy** | Build + test on every push → explicit manual approval gate → publish to GitHub Releases. |
| **Versioning** | Git tag `vX.Y.Z` is the single source of truth. Release: CI strips `v`, feeds `X.Y` to `/VERSION:` and `major,minor,build,revision` to `FILEVERSION`/`PRODUCTVERSION` via `rc.exe /D`. Local: placeholder `0.0` with build/revision from `git rev-list --count <tag>..HEAD`; `StringFileInfo` from `git describe --tags --always --dirty`. `version.rc` carries `#ifndef` fallbacks. Owned exclusively by `build-pipeline-engineer`. |
| **DLL names** | `build/x86/restifycapl-x86.dll` and `build/x64/restifycapl-x64.dll`. |
| **Make targets** | **Renamed for consistency: `build-x86` / `build-x64`** (plus `all`, `test`, `clean`). Every file referencing `build32`/`build64` is updated in Stage 0A. |
| **Toolchain** | **Visual Studio Build Tools, not the full VS IDE.** Development happens in VS Code. |
| **Environment setup** | Scripted via `scripts/setup-dev-env.ps1`; only the CANoe install and the in-CANoe smoke test stay manual. 04-FLOW section 6.2 is updated to match. |

---

## 3. Two explanations the user asked for

### 3.1 Why the `.def` file must not contain a `LIBRARY` line

A module-definition (`.def`) file is a small text file telling the linker which symbols a DLL exposes. Ours, `src/module/exports.def`, should contain only:

```
EXPORTS
    CAPLDLLEntryPoint
```

Some `.def` files also open with a `LIBRARY` line:

```
LIBRARY restifycapl-x86
EXPORTS
    CAPLDLLEntryPoint
```

That line names the module. It's a leftover from 16-bit Windows, where the name written in the `.def` genuinely determined the module's identity. On modern MSVC it is optional, and the linker's `/OUT:` flag decides the real filename — but the name still gets recorded inside the DLL, and when it disagrees with `/OUT:` some toolchains warn and older ones enforce it.

Why it matters *here* specifically: we build **two** DLLs with **two different filenames** (`restifycapl-x86.dll`, `restifycapl-x64.dll`) from **one shared** `exports.def`. A single `LIBRARY` line can only ever be correct for one of them. That would force one of two bad options:

- maintain **two nearly identical `.def` files** differing in one line — which reintroduces the exact "keep two files manually in sync" drift risk this whole plan is designed to remove, and does so on the most contract-critical file in the project; or
- **generate** the `.def` at build time — extra machinery for no benefit.

Omitting `LIBRARY` avoids the problem entirely: `/OUT:build/x86/restifycapl-x86.dll` and `/OUT:build/x64/restifycapl-x64.dll` each set their own name, and one `.def` serves both architectures. This is also normal modern practice regardless of our two-name situation.

**Concrete rule:** `exports.def` contains an `EXPORTS` section and nothing else. `code-reviewer` flags any added `LIBRARY` line as a defect.

### 3.2 Why Build Tools instead of the full Visual Studio — the user is right

The full VS IDE is not needed. What the build actually needs is `cl.exe`, `rc.exe`, `link.exe`, `dumpbin.exe`, the MSVC v143 toolset for **both** x86 and x64, the Windows SDK, and `vcvarsall.bat`. **Visual Studio Build Tools** (`vs_BuildTools.exe`) ships exactly that set without the IDE — smaller download, no IDE licensing considerations, and a better fit for a VS Code workflow.

It is also **more scriptable than the full IDE**, which changes the plan: silent unattended installation is a documented, supported flow, so this item **moves out of the manual P2 and into the automated P1 script**:

```
vs_BuildTools.exe --quiet --wait --norestart --nocache ^
  --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended
```

Caveats the script must handle: it requires elevation (detect and report rather than fail obscurely); the download is multi-GB; exit code `3010` means "success, reboot required" and must not be treated as failure; and it should first check for an existing installation via `vswhere.exe` at its fixed path (`%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe`) rather than reinstalling. `vswhere` is also the supported way to locate `vcvarsall.bat` afterwards.

Two consequences worth noting: Build Tools includes MSBuild, so the Vector sample project can still be compiled without the IDE (though *loading* it into CANoe stays manual); and CI is unaffected, since `windows-latest` already ships the MSVC toolchain and Stage 8 just activates it.

---

## 4. Stages

Constraints restated so executing agents need no access to `docs/.locals/`: **`/MT` static CRT for the DLL and every static dependency — never mix `/MT` and `/MD` in one link. x86 and x64 must both build and behave identically. The `CAPL_DLL_INFO4`/`CAPL_DLL_INFO_LIST` table is the real API contract — append-only, never rename/reorder/remove. Business logic must be unit-testable without CANoe. No version number is ever typed by hand.**

### Stage P — Bootstrap: scripted where possible, manual only where it must be

**P1 — `scripts/setup-dev-env.ps1` (automated).** Idempotent, re-runnable, safe to run twice, printing a pass/fail summary modelled on 04-FLOW section 6.9 rather than aborting at the first problem:

- Detect an existing MSVC toolchain via `vswhere`. If absent, download and silently install **VS Build Tools** with the `VCTools` workload per section 3.2. Detect non-elevated sessions and report the required command instead of failing obscurely; treat exit code `3010` as success-pending-reboot.
- Resolve and report the path to `vcvarsall.bat`; confirm `cl`, `rc`, `link`, `dumpbin` resolve for **both** architectures. Refuse to proceed silently in the wrong Native Tools environment — mixing the two is 04-FLOW section 6.3's named cause of hard-to-diagnose linker errors.
- Detect `make`; install via MSYS2/Chocolatey/Scoop if absent, or report the exact command when elevation is needed.
- Detect or bootstrap vcpkg; run `vcpkg install curl[schannel]:x86-windows-static` and `curl[schannel]:x64-windows-static`.
- Copy the resulting `.lib` files into `lib/x86/` and `lib/x64/`. Per 04-FLOW section 6.6 step 3, **copy into separate per-architecture folders** — do not point the Makefile at a shared vcpkg tree; the entire point is making a wrong-architecture link impossible.
- Download `json.hpp` from the pinned v3.11.3 Releases URL and **verify its SHA-256** before placing it in `include/vendor/`. A pin without a checksum is a pin in name only.
- Create the directory skeleton; write `lib/README` recording exact libcurl and json.hpp versions.
- **Verify** (never fetch) that `include/vendor/capl-dll-sdk/` holds the three SDK headers.

**P2 — manual, no automation path exists.**
- **Install Vector CANoe/CANalyzer.** Licensed, GUI-installed, machine-bound — blocked by licensing, not technology.
- **Build and load the official "Example of a Windows DLL for CAPL" sample unchanged in CANoe.** The step people skip and shouldn't: it proves the toolchain + CANoe pairing works before any project code can be blamed.

**Touches:** `scripts/setup-dev-env.ps1`, `lib/`, `include/vendor/`, `lib/README`.
**Agent:** `build-pipeline-engineer` (P1). **Human: all of P2.**
**Human approval before proceeding: YES** — approve the script before its first run (it installs software) and confirm P2 is done.

### Stage 0A — Reconcile configuration, naming, and permissions

Commit `b967c9b` fixed versioning in the config files but left the directory layout untouched. Combined with the new DLL names and renamed Make targets, five files now contradict 04-FLOW. Concrete proposed edits follow.

---

**File 1 — `CLAUDE.md`**

*Replace the `## Build` section:*

````markdown
## Build

- `make all` — builds both architectures (default target).
- `make build-x86` — builds `build/x86/restifycapl-x86.dll` (`/MACHINE:X86`).
- `make build-x64` — builds `build/x64/restifycapl-x64.dll` (`/MACHINE:X64`).
- `make test` — builds and runs the GoogleTest suite (outside CANoe).
- `make clean` — removes all build output and intermediate files.
- Dependencies live in `lib/x86/` and `lib/x64/`, matched to `/MT`.
- Both architecture targets are thin wrappers over a single parameterized
  rule, so compiler and linker flags cannot drift between x86 and x64.
- First-time local setup: `scripts/setup-dev-env.ps1`. It cannot install
  CANoe — that step is manual.
````

*Replace the `## Directory layout` block:*

````markdown
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
scripts/                setup-dev-env.ps1 — environment bootstrap
docs/                   project documentation
```

Folders map 1:1 onto dependency levels. `src/core/` must not import from
`src/http/`, `src/registry/`, or `src/mapping/`. Only `src/module/` may
include the CAPL SDK headers.
````

*Replace the Export contract bullet:*

````markdown
- **Export contract**: the real contract with CANoe is the
  `CAPL_DLL_INFO_LIST` / `CAPL_DLL_INFO4` table in `src/module/exports.cpp`,
  not just `src/module/exports.def`. Never rename, reorder, or remove an
  existing entry — see the `capl-export-contract` skill before touching
  this file.
````

*In `## Tech stack`, replace the dependencies line:*

````markdown
- Dependencies: libcurl (vcpkg static triplets, Schannel TLS backend), zlib
  (transitive), nlohmann/json v3.11.3 (header-only), plus Windows system
  libs (crypt32, bcrypt, secur32, ws2_32, normaliz, wldap32, advapi32).
- Toolchain: Visual Studio **Build Tools** (no IDE required); development
  in VS Code.
````

*Add a `## Scope` note after Tech stack:*

````markdown
## Scope

In scope now: sync + async REST/HTTP, JSON flattening, typed JSON path
accessors. Deferred until a demonstrated need: struct registry / JSON→struct
mapping, and CAPL-side request-body building. Do not build the deferred
modules pre-emptively.
````

---

**File 2 — `.claude/skills/msvc-build-conventions/SKILL.md`**

*Replace the "Architecture targets" bullets:*

````markdown
- `build-x86` compiles with `/MACHINE:X86`, links against `lib/x86/`,
  produces `build/x86/restifycapl-x86.dll`.
- `build-x64` compiles with `/MACHINE:X64`, links against `lib/x64/`,
  produces `build/x64/restifycapl-x64.dll`.
- `all` builds both; `test` and `clean` follow GNU target conventions.
- Both architecture targets must be thin wrappers over a **single
  parameterized rule**, with the architecture passed as a Make variable.
  Do not write two parallel recipes — flag drift between x86 and x64 is a
  top project risk, and one shared rule makes it structurally impossible.
- The two targets stay behaviorally identical: same source files, same
  flags (`/std:c++17`, `/EHsc`, `/MT`), same export table — only the
  architecture flag and library path differ.
````

*Replace the "Directory conventions" block:*

````markdown
lib/x86/, lib/x64/       static dependencies (.lib), built with /MT
build/x86/, build/x64/   build output (.dll, .lib, .exp, .res) — gitignored
include/vendor/          third-party headers (json.hpp, capl-dll-sdk/)
scripts/                 setup-dev-env.ps1 — environment bootstrap only,
                         never a second build system
````

*In the CI/CD section, replace the first bullet:*

````markdown
- The GitHub Actions workflow must invoke the same `build-x86`/`build-x64`
  Make targets used locally — do not duplicate the `cl.exe`/`rc.exe`
  invocation directly in YAML.
````

*Add a new section:*

````markdown
## Dependency acquisition

- libcurl: vcpkg, `curl[schannel]:x86-windows-static` and
  `curl[schannel]:x64-windows-static`. Static triplets are `/MT` by
  default — verify with `dumpbin /directives`, expecting
  `/DEFAULTLIB:LIBCMT` and never `MSVCRT`.
- zlib arrives transitively with libcurl.
- nlohmann/json: `json.hpp` pinned to v3.11.3, taken from the Releases page
  (amalgamated single file) and SHA-256 verified. Not `git clone`.
- Windows system libs, always link all of them: crypt32, bcrypt, secur32,
  ws2_32, normaliz, wldap32, advapi32.
- Record exact versions in `lib/README`.
- Toolchain: Visual Studio Build Tools with the `VCTools` workload; the
  full VS IDE is not required.
````

---

**File 3 — `.claude/skills/capl-export-contract/SKILL.md`**

*Frontmatter `description`:* replace "Load this before touching src/capl-rest-dll.cpp, includes/*.h, or the .def file." with "Load this before touching `src/module/exports.cpp`, `src/module/exports.def`, or anything in `include/vendor/capl-dll-sdk/`."

*Path replacements throughout:* `capl-rest-dll.def` → `src/module/exports.def`; `src/capl-rest-dll.cpp` → `src/module/exports.cpp` (both in "What the real contract is" and in the Rules section).

*Replace the first Bitness bullet pair:*

````markdown
- Both `build/x86/restifycapl-x86.dll` and `build/x64/restifycapl-x64.dll`
  must always be built and kept behaviorally identical (same exported
  table, same behavior) — only the target architecture differs.
````

*Add a new section:*

````markdown
## The .def file must not contain a LIBRARY statement

`src/module/exports.def` is shared by both architecture builds, which
produce two differently-named DLLs. It must contain only an EXPORTS
section:

    EXPORTS
        CAPLDLLEntryPoint

Do NOT add a `LIBRARY` line (e.g. `LIBRARY restifycapl-x86`). It pins one
internal module name into a file both builds share, so it can only ever be
correct for one of the two — forcing either duplicate .def files kept in
sync by hand, or build-time generation. Omitting it lets each build's
`/OUT:` flag alone determine its filename, so one .def serves both
architectures. `code-reviewer` flags any added LIBRARY line as a defect.
````

---

**File 4 — `.claude/agents/code-reviewer.md`**

*Frontmatter `description`:* "…any task that touched `src/`, `include/`, the `.def` file, or build scripts."

*Replace check #1:*

````markdown
1. **Export contract**: any change to the `CAPL_DLL_INFO_LIST` /
   `CAPL_DLL_INFO4` table in `src/module/exports.cpp`, or to
   `src/module/exports.def`. Flag renamed, reordered, removed, or retyped
   entries as a breaking change requiring explicit sign-off, per the
   `capl-export-contract` skill. Also flag any `LIBRARY` statement added
   to `exports.def`.
````

*Replace check #4 (Versioning) file references:* `version.rc`, `build32`, `build64` → `src/module/version.rc`, `build-x86`, `build-x64`.

*Insert a new check between current #3 and #4:*

````markdown
4. **Dependency direction**: `src/core/` must not include from `src/http/`,
   `src/registry/`, or `src/mapping/`. Only `src/module/` may include the
   CAPL SDK headers. A violation here is an architecture break, not a
   style issue.
````

(renumber the rest)

---

**File 5 — `.claude/agents/cpp-implementer.md`**

*Replace the first Responsibilities bullet:*

````markdown
- Implement and modify logic under `src/`, respecting the layered layout:
  - `src/core/` — type-conversion, json-path (pure logic, no I/O, no CANoe)
  - `src/http/` — http-client, sync-operations, async-operations
  - `src/registry/` — struct-registry (conditional, not yet in scope)
  - `src/mapping/` — json-flatten, json-accessors, struct-mapping
    (struct-mapping conditional, not yet in scope)
  - `src/module/` — exports.cpp: the ONLY file that knows about CANoe/CAPL
````

*In the second bullet,* name the file explicitly: "the code that fills `CAPL_DLL_INFO_LIST` in `src/module/exports.cpp` and the `extern "C"` wrapper functions".

*Add two hard rules:*

````markdown
- Never add a `LIBRARY` statement to `src/module/exports.def` — it is
  shared by both architecture builds. See `capl-export-contract`.
- Never import from a higher layer into a lower one. `src/core/` depends on
  nothing inside `src/` except the standard library and `json.hpp`.
````

*Replace workflow step 2:*

````markdown
2. Implement the change in the appropriate `src/` subfolder. Headers live
   beside their `.cpp` in the same folder — `include/` contains only
   `vendor/`, which is third-party code and is never edited by hand.
````

---

**`settings.json` — replacement `allow` array** (deny block unchanged):

```json
"allow": [
  "Bash(make)",
  "Bash(make all*)",
  "Bash(make build-x86*)",
  "Bash(make build-x64*)",
  "Bash(make test*)",
  "Bash(make clean*)",
  "Bash(cl.exe *)",
  "Bash(cl *)",
  "Bash(rc.exe *)",
  "Bash(rc *)",
  "Bash(link.exe *)",
  "Bash(link *)",
  "Bash(dumpbin.exe *)",
  "Bash(dumpbin *)",
  "Bash(vcpkg install *)",
  "Bash(vcpkg list*)",
  "Bash(git status)",
  "Bash(git diff *)",
  "Bash(git log *)",
  "Bash(git describe)",
  "Bash(git describe *)",
  "Bash(git rev-list *)",
  "Bash(git add *)",
  "Bash(git commit *)"
]
```

Rationale per change: Make entries become prefix patterns under the **new target names**, so the versioning mechanism can pass variables (`make build-x86 VER_MAJOR=0` would not match an exact-string entry and would prompt on every build); `Bash(make)` bare covers the default `all`; `dumpbin` (and now `cl`/`rc`/`link`) in **both** suffixed and bare forms, since bare invocation is common; bare `git describe` **and** the argument form, because the local-version path calls it bare; `vcpkg install`/`list` scoped rather than `vcpkg *`, so the allowlist doesn't cover state-mutating subcommands.

Deliberately **not** added: `Bash(powershell *)` or any wildcard shell entry — that is arbitrary code execution and would hollow out the deny list beneath it. If agents should run the bootstrap unprompted, scope it to exactly `"Bash(powershell -File scripts/setup-dev-env.ps1*)"`. Recommendation is to omit even that and run the bootstrap manually the first time, since it installs software.

Unchanged by design: **`git push` and `git tag` stay denied**, keeping 04-FLOW Step 0's push and every release tag a human action.

**Also in this stage:** move the three SDK headers into `include/vendor/capl-dll-sdk/`; update 04-FLOW section 6.2 to reflect that a local bootstrap script *is* used (the user confirmed this; leaving 6.2's "no script needed" conclusion in place would contradict the plan for the next reader, human or agent); and note in 04-FLOW section 6.3 that Build Tools, not the full IDE, is the target.

**Touches:** the five files above, `settings.json`, `include/vendor/capl-dll-sdk/`, 04-FLOW sections 6.2 and 6.3.
**Agent:** `build-pipeline-engineer` for the build-convention files. The `CLAUDE.md` and agent-definition edits are the user's or the main session's — no agent formally owns them, so route that deliberately rather than by default.
**Human approval before proceeding: YES.** These files govern every later stage; an error here silently misdirects every agent that follows.

### Stage 0B — Repo skeleton and Makefile with versioning wired in from day one

**Outcome:** `.gitignore` finished (drop dead `build-*/`; annotate lines 35 and 51). Full 04-FLOW section 3.3 tree. A single `Makefile` with `all`, `build-x86`, `build-x64`, `test`, `clean` — `clean` removing **all** intermediates — implemented over one parameterized per-architecture rule. GoogleTest built `/MT`. Windows system libs wired: crypt32, bcrypt, secur32, ws2_32, normaliz, wldap32, advapi32 — link all, they're transitively required by libcurl.

The **git-tag versioning mechanism is built in now, not retrofitted**: `version.rc` parameterized via `rc.exe /D VER_MAJOR/VER_MINOR/VER_BUILD/VER_REV` with `#ifndef` fallbacks, `/VERSION:X.Y` on the linker, `git describe --tags --always --dirty` into `StringFileInfo`, `git rev-list --count` for local build/revision. These files are being written fresh, so migration cost is zero.

Then commit and **push** (human action — agents are denied `git push`).
**Touches:** `.gitignore`, `Makefile`, `src/module/version.rc`, directory skeleton, `lib/README`.
**Agent:** `build-pipeline-engineer`.
**Human approval before proceeding: YES** — and confirm `dumpbin /directives` on every vendored `.lib` shows `/DEFAULTLIB:LIBCMT`, **not** `MSVCRT`. A `/MD` lib slipping through here poisons everything downstream.

### Stage 1 — "Hello DLL": one operation, verified inside CANoe

**Outcome:** a DLL exporting exactly **one** trivial operation (e.g. returning a fixed version string into a caller-supplied buffer), with `exports.def` (EXPORTS only, no `LIBRARY`) and `version.rc`, built for both architectures as `restifycapl-x86.dll` / `restifycapl-x64.dll`, and actually loaded and called from a real `.can` script in CANoe. No REST logic.

This stage permanently fixes the **CAPL-visible naming convention** for every operation the project will ever expose — the never-rename rule means it cannot be revised later without a major-version break. Decide it deliberately.

Two post-mortem rules are mandatory: (a) **never return a raw text pointer** from a CAPL-exposed operation — always write into a caller-supplied buffer with its size; one violation hid *every* operation in CANoe, not just the new one; (b) **1-byte packing must cover the entire export table through and including the terminating pointer** — pragma ordering is critical.

Verification is two-part: `dumpbin` export-list check, then the live CANoe test.
**Touches:** `src/module/exports.cpp`, `exports.def`, `version.rc`, `Makefile`.
**Agents:** `cpp-implementer` (table + glue), `build-pipeline-engineer` (link/resource wiring), then `code-reviewer` (mandatory).
**Human approval before proceeding: YES — the most important gate in this plan.** Do not start Stage 2 until the operation is confirmed callable from CAPL. This is exactly why 04-FLOW moves CANoe integration to Step 1 rather than leaving it until dozens of operations exist.

### Stage 2 — Level 0 pure logic: type conversion + JSON path resolution
**Outcome:** `src/core/type-conversion.*` and `src/core/json-path.*`, depending on `json.hpp` only — zero I/O, no CANoe knowledge, not yet exported. GoogleTest coverage: valid input, malformed/missing fields, type mismatches. `core/` imports nothing from `http/`, `registry/`, `mapping/`.
**Agents:** `cpp-implementer`, then `test-engineer`. **Human approval: no.**

### Stage 3 — HTTP layer + synchronous GET (logic only)
**Outcome:** `src/http/http-client.*` wrapping libcurl, `src/http/sync-operations.*` for blocking requests. Verified as a standalone console program against httpbin.org per 04-FLOW Step 2. Tests mock the libcurl boundary — no real network in the suite — covering simulated timeouts and error responses.
**Agents:** `cpp-implementer`, then `test-engineer`. **Human approval: no.**

### Stage 4 — Expose synchronous REST to CAPL (first contract append)
**Outcome:** sync operations appended to the export table, both architectures rebuilt, verified in CANoe.
**Agents:** `cpp-implementer`, then `code-reviewer` (mandatory).
**Human approval before proceeding: YES** — export contract append.

### Stage 5 — Asynchronous layer, response state designed correctly up front
**Outcome:** background dispatch plus readiness-check and wait-for-result operations. Response-state semantics settled **now, not retrofitted**: ready flag cleared once read, request ID tying a response to the call that produced it. 04-FLOW section 5 item 3 records that the previous iteration identified this early but never confirmed implementation. Shared state is global within the DLL with per-module synchronization; **one active response at a time** by deliberate design.
**Agents:** `cpp-implementer`, `test-engineer`, then `code-reviewer`.
**Human approval before proceeding: YES** — contract append, and the one-active-response simplification bounds what the DLL can ever do.

### Stage 6 — JSON flattening (highest user value — ship before struct mapping)
**Outcome:** flatten the response to a dot-notation key/value map, plus key count, key-by-index, value-by-key. 04-FLOW Step 4 is emphatic this is the highest-value API and that the previous iteration wrongly deprioritized it.
**Mandatory sub-step before any `.can` example:** verify associative-field syntax against the official CANoe help (`Help → CAPL → General → Associative Fields`). The correct form has **no extra keyword before the type** — `char[30] name[char[]];`. An invented keyword was copied across many docs and examples last time. Check the product help; do not trust generated snippets.
**Agents:** `cpp-implementer`, `test-engineer`, then `code-reviewer`.
**Human approval before proceeding: YES** — contract append; confirm the syntax check happened.

### Stage 7 — Typed JSON accessors
**Outcome:** typed point reads at a JSON path (integer/float/bool/string) without flattening, array helpers (length, element-by-index), optional cache for recent queries.
**Agents:** `cpp-implementer`, `test-engineer`, then `code-reviewer`.
**Human approval before proceeding: YES** — contract append.

### Stage 8 — CI/CD on GitHub Actions
Because the SDK headers are committed, **CI compiles `src/module/` and links both DLLs itself.**

**Outcome:** `windows-latest`, matrix over x86/x64, MSVC environment activated (`vcvarsall.bat` or equivalent) for the matching architecture before invoking Make, calling the **same `build-x86`/`build-x64` targets used locally** — never duplicating `cl.exe`/`rc.exe` in YAML. GoogleTest suite on every push. Both DLLs uploaded as workflow artifacts on every push, so a reviewable binary always exists. Release flow: tag push (`vX.Y.Z`) → build + test → **manual approval gate** (a GitHub Environment with required reviewers is the natural mechanism) → publish to GitHub Releases. Tag creation stays a human action.

Also here: generate the exposed-operation list from the export table at build time so documentation cannot drift from code — the previous iteration had different operation counts in different README files depending on when each was last edited.

Note the ordering: CI produces the artifact, the user verifies it manually in CANoe, and only then approves the publish. The approval gate is what makes manual CANoe verification a real precondition of release rather than an aspiration.
**Touches:** `.github/workflows/`, `Makefile`.
**Agent:** `build-pipeline-engineer`, then `code-reviewer`.
**Human approval before proceeding: YES** — this determines what ships.

### Stage 9 (CONDITIONAL) — Struct registry + JSON→struct mapping
Per 04-FLOW Step 6, build **only if** Stage 7's typed accessors prove insufficient for a concrete use case (large, stable response schemas).
**Agents:** `cpp-implementer`, `test-engineer`, `code-reviewer`. **Human approval: YES.**

### Stage 10 (CONDITIONAL) — CAPL-side request-body building
Per 04-FLOW Step 7, build **only if** hand-assembling JSON in CAPL proves genuinely cumbersome. Dead code last time.
**Agents:** as above. **Human approval: YES.**

### Stage 11 — Cleanup and consistency pass
**Outcome:** one `Makefile`, no historical variants; `clean` removes every intermediate; no build artifacts tracked; operation list generated, not hand-maintained; anything removed as dead code removed **in full** (table entry + implementation + docs) in a single commit.
**Agents:** `build-pipeline-engineer`, then `code-reviewer`. **Human approval: no**, unless it touches the export table.

---

## 5. Risks

**Export contract.** Stage 1 is irreversible in practice: the naming convention and version-entry layout chosen there bind every later append. Stages 4, 5, 6, 7, 9, 10 each append — each requires `code-reviewer`. The reserved first entry (`CDLL_VERSION_NAME`/`CDLL_VERSION`) must always be present. Exported functions must be `extern "C"` to keep names undecorated.

**ABI failure modes that hide *all* operations, not just the new one.** Raw text pointer instead of the caller-supplied buffer; incorrect 1-byte alignment coverage through the terminating pointer. 04-FLOW section 7 states plainly that this project's historical difficulty was never business logic — it was ABI compatibility with CANoe.

**`/MT` contamination.** A `/MD` libcurl/zlib/GoogleTest yields `LNK4098` at best and a second CRT heap inside the CANoe host process at worst. Static triplets are `/MT` by default, but verify with `dumpbin /directives` rather than trusting it. If a dependency is only available as `/MD`, **stop and ask**.

**Bitness parity.** CANoe loads only a matching-bitness DLL; a mismatch gives "Requested CAPL DLL is invalid". Identical source lists, identical flags, identical export table — only `/MACHINE:` and library path differ. **This risk drops materially because Stage 0B uses one parameterized rule** instead of two near-duplicate recipes. The related `.def` trap is neutralized by the no-`LIBRARY` rule in section 3.1. Environment trap from 04-FLOW section 6.3: building x64 from an x86 Native Tools prompt gives misleading linker errors.

**Vector SDK header redistribution — accepted risk.** The repository is public and the headers are committed. The user has made this call explicitly and knowingly; it is recorded here as a conscious decision rather than an oversight, and is not re-litigated. Practical note only: git history makes it effectively permanent, so if the position ever changes, removal requires history rewriting, not a delete commit.

**Configuration drift across five files.** `CLAUDE.md`, both skills and two agent definitions are actively wrong until Stage 0A lands. Agents load skills automatically, so a stale `capl-export-contract` skill pointing at `src/capl-rest-dll.cpp` with old DLL and target names would misdirect `cpp-implementer` and `code-reviewer` on the most safety-critical file in the project. Stage 0A must complete before Stage 1, not alongside it.

**A "tidy-up" regression in `.gitignore`.** The `Makefile` and `*.lib` rules are commented out, not deleted, and sit inside blocks labelled "CMake generated files" and "Compiled Static libraries" where they look like they belong. Without an inline note saying why they're disabled, someone — or some agent — will eventually restore them and silently untrack the Makefile and every vendored `.lib`.

**Bootstrap script scope creep.** `scripts/setup-dev-env.ps1` installs software and touches global state (Build Tools, vcpkg, package managers). It must stay idempotent and must never become a second build system — 04-FLOW section 5 explicitly warns against "a growing set of helper scripts" in place of one Makefile. The script sets up the environment; `make` builds the project. That line must not blur.

**Toolchain divergence between local Build Tools and the CI runner image.** Local setup installs Build Tools at whatever version is current; `windows-latest` ships its own. Usually harmless, but if a reproducibility question ever arises, pin the toolset version in both places rather than debugging a version-skew symptom.

**Versioning is new, untested machinery.** The tag mechanism removes the old three-places drift risk by design. What replaces it is narrow and mechanical: `FILEVERSION`/`PRODUCTVERSION` are four 16-bit fields capped at 65535 that **wrap silently** if exceeded — feed only small integers; `/VERSION:` accepts major.minor only; `version.rc` needs working `#ifndef` fallbacks so a bare `rc.exe` doesn't fail. `git describe --tags --always` carries both flags precisely so it works in a repo with no tags — this repo's current state. Test it deliberately in Stage 0B, before a real release depends on it.

**Steps no agent can verify.** Stage 1's CANoe test, Stage P2, and the pre-release verification in Stage 8 are human-only. 04-FLOW section 5's last checklist item names the previous iteration's habit of leaving these perpetually open: "status: ready to build" ≠ "status: tested and working".

**Scope creep toward struct mapping.** The strongest historical lesson: the previous iteration built the largest, most complex feature first while the highest-value one waited. Stages 6 and 7 must ship before Stage 9 is reconsidered.

---

## 6. Execution order

Stage P → 0A → 0B → 1 (hard gate) → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 11, with 9 and 10 only on demonstrated need. Stage 0A must not run in parallel with Stage 1.

**Source files (absolute paths):**
- `C:\Workspace\restifycapl\docs\.locals\04-FLOW-AND-DEPENDENCIES.md` — authoritative for layout and roadmap; sections 6.2 and 6.3 updated in Stage 0A
- `C:\Workspace\restifycapl\docs\.locals\capl-rest-dll-design-log.md` — section 7 is the versioning spec
- `C:\Workspace\restifycapl\CLAUDE.md`
- `C:\Workspace\restifycapl\.claude\skills\msvc-build-conventions\SKILL.md`
- `C:\Workspace\restifycapl\.claude\skills\capl-export-contract\SKILL.md`
- `C:\Workspace\restifycapl\.claude\agents\code-reviewer.md`, `...\cpp-implementer.md`, `...\build-pipeline-engineer.md`
- `C:\Workspace\restifycapl\.claude\settings.json`
- `C:\Workspace\restifycapl\.gitignore`
- `C:\Workspace\restifycapl\include\vendor\cdll.h`, `...\VIA.h`, `...\VIA_CDLL.h` — move to `include\vendor\capl-dll-sdk\`

**Status:** Approved by the user and finalized. Ready for Stage P.
