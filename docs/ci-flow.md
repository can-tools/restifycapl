# CAPL REST DLL – GitHub Actions Current Flow & Human Gates

Documentation and mapping of the current CI/CD flow and Human Gates for the `restifycapl` project.

---

## 1. End-to-End Overview

The diagram below illustrates the complete lifecycle of code: from pushing commits on a working branch, through automated Draft PR creation, CI pipeline execution, human decision points (CANoe verification, CHANGELOG cut), up to tagging and release publication with cryptographic build provenance.

```mermaid
flowchart TD
    classDef human fill:#ffe4e1,stroke:#ff69b4,stroke-width:2px;
    classDef bot fill:#e6f3ff,stroke:#4a90e2,stroke-width:2px;
    classDef gate fill:#fff3cd,stroke:#ffc107,stroke-width:2px;

    %% 1. Developer pushes branch
    H1["Human: Push commits to branch<br/>(stage/**, chore/**, fix/**, docs/**)"]:::human
    
    %% 2. Auto-PR
    APR_TRIGGER{"Bot: auto-pr.yml<br/>Verify guards"}:::bot
    APR_CREATE["Bot: Create Draft PR<br/>with dedicated checklist"]:::bot
    
    %% 3. CI Pipeline
    CI_TRIGGER{"Bot: ci.yml<br/>Trigger CI"}:::bot
    CI_X86["Bot: pipeline-x86<br/>(arch-pipeline.yml: build + test)"]:::bot
    CI_X64["Bot: pipeline-x64<br/>(arch-pipeline.yml: build + test)"]:::bot
    CI_TABLE["Bot: export-table<br/>(verify exports.cpp)"]:::bot
    
    GATE_X86{"Gate: gate-x86<br/>Required check"}:::gate
    GATE_X64{"Gate: gate-x64<br/>Required check"}:::gate
    
    %% 4. Human Verification & Checkpoints
    H2["Human: Review Draft PR checklist"]:::human
    H3["Human: Download DLL from CI & verify<br/>in Vector CANoe (if exports.cpp touched)"]:::human
    H4["Human: Cut CHANGELOG on chore/<br/>(fill in 12-char origin/main hash)"]:::human
    H5["Human: Squash-merge PR into main"]:::human
    
    %% 5. Release
    H6["Human: Create & push tag vX.Y.Z<br/>on squash-merge commit"]:::human
    REL_TRIGGER{"Bot: release.yml<br/>Trigger release process"}:::bot
    REL_VAL["Bot: validate<br/>(verify tag, ancestor & changelog)"]:::bot
    REL_X86["Bot: pipeline-x86<br/>(arch-pipeline.yml: build + test)"]:::bot
    REL_X64["Bot: pipeline-x64<br/>(arch-pipeline.yml: build + test)"]:::bot
    REL_TABLE["Bot: export-table<br/>(generate markdown table)"]:::bot
    REL_ASM["Bot: assemble<br/>(DLLs, SHA256SUMS, notes.md)"]:::bot
    REL_ATTEST["Bot: attest<br/>(Sigstore/OIDC build provenance)"]:::bot
    REL_PUB["Bot: publish<br/>(draft, verify, make public)"]:::bot
    
    H7["Human: Verify published release<br/>and SHA256 checksums"]:::human

    %% Connections
    H1 --> APR_TRIGGER
    APR_TRIGGER -->|Ahead of main & no PR| APR_CREATE
    APR_CREATE --> H2
    
    H1 --> CI_TRIGGER
    CI_TRIGGER --> CI_X86
    CI_TRIGGER --> CI_X64
    CI_TRIGGER --> CI_TABLE
    CI_X86 --> GATE_X86
    CI_X64 --> GATE_X64
    
    GATE_X86 --> H2
    GATE_X64 --> H2
    H2 --> H3
    H3 --> H4
    H4 --> H5
    H5 --> H6
    
    H6 --> REL_TRIGGER
    REL_TRIGGER --> REL_VAL
    REL_VAL --> REL_X86
    REL_VAL --> REL_X64
    REL_VAL --> REL_TABLE
    REL_X86 --> REL_ASM
    REL_X64 --> REL_ASM
    REL_TABLE --> REL_ASM
    REL_ASM --> REL_ATTEST
    REL_ATTEST --> REL_PUB
    REL_PUB --> H7
```

