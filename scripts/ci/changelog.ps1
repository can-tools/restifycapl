<#
.SYNOPSIS
Reads and generates CHANGELOG.md for the release workflow: selects and validates the release notes, computes the release level from the commits since the last tag, generates the release section, computes the next version.

.DESCRIPTION
Subcommands (first argument):
  notes     -Mode tag|dry-run -Sha <commit> -Output <file> [-ChangelogPath <file>]
            select the section, validate it, write the notes body (LF, UTF-8, no BOM)
            and append the "taken from" line to $GITHUB_STEP_SUMMARY
  check     -Mode tag|dry-run -Sha <commit> [-ChangelogPath <file>]
            the same selection and validation, but writes no notes file and no
            "taken from" line
  prs       [-Base <40-hex>]
            print the pull request numbers ("(#N)" at the end of a subject) of the commits
            since the last tag, one per line, whose commit messages the workflow must fetch
  level     [-Base <40-hex>] [-PrMessages <file>]
            print the release level of the commits since the last tag: major, minor, patch or none
  generate  [-Base <40-hex>] [-Date <YYYY-MM-DD>] [-RepoUrl <url>] [-PrMessages <file>] [-ChangelogPath <file>] [-Output <file>] [-SectionOutput <file>]
            add the release section for the commits since the last tag at the top of the file
            as "## [<first 12 of Base>] - <Date>", with the heading's link definition
            ("[<12>]: <RepoUrl>/commit/<Base>") above the existing ones at the end of the file
  next-version  -Level major|minor|patch|none | -Label bump:major|bump:minor|bump:patch
            print the next version as X.Y.Z (no "v" prefix; the tag is "v" plus this)
  fixtures  -FixtureDir <dir>
            run every <dir>/*/case.json against a simulated git history
Exit codes: 0 success; 1 a validation failed, git failed or CHANGELOG.md could not
be read (a file that is not valid UTF-8 included); 2 usage error (missing or invalid
-Mode, -Sha, -Output, -Base, -Date, -RepoUrl, -Level, -Label, -FixtureDir, or an unknown subcommand);
a parameter PowerShell itself cannot bind (one given twice, for instance) fails before the script runs
with PowerShell's own error and exit code 1.
Diagnostics are GitHub workflow commands (::error::, ::warning::, ::notice::) on stdout.
notes and check require $env:GITHUB_STEP_SUMMARY: the step summary is appended to the file
it names, and the run fails with exit 1 at the first summary write if it is unset (notes
always writes a summary line, check only for a warning or notice). To run locally, set it
first, e.g. $env:GITHUB_STEP_SUMMARY = 'summary.md'. prs, level, generate, next-version and
fixtures do not need it. The script never reaches the network: it runs git and reads the files
it is given.

Release content. The range is "git log --first-parent <last tag>..<Base>", the last tag being the
highest vX.Y.Z (no leading zeros, each field at most 65535) in numeric order, so v0.10.0 is above
v0.9.0; with none the run fails. Other tags are ignored; a version-shaped tag that is not valid is
ignored with a warning on stderr. Base defaults to git rev-parse origin/main. Every commit is a
squash-merge commit whose subject is the pull request title, which is the branch name, plus
"(#N)". The branch prefix gives the type: feat/ and stage/ minor, fix/ patch, chore/ and docs/ no
release, release/ (the release bot's own branch) ignored; any other subject has an unknown type,
counts as no release and is listed under Development with a warning. Commits up to and including
the one-time cut-off commit ($CutOffSha in this file) are listed under Development and raise no
level. -PrMessages names a JSON file the workflow builds from the pull requests' commits: an object
whose keys are pull request numbers and whose values are arrays of full commit messages. A message
is "type: description" or "type(scope): description"; "type!:" or a "BREAKING CHANGE:" footer marks
a breaking change, on any branch type. The marker is read only for a subject that has "(#N)" and a
number present in the file.
level: a breaking change gives minor while the last tag's major version is 0 and major from 1.0.0;
otherwise the highest of feat/stage (minor) and fix (patch); none without any.
generate: one entry per commit message of type feat, stage or fix (Added for feat and stage, Fixed
for fix) or with a breaking marker (Breaking changes, the footer text when there is one), each
ending in a link to the pull request. A feat/, stage/ or fix/ pull request with no such message
gets one entry made of its branch name and number, under Added or Fixed, and a warning. A
chore/ or docs/ pull request gets one Development entry; so does every commit up to the cut-off.
Sections appear in the order Breaking changes, Added, Fixed, Development (as "### " headings), the
empty ones left out. Nothing to release besides Development fails with exit 1, and so does a
heading for the same 12 characters already in the file. Warnings go to stdout: a releasing pull
request without a typed message, a subject without "(#N)", a subject of unknown type. The same
file, Base, Date, RepoUrl and messages give byte-identical output; line endings (per line) and the
final newline are kept. The result replaces -ChangelogPath unless -Output names another file;
-SectionOutput also receives the new section alone (LF, UTF-8, no BOM). Nothing is written on
failure. RepoUrl is -RepoUrl, else taken from the first existing link definition
[<12 hex>]: https://github.com/<owner>/<repo>/commit/<40 hex>; with neither the run fails.
next-version: the previous version is the highest tag as above. -Level or -Label (give exactly
one) decides the bump: major adds one to the first field, minor to the second, patch to the third.
-Level none and a resulting field above 65535 fail with exit 1. stdout carries only the version.
A major bump is never computed by level; the only way to it is the label bump:major.

A fixture case may declare "match": "error" instead of exact output; the run must then
exit non-zero and print at least one ::error:: line.
A fixture case with "command": "generate", "level", "prs" or "next-version" runs that subcommand
instead of notes and check. "params" holds base, date, repoUrl, prMessages (an object, written to
a file for the run) (generate), level or label (next-version); "git" may carry "tip" (the answer for the default
-Base), "allTags" (every tag name), "log" (objects with "sha" and "subject", newest first, the answer
for the commit range), "logRange" (the range the log is for; any other range fails), "cutOff" (replaces
the cut-off commit) and "cutOffChain" (the commits reachable from the cut-off along first parents).
"expect" holds "exit", "output" (the exact stdout lines, or "match": "error"), optionally "stderr"
(the exact stderr lines; not checked when absent) and, for a successful generate, the file
expected-CHANGELOG.md and optionally expected-section.md. Each generate runs twice and the two results
must be byte-identical. A case for level, prs or next-version needs no CHANGELOG.md.
"eol": "marked" keeps the line endings written in the files, where the text <CR> stands for a
carriage return (git would normalise a file with mixed endings).

.PARAMETER Command
The subcommand: notes, check, prs, level, generate, next-version or fixtures.

.PARAMETER Mode
tag (the run is for a pushed tag) or dry-run (a manual run that never publishes); required by notes and check.

.PARAMETER Sha
The commit being released or previewed; defaults to $env:GITHUB_SHA.

.PARAMETER Output
The file notes writes the release notes body to (required by notes), or the file generate writes its result to instead of rewriting -ChangelogPath.

.PARAMETER SectionOutput
The file generate also writes the new section alone to.

.PARAMETER ChangelogPath
The changelog to read; defaults to CHANGELOG.md in the current directory. generate rewrites it unless -Output is given.

.PARAMETER Base
The full 40-character lowercase hex hash of the commit the release is based on; defaults to git rev-parse origin/main.

.PARAMETER Date
The date in the section heading, YYYY-MM-DD; defaults to today's date in UTC.

.PARAMETER RepoUrl
The repository URL, https://github.com/<owner>/<repo>, used for pull request links and the link definition; defaults to the one in the file's existing link definitions.

.PARAMETER PrMessages
The JSON file of pull request commit messages described above; optional.

.PARAMETER Level
One of major, minor, patch, none; the bump level next-version applies.

.PARAMETER Label
One of bump:major, bump:minor, bump:patch; the bump level next-version applies.

.PARAMETER FixtureDir
The directory holding the fixture cases; required by fixtures.

.EXAMPLE
pwsh -NoProfile -File scripts/ci/changelog.ps1 notes -Mode dry-run -Sha (git rev-parse HEAD) -Output notes-body.md

.EXAMPLE
pwsh -NoProfile -File scripts/ci/changelog.ps1 check -Mode tag -Sha $env:GITHUB_SHA

.EXAMPLE
pwsh -NoProfile -File scripts/ci/changelog.ps1 level -PrMessages pr-messages.json

.EXAMPLE
pwsh -NoProfile -File scripts/ci/changelog.ps1 generate -PrMessages pr-messages.json -Output CHANGELOG.generated.md

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
  [string]$SectionOutput,
  [string]$ChangelogPath = 'CHANGELOG.md',
  [string]$Base,
  [string]$Date,
  [string]$RepoUrl,
  [string]$PrMessages,
  [string]$Level,
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

# Commits up to and including this one are Development entries and raise no level.
$CutOffSha = 'df84cf03894e55c41d2197e344f6e21e492490aa'

# Case-sensitive on purpose: PowerShell's -match is not.
$ReHead = [regex]'^## \['
$ReHashHead = [regex]'^## \[([0-9a-f]{12})\] - \d{4}-\d{2}-\d{2}\r?$'
$ReLinkDef = [regex]'^\[[^\]]+\]:[ \t]*\S'
$ReRepoDef = [regex]'^\[[0-9a-f]{12}\]:[ \t]*(https://github\.com/[^/\s]+/[^/\s]+)/commit/[0-9a-f]{40}\r?$'
$ReRepoUrl = [regex]'^https://github\.com/[^/\s]+/[^/\s]+$'
$ReBase = [regex]'^[0-9a-f]{40}$'
$ReDate = [regex]'^[0-9]{4}-[0-9]{2}-[0-9]{2}$'
$ReH3 = [regex]'^### '
$ReDev = [regex]'^### Development\r?$'
$ReBlank = [regex]'^[ \t]*\r?$'
$ReSquash = [regex]'^(.*\S) \(#([0-9]{1,9})\)$'
$ReBranchType = [regex]'^(feat|stage|fix|chore|docs|release)/'
$ReMessageHead = [regex]'^([a-z]+)(?:\([^()]*\))?(!)?: (\S.*)$'
$ReBreakingFooter = [regex]'^BREAKING[ -]CHANGE:[ \t]*(.*)$'
# Version-shaped tags; the release.yml rule (no leading zeros, at most 65535) is applied per field so a skipped tag can be reported.
$ReTagShape = [regex]'^v([0-9]+)\.([0-9]+)\.([0-9]+)$'
$MaxField = 65535
$Levels = @('major', 'minor', 'patch', 'none')

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
  if ($a.Count -eq 3 -and $a[0] -ceq 'rev-list' -and $a[1] -ceq '--first-parent' -and $a[2] -cne 'origin/main') {
    $c = @(); if ($f.ContainsKey('cutOffChain')) { $c = @($f['cutOffChain']) }
    return & $ok (($c -join "`n") + "`n")
  }
  if ($a.Count -eq 4 -and $a[0] -ceq 'log' -and $a[1] -ceq '--first-parent' -and $a[2] -ceq '--format=%H%x09%s') {
    if ($f.ContainsKey('logRange') -and $a[3] -cne $f['logRange']) { return & $no 128 }
    $rows = @(); if ($f.ContainsKey('log')) { $rows = @($f['log'] | ForEach-Object { $_['sha'] + "`t" + $_['subject'] }) }
    return & $ok (($rows -join "`n") + "`n")
  }
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
        Add-Annotation 'warning' ('The newest CHANGELOG section {0} was already released as {1}; this dry run previews its notes again.' -f $heading, $tags[0])
      }
      else {
        Add-Annotation 'notice' ('The cut {0} is merged on main as {1}; re-run the failed `tag-release.yml` run.' -f $heading, (Get-Prefix $child 12))
      }
      return
    }
  }
  $errors = New-Object System.Collections.Generic.List[string]
  if ($h -cne (Get-Prefix $tip 12)) {
    $t12 = Get-Prefix $tip 12
    $errors.Add(('the heading {0} names {1}, but the origin/main tip is {2}; main has moved: generate the section again from {2}' -f $heading, $h, $t12))
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

function Get-LastVersion {
  $latest = Get-LatestVersion (Split-Words (Get-GitOutput @('tag', '--list')))
  if ($null -eq $latest) { Stop-WithError 'no tag of the form vX.Y.Z found; fetch the tags (a shallow checkout has none) or tag the first release by hand' }
  return , $latest
}

function Get-NextVersion([string]$Level, [string]$Label) {
  $latest = Get-LastVersion
  if (-not [string]::IsNullOrEmpty($Label)) { $Level = $Label.Substring('bump:'.Length) }
  if ($Level -ceq 'none') { Stop-WithError 'nothing to release: the level is none' }
  $next = switch ($Level) {
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

function Get-CutOffSha {
  $f = $script:Ctx.Facts
  if ($null -ne $f -and $f.ContainsKey('cutOff')) { return [string]$f['cutOff'] }
  return $CutOffSha
}

function Resolve-Base([string]$Base) {
  if ([string]::IsNullOrEmpty($Base)) {
    $Base = Get-GitOutput @('rev-parse', 'origin/main')
    if (-not $ReBase.IsMatch($Base)) { Stop-WithError ('git rev-parse origin/main did not print a full commit hash: {0}' -f $Base) }
  }
  return $Base
}

# The pull request messages file: an object of pull request number -> array of full commit messages.
function Read-PrMessages([string]$path) {
  $messages = @{}
  if ([string]::IsNullOrEmpty($path)) { return $messages }
  try {
    $json = $Utf8Strict.GetString([System.IO.File]::ReadAllBytes((Resolve-FullPath $path))) | ConvertFrom-Json -AsHashtable
  }
  catch { Stop-WithError ('cannot read {0}: {1}' -f $path, $_.Exception.Message) }
  if ($json -isnot [hashtable]) { Stop-WithError ('{0} must hold a JSON object of pull request number to an array of commit messages' -f $path) }
  foreach ($key in $json.Keys) {
    $value = $json[$key]
    if ($value -is [string] -or $value -isnot [System.Collections.IEnumerable] -or @($value | Where-Object { $_ -isnot [string] }).Count -gt 0) {
      Stop-WithError ('{0}: the value of "{1}" must be an array of strings' -f $path, $key)
    }
    $messages[[string]$key] = [string[]]@($value)
  }
  return $messages
}

function Read-Subject([string]$subject) {
  $text = $subject.Trim()
  $number = 0
  $m = $ReSquash.Match($text)
  if ($m.Success) { $text = $m.Groups[1].Value; $number = [int]$m.Groups[2].Value }
  $type = 'unknown'
  $t = $ReBranchType.Match($text)
  if ($t.Success) { $type = $t.Groups[1].Value }
  return [pscustomobject]@{ Text = $text; Number = $number; Type = $type }
}

function Read-Message([string]$message) {
  $lines = $message.Replace("`r`n", "`n").Split([char]10)
  $first = $lines[0].Trim()
  $head = $ReMessageHead.Match($first)
  $type = ''
  $text = $first
  $bang = $false
  if ($head.Success) { $type = $head.Groups[1].Value; $bang = $head.Groups[2].Success; $text = $head.Groups[3].Value.Trim() }
  $footer = $null
  for ($i = 1; $i -lt $lines.Length; $i++) {
    $f = $ReBreakingFooter.Match($lines[$i])
    if ($f.Success) { $footer = $f.Groups[1].Value.Trim(); break }
  }
  if ($null -ne $footer -and $footer -cne '') { $text = $footer }
  return [pscustomobject]@{ Type = $type; Breaking = ($bang -or $null -ne $footer); Text = $text }
}

# The commits of git log --first-parent <last tag>..<Base>, oldest first, each with its subject and, when the subject names a pull request, that request's parsed messages.
function Get-Range([string]$Base, $Messages) {
  $latest = Get-LastVersion
  $tag = 'v{0}.{1}.{2}' -f $latest[0], $latest[1], $latest[2]
  $log = Get-GitOutput @('log', '--first-parent', '--format=%H%x09%s', ($tag + '..' + $Base))
  $before = New-Object 'System.Collections.Generic.HashSet[string]'
  foreach ($h in (Split-Words (Get-GitOutput @('rev-list', '--first-parent', (Get-CutOffSha))))) { [void]$before.Add($h) }
  $rows = @()
  if ($log -cne '') { $rows = @($log.Split([char]10)) }
  $commits = New-Object System.Collections.Generic.List[object]
  for ($i = $rows.Count - 1; $i -ge 0; $i--) {
    $row = $rows[$i].TrimEnd("`r")
    $tab = $row.IndexOf([char]9)
    if ($tab -lt 1) { Stop-WithError ('unexpected line in the git log output: {0}' -f $row) }
    $sha = $row.Substring(0, $tab)
    $subject = Read-Subject $row.Substring($tab + 1)
    $parsed = New-Object System.Collections.Generic.List[object]
    if ($subject.Number -gt 0 -and $subject.Type -cne 'release' -and $Messages.ContainsKey([string]$subject.Number)) {
      foreach ($raw in $Messages[[string]$subject.Number]) { $parsed.Add((Read-Message $raw)) }
    }
    $commits.Add([pscustomobject]@{ Sha = $sha; Pre = $before.Contains($sha); Subject = $subject; Messages = $parsed })
  }
  return [pscustomobject]@{ Latest = $latest; Tag = $tag; Commits = $commits }
}

function Get-Level($range) {
  $breaking = $false
  $minor = $false
  $patch = $false
  foreach ($c in $range.Commits) {
    if ($c.Pre -or $c.Subject.Type -ceq 'release') { continue }
    if (@($c.Messages | Where-Object { $_.Breaking }).Count -gt 0) { $breaking = $true }
    if ($c.Subject.Type -ceq 'feat' -or $c.Subject.Type -ceq 'stage') { $minor = $true }
    elseif ($c.Subject.Type -ceq 'fix') { $patch = $true }
  }
  if ($breaking -and $range.Latest[0] -ge 1) { return 'major' }
  if ($breaking -or $minor) { return 'minor' }
  if ($patch) { return 'patch' }
  return 'none'
}

function Format-Entry([string]$text, [int]$number, [string]$RepoUrl) {
  $line = '- ' + $text
  if ($number -gt 0) { $line += ' ([#{0}]({1}/pull/{0}))' -f $number, $RepoUrl }
  return $line
}

# Sorts the range into the section's entry lists; warnings are returned, not printed.
function Get-Entries($range, [string]$RepoUrl) {
  $e = [pscustomobject]@{
    Breaking = New-Object System.Collections.Generic.List[string]
    Added = New-Object System.Collections.Generic.List[string]
    Fixed = New-Object System.Collections.Generic.List[string]
    Development = New-Object System.Collections.Generic.List[string]
    Warnings = New-Object System.Collections.Generic.List[string]
  }
  foreach ($c in $range.Commits) {
    $s = $c.Subject
    if ($s.Type -ceq 'release') { continue }
    if ($c.Pre) { $e.Development.Add((Format-Entry $s.Text $s.Number $RepoUrl)); continue }
    if ($s.Number -le 0) {
      $e.Warnings.Add(('the subject "{0}" has no "(#N)"; its commit messages and breaking markers were not read' -f $s.Text))
    }
    $breakingMessages = @($c.Messages | Where-Object { $_.Breaking })
    foreach ($m in $breakingMessages) { $e.Breaking.Add((Format-Entry $m.Text $s.Number $RepoUrl)) }
    if ($s.Type -ceq 'feat' -or $s.Type -ceq 'stage' -or $s.Type -ceq 'fix') {
      $typed = @($c.Messages | Where-Object { -not $_.Breaking -and $_.Type -cin @('feat', 'stage', 'fix') })
      foreach ($m in $typed) {
        if ($m.Type -ceq 'fix') { $e.Fixed.Add((Format-Entry $m.Text $s.Number $RepoUrl)) }
        else { $e.Added.Add((Format-Entry $m.Text $s.Number $RepoUrl)) }
      }
      if ($typed.Count + $breakingMessages.Count -eq 0) {
        if ($s.Type -ceq 'fix') { $e.Fixed.Add((Format-Entry $s.Text $s.Number $RepoUrl)) }
        else { $e.Added.Add((Format-Entry $s.Text $s.Number $RepoUrl)) }
        $e.Warnings.Add(('"{0}" has no feat, stage, fix or breaking commit message; it is listed by its branch name' -f $s.Text))
      }
      continue
    }
    if ($s.Type -ceq 'unknown') {
      $e.Warnings.Add(('the subject "{0}" does not start with a feat/, stage/, fix/, chore/ or docs/ branch prefix; it is listed under Development' -f $s.Text))
    }
    if ($breakingMessages.Count -eq 0) { $e.Development.Add((Format-Entry $s.Text $s.Number $RepoUrl)) }
  }
  return $e
}

function Get-SectionLines($entries, [string]$heading) {
  $lines = New-Object System.Collections.Generic.List[string]
  $lines.Add($heading)
  foreach ($part in @(@('Breaking changes', $entries.Breaking), @('Added', $entries.Added), @('Fixed', $entries.Fixed), @('Development', $entries.Development))) {
    if ($part[1].Count -eq 0) { continue }
    $lines.Add('')
    $lines.Add('### ' + $part[0])
    $lines.Add('')
    foreach ($entry in $part[1]) { $lines.Add($entry) }
  }
  return , $lines
}

function Get-GeneratedText($lines, [string]$short, [string]$Base, [string]$RepoUrl, $section) {
  # New lines take the line ending of the line they sit next to.
  $first = -1
  $firstDef = -1
  for ($i = 0; $i -lt $lines.Count; $i++) {
    if ($first -lt 0 -and $ReHead.IsMatch($lines[$i])) { $first = $i }
    if ($firstDef -lt 0 -and $ReLinkDef.IsMatch($lines[$i])) { $firstDef = $i }
  }
  $at = $first
  if ($at -lt 0) { $at = $firstDef }
  if ($at -lt 0) { $at = $lines.Count; if ($lines.Count -gt 0 -and $lines[$lines.Count - 1] -ceq '') { $at = $lines.Count - 1 } }
  $reference = ''
  if ($at -lt $lines.Count -and $lines[$at] -cne '') { $reference = $lines[$at] }
  elseif ($at -gt 0) { $reference = $lines[$at - 1] }
  $cr = ''
  if ($reference.EndsWith("`r", [System.StringComparison]::Ordinal)) { $cr = "`r" }

  $out = New-Object System.Collections.Generic.List[string]
  $out.AddRange($lines.GetRange(0, $at))
  if ($at -gt 0 -and -not $ReBlank.IsMatch($lines[$at - 1])) { $out.Add($cr) }
  foreach ($line in $section) { $out.Add($line + $cr) }
  $out.Add($cr)
  $tailStart = $out.Count
  $out.AddRange($lines.GetRange($at, $lines.Count - $at))

  $definition = '[{0}]: {1}/commit/{2}' -f $short, $RepoUrl, $Base
  for ($j = $tailStart; $j -lt $out.Count; $j++) {
    if ($ReLinkDef.IsMatch($out[$j])) {
      $out.Insert($j, $definition + $cr)
      return $out -join "`n"
    }
  }
  $endsNewline = $out[$out.Count - 1] -ceq ''
  if ($endsNewline) { $out.RemoveAt($out.Count - 1) }
  if (-not $ReBlank.IsMatch($out[$out.Count - 1])) { $out.Add($cr) }
  if ($endsNewline) { $out.Add($definition + $cr); $out.Add('') } else { $out.Add($definition) }
  return $out -join "`n"
}

function Invoke-Guarded([scriptblock]$Body) {
  try {
    $null = & $Body
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
  [Console]::Error.WriteLine('usage: changelog.ps1 notes|check -Mode tag|dry-run -Sha <commit> [-Output <file>] [-ChangelogPath <file>] | prs [-Base <hash>] | level [-Base <hash>] [-PrMessages <file>] | generate [-Base <hash>] [-Date <YYYY-MM-DD>] [-RepoUrl <url>] [-PrMessages <file>] [-Output <file>] [-SectionOutput <file>] [-ChangelogPath <file>] | next-version -Level <level> | -Label <bump:level> | fixtures -FixtureDir <dir>')
}

function Invoke-Run([string]$Command, [string]$Mode, [string]$Sha, [byte[]]$Bytes, [string]$OutPath, [string]$Display) {
  return (Invoke-Guarded {
      try { $Text = $Utf8Strict.GetString($Bytes) }
      catch { Stop-WithError ('cannot read {0}: {1}' -f $Display, $_.Exception.Message) }
      $release = Resolve-Release $Mode $Sha $Text
      if ($Command -ceq 'notes') {
        [System.IO.File]::WriteAllText($OutPath, (($release.Body -join "`n") + "`n"), $Utf8)
        $line = 'Release notes taken from the CHANGELOG section `{0}` ({1} mode).' -f $release.Chosen, $Mode
        Add-Summary ($line + "`n")
      }
    })
}

function Test-BaseArgument([string]$Base) {
  if (-not [string]::IsNullOrEmpty($Base) -and -not $ReBase.IsMatch($Base)) { Write-Usage '-Base must be a full 40-character lowercase hex commit hash'; return $false }
  return $true
}

function Invoke-PrsRun([string]$Base) {
  if (-not (Test-BaseArgument $Base)) { return 2 }
  return (Invoke-Guarded {
      $range = Get-Range (Resolve-Base $Base) @{}
      $numbers = @($range.Commits | Where-Object { -not $_.Pre -and $_.Subject.Type -cne 'release' -and $_.Subject.Number -gt 0 } | ForEach-Object { [int]$_.Subject.Number } | Sort-Object -Unique)
      foreach ($n in $numbers) { Write-Line ([string]$n) }
    })
}

function Invoke-LevelRun([string]$Base, [string]$PrMessagesPath) {
  if (-not (Test-BaseArgument $Base)) { return 2 }
  return (Invoke-Guarded {
      $range = Get-Range (Resolve-Base $Base) (Read-PrMessages $PrMessagesPath)
      Write-Line (Get-Level $range)
    })
}

function Invoke-GenerateRun([byte[]]$Bytes, [string]$Display, [string]$Base, [string]$Date, [string]$RepoUrl, [string]$PrMessagesPath, [string]$OutPath, [string]$SectionPath) {
  if (-not (Test-BaseArgument $Base)) { return 2 }
  if (-not [string]::IsNullOrEmpty($RepoUrl) -and -not $ReRepoUrl.IsMatch($RepoUrl)) { Write-Usage '-RepoUrl must be https://github.com/<owner>/<repo>'; return 2 }
  if (-not [string]::IsNullOrEmpty($Date)) {
    $parsed = [datetime]::MinValue
    if (-not $ReDate.IsMatch($Date) -or -not [datetime]::TryParseExact($Date, 'yyyy-MM-dd', [cultureinfo]::InvariantCulture, [System.Globalization.DateTimeStyles]::None, [ref]$parsed)) {
      Write-Usage '-Date must be a real date as YYYY-MM-DD'
      return 2
    }
  }
  return (Invoke-Guarded {
      try { $Text = $Utf8Strict.GetString($Bytes) }
      catch { Stop-WithError ('cannot read {0}: {1}' -f $Display, $_.Exception.Message) }
      $lines = [System.Collections.Generic.List[string]]::new([string[]]$Text.Split([char]10))
      $resolvedBase = Resolve-Base $Base
      $resolvedDate = $Date
      if ([string]::IsNullOrEmpty($resolvedDate)) { $resolvedDate = [datetime]::UtcNow.ToString('yyyy-MM-dd', [cultureinfo]::InvariantCulture) }
      $short = Get-Prefix $resolvedBase 12
      foreach ($line in $lines) {
        if ($line.StartsWith('## [' + $short + ']', [System.StringComparison]::Ordinal)) { Stop-WithError ('the file already has a section for {0}' -f $short) }
      }
      $repo = $RepoUrl
      if ([string]::IsNullOrEmpty($repo)) {
        foreach ($line in $lines) {
          $m = $ReRepoDef.Match($line)
          if ($m.Success) { $repo = $m.Groups[1].Value; break }
        }
      }
      if ([string]::IsNullOrEmpty($repo)) { Stop-WithError 'no repository URL: the file has no link definition to take it from; pass -RepoUrl' }
      $range = Get-Range $resolvedBase (Read-PrMessages $PrMessagesPath)
      $entries = Get-Entries $range $repo
      foreach ($w in $entries.Warnings) { Write-Line ('::warning::' + $w) }
      if ($entries.Breaking.Count + $entries.Added.Count + $entries.Fixed.Count -eq 0) {
        Stop-WithError ('nothing to release since {0}: no feat/, stage/ or fix/ change and no breaking marker after the cut-off commit' -f $range.Tag)
      }
      $section = Get-SectionLines $entries ('## [{0}] - {1}' -f $short, $resolvedDate)
      $generated = Get-GeneratedText $lines $short $resolvedBase $repo $section
      [System.IO.File]::WriteAllBytes($OutPath, $Utf8.GetBytes($generated))
      if (-not [string]::IsNullOrEmpty($SectionPath)) { [System.IO.File]::WriteAllBytes($SectionPath, $Utf8.GetBytes((($section -join "`n") + "`n"))) }
    })
}

function Invoke-NextVersionRun([string]$Level, [string]$Label) {
  $hasLevel = -not [string]::IsNullOrEmpty($Level)
  $hasLabel = -not [string]::IsNullOrEmpty($Label)
  if ($hasLevel -eq $hasLabel) { Write-Usage 'give exactly one of -Level and -Label'; return 2 }
  if ($hasLevel -and $Level -cnotin $Levels) { Write-Usage '-Level must be major, minor, patch or none'; return 2 }
  if ($hasLabel -and $Label -cnotin @('bump:major', 'bump:minor', 'bump:patch')) { Write-Usage '-Label must be bump:major, bump:minor or bump:patch'; return 2 }
  return (Invoke-Guarded { Write-Line (Get-NextVersion $Level $Label) })
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

function Test-GenerateCase($case, [string]$caseDir, [byte[]]$bytes, $problems, [string]$tmp) {
  $expect = $case['expect']
  $again = $tmp + '.again'
  $section = $tmp + '.section'
  $sectionAgain = $tmp + '.section-again'
  $messages = $tmp + '.messages'
  try {
    $base = Get-CaseParam $case 'base'; $date = Get-CaseParam $case 'date'; $repo = Get-CaseParam $case 'repoUrl'
    $messagesArg = ''
    if ($case.ContainsKey('params') -and $case['params'].ContainsKey('prMessages')) {
      [System.IO.File]::WriteAllText($messages, (ConvertTo-Json $case['params']['prMessages'] -Depth 5), $Utf8)
      $messagesArg = $messages
    }
    $r = Invoke-Capture { Invoke-GenerateRun $bytes 'CHANGELOG.md' $base $date $repo $messagesArg $tmp $section }
    Test-ToolResult $r $expect $problems
    if (-not $expect.ContainsKey('exit')) { return }
    if ([int]$expect['exit'] -eq 0) {
      if (-not [System.IO.File]::Exists($tmp)) { $problems.Add('no result file was written'); return }
      $want = Convert-Eol $Utf8Strict.GetString([System.IO.File]::ReadAllBytes((Join-Path $caseDir 'expected-CHANGELOG.md'))) $case['eol']
      if ($Utf8Strict.GetString([System.IO.File]::ReadAllBytes($tmp)) -cne $want) { $problems.Add('result differs from expected-CHANGELOG.md') }
      $wantSection = Join-Path $caseDir 'expected-section.md'
      if ([System.IO.File]::Exists($wantSection)) {
        if ($Utf8Strict.GetString([System.IO.File]::ReadAllBytes($section)) -cne $Utf8Strict.GetString([System.IO.File]::ReadAllBytes($wantSection)).Replace("`r`n", "`n")) { $problems.Add('section differs from expected-section.md') }
      }
      [void](Invoke-Capture { Invoke-GenerateRun $bytes 'CHANGELOG.md' $base $date $repo $messagesArg $again $sectionAgain })
      if (-not [System.IO.File]::Exists($again) -or [Convert]::ToBase64String([System.IO.File]::ReadAllBytes($again)) -cne [Convert]::ToBase64String([System.IO.File]::ReadAllBytes($tmp))) { $problems.Add('a second run gave a different result') }
    }
    elseif ([System.IO.File]::Exists($tmp) -or [System.IO.File]::Exists($section)) { $problems.Add('a result file was written although the run failed') }
  }
  finally {
    foreach ($f in @($again, $section, $sectionAgain, $messages)) { if ([System.IO.File]::Exists($f)) { [System.IO.File]::Delete($f) } }
  }
}

function Test-ToolCase($case, [string]$command, $problems, [string]$tmp) {
  $messages = $tmp + '.messages'
  try {
    $messagesArg = ''
    if ($case.ContainsKey('params') -and $case['params'].ContainsKey('prMessages')) {
      [System.IO.File]::WriteAllText($messages, (ConvertTo-Json $case['params']['prMessages'] -Depth 5), $Utf8)
      $messagesArg = $messages
    }
    $base = Get-CaseParam $case 'base'
    $r = switch ($command) {
      'level' { Invoke-Capture { Invoke-LevelRun $base $messagesArg } }
      'prs' { Invoke-Capture { Invoke-PrsRun $base } }
      default { Invoke-Capture { Invoke-NextVersionRun (Get-CaseParam $case 'level') (Get-CaseParam $case 'label') } }
    }
    Test-ToolResult $r $case['expect'] $problems
  }
  finally {
    if ([System.IO.File]::Exists($messages)) { [System.IO.File]::Delete($messages) }
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
      $caseDir = Join-Path $dirFull $name
      $case = [System.IO.File]::ReadAllText((Join-Path $caseDir 'case.json'), $Utf8) | ConvertFrom-Json -AsHashtable
      $changelogFile = Join-Path $caseDir 'CHANGELOG.md'
      $bytes = [byte[]]@()
      if ([System.IO.File]::Exists($changelogFile)) { $bytes = [System.IO.File]::ReadAllBytes($changelogFile) }
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
      if ($command -ceq 'generate') { Test-GenerateCase $case $caseDir $bytes $problems $tmp }
      elseif ($command -cin @('level', 'prs', 'next-version')) { Test-ToolCase $case $command $problems $tmp }
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
if ($Command -cnotin @('notes', 'check', 'prs', 'level', 'generate', 'next-version')) { Write-Usage 'the first argument must be notes, check, prs, level, generate, next-version or fixtures'; exit 2 }
if ($Command -ceq 'notes' -or $Command -ceq 'check') {
  if ($Mode -cne 'tag' -and $Mode -cne 'dry-run') { Write-Usage '-Mode must be tag or dry-run'; exit 2 }
  if ([string]::IsNullOrEmpty($Sha)) { Write-Usage '-Sha is required (GITHUB_SHA is not set)'; exit 2 }
  if ($Command -ceq 'notes' -and [string]::IsNullOrEmpty($Output)) { Write-Usage '-Output is required for notes'; exit 2 }
}
if ($Command -ceq 'prs') { exit (Invoke-PrsRun $Base) }
if ($Command -ceq 'level') { exit (Invoke-LevelRun $Base $PrMessages) }
if ($Command -ceq 'next-version') { exit (Invoke-NextVersionRun $Level $Label) }

try {
  $bytes = [System.IO.File]::ReadAllBytes((Resolve-FullPath $ChangelogPath))
}
catch {
  Write-Line ('::error::cannot read {0}: {1}' -f $ChangelogPath, $_.Exception.Message)
  exit 1
}
if ($Command -ceq 'generate') {
  $target = $ChangelogPath
  if (-not [string]::IsNullOrEmpty($Output)) { $target = $Output }
  $sectionFull = ''
  if (-not [string]::IsNullOrEmpty($SectionOutput)) { $sectionFull = Resolve-FullPath $SectionOutput }
  exit (Invoke-GenerateRun $bytes $ChangelogPath $Base $Date $RepoUrl $PrMessages (Resolve-FullPath $target) $sectionFull)
}
$outFull = ''
if ($Command -ceq 'notes') { $outFull = Resolve-FullPath $Output }
exit (Invoke-Run $Command $Mode $Sha $bytes $outFull $ChangelogPath)
