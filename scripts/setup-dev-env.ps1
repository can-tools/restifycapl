<#
.SYNOPSIS
    Provisions the local development environment for the CAPL REST DLL
    (restifycapl): MSVC Build Tools, make, vcpkg-built libcurl (both
    architectures), and the pinned nlohmann/json single header.

.DESCRIPTION
    This script provisions the environment ONLY. It never compiles project
    sources and never becomes a second build system -- `make` builds the
    project; this script only makes sure the tools and libraries `make`
    depends on are present.

    Idempotent and safe to re-run: every step probes for existing state
    before attempting to change anything, and a failure in one step never
    aborts the rest -- all nine steps always run, and a pass/fail summary is
    printed at the end (mirrors 04-FLOW section 6.9).

    Steps performed, in order:
      1. Probe for an existing MSVC toolchain via vswhere.exe.
      2. If absent, silently install VS Build Tools (VCTools workload).
      3. Resolve vcvarsall.bat; confirm cl/rc/link/dumpbin resolve for both
         x86 and x64, and that each reports the matching architecture.
      4. Detect `make`; install it (MSYS2 / Chocolatey / Scoop) if absent.
      5. Detect or bootstrap vcpkg -- pinning the vcpkg TOOL itself to a
         known-good release tag ($VcpkgPinnedTag), separate from and in
         addition to vcpkg.json's own builtin-baseline pin on registry
         content -- then run a manifest-mode install
         (`vcpkg install --triplet=<T>`) against vcpkg.json at the repo
         root -- the single authoritative pin for curl and gtest versions
         (default curl features pull in SChannel automatically on
         Windows) -- for both x86-windows-static and x64-windows-static
         triplets. Manifest mode installs everything vcpkg.json lists in
         one command; there is no per-package argument any more.
      6. Copy the resulting curl/zlib .lib files into lib/x86/ and
         lib/x64/.
      7. Copy the resulting GoogleTest .lib files (installed by the same
         manifest-mode command as step 5, since gtest is also listed in
         vcpkg.json) into lib/gtest/x86/ and lib/gtest/x64/ (kept separate
         from lib/x86/ and lib/x64/ on purpose) -- a build-time-only test
         dependency, never linked into the shipped DLL -- and copy the
         headers into include/vendor/gtest/.
      8. Download json.hpp (nlohmann/json v3.11.3), verify its SHA-256, and
         place it in include/vendor/.
      9. Create the lib/x86, lib/x64, lib/gtest/x86 and lib/gtest/x64
         directories and write lib/README.
      10. Verify (never fetch) that include/vendor/capl-dll-sdk/ holds the
          three Vector SDK headers.

.NOTES
    Human approval gate: this script installs software and touches global
    machine state (VS Build Tools, make, vcpkg packages). Per the project
    plan it must be approved before its first run.

.PARAMETER SkipVsBuildTools
    Skip probing/installing MSVC Build Tools (step 1-2). Useful if the
    toolchain is already known-good and you only want to refresh
    dependencies.

.PARAMETER SkipMake
    Skip probing/installing `make` (step 4).

.PARAMETER SkipVcpkg
    Skip vcpkg bootstrap / manifest install / lib copy (steps 5-6).

.PARAMETER SkipGTest
    Skip copying GoogleTest's lib+header output (step 7). Note this is
    narrower than it was under classic mode: manifest mode installs curl
    and gtest together in one `vcpkg install --triplet=<T>` command (see
    vcpkg.json), so -SkipGTest cannot prevent gtest from being *built* by
    vcpkg when curl is being installed -- it only skips copying gtest's
    output into lib/gtest/<arch>/ and include/vendor/gtest/. Independent of
    -SkipVcpkg so a curl-only refresh can still skip the gtest copy step,
    or vice versa.

.PARAMETER SkipJson
    Skip the json.hpp download/verify step (step 7).

.PARAMETER VcpkgRoot
    Explicit path to an existing or desired vcpkg checkout. Defaults to
    $env:VCPKG_ROOT if set, otherwise "$env:LOCALAPPDATA\vcpkg" (a per-user
    location that does not require elevation).

.PARAMETER Force
    Re-download json.hpp even if a copy with a matching verified hash is
    already present.
#>

[CmdletBinding()]
param(
    [switch]$SkipVsBuildTools,
    [switch]$SkipMake,
    [switch]$SkipVcpkg,
    [switch]$SkipGTest,
    [switch]$SkipJson,
    [string]$VcpkgRoot,
    [switch]$Force
)

$ErrorActionPreference = 'Continue'
$ProgressPreference = 'SilentlyContinue'

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

$RepoRoot     = Split-Path -Parent $PSScriptRoot
$LibX86Dir    = Join-Path $RepoRoot 'lib\x86'
$LibX64Dir    = Join-Path $RepoRoot 'lib\x64'
$LibGtestX86Dir = Join-Path $RepoRoot 'lib\gtest\x86'
$LibGtestX64Dir = Join-Path $RepoRoot 'lib\gtest\x64'
$VendorDir    = Join-Path $RepoRoot 'include\vendor'
$SdkDir       = Join-Path $VendorDir 'capl-dll-sdk'
$GtestVendorDir = Join-Path $VendorDir 'gtest'
$JsonHppDst   = Join-Path $VendorDir 'json.hpp'

# nlohmann/json v3.11.3 amalgamated single header, pinned per
# msvc-build-conventions. Hash independently re-verified from two official
# sources, not taken from a third-party listing.
# Rationale: see docs/development-environment.md#jsonhpp-sha-256-verification
$JsonHppVersion = '3.11.3'
$JsonHppUrl     = 'https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp'
$JsonHppSha256  = '9BEA4C8066EF4A1C206B2BE5A36302F8926F7FDC6087AF5D20B417D0CF103EA6'

if (-not $VcpkgRoot) {
    if ($env:VCPKG_ROOT) {
        $VcpkgRoot = $env:VCPKG_ROOT
    } else {
        $VcpkgRoot = Join-Path $env:LOCALAPPDATA 'vcpkg'
    }
}

# vcpkg.json at $RepoRoot is the single authoritative dependency pin (curl,
# gtest, builtin-baseline) -- see msvc-build-conventions.
# Manifest-mode installs land under a per-TRIPLET install root, NOT
# $VcpkgRoot\installed\<triplet>\ (the latter is classic-mode only) --
# confirmed by running `vcpkg list` with cwd at $RepoRoot and observing it
# report against a project-local tree instead of the vcpkg checkout's own
# installed/ directory.
#
# Each triplet gets its OWN install root ($VcpkgInstalledRootX86,
# $VcpkgInstalledRootX64) passed explicitly via `--x-install-root`, rather
# than sharing one $RepoRoot\vcpkg_installed\ for both. This is load-bearing,
# not cosmetic: confirmed by real execution (a first real re-run,
# 2026-09-18) that running `vcpkg install --triplet=B` against a shared
# install root that already contains a prior `--triplet=A` install PRINTS
# "The following packages will be removed: curl:A, gtest:A, zlib:A" and then
# actually deletes A's triplet subdirectory -- manifest mode treats the
# installed tree as a closure to synchronize to exactly the currently
# requested triplet, not a set of independent per-triplet subtrees that
# happen to coexist under one root, even though the on-disk layout
# (installed/<triplet>/...) looks like it should support that. Verified with
# a direct `vcpkg install --triplet=x86-windows-static` followed by a direct
# `vcpkg install --triplet=x64-windows-static`, both from $RepoRoot with no
# script involved: the second call's own console output named the first
# call's packages for removal, and `x86-windows-static\` was gone from disk
# immediately afterward. Giving each triplet a separate `--x-install-root`
# eliminates the collision entirely (re-verified: two separate roots, both
# populated, neither call removed the other's directory).
# .gitignore excludes vcpkg_installed*/, matching both root names below.
# Rationale: see docs/development-environment.md#manifest-mode-triplet-installs-need-separate-install-roots
$VcpkgInstalledRootX86 = Join-Path $RepoRoot 'vcpkg_installed-x86'
$VcpkgInstalledRootX64 = Join-Path $RepoRoot 'vcpkg_installed-x64'

function Get-VcpkgInstallRoot {
    param([Parameter(Mandatory)][ValidateSet('x86-windows-static', 'x64-windows-static')][string]$Triplet)
    if ($Triplet -eq 'x86-windows-static') { return $VcpkgInstalledRootX86 }
    return $VcpkgInstalledRootX64
}

# ---------------------------------------------------------------------------
# Result tracking / output helpers
# ---------------------------------------------------------------------------

$script:Results = New-Object System.Collections.Generic.List[PSCustomObject]

function Add-Result {
    param(
        [Parameter(Mandatory)][string]$Step,
        [Parameter(Mandatory)][ValidateSet('OK', 'WARN', 'FAIL', 'SKIP')][string]$Status,
        [Parameter(Mandatory)][string]$Message
    )

    $color = switch ($Status) {
        'OK'   { 'Green' }
        'WARN' { 'Yellow' }
        'FAIL' { 'Red' }
        'SKIP' { 'DarkGray' }
    }

    Write-Host ("[{0,-4}] {1}: {2}" -f $Status, $Step, $Message) -ForegroundColor $color

    $script:Results.Add([PSCustomObject]@{
        Step    = $Step
        Status  = $Status
        Message = $Message
    })
}

