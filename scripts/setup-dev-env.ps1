<#
.SYNOPSIS
    Provisions the local development environment for the CAPL REST DLL
    (restifycapl): MSVC Build Tools, make, vcpkg-built libcurl (both
    architectures), and the pinned nlohmann/json single header.

.DESCRIPTION
    This script provisions the environment ONLY. It never compiles project
    sources and never becomes a second build system -- `make` builds the
    project; this script only makes sure the tools and libraries `make`
    depends on are present. See docs/work/capl-rest-dll-rebuild/plans/plan.md
    (Stage 2, task BPE-1) for the authoritative spec.

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
      5. Detect or bootstrap vcpkg; install curl (default features pull in
         SChannel automatically on Windows) for both x86-windows-static and
         x64-windows-static triplets.
      6. Copy the resulting .lib files into lib/x86/ and lib/x64/.
      7. Download json.hpp (nlohmann/json v3.11.3), verify its SHA-256, and
         place it in include/vendor/.
      8. Create the lib/x86 and lib/x64 directories and write lib/README.
      9. Verify (never fetch) that include/vendor/capl-dll-sdk/ holds the
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
    Skip vcpkg bootstrap / curl install / lib copy (steps 5-6).

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
    [switch]$SkipJson,
    [string]$VcpkgRoot,
    [switch]$Force
)

$ErrorActionPreference = 'Continue'
$ProgressPreference = 'SilentlyContinue'

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------

$RepoRoot   = Split-Path -Parent $PSScriptRoot
$LibX86Dir  = Join-Path $RepoRoot 'lib\x86'
$LibX64Dir  = Join-Path $RepoRoot 'lib\x64'
$VendorDir  = Join-Path $RepoRoot 'include\vendor'
$SdkDir     = Join-Path $VendorDir 'capl-dll-sdk'
$JsonHppDst = Join-Path $VendorDir 'json.hpp'

