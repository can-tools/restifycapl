# CAPL REST DLL – GitHub Actions Target Flow & Optimization Roadmap

Documentation of the target CI/CD flow model following optimizations and the automation roadmap for the `restifycapl` project.

Strict architectural constraints:
- **`build` and `test` jobs remain 100% separated and parallel** for both architectures (`x86` and `x64`).
- Strict export contract (`caplDllGetTable4`, `exports.cpp`) and static `/MT` runtime libraries are fully preserved.
- Product version (`vX.Y.Z`) is strictly derived from the Git tag.

---

## 1. Target End-to-End Flow

The diagram below illustrates the optimized CI/CD workflow. Automated tasks take over repetitive operations (tool provisioning, GoogleTest unit testing, export table validation, changelog cutting, tagging, and publication). The engineer's focus is concentrated on CANoe integration verification (locally pre-push or during PR approval) and substantive code review.

```mermaid
flowchart TD
    classDef human fill:#ffe4e1,stroke:#ff69b4,stroke-width:2px;
    classDef bot fill:#e6f3ff,stroke:#4a90e2,stroke-width:2px;
    classDef auto fill:#d4edda,stroke:#28a745,stroke-width:2px;
    classDef gate fill:#fff3cd,stroke:#ffc107,stroke-width:2px;

    DEV["Developer: Local development (verify in CANoe pre-push if exports.cpp changed)<br/>Push commits to working branch"]:::human
    
    subgraph AutoPR ["auto-pr.yml"]
        APR_GUARD{"Verify PR preconditions"}:::bot
        APR_ACTION["Create Draft PR & checklist"]:::bot
    end

    subgraph CI ["ci.yml (Optimized)"]
        CI_FAST_PROV["Fast Provisioning: GHA vcpkg cache & standalone Make"]:::auto
        
        subgraph X86_TRACK ["x86 Track (Independent Jobs)"]
            X86_BUILD["build-x86: Compile DLL /MT"]:::bot
            X86_TEST["test-x86: GoogleTest x86"]:::bot
        end
        
        subgraph X64_TRACK ["x64 Track (Independent Jobs)"]
            X64_BUILD["build-x64: Compile DLL /MT"]:::bot
            X64_TEST["test-x64: GoogleTest x64"]:::bot
        end
        
        EXP_CHECK["export-table: Verify exports.cpp contract"]:::bot
        
        GATES{"Aggregate CI status checks (gate-x86, gate-x64)"}:::gate
    end

    REVIEW["Human Gate: Code review & Gate HG-3 (CANoe verification if not pre-push)"]:::human
    AUTO_MERGE["Automated merge to main with 12-char parent hash computation"]:::auto
    AUTO_TAG["Bot: Automated tag creation vX.Y.Z on main"]:::auto

    subgraph ReleasePipeline ["release.yml (Zero-Touch Release)"]
        REL_VAL["Validate tag & changelog"]:::bot
        REL_BUILD_X86["Compile release x86 with version (build + test)"]:::bot
        REL_BUILD_X64["Compile release x64 with version (build + test)"]:::bot
        REL_TABLE["Generate export table"]:::bot
        REL_ASM["Assemble release bundle, SHA256SUMS & notes.md"]:::bot
        REL_ATTEST["Sigstore OIDC build provenance attestation"]:::bot
        REL_PUB["Automated GitHub Release publication"]:::auto
    end

    DEV --> APR_GUARD
    APR_GUARD -->|Branch ahead of main & no PR| APR_ACTION
    APR_ACTION --> CI_FAST_PROV
    DEV --> CI_FAST_PROV

    CI_FAST_PROV --> X86_BUILD
    CI_FAST_PROV --> X86_TEST
    CI_FAST_PROV --> X64_BUILD
    CI_FAST_PROV --> X64_TEST
    CI_FAST_PROV --> EXP_CHECK
    
    X86_BUILD --> GATES
    X86_TEST --> GATES
    X64_BUILD --> GATES
    X64_TEST --> GATES
    EXP_CHECK --> GATES

    GATES --> REVIEW
    REVIEW --> AUTO_MERGE
    AUTO_MERGE --> AUTO_TAG
    AUTO_TAG --> REL_VAL

    REL_VAL --> REL_BUILD_X86
    REL_VAL --> REL_BUILD_X64
    REL_VAL --> REL_TABLE
    REL_BUILD_X86 --> REL_ASM
    REL_BUILD_X64 --> REL_ASM
    REL_TABLE --> REL_ASM
    REL_ASM --> REL_ATTEST
    REL_ATTEST --> REL_PUB
```

---

## 2. Optimized Compilation & Test Pipeline (`arch-pipeline.yml`)

`build` and `test` jobs remain strictly decoupled across independent Windows runners, while their wall-clock runtimes are reduced through toolchain and caching optimizations.