function Invoke-Step {
    <#
        Runs a step's ScriptBlock, catching any terminating exception so one
        failing step never aborts the rest of the script. The ScriptBlock is
        responsible for calling Add-Result itself (possibly more than once);
        if it throws before doing so, this wrapper records a FAIL.
    #>
    param(
        [Parameter(Mandatory)][string]$Name,
        [Parameter(Mandatory)][scriptblock]$Body
    )

    try {
        & $Body
    } catch {
        Add-Result -Step $Name -Status 'FAIL' -Message "Unhandled error: $($_.Exception.Message)"
    }
}

function Test-IsElevated {
    $identity  = [Security.Principal.WindowsIdentity]::GetCurrent()
    $principal = New-Object Security.Principal.WindowsPrincipal($identity)
    return $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
}

# Manifest-mode vcpkg invocations (steps 5-7) need the working directory at
# $RepoRoot so vcpkg auto-detects vcpkg.json -- this vcpkg build exposes no
# --x-manifest-root flag on `install`/`list` (only --classic, to force the
# OTHER mode), which confirms cwd-based auto-detection is the supported
# mechanism for this version. Pushed once here, for the whole script, rather
# than around each individual vcpkg call, since every other path in this
# script is already built via Join-Path from an absolute root and is
# unaffected by cwd. Popped back before both exit points at the bottom.
Push-Location -LiteralPath $RepoRoot

# ===========================================================================
# Step 1/2 -- MSVC toolchain: probe via vswhere, install Build Tools if absent
# ===========================================================================

function Get-VsInstallation {
    <# Returns the VS installationPath string, or $null if no install with
       the VC.Tools.x86.x64 component is registered. #>
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        return $null
    }

    $installPath = & $vswhere -latest -products * `
        -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
        -property installationPath 2>$null

    if ([string]::IsNullOrWhiteSpace($installPath)) {
        return $null
    }
    return $installPath.Trim()
}

function Install-VsBuildTools {
    if (Test-IsElevated) {
        $bootstrapper = Join-Path $env:TEMP 'vs_buildtools.exe'
        try {
            if (-not (Test-Path -LiteralPath $bootstrapper)) {
                Invoke-WebRequest -Uri 'https://aka.ms/vs/17/release/vs_buildtools.exe' -OutFile $bootstrapper -UseBasicParsing
            }
        } catch {
            Add-Result -Step 'VS Build Tools install' -Status 'FAIL' `
                -Message "Could not download vs_buildtools.exe: $($_.Exception.Message)"
            return
        }

        $installArgs = @(
            '--quiet', '--wait', '--norestart', '--nocache',
            '--add', 'Microsoft.VisualStudio.Workload.VCTools',
            '--includeRecommended'
        )

        Add-Result -Step 'VS Build Tools install' -Status 'WARN' `
            -Message 'Installing silently, this can take several minutes...'

        $proc = Start-Process -FilePath $bootstrapper -ArgumentList $installArgs -Wait -PassThru
        switch ($proc.ExitCode) {
            0    { Add-Result -Step 'VS Build Tools install' -Status 'OK'   -Message 'Installed successfully.' }
            3010 { Add-Result -Step 'VS Build Tools install' -Status 'WARN' -Message 'Installed; a reboot is pending (exit 3010) -- this is success-pending-reboot, not a failure.' }
            default {
                Add-Result -Step 'VS Build Tools install' -Status 'FAIL' `
                    -Message "vs_buildtools.exe exited with code $($proc.ExitCode). See %TEMP%\dd_*.log for details."
            }
        }
        return
    }

    # Not elevated: never attempt the install silently, and never fail
    # obscurely -- download the bootstrapper (no admin needed for that part)
    # so the printed command references a concrete local path, then print
    # the exact command to run elevated.
    $bootstrapper = Join-Path $env:TEMP 'vs_buildtools.exe'
    try {
        if (-not (Test-Path -LiteralPath $bootstrapper)) {
            Invoke-WebRequest -Uri 'https://aka.ms/vs/17/release/vs_buildtools.exe' -OutFile $bootstrapper -UseBasicParsing
        }
    } catch {
        Add-Result -Step 'VS Build Tools install' -Status 'FAIL' `
            -Message "Not elevated, and could not pre-download vs_buildtools.exe either: $($_.Exception.Message)"
        return
    }

    $argsDisplay = "--quiet --wait --norestart --nocache --add Microsoft.VisualStudio.Workload.VCTools --includeRecommended"
    Add-Result -Step 'VS Build Tools install' -Status 'FAIL' -Message @"
This session is not elevated; MSVC Build Tools cannot be installed silently
without admin rights. Re-run this script from an elevated PowerShell, or run:
  Start-Process -FilePath '$bootstrapper' -ArgumentList '$argsDisplay' -Verb RunAs -Wait
"@
}

Invoke-Step -Name 'MSVC toolchain' -Body {
    if ($SkipVsBuildTools) {
        Add-Result -Step 'MSVC toolchain' -Status 'SKIP' -Message 'Skipped by -SkipVsBuildTools.'
        return
    }

    $installPath = Get-VsInstallation
    if ($installPath) {
        Add-Result -Step 'MSVC toolchain' -Status 'OK' -Message "Found via vswhere at: $installPath"
        return
    }

    Add-Result -Step 'MSVC toolchain' -Status 'WARN' -Message 'No MSVC toolchain with the VC.Tools.x86.x64 component found; attempting install.'
    Install-VsBuildTools

    # Re-probe once, in case an elevated install just happened.
    $installPath = Get-VsInstallation
    if ($installPath) {
        Add-Result -Step 'MSVC toolchain' -Status 'OK' -Message "Toolchain now present at: $installPath"
    }
}

# ===========================================================================
# Step 3 -- resolve vcvarsall.bat; confirm cl/rc/link/dumpbin for both arches
# ===========================================================================

function Test-NativeToolsArch {
    param(
        [Parameter(Mandatory)][string]$VcvarsallPath,
        [Parameter(Mandatory)][ValidateSet('x86', 'x64')][string]$Arch
    )

    # Throwaway cmd.exe subshell per architecture: keeps this out of the
    # current PowerShell session's environment and avoids a stacked env.
    #
    # `where` probes below are chained with bare `&`, not `&&`, so all four
    # always run even if an earlier one fails to resolve; the per-tool
    # re-check after this call is what actually verifies success, not
    # $LASTEXITCODE. The architecture check further below is a deliberately
    # separate invocation, so a missing-tool failure and an architecture
    # mismatch are reported as distinct reasons.
    # Rationale: see docs/development-environment.md#where-probes-chained-with-bare--not-
    $whereCmdLine = "call `"$VcvarsallPath`" $Arch >nul 2>&1 && where cl & where rc & where link & where dumpbin"
    $whereOutput = & cmd.exe /c $whereCmdLine 2>&1
    $whereExitCode = $LASTEXITCODE

    if ($whereExitCode -ne 0) {
        return [PSCustomObject]@{ Ok = $false; Reason = "vcvarsall.bat $Arch, or one of cl/rc/link/dumpbin, failed to resolve (exit $whereExitCode)." }
    }

    $whereOutputText = ($whereOutput -join "`n")
    foreach ($tool in 'cl.exe', 'rc.exe', 'link.exe', 'dumpbin.exe') {
        if ($whereOutputText -notmatch [regex]::Escape($tool)) {
            return [PSCustomObject]@{ Ok = $false; Reason = "``where $tool`` produced no path under $Arch Native Tools." }
        }
    }

    # Confirms the target architecture via %VSCMD_ARG_TGT_ARCH% (set by
    # vcvarsall.bat itself, locale-independent) rather than parsing cl.exe's
    # localized banner text. Requires delayed expansion (`!VAR!` + cmd.exe
    # `/v:on`), not `%VAR%` -- plain `%VAR%` expands at parse time, before
    # vcvarsall.bat has run, and would resolve to the literal token instead
    # of the value.
    # Rationale: see docs/development-environment.md#native-tools-architecture-detection-test-nativetoolsarch
    $archCmdLine = "call `"$VcvarsallPath`" $Arch >nul 2>&1 && echo !VSCMD_ARG_TGT_ARCH!"
    $archOutputText = ((& cmd.exe /v:on /c $archCmdLine 2>&1) -join "`n").Trim()

    if ($archOutputText -ne $Arch) {
        # Display '(unset)' instead of the raw output when the variable never
        # resolved (empty, or the literal undelayed token) -- clearer than
        # printing '!VSCMD_ARG_TGT_ARCH!' verbatim in the FAIL message.
        $archDisplay = if ([string]::IsNullOrWhiteSpace($archOutputText) -or $archOutputText -eq '!VSCMD_ARG_TGT_ARCH!') { '(unset)' } else { $archOutputText }
        return [PSCustomObject]@{ Ok = $false; Reason = "VSCMD_ARG_TGT_ARCH after vcvarsall.bat $Arch was '$archDisplay', expected '$Arch' -- wrong-architecture Native Tools environment." }
    }

    return [PSCustomObject]@{ Ok = $true; Reason = 'OK' }
}