# nlohmann/json v3.11.3 amalgamated single header, pinned per
# msvc-build-conventions. SHA-256 below was NOT taken on trust from any
# third-party listing -- it was independently computed on 2026-09-17 from
# two sources that must agree for the pin to be trustworthy:
#   (a) the official GitHub Release asset:
#       https://github.com/nlohmann/json/releases/download/v3.11.3/json.hpp
#   (b) the raw v3.11.3 tag content:
#       https://raw.githubusercontent.com/nlohmann/json/v3.11.3/single_include/nlohmann/json.hpp
# Both downloads were byte-identical (919975 bytes) and hashed to the same
# value, and the file's own NLOHMANN_JSON_VERSION_{MAJOR,MINOR,PATCH}
# macros read 3.11.3. A commonly-quoted checksum for this file
# (a22461d13119ac5c78f205d3df1db13403e58ce1bb1794469ccb779b737395bc) does
# NOT match either official download and must not be used -- re-verify
# against the two sources above before ever changing this constant.
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

    # Run in a throwaway cmd.exe subshell so this doesn't pollute the
    # current PowerShell session's environment, and so each architecture
    # gets a clean environment rather than a stacked one.
    #
    # Two separate invocations, deliberately not chained with `&&` into one
    # exit-code-gated command:
    #   1. Below, `where cl & where rc & where link & where dumpbin` are
    #      joined with bare `&`, not `&&`, so all four always run regardless
    #      of whether an earlier one failed to resolve -- this is
    #      deliberate: it means $LASTEXITCODE here only reflects the *last*
    #      command in the chain (`where dumpbin`), NOT a logical AND of all
    #      four, so it is NOT on its own a meaningful "all four resolved"
    #      signal. That's fine because it is never used alone -- the
    #      foreach loop right below independently and authoritatively
    #      re-checks that all four tool names appear in the captured output
    #      text, regardless of exit code. Running every `where` unconditionally
    #      (rather than short-circuiting on the first failure) is what makes
    #      that per-tool re-check possible: if an early tool fails to
    #      resolve, the later ones still run and still get checked, so a
    #      single pass reports every missing tool instead of only the first.
    #   2. Echoing %VSCMD_ARG_TGT_ARCH% (see below) is a separate invocation
    #      so a `where`-probe failure and an architecture mismatch are
    #      reported as distinct, unambiguous reasons rather than conflated
    #      into one command's exit code.
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

    # Architecture confirmation deliberately does NOT parse cl.exe's version
    # banner text. That banner is localized to the MSVC install's UI
    # language (e.g. on a Polish-language Build Tools install, cl prints
    # "...dla architektury x86" / "...dla x64" -- neither of which contains
    # the literal English substring "for x86"/"for x64"), so matching
    # English text against it is inherently locale-fragile and was
    # confirmed to false-FAIL on a real, correctly-configured install.
    #
    # Instead, use VSCMD_ARG_TGT_ARCH: vcvarsall.bat sets this itself (it
    # drives the rest of vcvarsall's own logic), it is not translated, and
    # its value is exactly "x86" or "x64" -- an authoritative,
    # locale-independent signal for which architecture the environment was
    # actually initialized for.
    #
    # This must use delayed expansion (`!VAR!`), NOT `%VAR%`, and the
    # `cmd.exe` invocation itself must be started with `/v:on`. cmd.exe
    # expands every `%VAR%` in a compound command line (things chained with
    # `&&`) once, at parse time, before ANY command in that line has
    # actually run -- so with plain `%VSCMD_ARG_TGT_ARCH%` here, the
    # variable is (correctly, per cmd.exe's own rules) substituted before
    # `call vcvarsall.bat` has had a chance to set it, since it is undefined
    # at parse time it is left as the literal text `%VSCMD_ARG_TGT_ARCH%`
    # rather than empty string -- this was confirmed empirically (both on a
    # real install and by design) and is exactly the FAIL text this
    # function used to produce. `/v:on` turns on delayed expansion for the
    # whole invocation *before* parsing begins, so `!VSCMD_ARG_TGT_ARCH!` is
    # resolved lazily, at execution time, after vcvarsall.bat has run.
    # Adding `setlocal enabledelayedexpansion` mid-line instead does NOT
    # work: the compound line is still parsed as a whole before any command
    # in it executes, so turning on delayed expansion partway through does
    # not retroactively change how the rest of that already-parsed line
    # resolves `!VAR!` references -- confirmed empirically to still return
    # the literal `!VSCMD_ARG_TGT_ARCH!` text. `/v:on` is the only fix that
    # works for this single-line chained shape.
    $archCmdLine = "call `"$VcvarsallPath`" $Arch >nul 2>&1 && echo !VSCMD_ARG_TGT_ARCH!"
    $archOutputText = ((& cmd.exe /v:on /c $archCmdLine 2>&1) -join "`n").Trim()

    if ($archOutputText -ne $Arch) {
        return [PSCustomObject]@{ Ok = $false; Reason = "VSCMD_ARG_TGT_ARCH after vcvarsall.bat $Arch was '$archOutputText', expected '$Arch' -- wrong-architecture Native Tools environment." }
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

    # Preference order and rationale (judgment call -- flagged in the
    # hand-off report, not mandated by the plan):
    #   1. Scoop  -- per-user installs, never needs elevation.
    #   2. an existing MSYS2 install (pacman) -- common on dev machines,
    #      no extra package manager to bootstrap.
    #   3. Chocolatey -- ubiquitous but installs to a machine-wide location
    #      and needs elevation.
    #   4. Fall back to reporting exact manual commands for all three.
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

function Get-VcpkgExe {
    param([Parameter(Mandatory)][string]$Root)
    $exe = Join-Path $Root 'vcpkg.exe'
    if (Test-Path -LiteralPath $exe) { return $exe }
    return $null
}

function Install-VcpkgIfMissing {
    param([Parameter(Mandatory)][string]$Root)

    $exe = Get-VcpkgExe -Root $Root
    if ($exe) {
        return $exe
    }

    if (-not (Test-Path -LiteralPath $Root)) {
        Add-Result -Step 'vcpkg bootstrap' -Status 'WARN' -Message "Cloning vcpkg into $Root ..."
        $gitCmd = Get-Command git -ErrorAction SilentlyContinue
        if (-not $gitCmd) {
            Add-Result -Step 'vcpkg bootstrap' -Status 'FAIL' -Message 'git is not on PATH; cannot clone vcpkg. Install git first.'
            return $null
        }
        & git clone --depth 1 https://github.com/microsoft/vcpkg.git $Root 2>&1 | Out-Null
        if ($LASTEXITCODE -ne 0) {
            Add-Result -Step 'vcpkg bootstrap' -Status 'FAIL' -Message "git clone of vcpkg failed (exit $LASTEXITCODE)."
            return $null
        }
    }

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

function Install-CurlTriplet {
    param(
        [Parameter(Mandatory)][string]$VcpkgExe,
        [Parameter(Mandatory)][ValidateSet('x86-windows-static', 'x64-windows-static')][string]$Triplet
    )

    # NOTE: no explicit "schannel" feature here. The current vcpkg curl port
    # (checked against port version curl 8.22.0-1) has no feature named
    # "schannel" -- `vcpkg install curl[schannel]:...` fails outright with
    # "curl has no feature named schannel." SChannel is instead wired in
    # automatically: curl's default-features include "ssl", and on
    # Windows (non-UWP), portfile.cmake adds -DCURL_USE_SCHANNEL=ON
    # whenever "ssl" is enabled and "http3" is not. So plain `curl:$Triplet`
    # already builds against SChannel with no OpenSSL dependency, matching
    # msvc-build-conventions' intent. If a future vcpkg port revision
    # reintroduces an explicit opt-in feature for this, re-add it here and
    # update msvc-build-conventions accordingly.
    $package = "curl:$Triplet"

    # Capture vcpkg's own stdout+stderr instead of discarding it, so a FAIL
    # here shows the real underlying error (missing feature, compiler
    # detection failure, network error, etc.) instead of just an exit code.
    $vcpkgOutput = & $VcpkgExe install $package 2>&1
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
            @("... ($($vcpkgOutputLines.Count - $maxLines) earlier line(s) omitted; re-run ``vcpkg install $package`` directly for the full log) ...") + ($vcpkgOutputLines | Select-Object -Last $maxLines)
        } else {
            $vcpkgOutputLines
        }
        $tailText = $tail -join "`n"
        Add-Result -Step "vcpkg install ($Triplet)" -Status 'FAIL' -Message @"
``vcpkg install $package`` exited $LASTEXITCODE. Output:
$tailText
"@
        return $false
    }
    Add-Result -Step "vcpkg install ($Triplet)" -Status 'OK' -Message "$package installed (or already up to date)."
    return $true
}

