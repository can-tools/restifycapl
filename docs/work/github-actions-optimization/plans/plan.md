# Planning Report: Mermaid Syntax Analysis & CI/CD Optimization Plan (Zero-Touch & Parallel Arch Pipeline)

Planning and architectural documentation for GitHub Actions workflow optimization in the `restifycapl` project.

---

## 1. Analysis of Mermaid Syntax & Decoding Issues

### Sources of decision node decoding errors in Markdown preview:
1. **Unescaped special characters and relations inside node labels**:
   - Nodes such as `VAL --> PARALLEL{'Uruchomienie równoległe'}:::decision` in certain Mermaid engines (e.g. VS Code Markdown Preview) caused parse errors due to single quotes `'` and unescaped format inside curly brackets `{}`.
   - The presence of raw inequality characters `<` and `>` (e.g. `< 125,000 characters` or `make test ARCH=<arch>`) and arrow characters `->` inside node labels caused parsers to treat them as HTML tags or relation tokens, breaking the AST parser tree.
2. **Use of multi-node operator `&` in edge relations**:
   - Syntax like `RX86 & RX64 & RTAB --> ASM` or `FORK --> A & B & C` is a Mermaid extension that triggers `Parse error on line ...: Unexpected token &` in strict or older renderers.
3. **Remediation rules applied in updated diagrams**:
   - All node labels are enclosed in double quotes `""`.
   - Inequality characters are replaced with safe HTML entities (`&lt;`, `&gt;`, `&le;`, `&ge;`).
   - All graph connections are defined individually (line-by-line), eliminating the `&` operator.

---

## 2. Target CI/CD Model (Target Flow & Optimization Architecture)

### 2.1 Key Principles
- **Strict decoupling and parallelism of `build` and `test` in `arch-pipeline.yml`**: By design, compilation (`build`) and GoogleTest unit/integration testing (`test`) remain separate, parallel jobs on Windows runners for both architectures (x86 and x64). This guarantees environment isolation and immediate disambiguation between build failures and test failures.
- **Rigorous quality gates**: Vector CANoe verification is formalized as a conscious Human Gate (pre-push or during PR approval).
- **Automation of repetitive operations**: Eliminating manual changelog cutting and manual tagging while retaining strict cryptographic build provenance and security guarantees.

### 2.2 Optimization Pillars
1. **Accelerated Toolchain Provisioning**:
   - Replacing `choco install make` with standalone GNU Make (fetching single binary from GitHub Releases or `actions/cache`), dropping setup time from ~60s to <2s.
   - Native vcpkg binary caching in GitHub Actions: `VCPKG_BINARY_SOURCES: "clear;x-gha,readwrite"`. Prebuilt `/MT` static packages (`nlohmann-json`, `openssl`) are restored instantly from GHA cache.
2. **Automated 12-char Hash Calculation and CHANGELOG Cut (Eliminating HG-4)**:
   - A `prepare-release-changelog` script automatically retrieves `git rev-parse --short=12 origin/main`, promotes the `[Unreleased]` section to `[vX.Y.Z] - YYYY-MM-DD (hash)`, and updates diff links in `CHANGELOG.md` without manual developer intervention.
3. **Formalized Vector CANoe Verification Gate (Quality Gate HG-3)**:
   - Verification of libraries in CANoe remains an engineering responsibility (cannot run on cloud runners due to hardware Vector CANoe licensing).
   - Occurs at one of two explicit checkpoints:
     a) **Local pre-push:** developer verifies CAPL script compilation and DLL loading on a CANoe workstation prior to pushing changes touching `exports.cpp`.
     b) **During PR approval (PR Approval Gate):** reviewer/maintainer verifies downloaded DLLs in CANoe before formal approval of the PR.
4. **Automated Post-Merge Tagging and Zero-Touch Release in `release.yml` (Eliminating HG-6 & HG-7)**:
   - Helper workflow or release-driven tagging upon squash-merge into `main`.
   - Tag push triggers `release.yml`, executing validation, parallel versioned compilation (with separate build and test), export table generation, Sigstore OIDC provenance attestation, and direct release publication.

---

## 3. Syntactically Valid Mermaid Diagrams

### Diagram 1: Target End-to-End Architecture
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