Invoke-Step -Name 'vcvarsall / cl+rc+link+dumpbin' -Body {
    $installPath = Get-VsInstallation
    if (-not $installPath) {
        Add-Result -Step 'vcvarsall / cl+rc+link+dumpbin' -Status 'SKIP' -Message 'Skipped: no MSVC installation found (see MSVC toolchain step above).'
        return
    }

    $vcvarsall = Join-Path $installPath 'VC\Auxiliary\Build\vcvarsall.bat'
    if (-not (Test-Path -LiteralPath $vcvarsall)) {
        Add-Result -Step 'vcvarsall / cl+rc+link+dumpbin' -Status 'FAIL' -Message "vcvarsall.bat not found at expected path: $vcvarsall"
        return
    }

    Add-Result -Step 'vcvarsall' -Status 'OK' -Message "Resolved: $vcvarsall"

    foreach ($arch in 'x86', 'x64') {
        $result = Test-NativeToolsArch -VcvarsallPath $vcvarsall -Arch $arch
        if ($result.Ok) {
            Add-Result -Step "Native Tools ($arch)" -Status 'OK' -Message 'cl, rc, link, dumpbin all resolve and report the correct architecture.'
        } else {
            Add-Result -Step "Native Tools ($arch)" -Status 'FAIL' -Message $result.Reason
        }
    }
}

# ===========================================================================
# Step 4 -- detect / install make
# ===========================================================================

Invoke-Step -Name 'make' -Body {
    if ($SkipMake) {
        Add-Result -Step 'make' -Status 'SKIP' -Message 'Skipped by -SkipMake.'
        return
    }

    if (Get-Command make -ErrorAction SilentlyContinue) {
        $ver = (& make --version 2>&1 | Select-Object -First 1)
        Add-Result -Step 'make' -Status 'OK' -Message "Already on PATH ($ver)."
        return
    }

    # Preference order: Scoop, then existing MSYS2, then Chocolatey, then
    # manual instructions (judgment call, not mandated by the plan).
    # Rationale: see docs/development-environment.md#make-installer-preference-order
    if (Get-Command scoop -ErrorAction SilentlyContinue) {
        Add-Result -Step 'make' -Status 'WARN' -Message 'Not found; installing via Scoop.'
        & scoop install make 2>&1 | Out-Null
        if (Get-Command make -ErrorAction SilentlyContinue) {
            Add-Result -Step 'make' -Status 'OK' -Message 'Installed via Scoop.'
        } else {
            Add-Result -Step 'make' -Status 'FAIL' -Message 'Scoop install of make did not put make on PATH; inspect `scoop install make` output manually.'
        }
        return
    }

    $msysPacman = 'C:\msys64\usr\bin\pacman.exe'
    if (Test-Path -LiteralPath $msysPacman) {
        Add-Result -Step 'make' -Status 'WARN' -Message 'Not found; installing via existing MSYS2 (pacman).'
        & $msysPacman -S --noconfirm make 2>&1 | Out-Null
        $msysMake = 'C:\msys64\usr\bin\make.exe'
        if (Test-Path -LiteralPath $msysMake) {
            Add-Result -Step 'make' -Status 'OK' -Message "Installed via MSYS2 at $msysMake. Add C:\msys64\usr\bin to PATH if ``make`` is not yet resolving."
        } else {
            Add-Result -Step 'make' -Status 'FAIL' -Message 'pacman -S make did not produce C:\msys64\usr\bin\make.exe; inspect pacman output manually.'
        }
        return
    }

    if (Get-Command choco -ErrorAction SilentlyContinue) {
        if (Test-IsElevated) {
            Add-Result -Step 'make' -Status 'WARN' -Message 'Not found; installing via Chocolatey.'
            & choco install make -y 2>&1 | Out-Null
            if (Get-Command make -ErrorAction SilentlyContinue) {
                Add-Result -Step 'make' -Status 'OK' -Message 'Installed via Chocolatey.'
            } else {
                Add-Result -Step 'make' -Status 'FAIL' -Message 'choco install make did not put make on PATH; a new shell may be required.'
            }
        } else {
            Add-Result -Step 'make' -Status 'FAIL' -Message @"
Chocolatey is available but this session is not elevated. Re-run this
script elevated, or run:
  Start-Process -FilePath choco -ArgumentList 'install','make','-y' -Verb RunAs -Wait
"@
        }
        return
    }

    Add-Result -Step 'make' -Status 'FAIL' -Message @"
No package manager (Scoop, MSYS2, Chocolatey) found to install make
automatically. Install one of the following manually, then re-run:
  Scoop:       scoop install make
  MSYS2:       winget install -e --id MSYS2.MSYS2   (then: pacman -S make)
  Chocolatey:  choco install make -y   (elevated)
"@
}

# ===========================================================================
# Steps 5/6 -- vcpkg bootstrap, curl (SChannel by default on Windows) for
# both triplets, copy .lib
# ===========================================================================

# Pins the vcpkg TOOL itself -- not just the registry CONTENT pinned via
# vcpkg.json's builtin-baseline -- to a specific, known-good release tag.
# This is separate from, and does not replace, vcpkg.json's baseline pin:
# the baseline pins WHICH port versions get resolved; this pins WHICH
# vcpkg.exe (and its command-line behavior, including flags like
# --x-install-root below) does the resolving in the first place.
#
# Why this matters: the triplet-isolation fix above (separate
# --x-install-root per architecture) depends on a flag vcpkg's own --help
# marks "(experimental)". An un-pinned `git clone` (previously: always
# tracking the moving default branch tip) could silently pick up a future
# vcpkg release that weakens or changes that flag's semantics without
# renaming or removing it -- which would at least fail loudly. Pinning the
# tool means a fresh bootstrap months from now reproduces the exact vcpkg
# build this project has already verified end-to-end, instead of whatever
# the default branch tip happens to be that day.
#
# 2026.07.29 confirmed via `git ls-remote --tags
# https://github.com/microsoft/vcpkg.git` on 2026-09-18 to be the latest
# real, tagged upstream release (annotated tag object
# c76c06644034521fb761a39f8f52d8e87d1103d5, dereferencing to commit
# 9e593bb18ea69cc5095e012465dcd675a822ed0d) -- not invented. It is also the
# same release generation already proven working end-to-end by this
# project's own successful run (that run's `vcpkg version` reported
# `2026-07-27-...`, i.e. a commit from two days before this tag), so
# pinning to it does not change behavior on an already-working machine.
# Re-derive this the same way (`git ls-remote --tags`, pick the newest
# non-`^{}` entry) if this pin is ever bumped; update this comment's
# provenance, not just the value.
# Rationale: see docs/development-environment.md#pinning-the-vcpkg-tool-itself
$VcpkgPinnedTag = '2026.07.29'

function Get-VcpkgExe {
    param([Parameter(Mandatory)][string]$Root)
    $exe = Join-Path $Root 'vcpkg.exe'
    if (Test-Path -LiteralPath $exe) { return $exe }
    return $null
}