function Copy-TripletLibs {
    param(
        [Parameter(Mandatory)][string]$VcpkgRootPath,
        [Parameter(Mandatory)][string]$Triplet,
        [Parameter(Mandatory)][string]$DestDir
    )

    $srcLibDir = Join-Path $VcpkgRootPath "installed\$Triplet\lib"
    if (-not (Test-Path -LiteralPath $srcLibDir)) {
        Add-Result -Step "lib copy ($Triplet)" -Status 'FAIL' -Message "Expected lib directory not found: $srcLibDir"
        return
    }

    New-Item -ItemType Directory -Force -Path $DestDir | Out-Null
    $libs = Get-ChildItem -LiteralPath $srcLibDir -Filter '*.lib' -File
    if ($libs.Count -eq 0) {
        Add-Result -Step "lib copy ($Triplet)" -Status 'FAIL' -Message "No .lib files found under $srcLibDir"
        return
    }

    foreach ($lib in $libs) {
        Copy-Item -LiteralPath $lib.FullName -Destination $DestDir -Force
    }
    Add-Result -Step "lib copy ($Triplet)" -Status 'OK' -Message "Copied $($libs.Count) .lib file(s) to $DestDir : $(($libs.Name) -join ', ')"
}

function Get-InstalledCurlVersion {
    param(
        [Parameter(Mandatory)][string]$VcpkgExe,
        [Parameter(Mandatory)][string]$Triplet
    )
    $line = & $VcpkgExe list "curl:$Triplet" 2>$null | Select-Object -First 1
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
        Add-Result -Step 'vcpkg / curl' -Status 'FAIL' -Message 'Skipping curl install and lib copy: vcpkg is not available.'
        return
    }
    Add-Result -Step 'vcpkg' -Status 'OK' -Message "Using vcpkg at: $vcpkgExe"

    $okX86 = Install-CurlTriplet -VcpkgExe $vcpkgExe -Triplet 'x86-windows-static'
    $okX64 = Install-CurlTriplet -VcpkgExe $vcpkgExe -Triplet 'x64-windows-static'

    if ($okX86) {
        Copy-TripletLibs -VcpkgRootPath $VcpkgRoot -Triplet 'x86-windows-static' -DestDir $LibX86Dir
        $script:CurlVersionX86ForReadme = Get-InstalledCurlVersion -VcpkgExe $vcpkgExe -Triplet 'x86-windows-static'
    } else {
        Add-Result -Step 'lib copy (x86-windows-static)' -Status 'SKIP' -Message 'Skipped: curl install for this triplet failed.'
    }

    if ($okX64) {
        Copy-TripletLibs -VcpkgRootPath $VcpkgRoot -Triplet 'x64-windows-static' -DestDir $LibX64Dir
        $script:CurlVersionX64ForReadme = Get-InstalledCurlVersion -VcpkgExe $vcpkgExe -Triplet 'x64-windows-static'
    } else {
        Add-Result -Step 'lib copy (x64-windows-static)' -Status 'SKIP' -Message 'Skipped: curl install for this triplet failed.'
    }

    if (($script:CurlVersionX86ForReadme -ne 'unknown') -and ($script:CurlVersionX64ForReadme -ne 'unknown') -and `
        ($script:CurlVersionX86ForReadme -ne $script:CurlVersionX64ForReadme)) {
        Add-Result -Step 'vcpkg / curl' -Status 'WARN' -Message "x86 curl version ($script:CurlVersionX86ForReadme) and x64 curl version ($script:CurlVersionX64ForReadme) differ -- both triplets should normally resolve to the same vcpkg port version."
    }
}

# ===========================================================================
# Step 7 -- json.hpp: download, verify SHA-256, place in include/vendor/
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

    $readmePath = Join-Path $RepoRoot 'lib\README'
    $readme = @"
# lib/

Static dependencies for restifycapl, built with /MT (static multithreaded
CRT) to match the project. Populated by scripts/setup-dev-env.ps1 -- do not
hand-edit the .lib files; re-run the script instead.

## Layout

lib/x86/   x86 static libraries (vcpkg triplet x86-windows-static)
lib/x64/   x64 static libraries (vcpkg triplet x64-windows-static)

## Versions

- libcurl: $curlVersion (vcpkg port ``curl`` default features, which pull in
  SChannel automatically on Windows -- no OpenSSL dependency)
- zlib: bundled transitively via vcpkg's libcurl dependency; see
  installed/<triplet>/lib for the exact file
- nlohmann/json: v$JsonHppVersion, single-header ``json.hpp`` at
  include/vendor/json.hpp, SHA-256 verified against the official v3.11.3
  GitHub Release asset (see scripts/setup-dev-env.ps1 for the pinned hash
  and verification provenance)

Verify /MT provenance with ``dumpbin /directives <lib>`` -- expect
``/DEFAULTLIB:LIBCMT``, never ``MSVCRT``.

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
from a Vector CANoe/CANalyzer installation. See Stage 3 of the project plan
(HUM-10: install Vector CANoe/CANalyzer; HUM-11: build the official
"Example of a Windows DLL for CAPL" sample) to obtain them, then place all
three at include/vendor/capl-dll-sdk/.
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

if ($failCount -gt 0) {
    exit 1
}
exit 0
