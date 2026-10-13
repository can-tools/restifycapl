<#
.SYNOPSIS
Selects the CHANGELOG.md section for the release notes and validates its cut.

.DESCRIPTION
Subcommands (first argument):
  notes     -Mode tag|dry-run -Sha <commit> -Output <file> [-ChangelogPath <file>]
            select the section, validate it, write the notes body (LF, UTF-8, no BOM)
            and append the "taken from" line to $GITHUB_STEP_SUMMARY
  check     -Mode tag|dry-run -Sha <commit> [-ChangelogPath <file>]
            the same selection and validation, but writes no notes file and no
            "taken from" line
  fixtures  -FixtureDir <dir>
            run every <dir>/*/case.json against a simulated git history
Exit codes: 0 success; 1 a validation failed, git failed or CHANGELOG.md could not
be read (a file that is not valid UTF-8 included); 2 usage error (missing or invalid
-Mode, -Sha, -Output, -FixtureDir, or an unknown subcommand).
Diagnostics are GitHub workflow commands (::error::, ::warning::, ::notice::) on stdout.
notes and check require $env:GITHUB_STEP_SUMMARY: the step summary is appended to the file
it names, and the run fails with exit 1 at the first summary write if it is unset (notes
always writes a summary line, check only for a warning or notice). To run locally, set it
first, e.g. $env:GITHUB_STEP_SUMMARY = 'summary.md'. fixtures does not need it.
A fixture case may declare "match": "error" instead of exact output; the run must then
exit non-zero and print at least one ::error:: line (used where only the message text
is a known difference from the former inline parser).

.PARAMETER Command
The subcommand: notes, check or fixtures.

.PARAMETER Mode
tag (the run is for a pushed tag) or dry-run (a manual run that never publishes); required by notes and check.

.PARAMETER Sha
The commit being released or previewed; defaults to $env:GITHUB_SHA.

.PARAMETER Output
The file notes writes the release notes body to; required by notes.

.PARAMETER ChangelogPath
The changelog to read; defaults to CHANGELOG.md in the current directory.

.PARAMETER FixtureDir
The directory holding the fixture cases; required by fixtures.

.EXAMPLE
pwsh -NoProfile -File scripts/ci/changelog.ps1 notes -Mode dry-run -Sha (git rev-parse HEAD) -Output notes-body.md

.EXAMPLE
pwsh -NoProfile -File scripts/ci/changelog.ps1 check -Mode tag -Sha $env:GITHUB_SHA

.EXAMPLE
pwsh -NoProfile -File scripts/ci/changelog.ps1 fixtures -FixtureDir tests/changelog
#>

#requires -Version 7
[CmdletBinding()]
param(
  [Parameter(Position = 0)]
  [string]$Command,
  [string]$Mode,
  [string]$Sha = $env:GITHUB_SHA,
  [string]$Output,
  [string]$ChangelogPath = 'CHANGELOG.md',
  [string]$FixtureDir
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

class ChangelogExit : System.Exception {
  [int]$Code
  ChangelogExit([int]$code) : base('exit') { $this.Code = $code }
}

$Utf8 = New-Object System.Text.UTF8Encoding($false)
$Utf8Strict = New-Object System.Text.UTF8Encoding($false, $true)

# Case-sensitive on purpose: PowerShell's -match is not.
$ReHead = [regex]'^## \['
$ReHashHead = [regex]'^## \[([0-9a-f]{12})\] - \d{4}-\d{2}-\d{2}\r?$'
$ReLinkDef = [regex]'^\[[^\]]+\]:[ \t]*\S'
$ReH3 = [regex]'^### '
$ReDev = [regex]'^### Development\r?$'
$ReBlank = [regex]'^[ \t]*\r?$'

$script:Ctx = @{ Git = 'Invoke-RealGit'; Out = $null; Summary = $null; Facts = $null; ShowText = $null }

function Resolve-FullPath([string]$p) {
  if (-not [System.IO.Path]::IsPathRooted($p)) { $p = Join-Path (Get-Location).ProviderPath $p }
  return [System.IO.Path]::GetFullPath($p)
}

function Write-Line([string]$s) {
  if ($null -ne $script:Ctx.Out) { $script:Ctx.Out.Add($s) } else { [Console]::Out.WriteLine($s) }
}

function Add-Summary([string]$s) {
  if ($null -ne $script:Ctx.Summary) { [void]$script:Ctx.Summary.Append($s); return }
  if ([string]::IsNullOrEmpty($env:GITHUB_STEP_SUMMARY)) { Stop-WithError 'GITHUB_STEP_SUMMARY is not set; set it to a file path to run this script outside GitHub Actions' }
  [System.IO.File]::AppendAllText($env:GITHUB_STEP_SUMMARY, $s, $Utf8)
}

function Stop-Run([int]$code) { throw [ChangelogExit]::new($code) }

function Stop-WithError([string]$message) {
  Write-Line ('::error::' + $message)
  Stop-Run 1
}

function Add-Annotation([string]$kind, [string]$message) {
  Write-Line ('::{0}::{1}' -f $kind, $message)
  Add-Summary ($message + "`n`n")
}

function Get-Prefix([string]$s, [int]$n) {
  if ($s.Length -le $n) { return $s }
  return $s.Substring(0, $n)
}

function Split-Words([string]$s) {
  return [string[]]@($s.Split([char[]]@(' ', "`t", "`r", "`n"), [System.StringSplitOptions]::RemoveEmptyEntries))
}

function Invoke-RealGit([string[]]$GitArgs) {
  $psi = [System.Diagnostics.ProcessStartInfo]::new('git')
  foreach ($a in $GitArgs) { $psi.ArgumentList.Add($a) }
  $psi.RedirectStandardOutput = $true
  $psi.RedirectStandardError = $true
  $psi.UseShellExecute = $false
  $psi.StandardOutputEncoding = $Utf8
  $psi.StandardErrorEncoding = $Utf8
  $p = [System.Diagnostics.Process]::Start($psi)
  $o = $p.StandardOutput.ReadToEndAsync()
  $e = $p.StandardError.ReadToEndAsync()
  $p.WaitForExit()
  return [pscustomobject]@{ Code = $p.ExitCode; Out = $o.Result; Err = $e.Result }
}

# Answers exactly the git calls this script makes, from the case's "git" facts.
function Invoke-MockGit([string[]]$GitArgs) {
  $f = $script:Ctx.Facts
  $a = $GitArgs
  $ok = { param($o) [pscustomobject]@{ Code = 0; Out = $o; Err = '' } }
  $no = { param($c) [pscustomobject]@{ Code = $c; Out = ''; Err = 'unsupported or unknown in the simulated history' } }
  $commits = @(); if ($f.ContainsKey('commits')) { $commits = @($f['commits']) }
  $ancestors = @(); if ($f.ContainsKey('ancestors')) { $ancestors = @($f['ancestors']) }
  if ($f.ContainsKey('failures') -and $f['failures'].ContainsKey($a -join ' ')) {
    $fl = $f['failures'][$a -join ' ']
    return [pscustomobject]@{ Code = [int]$fl['code']; Out = ''; Err = [string]$fl['stderr'] }
  }
  if ($a.Count -eq 4 -and $a[0] -ceq 'rev-parse' -and $a[1] -ceq '--verify' -and $a[2] -ceq '--quiet' -and $a[3].EndsWith('^{commit}', [System.StringComparison]::Ordinal)) {
    $h = $a[3].Substring(0, $a[3].Length - '^{commit}'.Length)
    $m = @($commits | Where-Object { $_.StartsWith($h, [System.StringComparison]::Ordinal) })
    if ($m.Count -eq 1) { return & $ok ($m[0] + "`n") }
    return & $no 1
  }
  if ($a.Count -eq 2 -and $a[0] -ceq 'rev-parse' -and $a[1] -ceq 'origin/main' -and $f.ContainsKey('tip')) { return & $ok ($f['tip'] + "`n") }
  if ($a.Count -eq 2 -and $a[0] -ceq 'rev-parse' -and $f.ContainsKey('parent') -and $a[1] -ceq ($f['sha'] + '^')) { return & $ok ($f['parent'] + "`n") }
  if ($a.Count -eq 4 -and $a[0] -ceq 'merge-base' -and $a[1] -ceq '--is-ancestor' -and $a[3] -ceq $f['sha']) {
    if ($ancestors -ccontains $a[2]) { return & $ok '' }
    return & $no 1
  }
  if ($a.Count -eq 3 -and $a[0] -ceq 'rev-list' -and $a[1] -ceq '--first-parent' -and $a[2] -ceq 'origin/main' -and $f.ContainsKey('chain')) { return & $ok ((@($f['chain']) -join "`n") + "`n") }
  if ($a.Count -eq 2 -and $a[0] -ceq 'show' -and $a[1].EndsWith(':CHANGELOG.md', [System.StringComparison]::Ordinal)) {
    $c = $a[1].Substring(0, $a[1].Length - ':CHANGELOG.md'.Length)
    if ($f.ContainsKey('showTexts') -and $f['showTexts'].ContainsKey($c)) { return & $ok ([string]$f['showTexts'][$c]) }
    $at = @(); if ($f.ContainsKey('changelogAt')) { $at = @($f['changelogAt']) }
    if ($at -ccontains $c) { return & $ok $script:Ctx.ShowText }
    return & $no 128
  }
  if ($a.Count -eq 3 -and $a[0] -ceq 'tag' -and $a[1] -ceq '--points-at') {
    $t = @(); if ($f.ContainsKey('tags') -and $f['tags'].ContainsKey($a[2])) { $t = @($f['tags'][$a[2]]) }
    return & $ok (($t -join "`n") + "`n")
  }
  return & $no 128
}

function Invoke-Git([string[]]$GitArgs) { return (& $script:Ctx.Git $GitArgs) }

function Get-GitOutput([string[]]$GitArgs) {
  $r = Invoke-Git $GitArgs
  if ($r.Code -ne 0) { Stop-WithError ('git {0} failed: {1}' -f ($GitArgs -join ' '), $r.Err.Trim()) }
  return $r.Out.Trim()
}

# A cut is either merged already (the first-parent child of its base carries the
# heading) or pending (its base must be the origin/main tip the branch contains).
function Test-Cut([string]$h, [string]$heading, [string]$sha) {
  $found = Invoke-Git @('rev-parse', '--verify', '--quiet', ($h + '^{commit}'))
  $base = $found.Out.Trim()
  if ($found.Code -ne 0 -or (Invoke-Git @('merge-base', '--is-ancestor', $base, $sha)).Code -ne 0) {
    Stop-WithError ("the heading {0} names {1}, which is not a commit in this run's history; take the hash from git rev-parse --short=12 origin/main" -f $heading, $h)
  }
  $tip = Get-GitOutput @('rev-parse', 'origin/main')
  $chain = Split-Words (Get-GitOutput @('rev-list', '--first-parent', 'origin/main'))
  $idx = [System.Array]::IndexOf($chain, $base)
  if ($idx -gt 0) {
    $child = $chain[$idx - 1]
    $shown = Invoke-Git @('show', ($child + ':CHANGELOG.md'))
    if ($shown.Code -eq 0 -and (@($shown.Out.Split([char]10) | ForEach-Object { $_.TrimEnd("`r") }) -ccontains $heading)) {
      $tags = @((Split-Words (Get-GitOutput @('tag', '--points-at', $child))) | Where-Object { $_ -clike 'v*.*.*' })
      if ($tags.Count -gt 0) {
        Add-Annotation 'warning' ('The newest CHANGELOG section {0} was already released as {1}; this dry run previews its notes again. Add entries to [Unreleased] for the next release.' -f $heading, $tags[0])
      }
      else {
        Add-Annotation 'notice' ('The cut {0} is merged on main as {1}; push the tag on {1}.' -f $heading, (Get-Prefix $child 12))
      }
      return
    }
  }
  $errors = New-Object System.Collections.Generic.List[string]
  if ($h -cne (Get-Prefix $tip 12)) {
    $t12 = Get-Prefix $tip 12
    $errors.Add(('the heading {0} names {1}, but the origin/main tip is {2}; main has moved: merge main into the cut branch, then rewrite the heading hash and the links to {2}' -f $heading, $h, $t12))
  }
  if ((Invoke-Git @('merge-base', '--is-ancestor', $tip, $sha)).Code -ne 0) {
    $errors.Add(('the cut branch does not contain the origin/main tip {0}; merge main into the branch so the dry run builds what the squash merge will produce' -f (Get-Prefix $tip 12)))
  }
  if ($errors.Count -gt 0) {
    foreach ($e in $errors) { Write-Line ('::error::' + $e) }
    Stop-Run 1
  }
}

# Returns the section's notes lines, or $null when nothing is left once "### Development" is removed.
function Get-SectionBody($lines, [int]$start) {
  $end = $lines.Count
  for ($i = $start + 1; $i -lt $lines.Count; $i++) {
    if ($ReHead.IsMatch($lines[$i]) -or $ReLinkDef.IsMatch($lines[$i])) { $end = $i; break }
  }
  $body = [System.Collections.Generic.List[string]]::new($lines.GetRange($start + 1, $end - $start - 1))
  $dev = New-Object System.Collections.Generic.List[int]
  for ($i = 0; $i -lt $body.Count; $i++) { if ($ReDev.IsMatch($body[$i])) { $dev.Add($i) } }
  if ($dev.Count -gt 1) { Stop-WithError 'the section has more than one "### Development" block' }
  if ($dev.Count -eq 1) {
    $stop = $body.Count
    for ($i = $dev[0] + 1; $i -lt $body.Count; $i++) {
      if ($ReH3.IsMatch($body[$i])) { $stop = $i; break }
    }
    $body.RemoveRange($dev[0], $stop - $dev[0])
  }
  while ($body.Count -gt 0 -and $ReBlank.IsMatch($body[0])) { $body.RemoveAt(0) }
  while ($body.Count -gt 0 -and $ReBlank.IsMatch($body[$body.Count - 1])) { $body.RemoveAt($body.Count - 1) }
  $has = $false
  foreach ($line in $body) {
    if (-not $ReBlank.IsMatch($line) -and -not $ReH3.IsMatch($line)) { $has = $true; break }
  }
  if (-not $has) { return $null }
  return , [string[]]@($body | ForEach-Object { $_.TrimEnd("`r") })
}

function Resolve-Release([string]$Mode, [string]$Sha, [string]$Text) {
  $lines = [System.Collections.Generic.List[string]]::new([string[]]$Text.Split([char]10))
  $chosen = $null
  $body = $null
  if ($Mode -ceq 'tag') {
    # The squash-merge commit's first parent is the main commit the cut was based on.
    $parent = Get-GitOutput @('rev-parse', ($Sha + '^'))
    $starts = New-Object System.Collections.Generic.List[int]
    for ($i = 0; $i -lt $lines.Count; $i++) {
      $m = $ReHashHead.Match($lines[$i])
      if ($m.Success -and $parent.StartsWith($m.Groups[1].Value, [System.StringComparison]::Ordinal)) { $starts.Add($i) }
    }
    if ($starts.Count -ne 1) {
      Stop-WithError ('the tag must be on the squash-merge commit of the CHANGELOG cut; expected exactly one "## [{0}] - YYYY-MM-DD" heading, found {1}' -f (Get-Prefix $parent 12), $starts.Count)
    }
    $chosen = $lines[$starts[0]].TrimEnd("`r")
    $body = Get-SectionBody $lines $starts[0]
    if ($null -eq $body) { Stop-WithError ('the CHANGELOG section {0} is empty once "### Development" is removed' -f $chosen) }
  }
  else {
    for ($i = 0; $i -lt $lines.Count; $i++) {
      if ($ReHead.IsMatch($lines[$i])) {
        $candidate = Get-SectionBody $lines $i
        if ($null -ne $candidate) { $chosen = $lines[$i].TrimEnd("`r"); $body = $candidate; break }
      }
    }
    if ($null -eq $body) { Stop-WithError 'no CHANGELOG.md section has release notes once "### Development" is removed' }
    $m = $ReHashHead.Match($chosen)
    if ($m.Success) { Test-Cut $m.Groups[1].Value $chosen $Sha }
  }
  return [pscustomobject]@{ Chosen = $chosen; Body = $body }
}

function Invoke-Run([string]$Command, [string]$Mode, [string]$Sha, [byte[]]$Bytes, [string]$OutPath, [string]$Display) {
  try {
    try { $Text = $Utf8Strict.GetString($Bytes) }
    catch { Stop-WithError ('cannot read {0}: {1}' -f $Display, $_.Exception.Message) }
    $release = Resolve-Release $Mode $Sha $Text
    if ($Command -ceq 'notes') {
      [System.IO.File]::WriteAllText($OutPath, (($release.Body -join "`n") + "`n"), $Utf8)
      $line = 'Release notes taken from the CHANGELOG section `{0}` ({1} mode).' -f $release.Chosen, $Mode
      Add-Summary ($line + "`n")
    }
    return 0
  }
  catch [ChangelogExit] { return $_.Exception.Code }
  catch {
    Write-Line ('::error::unexpected error: ' + $_.Exception.Message)
    return 1
  }
}

function Write-Usage([string]$msg) {
  [Console]::Error.WriteLine($msg)
  [Console]::Error.WriteLine('usage: changelog.ps1 notes|check -Mode tag|dry-run -Sha <commit> [-Output <file>] [-ChangelogPath <file>] | fixtures -FixtureDir <dir>')
}

function Invoke-Captured([string]$Command, [string]$Mode, [string]$Sha, [byte[]]$Bytes, [string]$OutPath) {
  $script:Ctx.Out = New-Object System.Collections.Generic.List[string]
  $script:Ctx.Summary = New-Object System.Text.StringBuilder
  try {
    $code = Invoke-Run $Command $Mode $Sha $Bytes $OutPath 'CHANGELOG.md'
    return [pscustomobject]@{ Code = $code; Out = $script:Ctx.Out.ToArray(); Summary = $script:Ctx.Summary.ToString() }
  }
  finally {
    $script:Ctx.Out = $null
    $script:Ctx.Summary = $null
  }
}

function Invoke-FixtureRun([string]$Dir) {
  $dirFull = Resolve-FullPath $Dir
  if (-not [System.IO.Directory]::Exists($dirFull)) { Write-Usage ('fixture directory not found: {0}' -f $Dir); return 2 }
  $cases = [string[]]@([System.IO.Directory]::GetDirectories($dirFull) | Where-Object { [System.IO.File]::Exists((Join-Path $_ 'case.json')) } | ForEach-Object { [System.IO.Path]::GetFileName($_) })
  if ($cases.Length -eq 0) { Write-Usage ('no case.json fixtures in {0}' -f $Dir); return 2 }
  [System.Array]::Sort($cases, [System.StringComparer]::Ordinal)
  $script:Ctx.Git = 'Invoke-MockGit'
  $passed = 0
  $failed = 0
  foreach ($name in $cases) {
    $display = Join-Path $Dir $name
    $problems = New-Object System.Collections.Generic.List[string]
    $tmp = Join-Path ([System.IO.Path]::GetTempPath()) ('changelog-fixture-' + [guid]::NewGuid().ToString('N') + '.md')
    try {
      $case = [System.IO.File]::ReadAllText((Join-Path (Join-Path $dirFull $name) 'case.json'), $Utf8) | ConvertFrom-Json -AsHashtable
      $bytes = [System.IO.File]::ReadAllBytes((Join-Path (Join-Path $dirFull $name) 'CHANGELOG.md'))
      $text = ''
      try {
        # Line endings are applied in memory so the result does not depend on the checkout's eol setting.
        $text = $Utf8Strict.GetString($bytes).Replace("`r`n", "`n")
        if ($case['eol'] -ceq 'crlf') { $text = $text.Replace("`n", "`r`n") }
        $bytes = $Utf8.GetBytes($text)
      }
      catch { $text = '' }
      $script:Ctx.Facts = $case['git']
      $script:Ctx.ShowText = $text.Replace("`r`n", "`n")
      $sha = $case['git']['sha']
      $expect = $case['expect']
      $r = Invoke-Captured 'notes' $case['mode'] $sha $bytes $tmp
      if ($r.Code -ne [int]$expect['exit']) { $problems.Add(('exit code {0}, expected {1}' -f $r.Code, $expect['exit'])) }
      if ($expect['match'] -ceq 'error') {
        if ($r.Code -eq 0 -or @($r.Out | Where-Object { $_.StartsWith('::error::', [System.StringComparison]::Ordinal) }).Count -eq 0) { $problems.Add('expected a non-zero exit and an ::error:: line') }
      }
      else {
        $wantOut = @($expect['output']) -join "`n"
        if ((@($r.Out) -join "`n") -cne $wantOut) { $problems.Add(("output differs`n  expected: {0}`n  actual:   {1}" -f $wantOut, (@($r.Out) -join "`n"))) }
        $gotSum = (@($r.Summary.Split([char]10)) | Where-Object { $_ -ne '' }) -join "`n"
        $wantSum = @($expect['summary']) -join "`n"
        if ($gotSum -cne $wantSum) { $problems.Add(("summary differs`n  expected: {0}`n  actual:   {1}" -f $wantSum, $gotSum)) }
      }
      $notesFile = Join-Path (Join-Path $dirFull $name) 'expected-notes.md'
      if ([int]$expect['exit'] -eq 0) {
        $want = $Utf8Strict.GetString([System.IO.File]::ReadAllBytes($notesFile)).Replace("`r`n", "`n")
        if (-not [System.IO.File]::Exists($tmp)) { $problems.Add('no notes file was written') }
        elseif ($Utf8Strict.GetString([System.IO.File]::ReadAllBytes($tmp)) -cne $want) { $problems.Add('notes body differs from expected-notes.md') }
      }
      elseif ([System.IO.File]::Exists($tmp)) { $problems.Add('a notes file was written although the run failed') }
      if ([System.IO.File]::Exists($tmp)) { [System.IO.File]::Delete($tmp) }
      $c = Invoke-Captured 'check' $case['mode'] $sha $bytes $tmp
      if ($c.Code -ne $r.Code -or (@($c.Out) -join "`n") -cne (@($r.Out) -join "`n")) { $problems.Add('check differs from notes in exit code or output') }
      if ([System.IO.File]::Exists($tmp)) { $problems.Add('check wrote a notes file') }
    }
    catch {
      $problems.Add('fixture error: ' + $_.Exception.Message)
    }
    finally {
      if ([System.IO.File]::Exists($tmp)) { [System.IO.File]::Delete($tmp) }
    }
    if ($problems.Count -eq 0) { Write-Line ('PASS {0}' -f $display); $passed++ }
    else { Write-Line ('FAIL {0}: {1}' -f $display, ($problems -join '; ')); $failed++ }
  }
  Write-Line ('fixtures: {0} passed, {1} failed of {2}' -f $passed, $failed, $cases.Length)
  if ($failed -gt 0) { return 1 }
  return 0
}

if ($Command -ceq 'fixtures') {
  if ([string]::IsNullOrEmpty($FixtureDir)) { Write-Usage '-FixtureDir is required for fixtures'; exit 2 }
  exit (Invoke-FixtureRun $FixtureDir)
}
if ($Command -cne 'notes' -and $Command -cne 'check') { Write-Usage 'the first argument must be notes, check or fixtures'; exit 2 }
if ($Mode -cne 'tag' -and $Mode -cne 'dry-run') { Write-Usage '-Mode must be tag or dry-run'; exit 2 }
if ([string]::IsNullOrEmpty($Sha)) { Write-Usage '-Sha is required (GITHUB_SHA is not set)'; exit 2 }
if ($Command -ceq 'notes' -and [string]::IsNullOrEmpty($Output)) { Write-Usage '-Output is required for notes'; exit 2 }

try {
  $bytes = [System.IO.File]::ReadAllBytes((Resolve-FullPath $ChangelogPath))
}
catch {
  Write-Line ('::error::cannot read {0}: {1}' -f $ChangelogPath, $_.Exception.Message)
  exit 1
}
$outFull = ''
if ($Command -ceq 'notes') { $outFull = Resolve-FullPath $Output }
exit (Invoke-Run $Command $Mode $Sha $bytes $outFull $ChangelogPath)