function Repair-ShallowVcpkgClone {
    <#
        Manifest mode's versioning system (builtin-baseline plus
        per-dependency version resolution, see vcpkg.json) checks out
        specific historical commits' port trees from the local git object
        database. A --depth 1 shallow clone only has objects reachable
        from the single cloned commit, so a manifest install against a
        shallow checkout fails with "failed to unpack tree object ...
        vcpkg was cloned as a shallow repository" as soon as it needs a
        port-version commit that isn't the tip -- confirmed by actually
        running `vcpkg install --dry-run` against this machine's
        pre-existing checkout, not by inspection (consistent with this
        project's static-review-is-not-enough pattern).

        Earlier revisions of this script bootstrapped with
        `git clone --depth 1`, so an existing checkout can still be
        shallow even though $Root already exists and vcpkg.exe already
        resolves (the early-return path in Install-VcpkgIfMissing).
        Self-heals by unshallowing in place -- idempotent: a fast
        `rev-parse` no-op on a checkout that is already full.

        Must run BEFORE Set-VcpkgPinnedVersion (see below), which it does
        in Install-VcpkgIfMissing's existing-checkout path: checking out a
        specific historical tag can itself fail with the same "shallow
        repository" error this function fixes, if that tag's commit isn't
        reachable from the shallow clone's single fetched commit.
    #>
    param([Parameter(Mandatory)][string]$Root)

    $gitCmd = Get-Command git -ErrorAction SilentlyContinue
    if (-not $gitCmd) {
        Add-Result -Step 'vcpkg bootstrap (unshallow check)' -Status 'WARN' -Message "git not on PATH; cannot verify whether the existing vcpkg checkout at $Root is shallow. If a later manifest install fails with `"cloned as a shallow repository`", run: git -C `"$Root`" fetch --unshallow"
        return
    }

    $isShallow = ((& git -C $Root rev-parse --is-shallow-repository 2>$null) -join '').Trim()
    if ($isShallow -ne 'true') {
        return
    }

    Add-Result -Step 'vcpkg bootstrap (unshallow check)' -Status 'WARN' -Message "Existing vcpkg checkout at $Root is a shallow clone (from an earlier version of this script); manifest-mode installs need full history. Unshallowing now -- downloads the full registry history, can take a few minutes."
    & git -C $Root fetch --unshallow 2>&1 | Out-Null
    $isShallowAfter = ((& git -C $Root rev-parse --is-shallow-repository 2>$null) -join '').Trim()
    if ($isShallowAfter -ne 'true') {
        Add-Result -Step 'vcpkg bootstrap (unshallow check)' -Status 'OK' -Message 'Unshallowed successfully.'
    } else {
        Add-Result -Step 'vcpkg bootstrap (unshallow check)' -Status 'FAIL' -Message "git fetch --unshallow did not complete; manifest installs will likely fail. Run manually: git -C `"$Root`" fetch --unshallow"
    }
}

function Set-VcpkgPinnedVersion {
    <#
        Checks out $VcpkgPinnedTag in the vcpkg checkout at $Root, so the
        vcpkg TOOL itself -- not just the registry content pinned via
        vcpkg.json's builtin-baseline -- is a specific, known-good version.
        Runs for BOTH a freshly-cloned checkout and a pre-existing one that
        might be sitting on an unrelated commit (e.g. a checkout from
        before this pin existed, or one manually `git pull`ed to the
        moving default-branch tip) -- idempotent: a fast no-op when HEAD
        is already on the pinned commit.

        Must run AFTER Repair-ShallowVcpkgClone (existing-checkout path) or
        after a fresh, full clone (fresh-clone path): checking out a
        specific historical tag needs that commit reachable in the local
        object database, which a --depth 1 shallow clone may not have.
        See Repair-ShallowVcpkgClone's doc comment for the same failure
        mode applied to manifest installs.

        Returns $true if HEAD actually moved (fresh clone, or an existing
        checkout that was on the wrong commit) so the caller knows to
        rebuild vcpkg.exe via Invoke-VcpkgBootstrap -- a vcpkg.exe built
        from a different commit than what's now checked out could mismatch
        the now-checked-out toolsrc/ sources. Returns $false when already
        pinned (no rebuild needed) or when the pin could not be resolved
        or enforced (caller treats this as best-effort: the existing
        vcpkg.exe, if any, is still usable, just unpinned).
    #>
    param([Parameter(Mandatory)][string]$Root)

    $gitCmd = Get-Command git -ErrorAction SilentlyContinue
    if (-not $gitCmd) {
        Add-Result -Step 'vcpkg tool pin' -Status 'WARN' -Message "git not on PATH; cannot verify or enforce the vcpkg tool pin ($VcpkgPinnedTag) at $Root."
        return $false
    }

    $currentCommit = ((& git -C $Root rev-parse HEAD 2>$null) -join '').Trim()
    $pinnedCommit  = ((& git -C $Root rev-parse "$VcpkgPinnedTag^{commit}" 2>$null) -join '').Trim()

    if (-not $pinnedCommit) {
        # Tag not resolvable locally yet -- e.g. a checkout cloned before
        # this pin was introduced. Repair-ShallowVcpkgClone only unshallows,
        # it doesn't force-refresh tags, so explicitly fetch this one tag by
        # name rather than assuming it's already present.
        & git -C $Root fetch origin "refs/tags/${VcpkgPinnedTag}:refs/tags/${VcpkgPinnedTag}" 2>&1 | Out-Null
        $pinnedCommit = ((& git -C $Root rev-parse "$VcpkgPinnedTag^{commit}" 2>$null) -join '').Trim()
    }

    if (-not $pinnedCommit) {
        Add-Result -Step 'vcpkg tool pin' -Status 'WARN' -Message "Could not resolve pinned tag $VcpkgPinnedTag to a commit in the vcpkg checkout at $Root, even after fetching it explicitly. Continuing with whatever commit is currently checked out ($currentCommit) -- verify network access and that the tag still exists upstream."
        return $false
    }

    if ($currentCommit -eq $pinnedCommit) {
        Add-Result -Step 'vcpkg tool pin' -Status 'OK' -Message "Already on pinned vcpkg tool version $VcpkgPinnedTag ($pinnedCommit)."
        return $false
    }

    & git -C $Root checkout --quiet $VcpkgPinnedTag 2>&1 | Out-Null
    $afterCommit = ((& git -C $Root rev-parse HEAD 2>$null) -join '').Trim()
    if ($afterCommit -ne $pinnedCommit) {
        Add-Result -Step 'vcpkg tool pin' -Status 'WARN' -Message "git checkout $VcpkgPinnedTag did not land on the expected commit ($pinnedCommit); currently at $afterCommit. Continuing with whatever is checked out -- inspect $Root manually (e.g. uncommitted local changes blocking checkout)."
        return $false
    }

    Add-Result -Step 'vcpkg tool pin' -Status 'OK' -Message "Checked out pinned vcpkg tool version $VcpkgPinnedTag ($pinnedCommit) -- was $currentCommit."
    return $true
}

function Invoke-VcpkgBootstrap {
    <#
        Runs bootstrap-vcpkg.bat and returns the resulting vcpkg.exe path
        (or $null on failure), recording its own Add-Result. Factored out
        so both Install-VcpkgIfMissing's "no vcpkg.exe yet" path and its
        "pin moved HEAD on an existing checkout" path share one
        implementation instead of two copies that could drift.
    #>
    param([Parameter(Mandatory)][string]$Root)

    $bootstrap = Join-Path $Root 'bootstrap-vcpkg.bat'
    if (-not (Test-Path -LiteralPath $bootstrap)) {
        Add-Result -Step 'vcpkg bootstrap' -Status 'FAIL' -Message "bootstrap-vcpkg.bat not found under $Root; the checkout may be corrupt."
        return $null
    }

    & cmd.exe /c "`"$bootstrap`" -disableMetrics" 2>&1 | Out-Null
    $exe = Get-VcpkgExe -Root $Root
    if (-not $exe) {
        Add-Result -Step 'vcpkg bootstrap' -Status 'FAIL' -Message 'bootstrap-vcpkg.bat ran but vcpkg.exe still not found.'
        return $null
    }

    Add-Result -Step 'vcpkg bootstrap' -Status 'OK' -Message "Bootstrapped at $Root"
    return $exe
}

function Assert-VcpkgBaselinePin {
    <#
        Baseline/tool-pin drift guard. vcpkg.json's builtin-baseline pins WHICH port
        versions resolve (registry content); $VcpkgPinnedTag / the checkout
        at $Root pins WHICH vcpkg.exe does the resolving. These are two
        independent axes that must still name the same commit in the
        microsoft/vcpkg registry -- if the baseline is newer than the tool's
        own checked-out commit, `vcpkg install` fails with "no version
        database entry for <port> at <version>" (the baseline's version
        isn't present in the older, checked-out version database), exactly
        the bug this guard is meant to catch immediately instead of via that
        much less obvious downstream error. Runs after
        Set-VcpkgPinnedVersion has had a chance to run (called from
        Install-VcpkgIfMissing, both the fresh-clone and existing-checkout
        paths), so $Root already reflects whatever commit the tool pin
        actually landed on.

        Mirrors the equivalent CI step in .github/workflows/ci.yml ("Verify
        vcpkg tool pin matches vcpkg.json's builtin-baseline") -- keep both
        in sync if this check's logic ever changes.
        Rationale: see docs/development-environment.md#pinning-the-vcpkg-tool-itself
    #>
    param([Parameter(Mandatory)][string]$Root)

    $vcpkgJsonPath = Join-Path $RepoRoot 'vcpkg.json'
    if (-not (Test-Path -LiteralPath $vcpkgJsonPath)) {
        Add-Result -Step 'vcpkg baseline/tool pin check' -Status 'WARN' -Message "vcpkg.json not found at $vcpkgJsonPath; cannot verify the baseline/tool-pin invariant."
        return
    }

    $baseline = $null
    try {
        $baseline = (Get-Content -LiteralPath $vcpkgJsonPath -Raw | ConvertFrom-Json).'builtin-baseline'
    } catch {
        Add-Result -Step 'vcpkg baseline/tool pin check' -Status 'WARN' -Message "Could not parse vcpkg.json to read builtin-baseline: $($_.Exception.Message)"
        return
    }
    if ([string]::IsNullOrWhiteSpace($baseline)) {
        Add-Result -Step 'vcpkg baseline/tool pin check' -Status 'WARN' -Message "vcpkg.json has no builtin-baseline value; cannot verify the baseline/tool-pin invariant."
        return
    }

    $gitCmd = Get-Command git -ErrorAction SilentlyContinue
    if (-not $gitCmd) {
        Add-Result -Step 'vcpkg baseline/tool pin check' -Status 'WARN' -Message "git not on PATH; cannot verify the vcpkg tool checkout at $Root matches vcpkg.json's builtin-baseline ($baseline)."
        return
    }

    $toolCommit = ((& git -C $Root rev-parse HEAD 2>$null) -join '').Trim()
    if (-not $toolCommit) {
        Add-Result -Step 'vcpkg baseline/tool pin check' -Status 'WARN' -Message "Could not resolve HEAD of the vcpkg tool checkout at $Root; cannot verify the baseline/tool-pin invariant."
        return
    }

    if ($toolCommit -ne $baseline) {
        Add-Result -Step 'vcpkg baseline/tool pin check' -Status 'FAIL' -Message @"
vcpkg.json's builtin-baseline ($baseline) does not match the pinned vcpkg
TOOL's checked-out commit ($toolCommit, from VcpkgPinnedTag=$VcpkgPinnedTag).
These are two independent pins that must name the same commit in the
microsoft/vcpkg registry -- otherwise version resolution reads baseline
versions from one commit but checks them against a version database pinned
at a different (often older) commit, producing "no version database entry"
errors. Fix by re-deriving the commit VcpkgPinnedTag ($VcpkgPinnedTag)
dereferences to (git ls-remote --tags https://github.com/microsoft/vcpkg.git <tag>)
and setting vcpkg.json's builtin-baseline to that exact commit.
"@
        return
    }

    Add-Result -Step 'vcpkg baseline/tool pin check' -Status 'OK' -Message "vcpkg tool pin ($toolCommit) matches vcpkg.json's builtin-baseline."
}

function Install-VcpkgIfMissing {
    param([Parameter(Mandatory)][string]$Root)

    $exe = Get-VcpkgExe -Root $Root
    if ($exe) {
        Repair-ShallowVcpkgClone -Root $Root
        # Pin check runs even on an existing, already-working checkout: it
        # may be sitting on an unrelated commit (a checkout that predates
        # this pin, or one manually updated). If enforcing the pin actually
        # moves HEAD, vcpkg.exe must be rebuilt from the now-checked-out
        # commit's own sources -- see Set-VcpkgPinnedVersion's doc comment.
        if (Set-VcpkgPinnedVersion -Root $Root) {
            $rebuiltExe = Invoke-VcpkgBootstrap -Root $Root
            if ($rebuiltExe) {
                return $rebuiltExe
            }
            # Rebuild failed; fall back to the pre-pin vcpkg.exe rather than
            # returning $null outright -- it's still a usable, just
            # unpinned, tool, and Invoke-VcpkgBootstrap already recorded
            # its own FAIL explaining why the rebuild didn't happen.
            return $exe
        }
        return $exe
    }

    if (-not (Test-Path -LiteralPath $Root)) {
        Add-Result -Step 'vcpkg bootstrap' -Status 'WARN' -Message "Cloning vcpkg into $Root ..."
        $gitCmd = Get-Command git -ErrorAction SilentlyContinue
        if (-not $gitCmd) {
            Add-Result -Step 'vcpkg bootstrap' -Status 'FAIL' -Message 'git is not on PATH; cannot clone vcpkg. Install git first.'
            return $null
        }
        # Full clone, NOT --depth 1 -- see Repair-ShallowVcpkgClone's
        # doc comment for why manifest mode requires full history, and
        # Set-VcpkgPinnedVersion's doc comment for why the tool pin below
        # also needs full history (the pinned tag's commit must be
        # reachable, not just whatever the branch tip was at clone time).
        & git clone https://github.com/microsoft/vcpkg.git $Root 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Add-Result -Step 'vcpkg bootstrap' -Status 'FAIL' -Message "git clone of vcpkg failed (exit $LASTEXITCODE)."
            return $null
        }
    }

    # Pin the vcpkg TOOL itself to a known-good release tag before building
    # it, so a fresh bootstrap reproduces the exact vcpkg version this
    # project has verified end-to-end rather than whatever the default
    # branch tip happens to be. See Set-VcpkgPinnedVersion's doc comment
    # and docs/development-environment.md for the full rationale.
    Set-VcpkgPinnedVersion -Root $Root | Out-Null

    return Invoke-VcpkgBootstrap -Root $Root
}

function Install-ManifestTriplet {
    <#
        Manifest-mode install: reads dependencies (curl, gtest), the
        builtin-baseline, and the curl version override from vcpkg.json at
        $RepoRoot -- no package name on the command line any more, unlike
        the old classic-mode `vcpkg install curl:<triplet>` /
        `vcpkg install gtest:<triplet>` invocations this replaces. Requires
        the working directory at $RepoRoot (set once via Push-Location near
        the top of this script) so vcpkg auto-detects vcpkg.json; this
        vcpkg build (`vcpkg version` = 2026-07-27-...) exposes no
        --x-manifest-root flag on `install` -- only --classic, to force the
        OTHER mode -- which confirms cwd-based auto-detection is the
        supported mechanism for this version.

        Installs everything vcpkg.json lists in one call -- curl AND gtest
        together, since manifest mode has no per-package argument. Called
        from both the "vcpkg / curl" and "vcpkg / gtest" steps below;
        whichever runs first does the real work, the second is a fast,
        idempotent no-op verification. $StepLabel lets each call site keep
        its own step name in the results table.

        $InstallRoot is passed as `--x-install-root` and MUST be a
        triplet-exclusive directory (see Get-VcpkgInstallRoot / the comment
        above $VcpkgInstalledRootX86 for why: a shared install root causes
        one triplet's install to delete the other's, confirmed by real
        execution, not by inspection).

        No explicit "schannel" feature: the current vcpkg curl port has no
        such feature, and SChannel is already wired in automatically via
        curl's default "ssl" feature on Windows.
        Rationale: see docs/development-environment.md#curl-no-schannel-vcpkg-feature
    #>
    param(
        [Parameter(Mandatory)][string]$VcpkgExe,
        [Parameter(Mandatory)][ValidateSet('x86-windows-static', 'x64-windows-static')][string]$Triplet,
        [Parameter(Mandatory)][string]$InstallRoot,
        [Parameter(Mandatory)][string]$StepLabel
    )

    # Capture vcpkg's own stdout+stderr instead of discarding it, so a FAIL
    # here shows the real underlying error (missing feature, compiler
    # detection failure, network error, etc.) instead of just an exit code.
    $vcpkgOutput = & $VcpkgExe install "--triplet=$Triplet" "--x-install-root=$InstallRoot" 2>&1
    # $vcpkgOutput collapses to $null (not @()) when the pipeline emits zero
    # objects -- e.g. an early process-spawn failure that exits non-zero
    # before printing anything. Piping $null straight into
    # ForEach-Object { $_.ToString() } throws "You cannot call a method on a
    # null-valued expression", which would dump a spurious exception right
    # in the code path meant to make failures cleanly diagnosable. Wrap in
    # @() to force a (possibly one-element, possibly $null-containing) array
    # first, then drop any $null entries before calling .ToString() on what
    # remains.
    $vcpkgOutputLines = @($vcpkgOutput) | Where-Object { $null -ne $_ } | ForEach-Object { $_.ToString() }

    if ($LASTEXITCODE -ne 0) {
        # Keep the summary table readable: show only the tail (where the
        # actual error line lives) and point at how to reproduce the full
        # output, rather than dumping a potentially huge build log into
        # Format-Table at the end of the script.
        $maxLines = 25
        $tail = if ($vcpkgOutputLines.Count -gt $maxLines) {
            @("... ($($vcpkgOutputLines.Count - $maxLines) earlier line(s) omitted; re-run ``vcpkg install --triplet=$Triplet`` directly from $RepoRoot for the full log) ...") + ($vcpkgOutputLines | Select-Object -Last $maxLines)
        } else {
            $vcpkgOutputLines
        }
        $tailText = $tail -join "`n"
        Add-Result -Step $StepLabel -Status 'FAIL' -Message @"
``vcpkg install --triplet=$Triplet`` (manifest mode, from $RepoRoot) exited $LASTEXITCODE. Output:
$tailText
"@
        return $false
    }

    # A zero exit code is necessary but not sufficient: it only says this
    # ONE invocation's own plan completed, not that the triplet's artifacts
    # are actually sitting on disk afterward. This closes exactly the gap
    # a real re-run exposed -- the previous version of this function
    # declared OK purely from $LASTEXITCODE, and that OK later turned out to
    # be meaningless once a subsequent call (for the other triplet, against
    # a then-shared install root) deleted what this one had just produced.
    # Separate per-triplet install roots (see $VcpkgInstalledRootX86 /
    # $VcpkgInstalledRootX64 above) remove that specific failure mode
    # structurally, but this check stays as a direct, defence-in-depth
    # verification that OK means "the lib directory genuinely exists", not
    # just "the subprocess exited zero".
    $tripletLibDir = Join-Path $InstallRoot "$Triplet\lib"
    if (-not (Test-Path -LiteralPath $tripletLibDir)) {
        Add-Result -Step $StepLabel -Status 'FAIL' -Message "``vcpkg install`` exited 0, but the expected lib directory does not exist afterward: $tripletLibDir -- treat this as a real failure, not a pass; do not trust exit code alone."
        return $false
    }

    Add-Result -Step $StepLabel -Status 'OK' -Message "Manifest dependencies for $Triplet installed (or already up to date) per vcpkg.json: curl, gtest. Verified $tripletLibDir exists."
    return $true
}

function Copy-TripletLibs {
    <#
        Copies ONLY the product-linked dependency files (libcurl.lib,
        zs.lib) into lib/<arch>/, which the Makefile's LIBS variable links
        directly into the shipped DLL -- deliberately NOT a blanket
        "copy everything in lib/" like the earlier version of this function.

        Under manifest mode, curl/zlib AND gtest/gmock are all listed in one
        vcpkg.json and land together in the same triplet lib/ directory
        (confirmed by real execution: the unfiltered
        version of this function copied gmock.lib and gtest.lib into
        lib/x64/ alongside libcurl.lib and zs.lib). gtest.lib/gmock.lib
        reaching lib/<arch>/ is not a linker-safety bug today -- the
        Makefile's LIBS names only libcurl.lib and zs.lib explicitly, so
        link.exe never pulls the extras in via /LIBPATH alone -- but it is
        unnecessary copy noise in the product-linked directory and violates
        the product-linked-vs-test-only split this project maintains
        elsewhere (see msvc-build-conventions). gmock specifically is not
        used by this project at all (TEST_LIBS only ever names gtest.lib
        gtest_main.lib), so it has no legitimate destination, product or
        test-only.
        Rationale: see docs/development-environment.md#manifest-mode-triplet-installs-need-separate-install-roots
    #>
    param(
        [Parameter(Mandatory)][string]$InstalledRoot,
        [Parameter(Mandatory)][string]$Triplet,
        [Parameter(Mandatory)][string]$DestDir
    )

    # $InstalledRoot is the triplet-exclusive install root (see
    # $VcpkgInstalledRootX86 / $VcpkgInstalledRootX64), NOT
    # $VcpkgRoot\installed\, which is where classic mode would have put this.
    $srcLibDir = Join-Path $InstalledRoot "$Triplet\lib"
    if (-not (Test-Path -LiteralPath $srcLibDir)) {
        Add-Result -Step "lib copy ($Triplet)" -Status 'FAIL' -Message "Expected lib directory not found: $srcLibDir"
        return
    }

    New-Item -ItemType Directory -Force -Path $DestDir | Out-Null

    $productLibNames = 'libcurl.lib', 'zs.lib'

    # Synchronising, not purely additive: prune any stale .lib left behind by
    # an older allow-list (gmock.lib/gtest.lib residue from before
    # the filter above existed) so DestDir can never drift from the allow-list.
    Get-ChildItem -LiteralPath $DestDir -Filter '*.lib' -File -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -notin $productLibNames } |
        Remove-Item -Force

    $libs = @(Get-ChildItem -LiteralPath $srcLibDir -File | Where-Object { $_.Name -in $productLibNames })
    if ($libs.Count -eq 0) {
        Add-Result -Step "lib copy ($Triplet)" -Status 'FAIL' -Message "None of the expected product-linked files ($($productLibNames -join ', ')) found under $srcLibDir"
        return
    }

    foreach ($lib in $libs) {
        Copy-Item -LiteralPath $lib.FullName -Destination $DestDir -Force
    }

    $missing = $productLibNames | Where-Object { $_ -notin $libs.Name }
    if ($missing.Count -gt 0) {
        Add-Result -Step "lib copy ($Triplet)" -Status 'WARN' -Message "Copied $($libs.Count) file(s) to $DestDir : $(($libs.Name) -join ', ') -- but expected file(s) not found: $($missing -join ', ')."
        return
    }

    Add-Result -Step "lib copy ($Triplet)" -Status 'OK' -Message "Copied $($libs.Count) .lib file(s) to $DestDir : $(($libs.Name) -join ', ')"
}

function Get-InstalledCurlVersion {
    param(
        [Parameter(Mandatory)][string]$VcpkgExe,
        [Parameter(Mandatory)][string]$Triplet,
        [Parameter(Mandatory)][string]$InstallRoot
    )
    # --x-install-root must match the root that Triplet was actually
    # installed into (see $VcpkgInstalledRootX86 / $VcpkgInstalledRootX64);
    # without it, `vcpkg list` resolves against its own default install
    # root, which no longer matches once triplets use separate roots.
    $line = & $VcpkgExe list "curl:$Triplet" "--x-install-root=$InstallRoot" 2>$null | Select-Object -First 1
    if ($line -match '\S+\s+(\S+)') {
        return $Matches[1]
    }
    return 'unknown'
}

$script:CurlVersionX86ForReadme = 'unknown'
$script:CurlVersionX64ForReadme = 'unknown'

Invoke-Step -Name 'vcpkg / curl' -Body {
    if ($SkipVcpkg) {
        Add-Result -Step 'vcpkg / curl' -Status 'SKIP' -Message 'Skipped by -SkipVcpkg.'
        return
    }

    $vcpkgExe = Install-VcpkgIfMissing -Root $VcpkgRoot
    if (-not $vcpkgExe) {
        Add-Result -Step 'vcpkg / curl' -Status 'FAIL' -Message 'Skipping manifest install and lib copy: vcpkg is not available.'
        return
    }
    Add-Result -Step 'vcpkg' -Status 'OK' -Message "Using vcpkg at: $vcpkgExe"

    # Baseline/tool-pin drift guard -- runs right after the tool pin (Set-VcpkgPinnedVersion,
    # inside Install-VcpkgIfMissing above) has had a chance to act, before any
    # manifest install is attempted, so a mismatch is reported clearly instead
    # of surfacing later as a confusing "no version database entry" error from
    # vcpkg itself. Mirrors the equivalent step in .github/workflows/ci.yml.
    Assert-VcpkgBaselinePin -Root $VcpkgRoot

    $okX86 = Install-ManifestTriplet -VcpkgExe $vcpkgExe -Triplet 'x86-windows-static' -InstallRoot $VcpkgInstalledRootX86 -StepLabel 'vcpkg install (x86-windows-static)'
    $okX64 = Install-ManifestTriplet -VcpkgExe $vcpkgExe -Triplet 'x64-windows-static' -InstallRoot $VcpkgInstalledRootX64 -StepLabel 'vcpkg install (x64-windows-static)'

    if ($okX86) {
        Copy-TripletLibs -InstalledRoot $VcpkgInstalledRootX86 -Triplet 'x86-windows-static' -DestDir $LibX86Dir
        $script:CurlVersionX86ForReadme = Get-InstalledCurlVersion -VcpkgExe $vcpkgExe -Triplet 'x86-windows-static' -InstallRoot $VcpkgInstalledRootX86
    } else {
        Add-Result -Step 'lib copy (x86-windows-static)' -Status 'SKIP' -Message 'Skipped: manifest install for this triplet failed.'
    }

    if ($okX64) {
        Copy-TripletLibs -InstalledRoot $VcpkgInstalledRootX64 -Triplet 'x64-windows-static' -DestDir $LibX64Dir
        $script:CurlVersionX64ForReadme = Get-InstalledCurlVersion -VcpkgExe $vcpkgExe -Triplet 'x64-windows-static' -InstallRoot $VcpkgInstalledRootX64
    } else {
        Add-Result -Step 'lib copy (x64-windows-static)' -Status 'SKIP' -Message 'Skipped: manifest install for this triplet failed.'
    }

    if (($script:CurlVersionX86ForReadme -ne 'unknown') -and ($script:CurlVersionX64ForReadme -ne 'unknown') -and `
        ($script:CurlVersionX86ForReadme -ne $script:CurlVersionX64ForReadme)) {
        Add-Result -Step 'vcpkg / curl' -Status 'WARN' -Message "x86 curl version ($script:CurlVersionX86ForReadme) and x64 curl version ($script:CurlVersionX64ForReadme) differ -- both triplets should normally resolve to the same vcpkg port version."
    }
}

# ===========================================================================
# Step 7 -- vcpkg / GoogleTest: build-time-only test dependency for
# `make test`. Kept as its own Invoke-Step block, separate from the curl
# step above, so a failure copying/verifying gtest's output is still
# reported independently of curl's -- per the same probe-before-acting /
# doesn't-abort-the-rest pattern this script uses throughout.
#
# Under manifest mode this step no longer has its own install call: curl
# and gtest are both listed in vcpkg.json, so the single
# `vcpkg install --triplet=<T>` command run by the "vcpkg / curl" step
# above already provisions gtest too. This step consumes that shared
# result (via Install-ManifestTriplet again, which is a fast, idempotent
# no-op if the curl step already ran) and is responsible for gtest's own
# concern: copying its .lib/.h output into the test-only locations.
# ===========================================================================

function Copy-GTestTripletLibs {
    <#
        Copies gtest*.lib into a single flat destination directory --
        specifically NOT a straight "copy everything in lib/" like
        Copy-TripletLibs, because the vcpkg gtest port installs
        gtest_main.lib to a DIFFERENT subdirectory than gtest.lib.

        Verified from the port's own fix-main-lib-path.patch (upstream
        GoogleTest's CMake install_project() puts any target whose name
        matches "_main" into lib/manual-link/, not plain lib/, so a
        consumer never accidentally links two main()s together):
          installed/<triplet>/lib/gtest.lib
          installed/<triplet>/lib/manual-link/gtest_main.lib
        (gmock.lib / gmock_main.lib follow the identical split, but are not
        copied -- the project depends on gtest, not gmock.)

        Only *.lib files matching "gtest*" are copied, from both source
        directories, flattened into one lib/gtest/<arch>/ destination --
        matching what the Makefile's GTESTDIR/TEST_LIBS already expect
        (gtest.lib gtest_main.lib, both resolved via one /LIBPATH:).
    #>
    param(
        [Parameter(Mandatory)][string]$InstalledRoot,
        [Parameter(Mandatory)][string]$Triplet,
        [Parameter(Mandatory)][string]$DestDir
    )

    # $InstalledRoot is the triplet-exclusive install root (see
    # $VcpkgInstalledRootX86 / $VcpkgInstalledRootX64), NOT
    # $VcpkgRoot\installed\, which is where classic mode would have put this
    # (see Copy-TripletLibs).
    $srcLibDir        = Join-Path $InstalledRoot "$Triplet\lib"
    $srcManualLinkDir = Join-Path $srcLibDir 'manual-link'

    if (-not (Test-Path -LiteralPath $srcLibDir)) {
        Add-Result -Step "gtest lib copy ($Triplet)" -Status 'FAIL' -Message "Expected lib directory not found: $srcLibDir"
        return
    }

    New-Item -ItemType Directory -Force -Path $DestDir | Out-Null

    $libs = @(Get-ChildItem -LiteralPath $srcLibDir -Filter 'gtest*.lib' -File -ErrorAction SilentlyContinue)
    if (Test-Path -LiteralPath $srcManualLinkDir) {
        $libs += @(Get-ChildItem -LiteralPath $srcManualLinkDir -Filter 'gtest*.lib' -File -ErrorAction SilentlyContinue)
    }

    if ($libs.Count -eq 0) {
        Add-Result -Step "gtest lib copy ($Triplet)" -Status 'FAIL' -Message "No gtest*.lib files found under $srcLibDir or $srcManualLinkDir"
        return
    }

    foreach ($lib in $libs) {
        Copy-Item -LiteralPath $lib.FullName -Destination $DestDir -Force
    }

    $missing = @('gtest.lib', 'gtest_main.lib') | Where-Object { $_ -notin $libs.Name }
    if ($missing.Count -gt 0) {
        Add-Result -Step "gtest lib copy ($Triplet)" -Status 'WARN' -Message "Copied $($libs.Count) file(s) to $DestDir : $(($libs.Name) -join ', ') -- but expected file(s) not found: $($missing -join ', '). vcpkg's gtest port may have changed its output layout; re-check fix-main-lib-path.patch and update this function."
        return
    }

    Add-Result -Step "gtest lib copy ($Triplet)" -Status 'OK' -Message "Copied $($libs.Count) .lib file(s) to $DestDir : $(($libs.Name) -join ', ')"
}

function Copy-GTestHeaders {
    <#
        GoogleTest headers are architecture-agnostic (no per-arch #ifdef
        content relevant here), so they are copied once from whichever
        triplet's install succeeded, not once per architecture -- matching
        the Makefile's TEST_INCLUDES comment ("GoogleTest headers are
        architecture-agnostic ... only the .lib binaries are per-architecture").
        Standard CMake/GNUInstallDirs layout for the googletest project
        installs public headers to <prefix>/include/gtest/*.h; this is
        upstream GoogleTest's own long-standing install convention, not
        something the vcpkg port changes.
    #>
    param(
        [Parameter(Mandatory)][string]$InstalledRoot,
        [Parameter(Mandatory)][string]$Triplet,
        [Parameter(Mandatory)][string]$DestDir
    )

    # $InstalledRoot is the triplet-exclusive install root (see
    # $VcpkgInstalledRootX86 / $VcpkgInstalledRootX64), NOT
    # $VcpkgRoot\installed\, which is where classic mode would have put this
    # (see Copy-TripletLibs).
    $srcIncludeDir = Join-Path $InstalledRoot "$Triplet\include\gtest"
    if (-not (Test-Path -LiteralPath $srcIncludeDir)) {
        Add-Result -Step 'gtest headers' -Status 'FAIL' -Message "Expected header directory not found: $srcIncludeDir"
        return $false
    }

    New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
    Copy-Item -Path (Join-Path $srcIncludeDir '*') -Destination $DestDir -Recurse -Force
    $count = @(Get-ChildItem -LiteralPath $DestDir -Filter '*.h' -Recurse -File).Count
    Add-Result -Step 'gtest headers' -Status 'OK' -Message "Copied $count header file(s) from $srcIncludeDir to $DestDir (source triplet: $Triplet)."
    return $true
}

function Get-InstalledGTestVersion {
    param(
        [Parameter(Mandatory)][string]$VcpkgExe,
        [Parameter(Mandatory)][string]$Triplet,
        [Parameter(Mandatory)][string]$InstallRoot
    )
    # Same reasoning as Get-InstalledCurlVersion: --x-install-root must
    # match the root Triplet was actually installed into (see
    # $VcpkgInstalledRootX86 / $VcpkgInstalledRootX64), or `vcpkg list`
    # resolves against its own default install root instead.
    $line = & $VcpkgExe list "gtest:$Triplet" "--x-install-root=$InstallRoot" 2>$null | Select-Object -First 1
    if ($line -match '\S+\s+(\S+)') {
        return $Matches[1]
    }
    return 'unknown'
}

$script:GTestVersionX86ForReadme = 'unknown'
$script:GTestVersionX64ForReadme = 'unknown'

Invoke-Step -Name 'vcpkg / gtest' -Body {
    if ($SkipGTest) {
        Add-Result -Step 'vcpkg / gtest' -Status 'SKIP' -Message 'Skipped by -SkipGTest.'
        return
    }
    if ($SkipVcpkg) {
        Add-Result -Step 'vcpkg / gtest' -Status 'SKIP' -Message 'Skipped: -SkipVcpkg implies no vcpkg-based installs.'
        return
    }

    $vcpkgExe = Install-VcpkgIfMissing -Root $VcpkgRoot
    if (-not $vcpkgExe) {
        Add-Result -Step 'vcpkg / gtest' -Status 'FAIL' -Message 'Skipping manifest install and lib/header copy: vcpkg is not available.'
        return
    }

    # Same manifest install as the "vcpkg / curl" step above -- curl and
    # gtest are both listed in vcpkg.json, so this call is a fast,
    # idempotent no-op if that step already ran; kept here too so this step
    # is still correct standalone (e.g. -SkipVcpkg not set but the curl
    # step somehow didn't run first).
    $okX86 = Install-ManifestTriplet -VcpkgExe $vcpkgExe -Triplet 'x86-windows-static' -InstallRoot $VcpkgInstalledRootX86 -StepLabel 'vcpkg install (x86-windows-static, gtest)'
    $okX64 = Install-ManifestTriplet -VcpkgExe $vcpkgExe -Triplet 'x64-windows-static' -InstallRoot $VcpkgInstalledRootX64 -StepLabel 'vcpkg install (x64-windows-static, gtest)'

    $headersCopied = $false

    if ($okX86) {
        Copy-GTestTripletLibs -InstalledRoot $VcpkgInstalledRootX86 -Triplet 'x86-windows-static' -DestDir $LibGtestX86Dir
        $script:GTestVersionX86ForReadme = Get-InstalledGTestVersion -VcpkgExe $vcpkgExe -Triplet 'x86-windows-static' -InstallRoot $VcpkgInstalledRootX86
        if (-not $headersCopied) {
            $headersCopied = Copy-GTestHeaders -InstalledRoot $VcpkgInstalledRootX86 -Triplet 'x86-windows-static' -DestDir $GtestVendorDir
        }
    } else {
        Add-Result -Step 'gtest lib copy (x86-windows-static)' -Status 'SKIP' -Message 'Skipped: manifest install for this triplet failed.'
    }

    if ($okX64) {
        Copy-GTestTripletLibs -InstalledRoot $VcpkgInstalledRootX64 -Triplet 'x64-windows-static' -DestDir $LibGtestX64Dir
        $script:GTestVersionX64ForReadme = Get-InstalledGTestVersion -VcpkgExe $vcpkgExe -Triplet 'x64-windows-static' -InstallRoot $VcpkgInstalledRootX64
        if (-not $headersCopied) {
            $headersCopied = Copy-GTestHeaders -InstalledRoot $VcpkgInstalledRootX64 -Triplet 'x64-windows-static' -DestDir $GtestVendorDir
        }
    } else {
        Add-Result -Step 'gtest lib copy (x64-windows-static)' -Status 'SKIP' -Message 'Skipped: manifest install for this triplet failed.'
    }

    if (-not $headersCopied) {
        Add-Result -Step 'gtest headers' -Status 'FAIL' -Message 'Neither triplet installed successfully; include/vendor/gtest/ was not populated.'
    }

    if (($script:GTestVersionX86ForReadme -ne 'unknown') -and ($script:GTestVersionX64ForReadme -ne 'unknown') -and `
        ($script:GTestVersionX86ForReadme -ne $script:GTestVersionX64ForReadme)) {
        Add-Result -Step 'vcpkg / gtest' -Status 'WARN' -Message "x86 gtest version ($script:GTestVersionX86ForReadme) and x64 gtest version ($script:GTestVersionX64ForReadme) differ -- both triplets should normally resolve to the same vcpkg port version."
    }
}

# ===========================================================================
# Step 8 -- json.hpp: download, verify SHA-256, place in include/vendor/
# ===========================================================================

Invoke-Step -Name 'json.hpp' -Body {
    if ($SkipJson) {
        Add-Result -Step 'json.hpp' -Status 'SKIP' -Message 'Skipped by -SkipJson.'
        return
    }

    New-Item -ItemType Directory -Force -Path $VendorDir | Out-Null

    if ((Test-Path -LiteralPath $JsonHppDst) -and -not $Force) {
        $existingHash = (Get-FileHash -LiteralPath $JsonHppDst -Algorithm SHA256).Hash
        if ($existingHash -eq $JsonHppSha256) {
            Add-Result -Step 'json.hpp' -Status 'OK' -Message "Already present with matching SHA-256 (v$JsonHppVersion)."
            return
        }
        Add-Result -Step 'json.hpp' -Status 'WARN' -Message 'Existing file present but hash mismatch; re-downloading pinned version.'
    }

    $tempFile = Join-Path $env:TEMP 'json.hpp.download'
    try {
        Invoke-WebRequest -Uri $JsonHppUrl -OutFile $tempFile -UseBasicParsing
    } catch {
        Add-Result -Step 'json.hpp' -Status 'FAIL' -Message "Download failed: $($_.Exception.Message)"
        return
    }

    $downloadedHash = (Get-FileHash -LiteralPath $tempFile -Algorithm SHA256).Hash
    if ($downloadedHash -ne $JsonHppSha256) {
        Remove-Item -LiteralPath $tempFile -Force -ErrorAction SilentlyContinue
        Add-Result -Step 'json.hpp' -Status 'FAIL' -Message @"
SHA-256 mismatch for downloaded json.hpp -- refusing to place an unverified
file into include/vendor/.
  expected: $JsonHppSha256
  actual:   $downloadedHash
"@
        return
    }

    Move-Item -LiteralPath $tempFile -Destination $JsonHppDst -Force
    Add-Result -Step 'json.hpp' -Status 'OK' -Message "Downloaded, SHA-256 verified, placed at $JsonHppDst (v$JsonHppVersion)."
}

# ===========================================================================
# Step 8 -- directory skeleton (lib/x86, lib/x64) + lib/README
# ===========================================================================

Invoke-Step -Name 'lib skeleton + README' -Body {
    New-Item -ItemType Directory -Force -Path $LibX86Dir | Out-Null
    New-Item -ItemType Directory -Force -Path $LibX64Dir | Out-Null
    Add-Result -Step 'lib skeleton' -Status 'OK' -Message "Confirmed lib\x86 and lib\x64 exist."

    $unknownMsg = '<unknown -- vcpkg step did not run or failed; re-run setup-dev-env.ps1>'
    $curlVersionX86 = if ($script:CurlVersionX86ForReadme -and $script:CurlVersionX86ForReadme -ne 'unknown') {
        $script:CurlVersionX86ForReadme
    } else {
        $unknownMsg
    }
    $curlVersionX64 = if ($script:CurlVersionX64ForReadme -and $script:CurlVersionX64ForReadme -ne 'unknown') {
        $script:CurlVersionX64ForReadme
    } else {
        $unknownMsg
    }
    $curlVersion = if (($curlVersionX86 -eq $curlVersionX64) -and ($curlVersionX86 -ne $unknownMsg)) {
        $curlVersionX86
    } else {
        "x86=$curlVersionX86, x64=$curlVersionX64"
    }

    $gtestVersionX86 = if ($script:GTestVersionX86ForReadme -and $script:GTestVersionX86ForReadme -ne 'unknown') {
        $script:GTestVersionX86ForReadme
    } else {
        $unknownMsg
    }
    $gtestVersionX64 = if ($script:GTestVersionX64ForReadme -and $script:GTestVersionX64ForReadme -ne 'unknown') {
        $script:GTestVersionX64ForReadme
    } else {
        $unknownMsg
    }
    $gtestVersion = if (($gtestVersionX86 -eq $gtestVersionX64) -and ($gtestVersionX86 -ne $unknownMsg)) {
        $gtestVersionX86
    } else {
        "x86=$gtestVersionX86, x64=$gtestVersionX64"
    }

    $readmePath = Join-Path $RepoRoot 'lib\README'
    $readme = @"
# lib/

These directories hold **provisioned, uncommitted** dependency binaries,
built with /MT (static multithreaded CRT) to match the project. They are
NOT vendored artifacts under version control -- .gitignore excludes
lib/x86/, lib/x64/, lib/gtest/ and include/vendor/gtest/ deliberately, on
purpose, every time. Each environment (developer machine, CI runner)
regenerates this tree independently, by running scripts/setup-dev-env.ps1
(local) or the CI provisioning step (CI), from its own toolchain. Do not
hand-edit the .lib files -- re-run the provisioning step instead. Do not
hand-edit this file either: it is generated by the "lib skeleton + README"
step of scripts/setup-dev-env.ps1 and will be silently overwritten on the
next run; edit the template in that script instead.

**The authoritative pin is vcpkg.json at the repo root**, not this file.
This README restates versions and filenames for convenience at the moment
they are needed (e.g. writing a linker line), but if this file and
vcpkg.json ever disagree, vcpkg.json wins -- re-run the provisioning step
to bring this file back in sync.

## Layout

lib/x86/        x86 static libraries (vcpkg triplet x86-windows-static) --
                 product-linked: linked into the product DLL
lib/x64/        x64 static libraries (vcpkg triplet x64-windows-static) --
                 product-linked: linked into the product DLL
lib/gtest/x86/  x86 GoogleTest static libraries (vcpkg triplet
                 x86-windows-static) -- test-only: build-time-only, linked
                 into the test executable, never the DLL
lib/gtest/x64/  x64 GoogleTest static libraries (vcpkg triplet
                 x64-windows-static) -- test-only: build-time-only, linked
                 into the test executable, never the DLL

## Versions

- libcurl: $curlVersion (vcpkg port ``curl`` default features, which pull in
  SChannel automatically on Windows -- no OpenSSL dependency); file:
  lib/x86/libcurl.lib, lib/x64/libcurl.lib
- zlib: bundled transitively via vcpkg's libcurl dependency; file:
  lib/x86/zs.lib, lib/x64/zs.lib -- note the filename is ``zs.lib``, not
  ``zlib.lib``; this is not a name anyone would guess from the port name,
  so it is spelled out here explicitly for whoever next writes a linker
  line against it.
- nlohmann/json: v$JsonHppVersion, single-header ``json.hpp`` at
  include/vendor/json.hpp, SHA-256 verified against the official v3.11.3
  GitHub Release asset (see scripts/setup-dev-env.ps1 for the pinned hash
  and verification provenance). Unlike everything else in this file,
  json.hpp IS committed source, not a provisioned binary -- it is a single
  checksummed header, outside the compiled-artifact principle above.
- GoogleTest: $gtestVersion (vcpkg port ``gtest``); files:
  lib/gtest/x86/gtest.lib, lib/gtest/x86/gtest_main.lib,
  lib/gtest/x64/gtest.lib, lib/gtest/x64/gtest_main.lib.
  Test-only -- build-time-only, linked into the test executable, never the
  shipped DLL. The vcpkg ``gtest`` port's own patch splits the install
  output: ``gtest.lib`` stays in the triplet's plain ``lib/``, while
  ``gtest_main.lib`` is relocated to ``lib/manual-link/`` (a vcpkg
  ``manual-link`` split, so a consumer never accidentally links two
  ``main()``s together); this is why the bootstrap script's copy logic
  reads from both source directories before flattening everything into
  lib/gtest/<arch>/.

Both libcurl/zlib and GoogleTest are resolved from the single pin in
vcpkg.json (``builtin-baseline`` plus the ``curl``/``gtest`` dependency
entries) -- the version numbers above are what that pin resolved to on
this machine at the time this file was last written, not a second,
independently-maintained pin.

Verify /MT provenance with ``dumpbin /directives <lib>`` -- expect
``/DEFAULTLIB:LIBCMT``, never ``MSVCRT``. This is a per-environment check:
each environment verifies its own provisioned copies.

Last refreshed: $(Get-Date -Format 'yyyy-MM-dd HH:mm:ss') by scripts/setup-dev-env.ps1
"@
    Set-Content -LiteralPath $readmePath -Value $readme -NoNewline
    Add-Result -Step 'lib/README' -Status 'OK' -Message "Written to $readmePath"
}

# ===========================================================================
# Step 9 -- verify (never fetch) the Vector CAPL SDK headers
# ===========================================================================

Invoke-Step -Name 'CAPL SDK headers' -Body {
    $required = 'cdll.h', 'VIA.h', 'VIA_CDLL.h'
    $missing = @()
    foreach ($name in $required) {
        $path = Join-Path $SdkDir $name
        if (-not (Test-Path -LiteralPath $path)) {
            $missing += $name
        }
    }

    if ($missing.Count -eq 0) {
        Add-Result -Step 'CAPL SDK headers' -Status 'OK' -Message "All three headers present at $SdkDir"
        return
    }

    Add-Result -Step 'CAPL SDK headers' -Status 'FAIL' -Message @"
Missing SDK header(s): $($missing -join ', ') under $SdkDir
This script never fetches these -- they are Vector-licensed material sourced
from a Vector CANoe/CANalyzer installation. Install Vector CANoe/CANalyzer
and build the official "Example of a Windows DLL for CAPL" sample to obtain
them, then place all three at include/vendor/capl-dll-sdk/.
"@
}

# ===========================================================================
# Summary
# ===========================================================================

Write-Host ''
Write-Host '=== setup-dev-env.ps1 summary ===' -ForegroundColor Cyan
$script:Results | Format-Table -AutoSize -Property Status, Step, Message | Out-Host

$failCount = ($script:Results | Where-Object { $_.Status -eq 'FAIL' }).Count
$warnCount = ($script:Results | Where-Object { $_.Status -eq 'WARN' }).Count
$okCount   = ($script:Results | Where-Object { $_.Status -eq 'OK' }).Count

Write-Host "$okCount OK, $warnCount WARN, $failCount FAIL" -ForegroundColor $(if ($failCount -gt 0) { 'Red' } elseif ($warnCount -gt 0) { 'Yellow' } else { 'Green' })

Pop-Location

if ($failCount -gt 0) {
    exit 1
}
exit 0