---

## 2. Detailed Flow: `auto-pr.yml`

Workflow executed on every push to working branches. Safely creates a Draft PR with a checklist tailored to the touched files.

```mermaid
flowchart TD
    classDef bot fill:#e6f3ff,stroke:#4a90e2,stroke-width:2px;
    classDef decision fill:#fff3cd,stroke:#ffc107,stroke-width:2px;
    classDef exit fill:#f8d7da,stroke:#dc3545,stroke-width:2px;

    START(["Push to stage/**, chore/**, fix/**, docs/**"]):::bot --> G1{"Guard 1: Is branch<br/>ahead of main?"}:::decision
    
    G1 -->|ahead_by == 0| STOP1["Stop (no new commits)"]:::exit
    G1 -->|ahead_by &gt; 0| G2{"Guard 2: Does an open PR<br/>already exist for branch?"}:::decision
    
    G2 -->|PR already exists| STOP2["Stop (PR already open)"]:::exit
    G2 -->|No open PR| COMPOSE["Analyze changed files (jq):<br/>• Plans only (plan_only)?<br/>• Was exports.cpp touched?<br/>• Extract commit subjects"]:::bot
    
    COMPOSE --> TEMPLATE["Compose PR body:<br/>• Merge checklist<br/>• Warning if exports.cpp touched<br/>• List of branch commits"]:::bot
    
    TEMPLATE --> CREATE["gh pr create --draft --base main<br/>--head BRANCH --title BRANCH"]:::bot
```

---

## 3. Detailed Flow: `ci.yml` and `arch-pipeline.yml`

CI continuously enforces architecture parity (x86 and x64) as well as compliance with the Vector CANoe export contract. The `build` and `test` jobs are **strictly separated and executed in parallel** on dedicated `windows-latest` runners.

```mermaid
flowchart TD
    classDef bot fill:#e6f3ff,stroke:#4a90e2,stroke-width:2px;
    classDef gate fill:#fff3cd,stroke:#ffc107,stroke-width:2px;
    classDef win fill:#e8f5e9,stroke:#4caf50,stroke-width:2px;

    TRIGGER(["Push (main, stage/**, chore/**, fix/**, docs/**)<br/>or Pull Request (main)"]):::bot --> FORK{"Parallel Fork"}:::bot

    %% x86 leg
    FORK --> X86_CALL["pipeline-x86 (arch-pipeline.yml)"]:::bot
    subgraph ArchPipelineX86 ["arch-pipeline.yml (x86) on windows-latest"]
        X86_B_PROV["Provision x86:<br/>• Activate MSVC<br/>• Pin VCPKG_ROOT ($RUNNER_TEMP)<br/>• Choco install make<br/>• vcpkg install x86-windows-static<br/>• Place libs (/MT)<br/>• dumpbin /directives (/MT check)"]:::win
        X86_B_MAKE["make build-x86"]:::win
        X86_B_EXP["dumpbin /exports<br/>(exactly 1 symbol: caplDllGetTable4)"]:::win
        X86_B_UP["Upload DLL: restifycapl-x86"]:::win
        
        X86_T_PROV["Provision x86 (isolated runner)"]:::win
        X86_T_MAKE["make test ARCH=x86 (GoogleTest)"]:::win
        
        X86_B_PROV --> X86_B_MAKE
        X86_B_MAKE --> X86_B_EXP
        X86_B_EXP --> X86_B_UP
        X86_T_PROV --> X86_T_MAKE
    end
    X86_CALL --> ArchPipelineX86

    %% x64 leg
    FORK --> X64_CALL["pipeline-x64 (arch-pipeline.yml)"]:::bot
    subgraph ArchPipelineX64 ["arch-pipeline.yml (x64) on windows-latest"]
        X64_B_PROV["Provision x64:<br/>• Activate MSVC<br/>• Pin VCPKG_ROOT ($RUNNER_TEMP)<br/>• Choco install make<br/>• vcpkg install x64-windows-static<br/>• Place libs (/MT)<br/>• dumpbin /directives (/MT check)"]:::win
        X64_B_MAKE["make build-x64"]:::win
        X64_B_EXP["dumpbin /exports<br/>(exactly 1 symbol: caplDllGetTable4)"]:::win
        X64_B_UP["Upload DLL: restifycapl-x64"]:::win
        
        X64_T_PROV["Provision x64 (isolated runner)"]:::win
        X64_T_MAKE["make test ARCH=x64 (GoogleTest)"]:::win
        
        X64_B_PROV --> X64_B_MAKE
        X64_B_MAKE --> X64_B_EXP
        X64_B_EXP --> X64_B_UP
        X64_T_PROV --> X64_T_MAKE
    end
    X64_CALL --> ArchPipelineX64

    %% export-table check
    FORK --> EXP_JOB["export-table (ubuntu-latest)<br/>list-export-table.ps1 on exports.cpp<br/>and tests/export-table/ fixtures"]:::bot

    %% Gates
    ArchPipelineX86 --> GATE_X86{"gate-x86 (ubuntu-latest)<br/>if: always()<br/>Requires pipeline-x86 == success"}:::gate
    ArchPipelineX64 --> GATE_X64{"gate-x64 (ubuntu-latest)<br/>if: always()<br/>Requires pipeline-x64 == success"}:::gate
```

