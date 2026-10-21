<#
.SYNOPSIS
Reads and generates CHANGELOG.md for the release workflow: locates the release notes, computes the release level from the commits since the last tag, generates the release section, computes the next version.

.DESCRIPTION
Subcommands (first argument):
  notes     -Mode tag|dry-run -Sha <commit> -Output <file> [-ChangelogPath <file>]
            locate the section, write the notes body (LF, UTF-8, no BOM)
            and append the "taken from" line to $GITHUB_STEP_SUMMARY
  check     -Mode tag|dry-run -Sha <commit> [-ChangelogPath <file>]
            the same selection, but writes no notes file and no
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
  release-prs  [-Base <40-hex>]
            print the pull request numbers of the release commits ("release/next (#N)") among the
            commits since the last tag, newest first, one per line, whose data the workflow must fetch
  pending-cut  [-PrReleases <file>]
            print proceed (no release commit since the last tag), waiting (the newest release commit
            is faithful and is the origin/main tip) or abandoned (it is not); abandoned also means proceed
  pr-body   -Output <file> [-Base <40-hex>] [-RepoUrl <url>] [-PrMessages <file>] [-PrFiles <file>] [-Label bump:major|bump:minor|bump:patch] [-ChangelogPath <file>]
            write the release pull request description for the commits since the last tag
  body-version  -Body <file> -Label bump:major|bump:minor|bump:patch -Output <file>
            copy a release pull request description, replacing only the text between the version markers
  fixtures  -FixtureDir <dir>
            run every <dir>/*/case.json against a simulated git history
Exit codes: 0 success; 1 a check failed, git failed or CHANGELOG.md could not
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
[<12 hex>]: https://github.com/<owner>/<repo>/commit/<40 hex>; with neither the run fails. Before the section
is added, every hash-headed section (and its link definition) whose 12 characters are not a hash heading of
"git show <last tag>:CHANGELOG.md" is removed from the result: it belongs to a release commit that was never
tagged. Other headings are kept; a failing git show fails the run.
next-version: the previous version is the highest tag as above. -Level or -Label (give exactly
one) decides the bump: major adds one to the first field, minor to the second, patch to the third.
-Level none and a resulting field above 65535 fail with exit 1. stdout carries only the version.
A major bump is never computed by level; the only way to it is the label bump:major.
A release commit is a "release/next (#N)" commit after the last tag whose pull request data matches; the
definition is in docs/ci-pipeline.md, "Content-free release recognition". Nothing is read from CHANGELOG.md.
release-prs prints the numbers N of the commits whose subject has that form, valid or not.
pending-cut looks at the newest valid release commit only. With none it prints proceed. A faithful one that is
the origin/main tip prints waiting (re-run the failed tag-release.yml run if it failed). Any other one is
abandoned: it prints abandoned, with the reason on stderr, and the caller proceeds. -PrReleases names a JSON
file the workflow builds from "gh api pulls/N" and "gh api commits/<head.sha>": an object whose keys are pull
request numbers and whose values are {"found": false} or an object with found (true) and every key below (a missing key fails with exit 1), merged (bool),
mergeCommitSha (string or null), baseRef, baseRepo, headRef (strings), headRepo (string or null), headSha
(string), headParents (array of strings, or null when the head commit was not fetchable) and headTree (string
or null). A release-commit subject whose number has no key, when it is evaluated, fails with exit 1.
pr-body: the description opens with "Version preview: <!-- version -->vX.Y.Z (vA.B.C + level)<!-- /version -->",
the version being the next version for -Label, else for the level computed from the commits. A "> [!CAUTION]"
block lists the breaking changes when there are any. Then come the release notes preview (the generated
section without Development), the warnings (those of generate, and a fix/ pull request whose -PrFiles entry
lists src/module/exports.cpp), fixed sentences and a reviewer checklist, every item unchecked. Nothing to
release besides Development fails with exit 1 and writes nothing. Without -Label the version comes from the
commits, so a description built after a human changed the label shows the computed level until body-version
rewrites the marker line.
body-version: exactly one line of the body must hold both markers; the text between them becomes
"vX.Y.Z (vA.B.C + level)" for -Label and every other byte, ticked checklist items included, is kept.
-PrFiles names a JSON file the workflow builds from "gh api pulls/N/files": an object whose keys are pull
request numbers and whose values are arrays of changed files, each a path string or an object with a
"filename" string. A pull request absent from the file is treated as having changed nothing.

A fixture case may declare "match": "error" instead of exact output; the run must then
exit non-zero and print at least one ::error:: line.
A fixture case with "command": "generate", "level", "prs", "release-prs", "next-version", "pending-cut",
"pr-body" or "body-version" runs that subcommand instead of notes and check. "params" holds base, date,
repoUrl, prMessages, prFiles and prReleases (objects, each written to a file for the run) (generate,
pr-body, pending-cut), level or label (next-version, pr-body, body-version); a body-version case reads
body.md and expects expected-body.md, a pr-body case expects expected-body.md; "git" may carry "tip" (the
answer for the default -Base), "allTags" (every tag name), "log" (objects with "sha" and "subject", newest
first, the answer for the commit range), "logRange" (the range the log is for; any other range fails),
"commitInfo" (an object from a commit to {"parents": [...], "tree": "..."}), "showTexts" (an object from a
tag to the CHANGELOG.md text at that tag), "cutOff" (replaces the cut-off commit) and "cutOffChain" (the
commits reachable from the cut-off along first parents). A generate case reads the CHANGELOG.md at the last
tag from git.showTexts, else from tag-CHANGELOG.md in the case directory, else it is the case's own
CHANGELOG.md (no section dropped).
"expect" holds "exit", "output" (the exact stdout lines, or "match": "error"), optionally "stderr"
(the exact stderr lines; not checked when absent) and, for a successful generate, the file
expected-CHANGELOG.md and optionally expected-section.md. Each generate runs twice and the two results
must be byte-identical. A case for level, prs, release-prs, pending-cut or next-version needs no CHANGELOG.md.
"eol": "marked" keeps the line endings written in the files, where the text <CR> stands for a
carriage return (git would normalise a file with mixed endings).

.PARAMETER Command
The subcommand: notes, check, prs, release-prs, level, generate, next-version, pending-cut, pr-body, body-version or fixtures.

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

.PARAMETER PrFiles
The JSON file of pull request changed files described above; optional.

.PARAMETER PrReleases
The JSON file of pull request data for the release commits described above; optional.

.PARAMETER Body
The release pull request description body-version rewrites.

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
pwsh -NoProfile -File scripts/ci/changelog.ps1 pr-body -PrMessages pr-messages.json -PrFiles pr-files.json -Output pr-body.md

.EXAMPLE
pwsh -NoProfile -File scripts/ci/changelog.ps1 body-version -Body pr-body.md -Label bump:patch -Output pr-body.md

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
  [string]$PrFiles,
  [string]$PrReleases,
  [string]$Body,
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
$ReDefHash = [regex]'^\[([0-9a-f]{12})\]:'
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
$ReVersionMarks = [regex]'^(.*<!-- version -->).*?(<!-- /version -->.*)$'
$ReBreakingFooter = [regex]'^BREAKING[ -]CHANGE:[ \t]*(.*)$'
# Version-shaped tags; the release.yml rule (no leading zeros, at most 65535) is applied per field so a skipped tag can be reported.
$ReTagShape = [regex]'^v([0-9]+)\.([0-9]+)\.([0-9]+)$'
$MaxField = 65535

$ReReleaseSubject = [regex]'^release/next \(#([1-9][0-9]{0,8})\)$'
$Levels = @('major', 'minor', 'patch', 'none')

$script:Ctx = @{ Git = 'Invoke-RealGit'; Out = $null; Err = $null; Summary = $null; Facts = $null; TagChangelog = $null }

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

function Invoke-MockGit([string[]]$GitArgs) {
  $f = $script:Ctx.Facts
  $a = $GitArgs
  $ok = { param($o) [pscustomobject]@{ Code = 0; Out = $o; Err = '' } }
  $no = { param($c) [pscustomobject]@{ Code = $c; Out = ''; Err = 'unsupported or unknown in the simulated history' } }
  if ($f.ContainsKey('failures') -and $f['failures'].ContainsKey($a -join ' ')) {
    $fl = $f['failures'][$a -join ' ']
    return [pscustomobject]@{ Code = [int]$fl['code']; Out = ''; Err = [string]$fl['stderr'] }
  }
  if ($a.Count -eq 2 -and $a[0] -ceq 'rev-parse' -and $a[1] -ceq 'origin/main' -and $f.ContainsKey('tip')) { return & $ok ($f['tip'] + "`n") }
  if ($a.Count -eq 2 -and $a[0] -ceq 'rev-parse' -and $f.ContainsKey('parent') -and $a[1] -ceq ($f['sha'] + '^')) { return & $ok ($f['parent'] + "`n") }
  if ($a.Count -eq 3 -and $a[0] -ceq 'rev-list' -and $a[1] -ceq '--first-parent') {
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
    if (-not $f.ContainsKey('showTexts') -and $null -ne $script:Ctx.TagChangelog) { return & $ok $script:Ctx.TagChangelog }
    return & $no 128
  }
  if ($a.Count -eq 4 -and $a[0] -ceq 'log' -and $a[1] -ceq '-1' -and $a[2] -ceq '--format=%P%x09%T') {
    if ($f.ContainsKey('commitInfo') -and $f['commitInfo'].ContainsKey($a[3])) {
      $info = $f['commitInfo'][$a[3]]
      return & $ok ((@($info['parents']) -join ' ') + "`t" + $info['tree'] + "`n")
    }
    return & $no 128
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

function Get-SectionEnd($lines, [int]$start) {
  for ($i = $start + 1; $i -lt $lines.Count; $i++) {
    if ($ReHead.IsMatch($lines[$i]) -or $ReLinkDef.IsMatch($lines[$i])) { return $i }
  }
  return $lines.Count
}

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
  }
  return [pscustomobject]@{ Chosen = $chosen; Body = $body }
}

function Write-Stderr([string]$s) {
  if ($null -ne $script:Ctx.Err) { $script:Ctx.Err.Add($s) } else { [Console]::Error.WriteLine($s) }
}

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

function Get-NextVersion([string]$Level, [string]$Label, $Latest = $null) {
  $latest = $Latest
  if ($null -eq $latest) { $latest = Get-LastVersion }
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

function Read-PrFiles([string]$path) {
  $files = @{}
  if ([string]::IsNullOrEmpty($path)) { return $files }
  try {
    $json = $Utf8Strict.GetString([System.IO.File]::ReadAllBytes((Resolve-FullPath $path))) | ConvertFrom-Json -AsHashtable
  }
  catch { Stop-WithError ('cannot read {0}: {1}' -f $path, $_.Exception.Message) }
  if ($json -isnot [hashtable]) { Stop-WithError ('{0} must hold a JSON object of pull request number to an array of changed files' -f $path) }
  foreach ($key in $json.Keys) {
    $value = $json[$key]
    if ($value -is [string] -or $value -is [hashtable] -or $value -isnot [System.Collections.IEnumerable]) {
      Stop-WithError ('{0}: the value of "{1}" must be an array of changed files' -f $path, $key)
    }
    $names = New-Object System.Collections.Generic.List[string]
    foreach ($item in $value) {
      if ($item -is [string]) { $names.Add($item) }
      elseif ($item -is [hashtable] -and $item.ContainsKey('filename') -and $item['filename'] -is [string]) { $names.Add($item['filename']) }
      else { Stop-WithError ('{0}: each changed file of "{1}" must be a path string or an object with a "filename" string' -f $path, $key) }
    }
    $files[[string]$key] = [string[]]$names.ToArray()
  }
  return $files
}

function Read-PrReleases([string]$path) {
  $releases = @{}
  if ([string]::IsNullOrEmpty($path)) { return $releases }
  try {
    $json = $Utf8Strict.GetString([System.IO.File]::ReadAllBytes((Resolve-FullPath $path))) | ConvertFrom-Json -AsHashtable
  }
  catch { Stop-WithError ('cannot read {0}: {1}' -f $path, $_.Exception.Message) }
  if ($json -isnot [hashtable]) { Stop-WithError ('{0} must hold a JSON object of pull request number to pull request data' -f $path) }
  foreach ($key in $json.Keys) {
    $e = $json[$key]
    if ($e -isnot [hashtable] -or $e['found'] -isnot [bool]) { Stop-WithError ('{0}: the value of "{1}" must be an object with a boolean "found"' -f $path, $key) }
    if ($e['found']) {
      foreach ($name in 'mergeCommitSha', 'headRepo', 'headParents', 'headTree') {
        if (-not $e.ContainsKey($name)) { Stop-WithError ('{0}: "{1}" of "{2}" is missing' -f $path, $name, $key) }
      }
      foreach ($name in 'baseRef', 'baseRepo', 'headRef', 'headSha') {
        if ($e[$name] -isnot [string]) { Stop-WithError ('{0}: "{1}" of "{2}" must be a string' -f $path, $name, $key) }
      }
      foreach ($name in 'mergeCommitSha', 'headRepo', 'headTree') {
        if ($null -ne $e[$name] -and $e[$name] -isnot [string]) { Stop-WithError ('{0}: "{1}" of "{2}" must be a string or null' -f $path, $name, $key) }
      }
      if ($e['merged'] -isnot [bool]) { Stop-WithError ('{0}: "merged" of "{1}" must be a boolean' -f $path, $key) }
      $parents = $e['headParents']
      if ($null -ne $parents -and ($parents -is [string] -or $parents -isnot [System.Collections.IEnumerable] -or @($parents | Where-Object { $_ -isnot [string] }).Count -gt 0)) {
        Stop-WithError ('{0}: "headParents" of "{1}" must be an array of strings or null' -f $path, $key)
      }
    }
    $releases[[string]$key] = $e
  }
  return $releases
}

function Remove-UntaggedSections($lines) {
  $latest = Get-LastVersion
  $tag = 'v{0}.{1}.{2}' -f $latest[0], $latest[1], $latest[2]
  $shown = Invoke-Git @('show', ($tag + ':CHANGELOG.md'))
  if ($shown.Code -ne 0) { Stop-WithError ('git show {0}:CHANGELOG.md failed: {1}' -f $tag, $shown.Err.Trim()) }
  $released = New-Object 'System.Collections.Generic.HashSet[string]'
  foreach ($line in $shown.Out.Split([char]10)) {
    $m = $ReHashHead.Match($line)
    if ($m.Success) { [void]$released.Add($m.Groups[1].Value) }
  }
  $dropped = New-Object 'System.Collections.Generic.HashSet[string]'
  foreach ($line in $lines) {
    $m = $ReHashHead.Match($line)
    if ($m.Success -and -not $released.Contains($m.Groups[1].Value)) { [void]$dropped.Add($m.Groups[1].Value) }
  }
  if ($dropped.Count -eq 0) { return , $lines }
  $kept = New-Object System.Collections.Generic.List[string]
  $i = 0
  while ($i -lt $lines.Count) {
    $m = $ReHashHead.Match($lines[$i])
    if ($m.Success -and $dropped.Contains($m.Groups[1].Value)) { $i = Get-SectionEnd $lines $i; continue }
    $d = $ReDefHash.Match($lines[$i])
    if ($d.Success -and $dropped.Contains($d.Groups[1].Value)) { $i++; continue }
    $kept.Add($lines[$i])
    $i++
  }
  return , $kept
}

function Get-RepoUrl($lines, [string]$RepoUrl) {
  if (-not [string]::IsNullOrEmpty($RepoUrl)) { return $RepoUrl }
  foreach ($line in $lines) {
    $m = $ReRepoDef.Match($line)
    if ($m.Success) { return $m.Groups[1].Value }
  }
  Stop-WithError 'no repository URL: the file has no link definition to take it from; pass -RepoUrl'
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

function Get-RangeLog([string]$Base) {
  $latest = Get-LastVersion
  $tag = 'v{0}.{1}.{2}' -f $latest[0], $latest[1], $latest[2]
  $log = Get-GitOutput @('log', '--first-parent', '--format=%H%x09%s', ($tag + '..' + $Base))
  $rows = @()
  if ($log -cne '') { $rows = @($log.Split([char]10)) }
  $entries = New-Object System.Collections.Generic.List[object]
  foreach ($raw in $rows) {
    $row = $raw.TrimEnd("`r")
    $tab = $row.IndexOf([char]9)
    if ($tab -lt 1) { Stop-WithError ('unexpected line in the git log output: {0}' -f $row) }
    $entries.Add([pscustomobject]@{ Sha = $row.Substring(0, $tab); Subject = $row.Substring($tab + 1) })
  }
  return [pscustomobject]@{ Latest = $latest; Tag = $tag; Entries = $entries }
}

function Get-Range([string]$Base, $Messages) {
  $log = Get-RangeLog $Base
  $before = New-Object 'System.Collections.Generic.HashSet[string]'
  foreach ($h in (Split-Words (Get-GitOutput @('rev-list', '--first-parent', (Get-CutOffSha))))) { [void]$before.Add($h) }
  $commits = New-Object System.Collections.Generic.List[object]
  for ($i = $log.Entries.Count - 1; $i -ge 0; $i--) {
    $sha = $log.Entries[$i].Sha
    $subject = Read-Subject $log.Entries[$i].Subject
    $parsed = New-Object System.Collections.Generic.List[object]
    if ($subject.Number -gt 0 -and $subject.Type -cne 'release' -and $Messages.ContainsKey([string]$subject.Number)) {
      foreach ($raw in $Messages[[string]$subject.Number]) { $parsed.Add((Read-Message $raw)) }
    }
    $commits.Add([pscustomobject]@{ Sha = $sha; Pre = $before.Contains($sha); Subject = $subject; Messages = $parsed })
  }
  return [pscustomobject]@{ Latest = $log.Latest; Tag = $log.Tag; Commits = $commits }
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
  [Console]::Error.WriteLine('usage: changelog.ps1 notes|check -Mode tag|dry-run -Sha <commit> [-Output <file>] [-ChangelogPath <file>] | prs [-Base <hash>] | release-prs [-Base <hash>] | level [-Base <hash>] [-PrMessages <file>] | generate [-Base <hash>] [-Date <YYYY-MM-DD>] [-RepoUrl <url>] [-PrMessages <file>] [-Output <file>] [-SectionOutput <file>] [-ChangelogPath <file>] | next-version -Level <level> | -Label <bump:level> | pending-cut [-PrReleases <file>] | pr-body -Output <file> [-Base <hash>] [-RepoUrl <url>] [-PrMessages <file>] [-PrFiles <file>] [-Label <bump:level>] [-ChangelogPath <file>] | body-version -Body <file> -Label <bump:level> -Output <file> | fixtures -FixtureDir <dir>')
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
      $repo = Get-RepoUrl $lines $RepoUrl
      $lines = [System.Collections.Generic.List[string]]::new((Remove-UntaggedSections $lines))
      foreach ($line in $lines) {
        if ($line.StartsWith('## [' + $short + ']', [System.StringComparison]::Ordinal)) { Stop-WithError ('the file already has a section for {0}' -f $short) }
      }
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

function Get-ReleaseCandidates([string]$Base) {
  $log = Get-RangeLog $Base
  $candidates = New-Object System.Collections.Generic.List[object]
  foreach ($e in $log.Entries) {
    $m = $ReReleaseSubject.Match($e.Subject)
    if ($m.Success) { $candidates.Add([pscustomobject]@{ Sha = $e.Sha; Number = [string]$m.Groups[1].Value }) }
  }
  return , $candidates
}

function Invoke-ReleasePrsRun([string]$Base) {
  if (-not (Test-BaseArgument $Base)) { return 2 }
  return (Invoke-Guarded {
      $seen = New-Object 'System.Collections.Generic.HashSet[string]'
      foreach ($c in (Get-ReleaseCandidates (Resolve-Base $Base))) {
        if ($seen.Add($c.Number)) { Write-Line $c.Number }
      }
    })
}

function Test-ValidReleaseCommit($entry, [string]$sha) {
  if (-not $entry['found']) { return $false }
  if ($entry['merged'] -isnot [bool] -or -not $entry['merged']) { return $false }
  if ($entry['mergeCommitSha'] -cne $sha) { return $false }
  if ($entry['baseRef'] -cne 'main' -or $entry['headRef'] -cne 'release/next') { return $false }
  return ($null -ne $entry['headRepo'] -and $entry['headRepo'] -ceq $entry['baseRepo'])
}

function Test-FaithfulReleaseCommit($entry, [string]$sha) {
  $info = (Get-GitOutput @('log', '-1', '--format=%P%x09%T', $sha)).Split([char]9)
  if ($info.Count -ne 2) { return $false }
  $parents = @(Split-Words $info[0])
  if ($parents.Count -ne 1 -or $null -eq $entry['headParents'] -or $null -eq $entry['headTree']) { return $false }
  $headParents = @($entry['headParents'])
  return ($headParents.Count -eq 1 -and $headParents[0] -ceq $parents[0] -and $entry['headTree'] -ceq $info[1].Trim())
}

function Invoke-PendingCutRun([string]$PrReleasesPath) {
  return (Invoke-Guarded {
      $tip = Resolve-Base ''
      $candidates = Get-ReleaseCandidates $tip
      $releases = Read-PrReleases $PrReleasesPath
      foreach ($c in $candidates) {
        if (-not $releases.ContainsKey($c.Number)) { Stop-WithError ('no pull request data for #{0}, the release commit {1}' -f $c.Number, (Get-Prefix $c.Sha 12)) }
        $entry = $releases[$c.Number]
        if (-not (Test-ValidReleaseCommit $entry $c.Sha)) { continue }
        $short = Get-Prefix $c.Sha 12
        $faithful = Test-FaithfulReleaseCommit $entry $c.Sha
        if ($faithful -and $c.Sha -ceq $tip) { Write-Line 'waiting'; return }
        if (-not $faithful) { Write-Stderr ('release commit {0} (#{1}) abandoned: its parent or tree differs from the head of the pull request' -f $short, $c.Number) }
        else { Write-Stderr ('release commit {0} (#{1}) abandoned: it has no release tag and is not the origin/main tip' -f $short, $c.Number) }
        Write-Line 'abandoned'
        return
      }
      Write-Line 'proceed'
    })
}

function Format-VersionPreview([string]$Level, [string]$Label, $Latest) {
  $shown = $Level
  if (-not [string]::IsNullOrEmpty($Label)) { $shown = $Label.Substring('bump:'.Length) }
  $next = Get-NextVersion $Level $Label $Latest
  return 'v{0} (v{1}.{2}.{3} + {4})' -f $next, $Latest[0], $Latest[1], $Latest[2], $shown
}

function Test-LabelArgument([string]$Label) {
  if ($Label -cnotin @('bump:major', 'bump:minor', 'bump:patch')) { Write-Usage '-Label must be bump:major, bump:minor or bump:patch'; return $false }
  return $true
}

function Invoke-PrBodyRun([byte[]]$Bytes, [string]$Display, [string]$Base, [string]$RepoUrl, [string]$PrMessagesPath, [string]$PrFilesPath, [string]$Label, [string]$OutPath) {
  if (-not (Test-BaseArgument $Base)) { return 2 }
  if (-not [string]::IsNullOrEmpty($RepoUrl) -and -not $ReRepoUrl.IsMatch($RepoUrl)) { Write-Usage '-RepoUrl must be https://github.com/<owner>/<repo>'; return 2 }
  if (-not [string]::IsNullOrEmpty($Label) -and -not (Test-LabelArgument $Label)) { return 2 }
  return (Invoke-Guarded {
      try { $Text = $Utf8Strict.GetString($Bytes) }
      catch { Stop-WithError ('cannot read {0}: {1}' -f $Display, $_.Exception.Message) }
      $lines = [System.Collections.Generic.List[string]]::new([string[]]$Text.Split([char]10))
      $repo = Get-RepoUrl $lines $RepoUrl
      $range = Get-Range (Resolve-Base $Base) (Read-PrMessages $PrMessagesPath)
      $files = Read-PrFiles $PrFilesPath
      $entries = Get-Entries $range $repo
      $warnings = New-Object System.Collections.Generic.List[string]
      $warnings.AddRange($entries.Warnings)
      foreach ($c in $range.Commits) {
        if ($c.Pre -or $c.Subject.Type -cne 'fix' -or $c.Subject.Number -le 0) { continue }
        $key = [string]$c.Subject.Number
        if ($files.ContainsKey($key) -and ($files[$key] -ccontains 'src/module/exports.cpp')) {
          $warnings.Add(('the fix/ pull request #{0} changed src/module/exports.cpp; confirm that no export table row was renamed, reordered or removed' -f $key))
        }
      }
      foreach ($w in $warnings) { Write-Line ('::warning::' + $w) }
      if ($entries.Breaking.Count + $entries.Added.Count + $entries.Fixed.Count -eq 0) {
        Stop-WithError ('nothing to release since {0}: no feat/, stage/ or fix/ change and no breaking marker after the cut-off commit' -f $range.Tag)
      }
      $preview = Format-VersionPreview (Get-Level $range) $Label $range.Latest
      $notes = [pscustomobject]@{ Breaking = $entries.Breaking; Added = $entries.Added; Fixed = $entries.Fixed; Development = (New-Object System.Collections.Generic.List[string]) }
      $out = New-Object System.Collections.Generic.List[string]
      $out.Add('Version preview: <!-- version -->' + $preview + '<!-- /version -->')
      $out.Add('')
      if ($entries.Breaking.Count -gt 0) {
        $out.Add('> [!CAUTION]')
        $out.Add('> **Breaking change.** This release contains:')
        $out.Add('>')
        foreach ($b in $entries.Breaking) { $out.Add('> ' + $b) }
        $out.Add('')
      }
      $out.Add('## Release notes')
      $out.Add('')
      $out.Add('Development entries stay in CHANGELOG.md and are left out of the GitHub release notes.')
      $section = Get-SectionLines $notes ''
      for ($i = 1; $i -lt $section.Count; $i++) { $out.Add($section[$i]) }
      $out.Add('')
      if ($warnings.Count -gt 0) {
        $out.Add('## Warnings')
        $out.Add('')
        foreach ($w in $warnings) { $out.Add('- ' + $w) }
        $out.Add('')
      }
      $out.Add('## Review')
      $out.Add('')
      $out.Add('The release bot maintains this pull request: commits pushed to `release/next` by hand are overwritten the next time it rebuilds the branch, and the checklist below is reset on every rebuild. Squash-merging the pull request releases the version.')
      $out.Add('')
      $out.Add('- [ ] The release notes above read correctly for DLL and CAPL users')
      $out.Add('- [ ] The `bump:*` label matches the intended version (the preview is the last tag plus the label''s level)')
      if ($entries.Breaking.Count -gt 0) { $out.Add('- [ ] The breaking changes in the caution block above are intended') }
      [System.IO.File]::WriteAllBytes($OutPath, $Utf8.GetBytes((($out -join "`n") + "`n")))
    })
}

function Invoke-BodyVersionRun([byte[]]$Bytes, [string]$Display, [string]$Label, [string]$OutPath) {
  if (-not (Test-LabelArgument $Label)) { return 2 }
  return (Invoke-Guarded {
      try { $Text = $Utf8Strict.GetString($Bytes) }
      catch { Stop-WithError ('cannot read {0}: {1}' -f $Display, $_.Exception.Message) }
      $lines = $Text.Split([char]10)
      $at = -1
      $count = 0
      for ($i = 0; $i -lt $lines.Count; $i++) {
        if ($lines[$i].Contains('<!-- version -->') -or $lines[$i].Contains('<!-- /version -->')) { if ($at -lt 0) { $at = $i }; $count++ }
      }
      $m = $null
      if ($count -eq 1) { $m = $ReVersionMarks.Match($lines[$at]) }
      if ($count -ne 1 -or -not $m.Success -or $lines[$at].Split([string[]]@('<!-- version -->'), [System.StringSplitOptions]::None).Count -ne 2 -or $lines[$at].Split([string[]]@('<!-- /version -->'), [System.StringSplitOptions]::None).Count -ne 2) {
        Stop-WithError ('{0} must hold exactly one line with both version markers in order; found {1} line(s) with a marker' -f $Display, $count)
      }
      $preview = Format-VersionPreview '' $Label (Get-LastVersion)
      $lines[$at] = $m.Groups[1].Value + $preview + $m.Groups[2].Value
      [System.IO.File]::WriteAllBytes($OutPath, $Utf8.GetBytes(($lines -join "`n")))
    })
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
      'release-prs' { Invoke-Capture { Invoke-ReleasePrsRun $base } }
      default { Invoke-Capture { Invoke-NextVersionRun (Get-CaseParam $case 'level') (Get-CaseParam $case 'label') } }
    }
    Test-ToolResult $r $case['expect'] $problems
  }
  finally {
    if ([System.IO.File]::Exists($messages)) { [System.IO.File]::Delete($messages) }
  }
}

function Write-CaseJson($case, [string]$key, [string]$path) {
  if (-not ($case.ContainsKey('params') -and $case['params'].ContainsKey($key))) { return '' }
  [System.IO.File]::WriteAllText($path, (ConvertTo-Json $case['params'][$key] -Depth 6), $Utf8)
  return $path
}

function Test-PendingCutCase($case, $problems, [string]$tmp) {
  $releases = $tmp + '.releases'
  try {
    $releasesArg = Write-CaseJson $case 'prReleases' $releases
    $r = Invoke-Capture { Invoke-PendingCutRun $releasesArg }
    Test-ToolResult $r $case['expect'] $problems
  }
  finally {
    if ([System.IO.File]::Exists($releases)) { [System.IO.File]::Delete($releases) }
  }
}

function Test-BodyResult($r, $case, [string]$caseDir, [string]$tmp, [string]$again, $problems) {
  $expect = $case['expect']
  Test-ToolResult $r $expect $problems
  if (-not $expect.ContainsKey('exit')) { return }
  if ([int]$expect['exit'] -eq 0) {
    if (-not [System.IO.File]::Exists($tmp)) { $problems.Add('no result file was written'); return }
    $want = Convert-Eol $Utf8Strict.GetString([System.IO.File]::ReadAllBytes((Join-Path $caseDir 'expected-body.md'))) $case['eol']
    if ($Utf8Strict.GetString([System.IO.File]::ReadAllBytes($tmp)) -cne $want) { $problems.Add('result differs from expected-body.md') }
    if (-not [System.IO.File]::Exists($again) -or [Convert]::ToBase64String([System.IO.File]::ReadAllBytes($again)) -cne [Convert]::ToBase64String([System.IO.File]::ReadAllBytes($tmp))) { $problems.Add('a second run gave a different result') }
  }
  elseif ([System.IO.File]::Exists($tmp)) { $problems.Add('a result file was written although the run failed') }
}

function Test-PrBodyCase($case, [string]$caseDir, [byte[]]$bytes, $problems, [string]$tmp) {
  $again = $tmp + '.again'
  $messages = $tmp + '.messages'
  $filesJson = $tmp + '.files'
  try {
    $messagesArg = Write-CaseJson $case 'prMessages' $messages
    $filesArg = Write-CaseJson $case 'prFiles' $filesJson
    $base = Get-CaseParam $case 'base'; $repo = Get-CaseParam $case 'repoUrl'; $label = Get-CaseParam $case 'label'
    $r = Invoke-Capture { Invoke-PrBodyRun $bytes 'CHANGELOG.md' $base $repo $messagesArg $filesArg $label $tmp }
    [void](Invoke-Capture { Invoke-PrBodyRun $bytes 'CHANGELOG.md' $base $repo $messagesArg $filesArg $label $again })
    Test-BodyResult $r $case $caseDir $tmp $again $problems
  }
  finally {
    foreach ($f in @($again, $messages, $filesJson)) { if ([System.IO.File]::Exists($f)) { [System.IO.File]::Delete($f) } }
  }
}

function Test-BodyVersionCase($case, [string]$caseDir, $problems, [string]$tmp) {
  $again = $tmp + '.again'
  try {
    $bodyFile = Join-Path $caseDir 'body.md'
    $bodyBytes = [byte[]]@()
    if ([System.IO.File]::Exists($bodyFile)) { $bodyBytes = $Utf8.GetBytes((Convert-Eol $Utf8Strict.GetString([System.IO.File]::ReadAllBytes($bodyFile)) $case['eol'])) }
    $label = Get-CaseParam $case 'label'
    $r = Invoke-Capture { Invoke-BodyVersionRun $bodyBytes 'body.md' $label $tmp }
    [void](Invoke-Capture { Invoke-BodyVersionRun $bodyBytes 'body.md' $label $again })
    Test-BodyResult $r $case $caseDir $tmp $again $problems
  }
  finally {
    if ([System.IO.File]::Exists($again)) { [System.IO.File]::Delete($again) }
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
      $script:Ctx.TagChangelog = $null
      if ($case.ContainsKey('git')) { $script:Ctx.Facts = $case['git'] }
      if ($case.ContainsKey('command') -and $case['command'] -ceq 'generate') {
        $tagFile = Join-Path $caseDir 'tag-CHANGELOG.md'
        if ([System.IO.File]::Exists($tagFile)) { $script:Ctx.TagChangelog = Convert-Eol $Utf8Strict.GetString([System.IO.File]::ReadAllBytes($tagFile)) $case['eol'] }
        else { $script:Ctx.TagChangelog = $text }
      }
      $command = 'notes'
      if ($case.ContainsKey('command')) { $command = $case['command'] }
      if ($command -ceq 'generate') { Test-GenerateCase $case $caseDir $bytes $problems $tmp }
      elseif ($command -cin @('level', 'prs', 'release-prs', 'next-version')) { Test-ToolCase $case $command $problems $tmp }
      elseif ($command -ceq 'pending-cut') { Test-PendingCutCase $case $problems $tmp }
      elseif ($command -ceq 'pr-body') { Test-PrBodyCase $case $caseDir $bytes $problems $tmp }
      elseif ($command -ceq 'body-version') { Test-BodyVersionCase $case $caseDir $problems $tmp }
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
if ($Command -cnotin @('notes', 'check', 'prs', 'release-prs', 'level', 'generate', 'next-version', 'pending-cut', 'pr-body', 'body-version')) { Write-Usage 'the first argument must be notes, check, prs, release-prs, level, generate, next-version, pending-cut, pr-body, body-version or fixtures'; exit 2 }
if ($Command -cin @('pr-body', 'body-version') -and [string]::IsNullOrEmpty($Output)) { Write-Usage ('-Output is required for ' + $Command); exit 2 }
if ($Command -ceq 'body-version') {
  if ([string]::IsNullOrEmpty($Body)) { Write-Usage '-Body is required for body-version'; exit 2 }
  try {
    $bodyBytes = [System.IO.File]::ReadAllBytes((Resolve-FullPath $Body))
  }
  catch {
    Write-Line ('::error::cannot read {0}: {1}' -f $Body, $_.Exception.Message)
    exit 1
  }
  exit (Invoke-BodyVersionRun $bodyBytes $Body $Label (Resolve-FullPath $Output))
}
if ($Command -ceq 'notes' -or $Command -ceq 'check') {
  if ($Mode -cne 'tag' -and $Mode -cne 'dry-run') { Write-Usage '-Mode must be tag or dry-run'; exit 2 }
  if ([string]::IsNullOrEmpty($Sha)) { Write-Usage '-Sha is required (GITHUB_SHA is not set)'; exit 2 }
  if ($Command -ceq 'notes' -and [string]::IsNullOrEmpty($Output)) { Write-Usage '-Output is required for notes'; exit 2 }
}
if ($Command -ceq 'prs') { exit (Invoke-PrsRun $Base) }
if ($Command -ceq 'release-prs') { exit (Invoke-ReleasePrsRun $Base) }
if ($Command -ceq 'pending-cut') { exit (Invoke-PendingCutRun $PrReleases) }
if ($Command -ceq 'level') { exit (Invoke-LevelRun $Base $PrMessages) }
if ($Command -ceq 'next-version') { exit (Invoke-NextVersionRun $Level $Label) }

try {
  $bytes = [System.IO.File]::ReadAllBytes((Resolve-FullPath $ChangelogPath))
}
catch {
  Write-Line ('::error::cannot read {0}: {1}' -f $ChangelogPath, $_.Exception.Message)
  exit 1
}
if ($Command -ceq 'pr-body') { exit (Invoke-PrBodyRun $bytes $ChangelogPath $Base $RepoUrl $PrMessages $PrFiles $Label (Resolve-FullPath $Output)) }
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