### Diagram 2: Optimized `arch-pipeline.yml` (Decoupled Build & Test)
```mermaid
flowchart TD
    classDef bot fill:#e6f3ff,stroke:#4a90e2,stroke-width:2px;
    classDef opt fill:#d4edda,stroke:#28a745,stroke-width:2px;
    classDef gate fill:#fff3cd,stroke:#ffc107,stroke-width:2px;

    TRIG(["Trigger: push or pull_request"]):::bot --> FORK{"Task Fork"}:::bot

    subgraph FastProvisioning ["Fast Provisioning Model"]
        CACHE_VCPKG["vcpkg Binary Caching: x-gha backend"]:::opt
        FAST_MAKE["Standalone GNU Make (eliminates Chocolatey overhead)"]:::opt
    end

    FORK --> FastProvisioning

    subgraph ArchX86 ["arch-pipeline.yml (x86 Architecture)"]
        X86_B["Job build-x86:<br/>• make build-x86 (/MT)<br/>• dumpbin /directives<br/>• dumpbin /exports (exactly 1 function)<br/>• Upload restifycapl-x86.dll"]:::bot
        X86_T["Job test-x86:<br/>• make test ARCH=x86 (GoogleTest)<br/>• Isolated windows-latest runner"]:::bot
    end

    subgraph ArchX64 ["arch-pipeline.yml (x64 Architecture)"]
        X64_B["Job build-x64:<br/>• make build-x64 (/MT)<br/>• dumpbin /directives<br/>• dumpbin /exports (exactly 1 function)<br/>• Upload restifycapl-x64.dll"]:::bot
        X64_T["Job test-x64:<br/>• make test ARCH=x64 (GoogleTest)<br/>• Isolated windows-latest runner"]:::bot
    end

    FastProvisioning --> X86_B
    FastProvisioning --> X86_T
    FastProvisioning --> X64_B
    FastProvisioning --> X64_T

    subgraph ExpCheck ["Contract Verification"]
        EXP_TAB["export-table (ubuntu-latest):<br/>list-export-table.ps1"]:::bot
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

### Diagram 3: Target `release.yml` (Zero-Touch Release)
```mermaid
flowchart TD
    classDef bot fill:#e6f3ff,stroke:#4a90e2,stroke-width:2px;
    classDef auto fill:#d4edda,stroke:#28a745,stroke-width:2px;
    classDef win fill:#e8f5e9,stroke:#4caf50,stroke-width:2px;

    TAG_EVT(["Event detected: push tag vX.Y.Z"]):::auto --> VAL["1. validate (ubuntu-latest):<br/>• Validate 16-bit semver format<br/>• Ancestor check against origin/main<br/>• First-parent verification GITHUB_SHA^<br/>• Extract CHANGELOG.md section"]:::bot

    VAL --> RX86["2a. pipeline-x86 (arch-pipeline.yml):<br/>• build: /MT compilation, version in .rc resource<br/>• test: GoogleTest x86<br/>• Upload restifycapl-x86.dll"]:::win
    VAL --> RX64["2b. pipeline-x64 (arch-pipeline.yml):<br/>• build: /MT compilation, version in .rc resource<br/>• test: GoogleTest x64<br/>• Upload restifycapl-x64.dll"]:::win
    VAL --> REXP["2c. export-table (ubuntu-latest):<br/>• CAPL function table in markdown"]:::bot

    RX86 --> ASM["3. assemble (ubuntu-latest):<br/>• Assemble dist/<br/>• Compute SHA256SUMS checksums<br/>• Validate notes.md length (&le; 125,000 chars)"]:::bot
    RX64 --> ASM
    REXP --> ASM

    ASM --> ATT["4. attest (ubuntu-latest):<br/>• Sigstore OIDC build provenance<br/>• gh attestation verify with retry loop"]:::bot

    ATT --> PUB["5. publish (ubuntu-latest):<br/>• Verify no existing release<br/>• Create draft & verify sha256sum -c<br/>• Atomic release publication: --draft=false"]:::auto
```

---

## 4. Human Gates Reduction Matrix

| Gate ID | Before Optimization ([docs/ci-flow.md](file:///C:/Workspace/restifycapl/docs/ci-flow.md)) | After Optimization | Reduction Status |
|---|---|---|---|
| **HG-1** | Manual branch creation & push | Unchanged (developer decision) | None (Natural start of work) |
| **HG-2** | Manual checklist review | Automated CI status checks & bot assertions | Partial (Substantive code review) |
| **HG-3** | **Library verification in CANoe** | **Strict quality gate: local pre-push verification by developer OR verification by maintainer during PR approval (PR Approval Gate)** | **Preserved as a conscious human gate** (cannot be automated on cloud runners due to Vector CANoe hardware licensing) |
| **HG-4** | **Manual 12-char hash entry & CHANGELOG cut** | **Automated script computing parent commit hash and formatting changelog section** | **Fully Automated (Eliminated)** |
| **HG-5** | Manual squash-merge | Bot auto-merge upon approved review (or single-click maintainer merge) | **Optimized** |
| **HG-6** | **Manual tag creation & push `vX.Y.Z`** | **Helper workflow or release-driven tag automation** | **Optimized** |
| **HG-7** | Manual release verification & publishing | Automated Sigstore attestation & 7-step atomic publish workflow | **Fully Automated (Zero-Touch)** |

---

## 5. Role Allocation and Implementation Stages

- **Stage 1**: Standardization & synchronization of process documentation (`ci-flow.md` and `ci-optimized-flow.md`) -> `plan-writer` / `build-pipeline-engineer`.
- **Stage 2**: CI provisioning optimization (standalone GNU make, vcpkg `x-gha` cache) strictly preserving parallel `build` and `test` jobs and `/MT` flags -> `build-pipeline-engineer`. Checkpoint: verify no `/MT` regressions.
- **Stage 3**: Automated 12-char hash calculation and `CHANGELOG.md` header formatting on `chore/` branches -> `build-pipeline-engineer` + `test-engineer`.
- **Stage 4**: Release automation and publication hardening (Zero-Touch Release) preserving all 7 atomic publishing safeguards in `publish` -> `build-pipeline-engineer`. Checkpoint: verify test release on staging environment.