---

## 4. Detailed Flow: `release.yml`

The `release.yml` workflow provides deterministic, secure, and atomic releases of `restifycapl-x86.dll` and `restifycapl-x64.dll`. It can be run in two modes:
1. **Release Mode (Push tag `v*.*.*`)**: full build injecting the version from the tag into the Windows RC resource, full GoogleTest suite, export table verification, `SHA256SUMS` calculation, `notes.md` generation, Sigstore OIDC cryptographic attestation, and secure multi-step publication to GitHub Releases.
2. **Dry-Run Mode (`workflow_dispatch`)**: triggered manually on any branch prior to merge or release. Executes identical validation of the changelog against `origin/main`, compilation (dev version), full GoogleTest suite for both architectures, notes generation, and Sigstore attestation. **The `publish` job is skipped** (`if: github.event_name == 'push' && github.ref_type == 'tag'`), verifying the entire pipeline without publishing anything externally.

### 4.1. Architecture and Artifact Flow Diagram: `release.yml`

The diagram below reflects the actual GitHub Actions DAG structure. Jobs `pipeline-x86`, `pipeline-x64`, and `export-table` declare `needs: validate`, meaning they fan out **concurrently and independently** upon completion of validation.

```mermaid
flowchart TD
    classDef bot fill:#e6f3ff,stroke:#4a90e2,stroke-width:2px;
    classDef decision fill:#fff3cd,stroke:#ffc107,stroke-width:2px;
    classDef win fill:#e8f5e9,stroke:#4caf50,stroke-width:2px;
    classDef pub fill:#d4edda,stroke:#28a745,stroke-width:2px;
    classDef art fill:#f1f8ff,stroke:#0366d6,stroke-width:1px,stroke-dasharray: 5 5;

    %% Triggers
    TRIG_TAG(["Trigger: push tag v*.*.*"]):::bot
    TRIG_DISPATCH(["Trigger: workflow_dispatch (dry-run)"]):::bot

    TRIG_TAG --> VAL
    TRIG_DISPATCH --> VAL

    %% Job 1: Validate
    VAL["<b>1. Job: validate</b> (ubuntu-latest)<br/>• Validate vX.Y.Z semver format & 16-bit fields (&le; 65535)<br/>• Ancestor test: commit &isin; origin/main<br/>• First-parent verification (GITHUB_SHA^)<br/>• Check no existing release in API (404)<br/>• Extract CHANGELOG.md section without 'Development'"]:::bot

    ART_NOTES[("Artifact: release-notes-body<br/>(notes-body.md)")]:::art
    VAL -.-> ART_NOTES

    %% Job 2: Parallel legs
    VAL --> PIPE_X86
    VAL --> PIPE_X64
    VAL --> EXP_TAB

    subgraph LegX86 ["2a. pipeline-x86 (arch-pipeline.yml on windows-latest)"]
        PIPE_X86["Run arch-pipeline.yml (arch=x86)"]:::win
        X86_B["build:<br/>• MSVC /MT compilation<br/>• Embed version in .rc resource<br/>• dumpbin /exports (exactly 1 symbol)"]:::win
        X86_T["test:<br/>• Dedicated windows-latest runner<br/>• make test ARCH=x86 (GoogleTest)"]:::win
        PIPE_X86 --> X86_B
        PIPE_X86 --> X86_T
    end

    subgraph LegX64 ["2b. pipeline-x64 (arch-pipeline.yml on windows-latest)"]
        PIPE_X64["Run arch-pipeline.yml (arch=x64)"]:::win
        X64_B["build:<br/>• MSVC /MT compilation<br/>• Embed version in .rc resource<br/>• dumpbin /exports (exactly 1 symbol)"]:::win
        X64_T["test:<br/>• Dedicated windows-latest runner<br/>• make test ARCH=x64 (GoogleTest)"]:::win
        PIPE_X64 --> X64_B
        PIPE_X64 --> X64_T
    end

    EXP_TAB["<b>2c. Job: export-table</b> (ubuntu-latest)<br/>• Run composite action .github/actions/export-table<br/>• Extract export table from exports.cpp"]:::bot

    ART_X86[("Artifact: restifycapl-x86<br/>(restifycapl-x86.dll)")]:::art
    ART_X64[("Artifact: restifycapl-x64<br/>(restifycapl-x64.dll)")]:::art
    ART_TAB[("Artifact: export-table-markdown<br/>(export-table.md)")]:::art

    X86_B -.-> ART_X86
    X64_B -.-> ART_X64
    EXP_TAB -.-> ART_TAB

    %% Job 3: Assemble
    ART_X86 -.-> ASM
    ART_X64 -.-> ASM
    ART_NOTES -.-> ASM
    ART_TAB -.-> ASM

    PIPE_X86 --> ASM
    PIPE_X64 --> ASM
    EXP_TAB --> ASM

    ASM["<b>3. Job: assemble</b> (ubuntu-latest)<br/>• Verify dist/: exactly two DLLs (x86 and x64)<br/>• Compute checksums: sha256sum restifycapl-*.dll > SHA256SUMS<br/>• Verify non-empty export table (&ge; 1 function)<br/>• Assemble notes.md: notes-body + instructions + table in &lt;details&gt;<br/>• Validate notes.md length (&le; 125,000 characters)"]:::bot

    ART_ASSETS[("Artifact: release-assets<br/>(dist/: DLLs, SHA256SUMS, notes.md)")]:::art
    ASM -.-> ART_ASSETS

    %% Job 4: Attest
    ASM --> ATT
    ART_ASSETS -.-> ATT

    ATT["<b>4. Job: attest</b> (ubuntu-latest)<br/>• Download release-assets<br/>• actions/attest-build-provenance for both DLLs<br/>• Verify: gh attestation verify (retry 3x10s)<br/>(Executed in both Release and Dry-Run modes)"]:::bot

    %% Branching
    ATT --> COND_PUBLISH{"Is this a tag push?<br/>(event == 'push' && ref_type == 'tag')"}:::decision

    COND_PUBLISH -->|NO: workflow_dispatch| DRY_DONE(["<b>Dry-Run Completed</b><br/>All assertions, tests, SHA256 sums,<br/>and attestations verified successfully.<br/>No public release created."]):::bot

    COND_PUBLISH -->|YES: tag v*.*.*| PUB

    %% Job 5: Publish
    PUB["<b>5. Job: publish</b> (ubuntu-latest)<br/>• Download release-assets (dist/)<br/>• Check 1: verify no existing release in API (404)<br/>• Check 2: delete any dangling draft release<br/>• Create draft release with 3 assets (DLLs + SHA256SUMS)<br/>• Rigorous draft verification:<br/>  - Fetch assets from GitHub API into verify/<br/>  - Verify checksums: sha256sum -c SHA256SUMS<br/>  - Verify body length (&le; 125,000 chars)<br/>• Make release public: gh release edit --draft=false"]:::pub

    PUB --> PUB_SUCCESS(["<b>Release Published Successfully</b><br/>Assets, checksums, and attestations live"]):::pub
```

