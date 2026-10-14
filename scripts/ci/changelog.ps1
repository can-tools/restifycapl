<#
.SYNOPSIS
Reads CHANGELOG.md for the release workflow: selects and validates the release notes, cuts a release section, computes the next version.

.DESCRIPTION
Subcommands (first argument):
  notes     -Mode tag|dry-run -Sha <commit> -Output <file> [-ChangelogPath <file>]
            select the section, validate it, write the notes body (LF, UTF-8, no BOM)
            and append the "taken from" line to $GITHUB_STEP_SUMMARY
  check     -Mode tag|dry-run -Sha <commit> [-ChangelogPath <file>]
            the same selection and validation, but writes no notes file and no
            "taken from" line
  cut       [-Base <40-hex>] [-Date <YYYY-MM-DD>] [-RepoUrl <url>] [-ChangelogPath <file>] [-Output <file>]
            turn "## [Unreleased]" into "## [<first 12 of Base>] - <Date>" under a fresh
            empty "## [Unreleased]", and add the heading's link definition
            ("[<12>]: <RepoUrl>/commit/<Base>") at the end of the file
  next-version  [-Label bump:major|bump:minor|bump:patch] [-ChangelogPath <file>]
            print the next version as X.Y.Z (no "v" prefix; the tag is "v" plus this)
  fixtures  -FixtureDir <dir>
            run every <dir>/*/case.json against a simulated git history
Exit codes: 0 success; 1 a validation failed, git failed or CHANGELOG.md could not
be read (a file that is not valid UTF-8 included); 2 usage error (missing or invalid
-Mode, -Sha, -Output, -Base, -Date, -RepoUrl, -Label, -FixtureDir, or an unknown subcommand);
a parameter PowerShell itself cannot bind (one given twice, for instance) fails before the script runs
with PowerShell's own error and exit code 1.
Diagnostics are GitHub workflow commands (::error::, ::warning::, ::notice::) on stdout.
notes and check require $env:GITHUB_STEP_SUMMARY: the step summary is appended to the file
it names, and the run fails with exit 1 at the first summary write if it is unset (notes
always writes a summary line, check only for a warning or notice). To run locally, set it
first, e.g. $env:GITHUB_STEP_SUMMARY = 'summary.md'. cut, next-version and fixtures do not
need it.

cut: moves what "## [Unreleased]" holds, "### Development" included (notes leaves it out
of the release notes), under the new heading and leaves a fresh empty "## [Unreleased]".
Relative inline links, [text](path), [text](<path>) and with a title, become
[text](<RepoUrl>/blob/<Base>/path); spaces between "(" and the target are kept. Links with a
scheme, "//" links and "#" anchors stay, and ".." fails. Code spans (within one line) and fenced
code blocks are not touched. Code blocks indented four spaces or more, fenced blocks indented
that deeply (in a nested list item, for instance) included, are not recognised, so links in
them are rewritten like any other: use a fenced block indented at most three spaces.
A "][" after non-empty bracketed text outside backticks, as in buf[2][3], is read as a
reference-style link and fails the cut, and so does a link definition inside the moved text; put
such text in backticks. Empty brackets, as in char name[][], are not.
notes ends a section at a line that looks like a link definition ([x]: y) or like a "## [" heading,
and ends the "### Development" block at a "### " line and starts it at a "### Development" line,
even inside a fenced code block. cut therefore refuses, inside a fenced block, any line that
would end the section, a "### " line inside "### Development" and a "### Development" line
outside it; other "### " lines in fenced blocks are left as they are.
RepoUrl is -RepoUrl, else taken from the first existing link definition
[<12 hex>]: https://github.com/<owner>/<repo>/commit/<40 hex>; with neither the cut fails. The
new definition goes above the first existing one at the end of the file (newest first), or after
a blank line at the end when there is none. A section with nothing to release besides
"### Development" (an already cut file included) and a heading for the same 12 characters already
in the file fail with exit 1. The same file and the same -Base, -Date and -RepoUrl give
byte-identical output; line endings (per line) and the final newline are kept. The result
replaces -ChangelogPath unless -Output names another file; nothing is written on failure.
next-version: the previous version is the highest tag vX.Y.Z (no leading zeros, each field at
most 65535) in numeric order, so v0.10.0 is above v0.9.0; with none the run fails. Other tags are
ignored; a version-shaped tag that is not valid (leading zero, field above 65535) is ignored with a
warning on stderr. The level comes from "## [Unreleased]" without "### Development", ignoring fenced
code: "### Added", "### Changed" or "### Deprecated" with entries gives minor, anything else (content
outside any heading included) patch; "### Removed" with entries is a breaking change and fails
unless -Label decides. A major bump is never computed. -Label overrides the level, never the version;
give at most one. The same rules apply from 1.0.0 on. A section with nothing to release fails with
exit 1 as for cut, label or not, and so does a resulting field above 65535. stdout carries only
the version.

A fixture case may declare "match": "error" instead of exact output; the run must then
exit non-zero and print at least one ::error:: line (used where only the message text
is a known difference from the former inline parser).
A fixture case with "command": "cut" or "command": "next-version" runs that subcommand
instead of notes and check. "params" holds base, date, repoUrl (cut) or label (next-version);
"git" may carry "tip" (the answer for the default -Base) and "allTags" (every tag name
next-version sees). "expect" holds "exit", "output" (the exact stdout lines, or "match": "error"),
optionally "stderr" (the exact stderr lines; not checked when absent) and, for a successful cut,
the file expected-CHANGELOG.md. Each cut runs twice and the two results must be byte-identical.
"eol": "marked" keeps the line endings written in the files, where the text <CR> stands for a
carriage return (git would normalise a file with mixed endings).

.PARAMETER Command
The subcommand: notes, check, cut, next-version or fixtures.

.PARAMETER Mode
tag (the run is for a pushed tag) or dry-run (a manual run that never publishes); required by notes and check.

.PARAMETER Sha
The commit being released or previewed; defaults to $env:GITHUB_SHA.

.PARAMETER Output
The file notes writes the release notes body to (required by notes), or the file cut writes its result to instead of rewriting -ChangelogPath.

.PARAMETER ChangelogPath
The changelog to read; defaults to CHANGELOG.md in the current directory. cut rewrites it unless -Output is given.

.PARAMETER Base
The full 40-character lowercase hex hash of the commit the cut is based on; defaults to git rev-parse origin/main.

.PARAMETER Date
The date in the cut heading, YYYY-MM-DD; defaults to today's date in UTC.

.PARAMETER RepoUrl
The repository URL, https://github.com/<owner>/<repo>, used for pinned links and the link definition; defaults to the one in the file's existing link definitions.

.PARAMETER Label
One of bump:major, bump:minor, bump:patch; overrides the computed bump level.

.PARAMETER FixtureDir
The directory holding the fixture cases; required by fixtures.

.EXAMPLE
pwsh -NoProfile -File scripts/ci/changelog.ps1 notes -Mode dry-run -Sha (git rev-parse HEAD) -Output notes-body.md

.EXAMPLE
pwsh -NoProfile -File scripts/ci/changelog.ps1 check -Mode tag -Sha $env:GITHUB_SHA

.EXAMPLE
pwsh -NoProfile -File scripts/ci/changelog.ps1 cut -Output CHANGELOG.cut.md

.EXAMPLE
pwsh -NoProfile -File scripts/ci/changelog.ps1 next-version -Label bump:patch

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
  [string]$Base,
  [string]$Date,
  [string]$RepoUrl,
  [string]$Label,
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
$ReUnreleased = [regex]'^## \[Unreleased\]\r?$'
$ReLinkDef = [regex]'^\[[^\]]+\]:[ \t]*\S'
$ReRepoDef = [regex]'^\[[0-9a-f]{12}\]:[ \t]*(https://github\.com/[^/\s]+/[^/\s]+)/commit/[0-9a-f]{40}\r?$'
$ReRepoUrl = [regex]'^https://github\.com/[^/\s]+/[^/\s]+$'
$ReBase = [regex]'^[0-9a-f]{40}$'
$ReDate = [regex]'^[0-9]{4}-[0-9]{2}-[0-9]{2}$'
$ReScheme = [regex]'^[A-Za-z][A-Za-z0-9+.-]*:'
$ReFence = [regex]'^ {0,3}(`{3,}(?=[^`]*$)|~{3,})'
$ReFenceClose = [regex]'^ {0,3}(`+|~+)[ \t]*\r?$'
$ReCodeSpan = [regex]'(?<!`)(`+)(?!`)(.+?)(?<!`)\1(?!`)'
$ReRefLink = [regex]'\[[^\[\]]+\]\['
$ReIndentedDef = [regex]'^ {1,3}\[[^\]]+\]:[ \t]*\S'
$ReH3 = [regex]'^### '
$ReDev = [regex]'^### Development\r?$'
$ReBlank = [regex]'^[ \t]*\r?$'
# Version-shaped tags; the release.yml rule (no leading zeros, at most 65535) is applied per field so a skipped tag can be reported.
$ReTagShape = [regex]'^v([0-9]+)\.([0-9]+)\.([0-9]+)$'
$MaxField = 65535
$RefMessage = 'reference-style links and link definitions are not supported inside the section being cut; use inline links'

$script:Ctx = @{ Git = 'Invoke-RealGit'; Out = $null; Err = $null; Summary = $null; Facts = $null; ShowText = $null }

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
  if ($a.Count -eq 2 -and $a[0] -ceq 'tag' -and $a[1] -ceq '--list') {
    $t = @(); if ($f.ContainsKey('allTags')) { $t = @($f['allTags']) }
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

# A section ends at the next "## [" heading or at a link definition.
function Get-SectionEnd($lines, [int]$start) {
  for ($i = $start + 1; $i -lt $lines.Count; $i++) {
    if ($ReHead.IsMatch($lines[$i]) -or $ReLinkDef.IsMatch($lines[$i])) { return $i }
  }
  return $lines.Count
}

# Returns the section's notes lines, or $null when nothing is left once "### Development" is removed.
function Get-SectionBody($lines, [int]$start) {
  $end = Get-SectionEnd $lines $start
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

# Finds the only "## [Unreleased]" section and its release content (Development removed); fails when there is none.
function Get-Unreleased($lines) {
  $starts = @(for ($i = 0; $i -lt $lines.Count; $i++) { if ($ReUnreleased.IsMatch($lines[$i])) { $i } })
  if ($starts.Count -ne 1) { Stop-WithError ('expected exactly one "## [Unreleased]" heading, found {0}' -f $starts.Count) }
  $body = Get-SectionBody $lines $starts[0]
  if ($null -eq $body) { Stop-WithError '"## [Unreleased]" has nothing to release once "### Development" is removed (an already cut file looks the same)' }
  return [pscustomobject]@{ Start = [int]$starts[0]; Body = $body }
}

function Format-LinkTarget([string]$target, [string]$prefix) {
  if ($target -ceq '' -or $target.StartsWith('#', [System.StringComparison]::Ordinal) -or $target.StartsWith('//', [System.StringComparison]::Ordinal) -or $ReScheme.IsMatch($target)) { return $target }
  $path = $target -creplace '^(\./|/)+', ''
  if ($path -ceq '..' -or $path.StartsWith('../', [System.StringComparison]::Ordinal)) { Stop-WithError ('the link {0} leaves the repository and cannot be pinned' -f $target) }
  return $prefix + $path
}

function Format-LinkTargetAny([string]$target, [string]$prefix) {
  if ($target.Length -ge 2 -and $target.StartsWith('<', [System.StringComparison]::Ordinal) -and $target.EndsWith('>', [System.StringComparison]::Ordinal)) {
    return '<' + (Format-LinkTarget $target.Substring(1, $target.Length - 2) $prefix) + '>'
  }
  return Format-LinkTarget $target $prefix
}

# Returns the fence marker while a fenced code block is open after this line, otherwise ''.
function Get-Fence([string]$fence, [string]$line) {
  if ($fence -ceq '') {
    $m = $ReFence.Match($line)
    if ($m.Success) { return $m.Groups[1].Value }
    return ''
  }
  $m = $ReFenceClose.Match($line)
  if ($m.Success -and $m.Groups[1].Value.Length -ge $fence.Length -and $m.Groups[1].Value.Trim($fence[0]) -ceq '') { return '' }
  return $fence
}

# "][" after non-empty bracketed text outside a code span is a reference-style link, which an inline-link rewrite cannot pin.
function Convert-Text([string]$text, [string]$prefix) {
  if ($ReRefLink.IsMatch($text)) { Stop-WithError $RefMessage }
  return ($text -creplace '(?<=\]\([ \t]*)(<[^>]*>|[^)\s]*)', { Format-LinkTargetAny $_.Value $prefix })
}

function Convert-Line([string]$line, [string]$prefix) {
  $sb = New-Object System.Text.StringBuilder
  $pos = 0
  foreach ($m in $ReCodeSpan.Matches($line)) {
    [void]$sb.Append((Convert-Text $line.Substring($pos, $m.Index - $pos) $prefix)).Append($m.Value)
    $pos = $m.Index + $m.Length
  }
  [void]$sb.Append((Convert-Text $line.Substring($pos) $prefix))
  return $sb.ToString()
}

# notes splits sections at heading- and definition-like lines even in fenced code, so a cut must refuse such lines there.
function Convert-SectionLines($lines, [int]$from, [int]$to, [string]$prefix) {
  $out = New-Object System.Collections.Generic.List[string]
  $fence = ''
  $inDevelopment = $false
  for ($i = $from; $i -lt $to; $i++) {
    $line = $lines[$i]
    $was = $fence
    $fence = Get-Fence $fence $line
    if ($was -cne '' -or $fence -cne '') {
      if ($was -cne '' -and $inDevelopment -and $ReH3.IsMatch($line)) { Stop-WithError 'a "### " line inside a fenced code block would end the "### Development" block for the release notes; remove or reword it' }
      if ($was -cne '' -and -not $inDevelopment -and $ReDev.IsMatch($line)) { Stop-WithError 'a "### Development" line inside a fenced code block would start the "### Development" block for the release notes; remove or reword it' }
      $out.Add($line); continue
    }
    if ($ReH3.IsMatch($line)) { $inDevelopment = $ReDev.IsMatch($line) }
    if ($ReIndentedDef.IsMatch($line)) { Stop-WithError $RefMessage }
    $out.Add((Convert-Line $line $prefix))
  }
  if ($fence -cne '' -and $to -lt $lines.Count) {
    if ($ReLinkDef.IsMatch($lines[$to])) { Stop-WithError 'a line inside a fenced code block looks like a link definition and would end the section for the release notes; remove or reword it' }
    Stop-WithError 'a line inside a fenced code block looks like a "## [" heading and would end the section for the release notes; remove or reword it'
  }
  return , $out
}

function Get-CutText([string]$Text, [string]$Base, [string]$Date, [string]$RepoUrl) {
  $lines = [System.Collections.Generic.List[string]]::new([string[]]$Text.Split([char]10))
  $unreleased = Get-Unreleased $lines
  $start = $unreleased.Start
  $short = Get-Prefix $Base 12
  $heading = '## [{0}] - {1}' -f $short, $Date
  foreach ($line in $lines) {
    if ($line.StartsWith('## [' + $short + ']', [System.StringComparison]::Ordinal)) { Stop-WithError ('the file already has a section for {0}' -f $short) }
  }
  if ([string]::IsNullOrEmpty($RepoUrl)) {
    foreach ($line in $lines) {
      $m = $ReRepoDef.Match($line)
      if ($m.Success) { $RepoUrl = $m.Groups[1].Value; break }
    }
  }
  if ([string]::IsNullOrEmpty($RepoUrl)) { Stop-WithError 'no repository URL: the file has no link definition to take it from; pass -RepoUrl' }
  $prefix = '{0}/blob/{1}/' -f $RepoUrl, $Base
  $end = Get-SectionEnd $lines $start
  # New lines take the line ending of the heading they sit next to.
  $cr = ''; if ($lines[$start].EndsWith("`r", [System.StringComparison]::Ordinal)) { $cr = "`r" }

  $out = New-Object System.Collections.Generic.List[string]
  $out.AddRange($lines.GetRange(0, $start + 1))
  $out.Add($cr)
  $out.Add($heading + $cr)
  $out.AddRange((Convert-SectionLines $lines ($start + 1) $end $prefix))
  $tailStart = $out.Count
  $out.AddRange($lines.GetRange($end, $lines.Count - $end))

  $definition = '[{0}]: {1}/commit/{2}' -f $short, $RepoUrl, $Base
  $at = -1
  for ($j = $tailStart; $j -lt $out.Count; $j++) {
    if ($ReLinkDef.IsMatch($out[$j])) { $at = $j; break }
  }
  if ($at -ge 0) {
    foreach ($line in $out.GetRange($at, $out.Count - $at)) {
      if (-not $ReLinkDef.IsMatch($line) -and -not $ReBlank.IsMatch($line)) { Stop-WithError $RefMessage }
    }
    $out.Insert($at, $definition + $cr)
    return $out -join "`n"
  }
  $endsNewline = $out[$out.Count - 1] -ceq ''
  if ($endsNewline) { $out.RemoveAt($out.Count - 1) }
  $last = $out[$out.Count - 1]
  if (-not $ReBlank.IsMatch($last)) { $out.Add($cr) }
  if ($endsNewline) { $out.Add($definition + $cr); $out.Add('') } else { $out.Add($definition) }
  return $out -join "`n"
}

function Get-Categories([string[]]$body) {
  $category = ''
  $fence = ''
  $minor = $false
  $removed = $false
  foreach ($line in $body) {
    $was = $fence
    $fence = Get-Fence $fence $line
    if ($was -ceq '' -and $fence -ceq '' -and $ReH3.IsMatch($line)) { $category = $line.Substring(4).Trim(); continue }
    if ($ReBlank.IsMatch($line)) { continue }
    if ($category -ceq 'Removed') { $removed = $true }
    elseif ($category -ceq 'Added' -or $category -ceq 'Changed' -or $category -ceq 'Deprecated') { $minor = $true }
  }
  return [pscustomobject]@{ Minor = $minor; Removed = $removed }
}

function Write-Stderr([string]$s) {
  if ($null -ne $script:Ctx.Err) { $script:Ctx.Err.Add($s) } else { [Console]::Error.WriteLine($s) }
}

# A tag shaped like a version that cannot be a release tag is skipped with a warning; other tags silently.
function Get-LatestVersion([string[]]$names) {
  $best = $null
  foreach ($name in $names) {
    $shape = $ReTagShape.Match($name)
    if (-not $shape.Success) { continue }
    $v = @(1, 2, 3 | ForEach-Object { $shape.Groups[$_].Value })
    if (@($v | Where-Object { $_.Length -gt 1 -and $_.StartsWith('0', [System.StringComparison]::Ordinal) }).Count -gt 0) {
      Write-Stderr ('ignored tag {0}: a field has a leading zero' -f $name); continue
    }
    if (@($v | Where-Object { $_.Length -gt 5 -or [int]$_ -gt $MaxField }).Count -gt 0) {
      Write-Stderr ('ignored tag {0}: a field is above {1}' -f $name, $MaxField); continue
    }
    $n = [int[]]@($v | ForEach-Object { [int]$_ })
    if ($null -eq $best -or $n[0] -gt $best[0] -or ($n[0] -eq $best[0] -and ($n[1] -gt $best[1] -or ($n[1] -eq $best[1] -and $n[2] -gt $best[2])))) { $best = $n }
  }
  return , $best
}

function Get-NextVersion([string]$Text, [string]$Label) {
  $lines = [System.Collections.Generic.List[string]]::new([string[]]$Text.Split([char]10))
  $unreleased = Get-Unreleased $lines
  $latest = Get-LatestVersion (Split-Words (Get-GitOutput @('tag', '--list')))
  if ($null -eq $latest) { Stop-WithError 'no tag of the form vX.Y.Z found; fetch the tags (a shallow checkout has none) or tag the first release by hand' }
  $categories = Get-Categories $unreleased.Body
  if (-not [string]::IsNullOrEmpty($Label)) { $level = $Label.Substring('bump:'.Length) }
  elseif ($categories.Removed) { Stop-WithError '"### Removed" is a breaking change; pass -Label bump:major, bump:minor or bump:patch to decide the level' }
  elseif ($categories.Minor) { $level = 'minor' }
  else { $level = 'patch' }
  $next = switch ($level) {
    'major' { @(($latest[0] + 1), 0, 0) }
    'minor' { @($latest[0], ($latest[1] + 1), 0) }
    default { @($latest[0], $latest[1], ($latest[2] + 1)) }
  }
  $version = $next -join '.'
  if (@($next | Where-Object { $_ -gt $MaxField }).Count -gt 0) {
    Stop-WithError ('the next version {0} has a field above {1} (version resource fields are 16-bit)' -f $version, $MaxField)
  }
  return $version
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

function Invoke-CutRun([byte[]]$Bytes, [string]$Display, [string]$Base, [string]$Date, [string]$RepoUrl, [string]$OutPath) {
  if (-not [string]::IsNullOrEmpty($Base) -and -not $ReBase.IsMatch($Base)) { Write-Usage '-Base must be a full 40-character lowercase hex commit hash'; return 2 }
  if (-not [string]::IsNullOrEmpty($RepoUrl) -and -not $ReRepoUrl.IsMatch($RepoUrl)) { Write-Usage '-RepoUrl must be https://github.com/<owner>/<repo>'; return 2 }
  if (-not [string]::IsNullOrEmpty($Date)) {
    $parsed = [datetime]::MinValue
    if (-not $ReDate.IsMatch($Date) -or -not [datetime]::TryParseExact($Date, 'yyyy-MM-dd', [cultureinfo]::InvariantCulture, [System.Globalization.DateTimeStyles]::None, [ref]$parsed)) {
      Write-Usage '-Date must be a real date as YYYY-MM-DD'
      return 2
    }
  }
  try {
    try { $Text = $Utf8Strict.GetString($Bytes) }
    catch { Stop-WithError ('cannot read {0}: {1}' -f $Display, $_.Exception.Message) }
    if ([string]::IsNullOrEmpty($Base)) {
      $Base = Get-GitOutput @('rev-parse', 'origin/main')
      if (-not $ReBase.IsMatch($Base)) { Stop-WithError ('git rev-parse origin/main did not print a full commit hash: {0}' -f $Base) }
    }
    if ([string]::IsNullOrEmpty($Date)) { $Date = [datetime]::UtcNow.ToString('yyyy-MM-dd', [cultureinfo]::InvariantCulture) }
    $cut = Get-CutText $Text $Base $Date $RepoUrl
    [System.IO.File]::WriteAllBytes($OutPath, $Utf8.GetBytes($cut))
    return 0
  }
  catch [ChangelogExit] { return $_.Exception.Code }
  catch {
    Write-Line ('::error::unexpected error: ' + $_.Exception.Message)
    return 1
  }
}

function Invoke-NextVersionRun([byte[]]$Bytes, [string]$Display, [string]$Label) {
  if (-not [string]::IsNullOrEmpty($Label) -and $Label -cnotin @('bump:major', 'bump:minor', 'bump:patch')) { Write-Usage '-Label must be bump:major, bump:minor or bump:patch'; return 2 }
  try {
    try { $Text = $Utf8Strict.GetString($Bytes) }
    catch { Stop-WithError ('cannot read {0}: {1}' -f $Display, $_.Exception.Message) }
    Write-Line (Get-NextVersion $Text $Label)
    return 0
  }
  catch [ChangelogExit] { return $_.Exception.Code }
  catch {
    Write-Line ('::error::unexpected error: ' + $_.Exception.Message)
    return 1
  }
}

function Write-Usage([string]$msg) {
  # Fixture runs would otherwise print every expected usage error to the log.
  if ($null -ne $script:Ctx.Out) { return }
  [Console]::Error.WriteLine($msg)
  [Console]::Error.WriteLine('usage: changelog.ps1 notes|check -Mode tag|dry-run -Sha <commit> [-Output <file>] [-ChangelogPath <file>] | cut [-Base <hash>] [-Date <YYYY-MM-DD>] [-RepoUrl <url>] [-Output <file>] [-ChangelogPath <file>] | next-version [-Label <bump:level>] [-ChangelogPath <file>] | fixtures -FixtureDir <dir>')
}

function Invoke-Capture([scriptblock]$Body) {
  $script:Ctx.Out = New-Object System.Collections.Generic.List[string]
  $script:Ctx.Err = New-Object System.Collections.Generic.List[string]
  $script:Ctx.Summary = New-Object System.Text.StringBuilder
  try {
    $code = & $Body
    return [pscustomobject]@{ Code = $code; Out = $script:Ctx.Out.ToArray(); Err = $script:Ctx.Err.ToArray(); Summary = $script:Ctx.Summary.ToString() }
  }
  finally {
    $script:Ctx.Out = $null
    $script:Ctx.Err = $null
    $script:Ctx.Summary = $null
  }
}

function Invoke-Captured([string]$Command, [string]$Mode, [string]$Sha, [byte[]]$Bytes, [string]$OutPath) {
  return (Invoke-Capture { Invoke-Run $Command $Mode $Sha $Bytes $OutPath 'CHANGELOG.md' })
}

function Convert-Eol([string]$text, [string]$eol) {
  $text = $text.Replace("`r`n", "`n")
  if ($eol -ceq 'crlf') { $text = $text.Replace("`n", "`r`n") }
  if ($eol -ceq 'marked') { $text = $text.Replace('<CR>', "`r") }
  return $text
}

function Get-CaseParam($case, [string]$key) {
  if ($case.ContainsKey('params') -and $case['params'].ContainsKey($key)) { return [string]$case['params'][$key] }
  return ''
}

function Test-Result($r, $expect, $problems) {
  if (-not $expect.ContainsKey('exit')) { $problems.Add('case.json has no expect.exit'); return }
  if ($r.Code -ne [int]$expect['exit']) { $problems.Add(('exit code {0}, expected {1}' -f $r.Code, $expect['exit'])) }
  if ($expect['match'] -ceq 'error') {
    if ($r.Code -eq 0 -or @($r.Out | Where-Object { $_.StartsWith('::error::', [System.StringComparison]::Ordinal) }).Count -eq 0) { $problems.Add('expected a non-zero exit and an ::error:: line') }
  }
  else {
    $wantOut = @($expect['output']) -join "`n"
    if ((@($r.Out) -join "`n") -cne $wantOut) { $problems.Add(("output differs`n  expected: {0}`n  actual:   {1}" -f $wantOut, (@($r.Out) -join "`n"))) }
  }
}

function Test-ToolResult($r, $expect, $problems) {
  Test-Result $r $expect $problems
  if ($r.Summary -ne '') { $problems.Add('a step summary was written') }
  if ($expect.ContainsKey('stderr') -and (@($r.Err) -join "`n") -cne (@($expect['stderr']) -join "`n")) {
    $problems.Add(("stderr differs`n  expected: {0}`n  actual:   {1}" -f (@($expect['stderr']) -join "`n"), (@($r.Err) -join "`n")))
  }
}

function Test-NotesCase($case, [string]$caseDir, [byte[]]$bytes, [string]$text, $problems, [string]$tmp) {
  $script:Ctx.ShowText = $text.Replace("`r`n", "`n")
  $sha = $case['git']['sha']
  $expect = $case['expect']
  $r = Invoke-Captured 'notes' $case['mode'] $sha $bytes $tmp
  Test-Result $r $expect $problems
  if ($expect['match'] -cne 'error') {
    $gotSum = (@($r.Summary.Split([char]10)) | Where-Object { $_ -ne '' }) -join "`n"
    $wantSum = @($expect['summary']) -join "`n"
    if ($gotSum -cne $wantSum) { $problems.Add(("summary differs`n  expected: {0}`n  actual:   {1}" -f $wantSum, $gotSum)) }
  }
  $notesFile = Join-Path $caseDir 'expected-notes.md'
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

function Test-CutCase($case, [string]$caseDir, [byte[]]$bytes, $problems, [string]$tmp) {
  $expect = $case['expect']
  $again = $tmp + '.again'
  try {
    $base = Get-CaseParam $case 'base'; $date = Get-CaseParam $case 'date'; $repo = Get-CaseParam $case 'repoUrl'
    $r = Invoke-Capture { Invoke-CutRun $bytes 'CHANGELOG.md' $base $date $repo $tmp }
    Test-ToolResult $r $expect $problems
    if (-not $expect.ContainsKey('exit')) { return }
    if ([int]$expect['exit'] -eq 0) {
      if (-not [System.IO.File]::Exists($tmp)) { $problems.Add('no result file was written'); return }
      $want = Convert-Eol $Utf8Strict.GetString([System.IO.File]::ReadAllBytes((Join-Path $caseDir 'expected-CHANGELOG.md'))) $case['eol']
      if ($Utf8Strict.GetString([System.IO.File]::ReadAllBytes($tmp)) -cne $want) { $problems.Add('result differs from expected-CHANGELOG.md') }
      [void](Invoke-Capture { Invoke-CutRun $bytes 'CHANGELOG.md' $base $date $repo $again })
      if (-not [System.IO.File]::Exists($again) -or [Convert]::ToBase64String([System.IO.File]::ReadAllBytes($again)) -cne [Convert]::ToBase64String([System.IO.File]::ReadAllBytes($tmp))) { $problems.Add('a second run gave a different result') }
    }
    elseif ([System.IO.File]::Exists($tmp)) { $problems.Add('a result file was written although the run failed') }
  }
  finally {
    if ([System.IO.File]::Exists($again)) { [System.IO.File]::Delete($again) }
  }
}

function Test-NextVersionCase($case, [byte[]]$bytes, $problems) {
  $label = Get-CaseParam $case 'label'
  $r = Invoke-Capture { Invoke-NextVersionRun $bytes 'CHANGELOG.md' $label }
  Test-ToolResult $r $case['expect'] $problems
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
      $caseDir = Join-Path $dirFull $name
      $case = [System.IO.File]::ReadAllText((Join-Path $caseDir 'case.json'), $Utf8) | ConvertFrom-Json -AsHashtable
      $bytes = [System.IO.File]::ReadAllBytes((Join-Path $caseDir 'CHANGELOG.md'))
      $text = ''
      try {
        # Line endings are applied in memory so the result does not depend on the checkout's eol setting.
        $text = Convert-Eol $Utf8Strict.GetString($bytes) $case['eol']
        $bytes = $Utf8.GetBytes($text)
      }
      catch { $text = '' }
      $script:Ctx.Facts = @{}
      if ($case.ContainsKey('git')) { $script:Ctx.Facts = $case['git'] }
      $command = 'notes'
      if ($case.ContainsKey('command')) { $command = $case['command'] }
      if ($command -ceq 'cut') { Test-CutCase $case $caseDir $bytes $problems $tmp }
      elseif ($command -ceq 'next-version') { Test-NextVersionCase $case $bytes $problems }
      else { Test-NotesCase $case $caseDir $bytes $text $problems $tmp }
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
if ($Command -cnotin @('notes', 'check', 'cut', 'next-version')) { Write-Usage 'the first argument must be notes, check, cut, next-version or fixtures'; exit 2 }
if ($Command -ceq 'notes' -or $Command -ceq 'check') {
  if ($Mode -cne 'tag' -and $Mode -cne 'dry-run') { Write-Usage '-Mode must be tag or dry-run'; exit 2 }
  if ([string]::IsNullOrEmpty($Sha)) { Write-Usage '-Sha is required (GITHUB_SHA is not set)'; exit 2 }
  if ($Command -ceq 'notes' -and [string]::IsNullOrEmpty($Output)) { Write-Usage '-Output is required for notes'; exit 2 }
}

try {
  $bytes = [System.IO.File]::ReadAllBytes((Resolve-FullPath $ChangelogPath))
}
catch {
  Write-Line ('::error::cannot read {0}: {1}' -f $ChangelogPath, $_.Exception.Message)
  exit 1
}
if ($Command -ceq 'cut') {
  $target = $ChangelogPath
  if (-not [string]::IsNullOrEmpty($Output)) { $target = $Output }
  exit (Invoke-CutRun $bytes $ChangelogPath $Base $Date $RepoUrl (Resolve-FullPath $target))
}
if ($Command -ceq 'next-version') { exit (Invoke-NextVersionRun $bytes $ChangelogPath $Label) }
$outFull = ''
if ($Command -ceq 'notes') { $outFull = Resolve-FullPath $Output }
exit (Invoke-Run $Command $Mode $Sha $bytes $outFull $ChangelogPath)