```mermaid
flowchart TD
    classDef bot fill:#e6f3ff,stroke:#4a90e2,stroke-width:2px;
    classDef opt fill:#d4edda,stroke:#28a745,stroke-width:2px;
    classDef gate fill:#fff3cd,stroke:#ffc107,stroke-width:2px;

    TRIG(["Trigger: push or pull_request"]):::bot --> FORK{"Task Fork"}:::bot

    subgraph ProvisioningModel ["Fast Provisioning Model (&lt; 2s restore)"]
        VCPKG_CACHE["vcpkg Binary Cache: GHA Backend x-gha"]:::opt
        FAST_MAKE["Standalone GNU Make (eliminates Chocolatey overhead)"]:::opt
    end

    FORK --> ProvisioningModel

    subgraph ArchX86 ["arch-pipeline.yml (x86 Architecture)"]
        X86_B["Job 1: build-x86<br/>• Compile DLL /MT<br/>• dumpbin /directives<br/>• dumpbin /exports (exactly 1 function)<br/>• Upload DLL artifact"]:::bot
        X86_T["Job 2: test-x86<br/>• Parallel GoogleTest execution<br/>• Isolated test runner"]:::bot
    end

    subgraph ArchX64 ["arch-pipeline.yml (x64 Architecture)"]
        X64_B["Job 3: build-x64<br/>• Compile DLL /MT<br/>• dumpbin /directives<br/>• dumpbin /exports (exactly 1 function)<br/>• Upload DLL artifact"]:::bot
        X64_T["Job 4: test-x64<br/>• Parallel GoogleTest execution<br/>• Isolated test runner"]:::bot
    end

    ProvisioningModel --> X86_B
    ProvisioningModel --> X86_T
    ProvisioningModel --> X64_B
    ProvisioningModel --> X64_T

    subgraph Contracts ["Export Contract Verification"]
        EXP_TAB["export-table (ubuntu-latest):<br/>list-export-table.ps1 on exports.cpp"]:::bot
    end

    FORK --> EXP_TAB

    X86_B --> GATE_X86{"gate-x86"}:::gate
    X86_T --> GATE_X86
    X64_B --> GATE_X64{"gate-x64"}:::gate
    X64_T --> GATE_X64
    EXP_TAB --> GATE_ALL{"Primary CI Gate"}:::gate
    GATE_X86 --> GATE_ALL
    GATE_X64 --> GATE_ALL
```

---

## 3. Release Automation Pipeline (`release.yml`)

Clear DAG dependency graph without synthetic intermediary nodes, preserving full parallelism across `pipeline-x86`, `pipeline-x64`, and `export-table`.

```mermaid
flowchart TD
    classDef bot fill:#e6f3ff,stroke:#4a90e2,stroke-width:2px;
    classDef auto fill:#d4edda,stroke:#28a745,stroke-width:2px;
    classDef win fill:#e8f5e9,stroke:#4caf50,stroke-width:2px;

    TAG_EVT(["Event detected: push tag vX.Y.Z"]):::auto --> VAL["1. validate (ubuntu-latest):<br/>• Validate 16-bit semver format<br/>• Ancestor check against origin/main<br/>• First-parent verification (GITHUB_SHA^)<br/>• Extract CHANGELOG.md section"]:::bot

    VAL --> RX86["2a. pipeline-x86 (arch-pipeline.yml):<br/>• build: /MT compilation, version in .rc resource<br/>• test: GoogleTest x86<br/>• Upload restifycapl-x86.dll"]:::win
    VAL --> RX64["2b. pipeline-x64 (arch-pipeline.yml):<br/>• build: /MT compilation, version in .rc resource<br/>• test: GoogleTest x64<br/>• Upload restifycapl-x64.dll"]:::win
    VAL --> REXP["2c. export-table (ubuntu-latest):<br/>• CAPL function table in markdown"]:::bot

    RX86 --> ASM["3. assemble (ubuntu-latest):<br/>• Collect dist/ artifacts<br/>• Compute SHA256SUMS checksums<br/>• Prepare & validate notes.md (&le; 125,000 chars)"]:::bot
    RX64 --> ASM
    REXP --> ASM

    ASM --> ATT["4. attest (ubuntu-latest):<br/>• Sigstore OIDC build provenance<br/>• gh attestation verify with retry loop"]:::bot

    ATT --> PUB["5. publish (ubuntu-latest):<br/>• Verify no existing release<br/>• Create draft & verify sha256sum -c<br/>• Atomic release publication: --draft=false"]:::auto
```

---

## 4. Human Gates Reduction Matrix

| Gate ID | Before Optimization ([docs/ci-flow.md](file:///C:/Workspace/restifycapl/docs/ci-flow.md)) | After Optimization | Optimization Status |
|---|---|---|---|
| **HG-1** | Manual branch creation & push | Unchanged (developer decision) | Unchanged (Natural start of work) |
| **HG-2** | Manual checklist review | Automated CI status checks & bot assertions | Partial (Substantive code review) |
| **HG-3** | **Library verification in CANoe** | **Strict quality gate: local pre-push verification by developer OR verification by maintainer during PR approval (PR Approval Gate)** | **Preserved as a conscious human gate** (cannot be automated on cloud runners due to Vector CANoe hardware licensing) |
| **HG-4** | Manual 12-char hash entry & CHANGELOG cut | Automated script computing parent commit hash and formatting changelog section | **Automated** |
| **HG-5** | Manual squash-merge | Bot auto-merge upon approved review (or single-click maintainer merge) | **Optimized** |
| **HG-6** | Manual tag creation & push `vX.Y.Z` | Helper workflow or release-driven tag automation | **Optimized** |
| **HG-7** | Manual release verification & publishing | Automated Sigstore attestation & 7-step atomic publish workflow | **Fully Automated (Zero-Touch)** |

---

## 5. Role Allocation and Implementation Stages

- **Stage 1**: Standardization & synchronization of process documentation (`ci-flow.md` and `ci-optimized-flow.md`) -> `plan-writer` / `build-pipeline-engineer`.
- **Stage 2**: CI provisioning optimization (standalone GNU make, vcpkg `x-gha` cache, startup time reduced from ~30s to <2s) strictly preserving parallel `build` and `test` jobs and `/MT` flags -> `build-pipeline-engineer`. Checkpoint: verify no `/MT` regressions.
- **Stage 3**: Automated 12-char hash calculation and `CHANGELOG.md` header formatting on `chore/` branches -> `build-pipeline-engineer`.
- **Stage 4**: Release automation and publication hardening (Zero-Touch Release) preserving all 7 atomic publishing safeguards in `publish` -> `build-pipeline-engineer`. Checkpoint: verify test release on staging environment.