### 4.2. Detailed `release.yml` Job Breakdown

| Job | Environment (`runs-on`) | Permissions (`permissions`) | Key Tasks & Assertions | Generated / Consumed Artifacts |
|---|---|---|---|---|
| `validate` | `ubuntu-latest` | `contents: read` | <ul><li>Full repository checkout (`fetch-depth: 0` – required for ancestor relationship check against `origin/main`).</li><li>Mode detection: tag `push` vs `dry-run` (`workflow_dispatch`).</li><li>For tag mode: validate semver regex format `vX.Y.Z` (no leading zeroes, each field $\le 65535$).</li><li>Assert `git merge-base --is-ancestor $GITHUB_SHA origin/main` (tag commit must be integrated into `main`).</li><li>Verify first parent (`GITHUB_SHA^` – squash-merge of the cut).</li><li>Check no existing release in GitHub API (must return 404).</li><li>Extract section from `CHANGELOG.md` (inline Python script), strip `### Development` header, and assert non-empty.</li></ul> | **Produces:** `release-notes-body` (`notes-body.md`).<br/>**Outputs:** `tag`, `version`. |
| `pipeline-x86` | Reusable workflow: `arch-pipeline.yml` (`windows-latest`) | `contents: read` | <ul><li>Runs two parallel jobs: `build` and `test` on independent Windows runners.</li><li>In `build`: MSVC `/MT` compilation (static CRT), inject version from tag into `restifycapl.rc`, verify directive flags, inspect export table (`dumpbin /exports` – exactly 1 symbol: `caplDllGetTable4`).</li><li>In `test`: provision test environment and execute full GoogleTest suite for x86.</li></ul> | **Produces:** `restifycapl-x86` (`restifycapl-x86.dll`). |
| `pipeline-x64` | Reusable workflow: `arch-pipeline.yml` (`windows-latest`) | `contents: read` | <ul><li>Identical scope to x86, but compiled and tested natively for 64-bit architecture (`x64`).</li><li>Guarantees strict binary and functional parity across architectures.</li></ul> | **Produces:** `restifycapl-x64` (`restifycapl-x64.dll`). |
| `export-table` | `ubuntu-latest` | `contents: read` | <ul><li>Runs composite action `.github/actions/export-table`.</li><li>Parses `src/exports.cpp` and runs regression tests against `tests/export-table/` fixtures.</li><li>Generates formatted Markdown table containing CAPL function names, return types, and parameter signatures.</li></ul> | **Produces:** `export-table-markdown` (`export-table.md`). |
| `assemble` | `ubuntu-latest` | `contents: read` | <ul><li>Waits for completion of: `validate`, `pipeline-x86`, `pipeline-x64`, `export-table`.</li><li>Downloads compiled DLLs and generated Markdown files.</li><li>Asserts contents of `dist/`: must contain exactly and only `restifycapl-x64.dll` and `restifycapl-x86.dll`.</li><li>Computes checksums: `sha256sum restifycapl-x86.dll restifycapl-x64.dll > dist/SHA256SUMS`.</li><li>Asserts that export table contains $\ge 1$ valid function row.</li><li>Assembles final `dist/notes.md` combining: changelog body, `### Using this release` block, and collapsible `<details>` containing the export table.</li><li>Validates notes length against GitHub API limit ($\le 125,000$ characters).</li><li>Appends preview of `SHA256SUMS` and `notes.md` to `$GITHUB_STEP_SUMMARY`.</li></ul> | **Produces:** `release-assets` (contains `dist/`: both DLLs, `SHA256SUMS`, `notes.md`). |
| `attest` | `ubuntu-latest` | `id-token: write`<br/>`attestations: write`<br/>`contents: read` | <ul><li>Downloads `release-assets` bundle.</li><li>Generates cryptographic Sigstore OIDC build provenance attestations for `restifycapl-x86.dll` and `restifycapl-x64.dll`.</li><li>Immediately verifies generated attestations using `gh attestation verify` with retry loop (3 attempts, 10s backoff, mitigating Sigstore index propagation delays).</li><li>Executed in both production and dry-run modes!</li></ul> | Confirms entry in GitHub Sigstore transparency log. |
| `publish` | `ubuntu-latest` | `contents: write` | <ul><li>Conditional: executed **only** on tag `push` (`if: github.event_name == 'push' && github.ref_type == 'tag'`).</li><li>Implements multi-step publication atomicity guards (see Section 4.3).</li><li>Publishes release via `gh release edit "$TAG" --draft=false`.</li><li>On failure, deterministic diagnostics step `if: failure()` inspects API state, reports whether release stayed private or went public, and prints recovery instructions.</li></ul> | **Publishes:** Official GitHub Release with 3 assets (`restifycapl-x86.dll`, `restifycapl-x64.dll`, `SHA256SUMS`). |

