# README rewrite: audit and redraft `README.md`

## Goal

Replace the current `README.md` (108 lines) with a README that follows the `readme-writer` structure and meets this project's `project-docs` constraints, written for a first-time GitHub visitor who wants to call the DLL from CAPL. Every claim must be checked against the repo.

## Settled decisions

- `docs-writer` is the only agent that edits `README.md`. Where `project-docs` and `readme-writer` conflict, `project-docs` wins.
- The main Usage example is a synchronous GET, labelled as compile-checked only.
- Create an empty `LICENSE` placeholder. The License section wording below is approved.
- The Roadmap is drafted separately and only goes into the README after the user approves it.
- Keep the Vector SDK headers and `docs/vector-capl-dll-docs/` in the repo as they are.
- The project does not track the CANoe 13+ `.vmodule` mechanism.

## Stage 1: audit and redraft (responsible: `docs-writer`; human approval before it starts: no)

Follow your `readme-writer` workflow in audit mode:
- read `references/readme-rules.md` fresh;
- list the gaps in the current `README.md`;
- produce the complete revised file.

`project-docs` takes precedence over `readme-writer`. Report every deviation.

**Hard constraints (repeated here so you don't rely on other documents):**
- The repo is **public**.
- There is no tagged release. The only way to get the DLL is to build it from source.
- **Verification status.** State only this:
  - The CAPL compiler recognises every exported function in real CANoe. The x64 build has been confirmed. The x86 build is built and unit-tested in CI on every push.
  - No request has been run from a live measurement yet.
  - Name no specific CANoe version.
  - Never make up example output.
- JSON features do not exist yet. Never mention struct mapping or CAPL-side request building anywhere.
- No internal stage numbers, task IDs or plan references in README prose or TODO lines. Write "once tagged releases are published", not "at Stage 14".
- Anything not yet applicable gets a visible, plain-language `TODO:` line. It must not be silently left out, except for items listed under Skip below.
- Never tell readers to run `make all`. It fails in a single shell at the other architecture's link step. Show `make build-x64` and `make build-x86`, each run from an MSVC developer shell for that architecture.
- Every operation name must match `src/module/exports.cpp` exactly. There are 18 rows today:
  - Common: `restifyReadVersion`
  - Sync: `restifyGetSync`, `restifyDeleteSync`, `restifyPostSync`, `restifyPutSync`, `restifyPatchSync`, `restifyRequestSync`
  - Async: `restifyGetAsync`, `restifyDeleteAsync`, `restifyPostAsync`, `restifyPutAsync`, `restifyPatchAsync`, `restifyRequestAsync`, `restifyPollResponse`, `restifyAwaitResponse`, `restifyReadResponse`, `restifyDiscardResponse`, `restifyDiscardAllResponses`

  Re-check the list yourself against the file.
- No git commands and no build commands.

**Sections, in this order:**

1. **`# restifycapl`**
2. **Badges:** the CI badge only.
   - Image: `https://github.com/can-tools/restifycapl/actions/workflows/ci.yml/badge.svg?branch=main`
   - Link target: the workflow page.
   - Add an HTML comment `<!-- TODO: version badge once tagged releases are published; license badge once a license is chosen -->`. This is the one allowed invisible TODO. List both badges in your TODO report as well.
3. **One-line description.** Use this text exactly: "Native Windows DLL for Vector CANoe that gives CAPL scripts synchronous and asynchronous REST/HTTP calls (libcurl + Schannel), built for x86 and x64". It must stay identical to the GitHub About text.
4. **Project status**, as a `> [!NOTE]`:
   - pre-1.0, no tagged release, build from source;
   - the CAPL compiler recognises the exports in CANoe (x64 confirmed);
   - requests have not yet been run from a live measurement;
   - JSON support is not available yet.
5. **Features**, 4–6 bullets:
   - sync GET/POST/PUT/PATCH/DELETE, plus a general request function with explicit timeouts and a response-size cap;
   - async dispatch/poll/read/discard, the realtime-safe way to use REST from a Simulation Setup node;
   - TLS through Windows Schannel (no OpenSSL);
   - a single DLL with the runtime (`/MT`) and libcurl statically linked;
   - x86 and x64 builds;
   - TLS certificate verification is always on and cannot be turned off from CAPL.

   Check each bullet against `CLAUDE.md`, `docs/http-layer.md`, `docs/capl-sync-surface.md` and `docs/capl-async-surface.md`. Mark anything you can't confirm with `TODO`.
6. **Table of contents**, covering all `##` headings. The target length is about 120–150 lines.
7. **Why this exists**: one short paragraph.
   - Why both sync and async: blocking calls are not acceptable on CANoe's realtime branch (source: `docs/capl-async-surface.md`, "Realtime-safety summary").
   - Why Schannel: covered by CLAUDE.md.
   - Do **not** claim that CAPL lacks a built-in HTTP client unless you find repo evidence for it. Otherwise leave a `TODO` or rephrase so the claim isn't needed.
8. **Requirements:**
   - Windows;
   - a CANoe installation whose bitness matches the DLL (32-bit CANoe needs the x86 DLL, 64-bit CANoe needs the x64 DLL, and CANoe refuses a mismatched one);
   - for building: what `scripts/setup-dev-env.ps1` provisions (MSVC Build Tools, `make`, vcpkg-built libcurl, the pinned `json.hpp`).
9. **Building the DLL:**
   - State plainly that running the setup script is **required** on a fresh clone, because no compiled dependency is committed.
   - Run it once from an elevated PowerShell prompt at the repo root: `.\scripts\setup-dev-env.ps1`.
   - Then run `make build-x64` or `make build-x86`. The outputs are `build/x64/restifycapl-x64.dll` and `build/x86/restifycapl-x86.dll`.
   - **Move** the switch table and the two ExecutionPolicy variants from the current README into `docs/development-environment.md`. They don't exist there today. Leave a one-line link in the README.
   - Add: `TODO: download instructions once tagged releases are published on GitHub Releases.`
10. **Usage from CAPL:**
    - (a) Choose the DLL that matches CANoe's bitness.
    - (b) Reference the DLL from the CAPL program's includes section with `#pragma library("<path to the DLL>")`, or register it for all CAPL programs in CANoe's Options dialog. Source: `docs/vector-capl-dll-docs/CAPLIncludeWindowsDLL.htm.md` and `CAPLIncludeWindowsDLLExample.htm.md`. Add one neutral sentence, with no version numbers and no TODO: "How CANoe resolves the path given to `#pragma library` can differ between CANoe versions; consult the CAPL DLL documentation for your version." Do not mention `.vmodule` or module description files. Do not link to any VModule document.
    - (c) **One minimal `restifyGetSync` snippet**, labelled directly above or below it: "Accepted by the CAPL compiler in CANoe; not yet run against a live server in a measurement." Constraints:
      - Put the call in an `on key '<char>'` handler of a **Measurement Setup or test node**. The `on key 'a'` form is confirmed in `docs/vector-capl-dll-docs/capl/CAPLfunctionOnTimer.htm.md`. Do not use `on start`: the call blocks and would hold up measurement start.
      - Declare every text argument as a `char[]` variable, initialised from a string literal where needed, one element longer than the text. This is confirmed in `docs/vector-capl-dll-docs/capl/CAPLfunctionsStringLiteral.htm.md` and `VariablesDeclarationInitialization.htm.md`. Pass `elcount()` of each as its size. For headers, pass an empty array (`char noHeaders[1] = "";`).
      - Use the signature `long restifyGetSync(char url[], dword urlSize, char requestHeaders[], dword requestHeadersSize, char responseBody[], dword responseBodySize, long& httpStatusCode, dword& responseBodyLength)`. Pass the `long`/`dword` out-variables as plain variables.
      - Check whether the return value is `0` (Ok; see `docs/status-codes.md`), then `write()` out `httpStatusCode` and `responseBodyLength`. Check the format specifiers against `CAPLFunctionsWriteFormatExpressions.htm.md`.
      - Use `https://example.com/` as the URL. It is a reserved example domain, not a project endpoint.
      - Directly below the snippet: `TODO: expected output once verified in a running measurement.`
    - (d) **Realtime caveat, made prominent:** sync calls, `restifyAwaitResponse` and `restifyDiscardAllResponses` block or free memory on the calling thread. Use them from Measurement Setup or test nodes only. From Simulation Setup nodes, use async dispatch, then poll with `restifyPollResponse` from an `on timer` handler, then `restifyReadResponse`.
    - (e) A short async outline. A `<details>` block is fine. Link to `docs/capl-async-surface.md`.
    - Note that argument rules (declared `char[]` plus `elcount()`, the header-block format) are in `docs/capl-sync-surface.md`. Do not state "string literals cannot be passed as arguments" as a Vector-backed fact: `CAPLfunctionsStringLiteral.htm.md` covers literals and escapes (including `\"`) but not passing literals as function arguments.
    - Add: `TODO: link to runnable examples/ once they exist.` Do not link to `examples/` now; it contains only `.gitkeep`.
11. **Operations:**
    - A compact table grouped Common / Sync / Async, one line of purpose per operation, with no full signatures.
    - Link to `docs/capl-sync-surface.md`, `docs/capl-async-surface.md` and `docs/status-codes.md`.
    - Add: `TODO: JSON operations are not available yet.`
    - Replace the operations list that is currently misplaced under `## Build`.
12. **Development and testing:**
    - `make test` (defaults to x64);
    - CI builds and tests x86 and x64 on every push;
    - a link to `CLAUDE.md` for layout and conventions. Don't duplicate its content.
13. **Roadmap: do not put this in `README.md`.** Draft it separately in your report for user approval. Limit it to planned, non-conditional work: JSON flattening, typed JSON accessors, tagged releases. No dates, no stage numbers. Leave no placeholder in the README for it.
14. **Changelog:** one line linking `CHANGELOG.md`.
15. **Support:** GitHub Issues for this repo (`https://github.com/can-tools/restifycapl/issues`). Mark `TODO: confirm Issues are enabled` in your report, not in the README.
16. **Contributing:** "Currently maintained by a single maintainer; pull requests are welcome and will be reviewed." Point to `CLAUDE.md` for conventions. Add one line: "Please note that no license has been chosen yet (see License)."
17. **License**, last. Create an **empty** `LICENSE` file at the repo root (zero bytes, no text). Use this section text exactly:

    > No license has been chosen yet — `LICENSE` is a placeholder. Until one is added, no permission to use, modify or redistribute this code is granted beyond what GitHub's Terms of Service allow for public repositories. TODO: choose a license.

    After that, you may add one neutral line crediting the bundled third-party components (libcurl, nlohmann/json, Vector CAPL DLL SDK headers). Do not say anything about what their licenses mean for this project.

**Skip entirely, with no trace:** banner/logo, Visuals, a separate Security section, Maintainers, coverage badge.

**Additional file edit:** in `docs/capl-sync-surface.md`, the `categoryName` table row for `Async` still says "Reserved for Stage 11's async exports. Nothing uses it yet." Replace it with the actual async rows. Remove the stage reference; `project-docs` bans identifiers like that in docs prose as well.

**Your report must include:**
1. the gap list for the current README;
2. the complete new `README.md`;
3. what you moved into `docs/development-environment.md` (exact text);
4. the `docs/capl-sync-surface.md` edit;
5. confirmation that the empty `LICENSE` was created;
6. the **proposed Roadmap text**, marked "for user approval, not merged";
7. the `readme-rules.md` checklist, item by item. Expected accepted deviations: "no leftover TODO", "License present", "examples tested in CI" (CAPL can't run in CI), "expected output shown";
8. every `TODO` and assumption.

## Stage 2: verification review (responsible: `code-reviewer`; human approval before it starts: no)

- Check every command, path, link and operation name against the repo and `src/module/exports.cpp`.
- Confirm the `restifyGetSync` snippet matches its export-table row: 8 parameters, with the last two by reference.
- Confirm there are no leftover references to `.vmodule`, stage numbers or task IDs in the README.
- Confirm the Roadmap is not in `README.md`.
- Run the `readme-rules.md` checklist.

## Stage 3: Roadmap approval (responsible: human; approval gate: yes)

The user approves, edits or drops the Roadmap text. If approved, a small `docs-writer` pass inserts it between "Development and testing" and "Changelog".

## Stage 4: README approval and commit (responsible: human; approval gate: yes)

The user approves the whole README, then commits and pushes by hand.

## Risks

- **Snippet drift.** The snippet's argument order or count could drift from the export row. The compiler accepts the table as written, so a wrong README snippet fails only for the user.
- **Overstated verification.** Wording could suggest requests have run successfully in a measurement.
- **Roadmap leak.** The Roadmap could reach `README.md` before approval.
- **Stage numbers.** A stage number could slip into a TODO line.
- **Export contract and bitness.** Not affected. This work touches no `src/`, `.def`, build or CI file.