### 4.3. Seven Strict Publishing Guards in the `publish` Job

Releasing native libraries for industrial automation and Vector CANoe automotive environments requires absolute guarantees of integrity. Therefore, the `publish` job in `release.yml` implements 7 strict safeguards:

1. **Double tag uniqueness check**: Checking that no published release already exists for the tag is performed first in `validate`, and repeated in `publish` immediately prior to repository mutation (`gh api repos/.../releases/tags/$TAG` must return HTTP 404). Once published, a tag must never be overwritten.
2. **Dangling draft cleanup**: If a previous run was aborted before publication, the script finds and deletes existing drafts for this tag (`DELETE /releases/{id}`), guaranteeing idempotent re-runs.
3. **Draft-First publication pattern**: The release is created in private mode as a draft (`gh release create "$TAG" --draft --verify-tag`). No external user or automation process has access until all assets are successfully uploaded and validated.
4. **Server-side asset completeness verification**: After draft creation, the script queries GitHub API to verify that the uploaded asset list is exactly and only: `SHA256SUMS`, `restifycapl-x64.dll`, and `restifycapl-x86.dll`.
5. **On-the-fly checksum validation (`sha256sum -c`)**: Assets are downloaded directly from GitHub servers via `/releases/assets/{id}` into a clean `verify/` folder, followed by `sha256sum -c SHA256SUMS`. This proves that files on GitHub servers were not corrupted or truncated during transit.
6. **Atomic release publication (`--draft=false`)**: Transitioning from draft to public occurs in a single, final invocation: `gh release edit "$TAG" --draft=false`.
7. **Deterministic failure diagnostics (`if: failure()`)**: If any step in `publish` fails, the `if: failure()` hook inspects the release state via API. If the release never went public, the maintainer is instructed that deleting the tag and retrying is safe. If it became public, the script warns never to move the tag and instructs to follow the yank/withdraw procedure and release a new patch version.

---

## 5. Human Gates Table

| Gate ID | Timing | Human Action | Why Not Automated Today |
|---|---|---|---|
| **HG-1** | Start of work | Creates branch (`stage/**`, `chore/**`, `fix/**`, `docs/**`) and pushes initial commit. | Architectural decision to initiate a new task. |
| **HG-2** | After Draft PR creation | Reviews PR checklist: verifies CI is green, reviews clean, plan executed. | Formal quality and policy approval requirement prior to merge. |
| **HG-3** | Before merging changes to `exports.cpp` | Downloads compiled DLLs from CI, launches CANoe workstation (Windows VM / hardware), and verifies CAPL script compilation. | **No Vector CANoe licenses available on GitHub-hosted runners**. CANoe requires a hardware USB dongle or on-premise license server. |
| **HG-4** | Release preparation | Creates branch `chore/bpe-14-changelog-...`, retrieves `git rev-parse --short=12 origin/main`, writes parent `main` commit hash into `CHANGELOG.md` header, and updates diff links. | Strict project convention: product version cannot be hardcoded in files; changelog section is explicitly anchored to the base commit hash. |
| **HG-5** | After changelog approval | Performs Squash-Merge of PR into `main`. | Human maintainer decision on when to integrate the release into `main`. |
| **HG-6** | After squash-merge | Creates and pushes tag `vX.Y.Z` on the merge commit on `main`. | By architectural design, **pushing the tag is the sole and definitive publication trigger** (replaces approval gates inside GitHub Actions). |
| **HG-7** | After publication | Inspects published GitHub Release and SHA256 checksums. | Final sanity sign-off by repository maintainer. |
