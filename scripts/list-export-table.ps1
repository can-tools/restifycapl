#requires -Version 7
<#
.SYNOPSIS
Checks the CAPL export table in exports.cpp (checks 1-12) and renders it as markdown.

.DESCRIPTION
Modes (exactly one):
  -Path <exports.cpp> [-MarkdownPath <file>]   check the table, optionally write the markdown
  -FixtureDir <dir>                            run every *.fixture; its first line is
                                               "// expect: pass" or "// expect: fail N [N...]"
Exit codes: 0 all checks pass / every fixture met its expectation; 1 a check failed, a
fixture misbehaved or its first line is not a valid directive; 2 usage or I/O error.

Policies:
  - All failures are reported, not just the first. Only an unlocatable or unparsable
    table (check 1) stops the run.
  - A row failing check 3 skips checks 4-12 for that row; a row failing check 7 skips
    checks 9-12. Row 0 (the version row) is validated by check 1 only.
  - kRef constants are resolved from the same file before any type lookup.
  - A type character outside Vector's table fires check 9 only; check 10 applies to
    characters that are in the table.
  - If the declaration occurs more than once, check 1 fails and checks 2-12 run on the
    first table only.
  - parTypes and array may be a string literal or a brace list; both count parCount
    entries. A function without parameters must use "", "", {""} (a project rule,
    stricter than Vector).
  - Names are at most 49 characters (MAX_CDL_NAME2 is 50 including the NUL).

The script is written to also run unchanged in Windows PowerShell 5.1.
#>
[CmdletBinding()]
param(
  [string]$Path,
  [string]$MarkdownPath,
  [string]$FixtureDir
)

Set-StrictMode -Version 2.0
$ErrorActionPreference = 'Stop'

$TypeNames = @{ 'V' = 'void'; 'C' = 'char'; 'B' = 'byte'; 'I' = 'int'; 'W' = 'word'; 'L' = 'long'; 'D' = 'dword'; '6' = 'int64'; 'U' = 'qword'; 'F' = 'float' }
$ScalarTypes = 'VCBIWLD6UF'
$RefableTypes = 'CBIWLD6UF'
$ArrayOnlyTypes = 'CBIW'
$ReturnTypes = 'VLD6UF'

function Resolve-FullPath([string]$p) {
  if (-not [System.IO.Path]::IsPathRooted($p)) {
    $p = Join-Path (Get-Location).ProviderPath $p
  }
  return [System.IO.Path]::GetFullPath($p)
}

function ConvertTo-Display([string]$s) {
  $b = New-Object -TypeName 'byte[]' -ArgumentList $s.Length
  for ($k = 0; $k -lt $s.Length; $k++) { $b[$k] = [byte]([int]$s[$k] -band 0xFF) }
  return [System.Text.Encoding]::UTF8.GetString($b)
}

function Format-Escaped([string]$s) {
  $sb = New-Object System.Text.StringBuilder
  foreach ($ch in $s.ToCharArray()) {
    $c = [int]$ch
    if ($c -ge 32 -and $c -le 126 -and $c -ne 34 -and $c -ne 92) { [void]$sb.Append($ch) }
    else { [void]$sb.Append('\' + [Convert]::ToString($c, 8).PadLeft(3, '0')) }
  }
  return $sb.ToString()
}

function New-Token([string]$kind, $value, [int]$line) {
  return [pscustomobject]@{ Kind = $kind; Value = $value; Line = $line }
}

# Decodes one string or char literal into a "byte string" (one char per byte, source
# characters above 0x7F expanded to UTF-8) so lengths match what the C++ compiler stores.
function Read-Literal([string]$text, [int]$start, [string]$quote, [int]$line) {
  $sb = New-Object System.Text.StringBuilder
  $n = $text.Length
  $i = $start + 1
  $closed = $false
  while ($i -lt $n) {
    $ch = $text[$i]
    if ($ch -ceq $quote) { $closed = $true; $i++; break }
    if ($ch -ceq "`n") { break }
    if ($ch -ceq '\') {
      $i++
      if ($i -ge $n) { break }
      $e = [string]$text[$i]
      if ([regex]::IsMatch($e, '^[0-7]$')) {
        $o = $i
        while ($i -lt $n -and ($i - $o) -lt 3 -and [regex]::IsMatch([string]$text[$i], '^[0-7]$')) { $i++ }
        $v = [Convert]::ToInt32($text.Substring($o, $i - $o), 8)
        if ($v -gt 255) { throw ('{0}|octal escape sequence out of range (value above 0xFF)' -f $line) }
        [void]$sb.Append([char]$v)
        continue
      }
      if ($e -ceq 'x') {
        $i++
        $h = $i
        # C++ hex escapes are greedy: every following hex digit belongs to the escape.
        while ($i -lt $n -and [regex]::IsMatch([string]$text[$i], '^[0-9A-Fa-f]$')) { $i++ }
        if ($i -eq $h) { throw ('{0}|\x escape without hex digits' -f $line) }
        $digits = $text.Substring($h, $i - $h).TrimStart('0')
        if ($digits.Length -gt 2) { throw ('{0}|hex escape sequence out of range (value above 0xFF)' -f $line) }
        $v = 0
        if ($digits.Length -gt 0) { $v = [Convert]::ToInt32($digits, 16) }
        [void]$sb.Append([char]$v)
        continue
      }
      $v = -1
      switch -CaseSensitive ($e) {
        'n' { $v = 10 }
        't' { $v = 9 }
        'r' { $v = 13 }
        'a' { $v = 7 }
        'b' { $v = 8 }
        'f' { $v = 12 }
        'v' { $v = 11 }
        '\' { $v = 92 }
        "'" { $v = 39 }
        '"' { $v = 34 }
        '?' { $v = 63 }
      }
      if ($v -lt 0) { throw ('{0}|unknown escape sequence \{1}' -f $line, $e) }
      [void]$sb.Append([char]$v)
      $i++
      continue
    }
    if ([int]$ch -lt 128) { [void]$sb.Append($ch); $i++ }
    else {
      $len = 1
      if ([char]::IsHighSurrogate($ch) -and ($i + 1) -lt $n) { $len = 2 }
      foreach ($b in [System.Text.Encoding]::UTF8.GetBytes($text.Substring($i, $len))) { [void]$sb.Append([char]$b) }
      $i += $len
    }
  }
  if (-not $closed) { throw ('{0}|unterminated string or character literal' -f $line) }
  return [pscustomobject]@{ Value = $sb.ToString(); End = $i }
}

function Read-Tokens([string]$text) {
  $tokens = New-Object System.Collections.Generic.List[object]
  $n = $text.Length
  $i = 0
  $line = 1
  $bol = $true
  while ($i -lt $n) {
    $ch = $text[$i]
    if ($ch -ceq "`n") { $line++; $bol = $true; $i++; continue }
    if ([char]::IsWhiteSpace($ch)) { $i++; continue }
    $next = [char]0
    if (($i + 1) -lt $n) { $next = $text[$i + 1] }
    if ($ch -ceq '/' -and $next -ceq '/') {
      while ($i -lt $n -and $text[$i] -cne "`n") { $i++ }
      continue
    }
    if ($ch -ceq '/' -and $next -ceq '*') {
      $end = $text.IndexOf('*/', $i + 2, [System.StringComparison]::Ordinal)
      if ($end -lt 0) { throw ('{0}|unterminated block comment' -f $line) }
      for ($k = $i; $k -lt $end; $k++) { if ($text[$k] -ceq "`n") { $line++; $bol = $true } }
      $i = $end + 2
      continue
    }
    if ($ch -ceq '#' -and $bol) {
      $startLine = $line
      $s = $i
      while ($i -lt $n) {
        if ($text[$i] -ceq "`n") {
          $j = $i - 1
          if ($j -ge $s -and $text[$j] -ceq "`r") { $j-- }
          if ($j -ge $s -and $text[$j] -ceq '\') { $line++; $i++; continue }
          break
        }
        $i++
      }
      $tokens.Add((New-Token 'pp' $text.Substring($s, $i - $s).Trim() $startLine))
      continue
    }
    $bol = $false
    if ($ch -ceq '"') {
      $lit = Read-Literal $text $i '"' $line
      $tokens.Add((New-Token 'str' $lit.Value $line))
      $i = $lit.End
      continue
    }
    if ($ch -ceq "'") {
      $lit = Read-Literal $text $i "'" $line
      if ($lit.Value.Length -ne 1) { throw ('{0}|character literal must hold exactly one character' -f $line) }
      $tokens.Add((New-Token 'chr' ([int]$lit.Value[0]) $line))
      $i = $lit.End
      continue
    }
    if ([regex]::IsMatch([string]$ch, '^[0-9]$')) {
      $s = $i
      while ($i -lt $n -and [regex]::IsMatch([string]$text[$i], '^[0-9A-Za-z_.]$')) { $i++ }
      $word = $text.Substring($s, $i - $s)
      $m = [regex]::Match($word, '^(0[xX][0-9a-fA-F]+|[0-9]+)[uUlL]*$')
      if (-not $m.Success) { throw ('{0}|invalid numeric literal {1}' -f $line, $word) }
      $digits = $m.Groups[1].Value
      if ($digits.Length -gt 15) { throw ('{0}|numeric literal {1} is out of range' -f $line, $word) }
      if ($digits.Length -gt 2 -and ($digits.Substring(0, 2) -ceq '0x' -or $digits.Substring(0, 2) -ceq '0X')) { $v = [Convert]::ToInt64($digits.Substring(2), 16) }
      elseif ($digits.Length -gt 1 -and $digits[0] -ceq '0' -and [regex]::IsMatch($digits, '^[0-7]+$')) { $v = [Convert]::ToInt64($digits, 8) }
      else { $v = [Convert]::ToInt64($digits, 10) }
      $tokens.Add((New-Token 'num' $v $line))
      continue
    }
    if ([regex]::IsMatch([string]$ch, '^[A-Za-z_]$')) {
      $s = $i
      while ($i -lt $n -and [regex]::IsMatch([string]$text[$i], '^[A-Za-z0-9_]$')) { $i++ }
      $tokens.Add((New-Token 'id' $text.Substring($s, $i - $s) $line))
      continue
    }
    $tokens.Add((New-Token 'punct' ([string]$ch) $line))
    $i++
  }
  return , $tokens
}

function Get-TokSig($t) {
  switch ($t.Kind) {
    'id' { return 'id:' + $t.Value }
    'str' { return 's:' + $t.Value }
    'chr' { return 'c:' + $t.Value }
    'num' { return 'n:' + $t.Value }
    default { return [string]$t.Value }
  }
}

function Get-FieldSig($toks) {
  return (@($toks | ForEach-Object { Get-TokSig $_ }) -join ' ')
}

function Get-FieldText($toks) {
  $parts = @($toks | ForEach-Object {
      if ($_.Kind -ceq 'str') { '"' + (Format-Escaped $_.Value) + '"' }
      elseif ($_.Kind -ceq 'chr') { "'" + (Format-Escaped ([string][char]$_.Value)) + "'" }
      else { [string]$_.Value }
    })
  return ($parts -join ' ')
}

function Test-Punct($t, [string]$p) {
  return ($t.Kind -ceq 'punct' -and $t.Value -ceq $p)
}

function Test-Str($toks) {
  return ($toks.Count -eq 1 -and $toks[0].Kind -ceq 'str')
}

function Test-EmptyStr($toks) {
  return ((Test-Str $toks) -and $toks[0].Value.Length -eq 0)
}

function Test-Num($toks) {
  return ($toks.Count -eq 1 -and $toks[0].Kind -ceq 'num')
}

function Test-Brace($toks) {
  if ($toks.Count -lt 2) { return $false }
  if (-not (Test-Punct $toks[0] '{')) { return $false }
  $depth = 0
  for ($k = 0; $k -lt $toks.Count; $k++) {
    if (Test-Punct $toks[$k] '{') { $depth++ }
    elseif (Test-Punct $toks[$k] '}') {
      $depth--
      if ($depth -eq 0) { return ($k -eq ($toks.Count - 1)) }
    }
  }
  return $false
}

function Get-BraceInner($toks) {
  if ($toks.Count -le 2) { return , @() }
  return , @($toks[1..($toks.Count - 2)])
}

function Split-Top($toks) {
  $parts = New-Object System.Collections.Generic.List[object]
  $cur = New-Object System.Collections.Generic.List[object]
  $depth = 0
  foreach ($t in $toks) {
    if ($t.Kind -ceq 'punct') {
      if ($t.Value -ceq '{' -or $t.Value -ceq '(') { $depth++ }
      elseif ($t.Value -ceq '}' -or $t.Value -ceq ')') { $depth-- }
      elseif ($t.Value -ceq ',' -and $depth -eq 0) {
        $parts.Add($cur.ToArray())
        $cur = New-Object System.Collections.Generic.List[object]
        continue
      }
    }
    $cur.Add($t)
  }
  if ($cur.Count -gt 0) { $parts.Add($cur.ToArray()) }
  return , $parts
}

function Get-ListItems($toks) {
  $items = New-Object System.Collections.Generic.List[object]
  if (Test-Str $toks) {
    $s = $toks[0].Value
    for ($k = 0; $k -lt $s.Length; $k++) { $items.Add([pscustomobject]@{ Code = [int]$s[$k]; Tokens = $null }) }
    return [pscustomobject]@{ Form = 'string'; Items = $items }
  }
  if (Test-Brace $toks) {
    $inner = Get-BraceInner $toks
    $parts = Split-Top $inner
    foreach ($p in $parts) { $items.Add([pscustomobject]@{ Code = $null; Tokens = $p }) }
    return [pscustomobject]@{ Form = 'brace'; Items = $items }
  }
  return $null
}

function Get-RefConstants($raw) {
  $consts = @{}
  for ($i = 0; $i -lt $raw.Count - 4; $i++) {
    $t = $raw[$i]
    if ($t.Kind -ceq 'id' -and $t.Value -ceq 'constexpr' -and $raw[$i + 1].Kind -ceq 'id' -and $raw[$i + 1].Value -ceq 'char' -and $raw[$i + 2].Kind -ceq 'id' -and (Test-Punct $raw[$i + 3] '=')) {
      $j = $i + 4
      $expr = New-Object System.Collections.Generic.List[string]
      while ($j -lt $raw.Count -and -not (Test-Punct $raw[$j] ';')) { $expr.Add((Get-TokSig $raw[$j])); $j++ }
      $m = [regex]::Match(($expr -join ' '), '^id:static_cast < id:char > \( c:([0-9]+) - n:([0-9]+) \)$')
      if ($m.Success) { $consts[$raw[$i + 2].Value] = [int64]$m.Groups[1].Value - [int64]$m.Groups[2].Value }
    }
  }
  return $consts
}

function Get-TypeClass([int64]$v) {
  $bad = [pscustomobject]@{ Valid = $false; Base = ''; Ref = $false; Char = '' }
  if ($v -lt -128 -or $v -gt 255) { return $bad }
  $u = [int](($v + 256) % 256)
  if ($u -ge 128) {
    $base = [string][char]($u - 128)
    if ($RefableTypes.IndexOf($base, [System.StringComparison]::Ordinal) -ge 0) { return [pscustomobject]@{ Valid = $true; Base = $base; Ref = $true; Char = $base } }
    return $bad
  }
  $base = [string][char]$u
  if ($ScalarTypes.IndexOf($base, [System.StringComparison]::Ordinal) -ge 0) { return [pscustomobject]@{ Valid = $true; Base = $base; Ref = $false; Char = $base } }
  return $bad
}

# Returns the type class of one parTypes/resultType item, or a Problem for check 9.
function Resolve-TypeItem($item, $consts) {
  $value = $null
  $problem = $null
  if ($null -ne $item.Code) { $value = [int64]$item.Code }
  else {
    $toks = $item.Tokens
    if ($toks.Count -eq 1 -and ($toks[0].Kind -ceq 'chr' -or $toks[0].Kind -ceq 'num')) { $value = [int64]$toks[0].Value }
    elseif ($toks.Count -eq 1 -and $toks[0].Kind -ceq 'id') {
      if ($consts.ContainsKey($toks[0].Value)) { $value = [int64]$consts[$toks[0].Value] }
      elseif ($toks[0].Value.StartsWith('kRef', [System.StringComparison]::Ordinal)) { $problem = ('reference constant {0} is not defined in this file as constexpr char {0} = static_cast<char>(''X'' - 128)' -f $toks[0].Value) }
      else { $problem = ('identifier {0} is not a known type constant' -f $toks[0].Value) }
    }
    elseif ($toks.Count -eq 3 -and $toks[0].Kind -ceq 'chr' -and (Test-Punct $toks[1] '-') -and $toks[2].Kind -ceq 'num') {
      $problem = 'a reference type must be written with a named kRef constant (kRefLong, kRefDword, kRefDouble), not as an inline expression'
    }
    else { $problem = ('unrecognised type expression: {0}' -f (Get-FieldText $toks)) }
  }
  if ($null -ne $problem) { return [pscustomobject]@{ Class = $null; Problem = $problem } }
  $cls = Get-TypeClass $value
  if (-not $cls.Valid) {
    return [pscustomobject]@{ Class = $null; Problem = ('type value {0} is not in Vector''s type table (V C B I W L D 6 U F, or a scalar type minus 128 for a reference)' -f $value) }
  }
  return [pscustomobject]@{ Class = $cls; Problem = $null }
}

function Add-Fail($res, [int]$check, [int]$line, $row, $name, [string]$msg) {
  $res.Failures.Add([pscustomobject]@{ Check = $check; Line = $line; Row = $row; Name = $name; Message = $msg })
}

function Invoke-ExportTableCheck([string]$text) {
  $res = [pscustomobject]@{
    Failures = (New-Object System.Collections.Generic.List[object])
    Parsed = $false
    FunctionCount = 0
    Rows = (New-Object System.Collections.Generic.List[object])
  }
  $unparsable = 'table cannot be located or parsed: '

  try {
    $raw = Read-Tokens $text
  }
  catch {
    $parts = $_.Exception.Message.Split('|', 2)
    $ln = 1
    $msg = $_.Exception.Message
    if ($parts.Count -eq 2 -and [regex]::IsMatch($parts[0], '^[0-9]+$')) { $ln = [int]$parts[0]; $msg = $parts[1] }
    Add-Fail $res 1 $ln $null $null ($unparsable + $msg)
    return $res
  }
  $consts = Get-RefConstants $raw

  $starts = New-Object System.Collections.Generic.List[int]
  for ($i = 0; $i -lt $raw.Count - 5; $i++) {
    if ($raw[$i].Kind -ceq 'id' -and $raw[$i].Value -ceq 'CAPL_DLL_INFO4' -and
      $raw[$i + 1].Kind -ceq 'id' -and $raw[$i + 1].Value -ceq 'CAPL_DLL_INFO_LIST4' -and
      (Test-Punct $raw[$i + 2] '[') -and (Test-Punct $raw[$i + 3] ']') -and
      (Test-Punct $raw[$i + 4] '=') -and (Test-Punct $raw[$i + 5] '{')) {
      $starts.Add($i + 5)
    }
  }
  if ($starts.Count -eq 0) {
    Add-Fail $res 1 1 $null $null ($unparsable + 'start marker "CAPL_DLL_INFO4 CAPL_DLL_INFO_LIST4[] = {" not found')
    return $res
  }
  if ($starts.Count -gt 1) {
    Add-Fail $res 1 $raw[$starts[1]].Line $null $null ('the table declaration occurs {0} times, expected exactly once; checks 2-12 run on the first table only' -f $starts.Count)
  }

  $open = $starts[0]
  $depth = 0
  $close = -1
  for ($i = $open; $i -lt $raw.Count; $i++) {
    if (Test-Punct $raw[$i] '{') { $depth++ }
    elseif (Test-Punct $raw[$i] '}') { $depth--; if ($depth -eq 0) { $close = $i; break } }
  }
  if ($close -lt 0) {
    Add-Fail $res 1 $raw[$open].Line $null $null ($unparsable + 'unbalanced braces, the table end was not found')
    return $res
  }

  $body = New-Object System.Collections.Generic.List[object]
  for ($i = $open + 1; $i -lt $close; $i++) {
    $t = $raw[$i]
    if ($t.Kind -ceq 'pp') {
      Add-Fail $res 8 $t.Line $null $null ('preprocessor directive inside the table: {0}' -f $t.Value)
      continue
    }
    if ($t.Kind -ceq 'str' -and $body.Count -gt 0 -and $body[$body.Count - 1].Kind -ceq 'str') {
      $prev = $body[$body.Count - 1]
      $body[$body.Count - 1] = New-Token 'str' ($prev.Value + $t.Value) $prev.Line
      continue
    }
    $body.Add($t)
  }

  $rows = New-Object System.Collections.Generic.List[object]
  $i = 0
  while ($i -lt $body.Count) {
    if (-not (Test-Punct $body[$i] '{')) {
      Add-Fail $res 1 $body[$i].Line $null $null ($unparsable + 'unexpected token between rows')
      return $res
    }
    $d = 0
    $e = -1
    for ($k = $i; $k -lt $body.Count; $k++) {
      if (Test-Punct $body[$k] '{') { $d++ }
      elseif (Test-Punct $body[$k] '}') { $d--; if ($d -eq 0) { $e = $k; break } }
    }
    if ($e -lt 0) {
      Add-Fail $res 1 $body[$i].Line $null $null ($unparsable + 'unbalanced braces in a row')
      return $res
    }
    $inner = @()
    if ($e -gt $i + 1) { $inner = @($body.GetRange($i + 1, $e - $i - 1).ToArray()) }
    $fields = Split-Top $inner
    $rows.Add([pscustomobject]@{ Index = $rows.Count; Line = $body[$i].Line; Fields = $fields })
    $i = $e + 1
    if ($i -lt $body.Count -and (Test-Punct $body[$i] ',')) { $i++ }
  }
  $res.Parsed = $true

  if ($rows.Count -lt 1) {
    Add-Fail $res 1 $raw[$open].Line $null $null 'the table has no rows; row 0 must be the CDLL_VERSION_NAME row'
    return $res
  }

  $r0 = $rows[0]
  $want0 = @('id:CDLL_VERSION_NAME', '( id:CAPL_FARCALL ) id:CDLL_VERSION', 's:', 's:', 'n:0', 'n:0', 's:', 's:', '{ s: }')
  $row0ok = ($r0.Fields.Count -eq 9)
  if ($row0ok) {
    for ($k = 0; $k -lt 9; $k++) { if ((Get-FieldSig $r0.Fields[$k]) -cne $want0[$k]) { $row0ok = $false } }
  }
  if (-not $row0ok) {
    Add-Fail $res 1 $r0.Line 0 $null 'row 0 must be exactly {CDLL_VERSION_NAME, (CAPL_FARCALL)CDLL_VERSION, "", "", 0, 0, "", "", {""}}'
  }

  $last = $rows[$rows.Count - 1]
  $hasSentinel = ($rows.Count -ge 2 -and $last.Fields.Count -eq 1 -and (Get-FieldSig $last.Fields[0]) -ceq 'n:0')
  if (-not $hasSentinel) {
    Add-Fail $res 1 $last.Line $null $null 'the last row must be the terminating sentinel {0}'
  }
  $lastFunc = $rows.Count - 1
  if ($hasSentinel) { $lastFunc = $rows.Count - 2 }
  $funcRows = New-Object System.Collections.Generic.List[object]
  for ($k = 1; $k -le $lastFunc; $k++) { $funcRows.Add($rows[$k]) }
  $res.FunctionCount = $funcRows.Count
  if ($funcRows.Count -eq 0) {
    Add-Fail $res 2 $raw[$open].Line $null $null 'the table has no function rows'
  }

  $seen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
  foreach ($r in $funcRows) {
    $f = $r.Fields
    $ri = $r.Index
    $ln = $r.Line
    $nameRaw = $null
    $name = $null
    if ($f.Count -gt 0 -and (Test-Str $f[0])) { $nameRaw = $f[0][0].Value; $name = ConvertTo-Display $nameRaw }
    if ($f.Count -ne 9) {
      Add-Fail $res 3 $ln $ri $name ('expected exactly 9 fields, found {0}' -f $f.Count)
      continue
    }
    $sigOk = $true

    $sig1 = Get-FieldSig $f[1]
    $want1 = '( id:CAPL_FARCALL ) id:'
    $ok4 = $false
    if ($null -ne $nameRaw) { $ok4 = ($sig1 -ceq ($want1 + $nameRaw)) }
    else { $ok4 = $sig1.StartsWith($want1, [System.StringComparison]::Ordinal) }
    if (-not $ok4) {
      Add-Fail $res 4 $ln $ri $name ('field 1 must be (CAPL_FARCALL) followed by the same name as field 0; found: {0}' -f (Get-FieldText $f[1]))
    }

    if ($null -eq $nameRaw) {
      $sigOk = $false
      Add-Fail $res 5 $ln $ri $null 'field 0 (the name) must be a string literal'
    }
    else {
      if (-not $nameRaw.StartsWith('restify', [System.StringComparison]::Ordinal)) { Add-Fail $res 5 $ln $ri $name 'the name must start with "restify"' }
      if ($nameRaw.Length -gt 49) { Add-Fail $res 5 $ln $ri $name ('the name has {0} characters, at most 49 are allowed' -f $nameRaw.Length) }
      if (-not $seen.Add($nameRaw)) { Add-Fail $res 5 $ln $ri $name 'the name is not unique in the table' }
    }

    $category = $null
    if ((Test-Str $f[2]) -and $f[2][0].Value.Length -gt 0) { $category = ConvertTo-Display $f[2][0].Value }
    else { $sigOk = $false; Add-Fail $res 6 $ln $ri $name 'the category (field 2) must be a non-empty string literal' }
    $desc = $null
    if ((Test-Str $f[3]) -and $f[3][0].Value.Length -gt 0) { $desc = ConvertTo-Display $f[3][0].Value }
    else { $sigOk = $false; Add-Fail $res 6 $ln $ri $name 'the description (field 3) must be a non-empty string literal' }

    $pc = $null
    $fail7 = $false
    if (-not (Test-Num $f[5])) {
      $fail7 = $true
      Add-Fail $res 7 $ln $ri $name ('parCount must be an integer literal; found: {0}' -f (Get-FieldText $f[5]))
    }
    else {
      $pc = [int]$f[5][0].Value
      if ($pc -lt 0 -or $pc -gt 64) {
        $fail7 = $true
        Add-Fail $res 7 $ln $ri $name ('parCount {0} is outside the range 0 to 64' -f $pc)
      }
    }

    $types = $null
    $dims = $null
    $pnames = $null
    if (-not $fail7 -and $pc -eq 0) {
      $nl = Get-ListItems $f[8]
      $okNames = ($null -ne $nl -and $nl.Form -ceq 'brace' -and $nl.Items.Count -eq 1 -and (Test-EmptyStr $nl.Items[0].Tokens))
      if (-not ((Test-EmptyStr $f[6]) -and (Test-EmptyStr $f[7]) -and $okNames)) {
        $fail7 = $true
        Add-Fail $res 7 $ln $ri $name ('a function without parameters must be written as parCount 0, "", "", {{""}} (project rule, stricter than Vector, which also accepts other forms); found: {0}, {1}, {2}' -f (Get-FieldText $f[6]), (Get-FieldText $f[7]), (Get-FieldText $f[8]))
      }
    }
    elseif (-not $fail7) {
      $types = Get-ListItems $f[6]
      $dims = Get-ListItems $f[7]
      $pnames = Get-ListItems $f[8]
      if ($null -eq $types) { $fail7 = $true; Add-Fail $res 7 $ln $ri $name 'parTypes must be a string literal or a brace list' }
      elseif ($types.Items.Count -ne $pc) { $fail7 = $true; Add-Fail $res 7 $ln $ri $name ('parTypes has {0} entries, parCount is {1}' -f $types.Items.Count, $pc) }
      if ($null -eq $dims) { $fail7 = $true; Add-Fail $res 7 $ln $ri $name 'array must be a string literal or a brace list' }
      elseif ($dims.Items.Count -ne $pc) {
        $fail7 = $true
        $extra = ''
        if ($dims.Form -ceq 'string' -and $dims.Items.Count -eq 0) { $extra = ' (an empty "" dimension string is not accepted when parCount is 1 or more)' }
        Add-Fail $res 7 $ln $ri $name ('array has {0} entries, parCount is {1}{2}' -f $dims.Items.Count, $pc, $extra)
      }
      if ($null -eq $pnames -or $pnames.Form -cne 'brace') { $fail7 = $true; Add-Fail $res 7 $ln $ri $name 'parNames must be a brace list of string literals' }
      else {
        if ($pnames.Items.Count -ne $pc) { $fail7 = $true; Add-Fail $res 7 $ln $ri $name ('parNames has {0} entries, parCount is {1}' -f $pnames.Items.Count, $pc) }
        foreach ($it in $pnames.Items) {
          if (-not (Test-Str $it.Tokens)) { $fail7 = $true; Add-Fail $res 7 $ln $ri $name 'every parNames entry must be a string literal'; break }
        }
      }
    }
    if ($fail7) { continue }

    $retType = Resolve-TypeItem ([pscustomobject]@{ Code = $null; Tokens = $f[4] }) $consts
    if ($null -ne $retType.Problem) { $sigOk = $false; Add-Fail $res 9 $ln $ri $name ('return type: {0}' -f $retType.Problem) }
    elseif (-not ($retType.Class.Ref -eq $false -and $ReturnTypes.IndexOf($retType.Class.Base, [System.StringComparison]::Ordinal) -ge 0)) {
      $sigOk = $false
      Add-Fail $res 10 $ln $ri $name ('return type {0}{1} is not allowed; a return type must be one of V, L, D, 6, U, F (no array-only type, no reference)' -f $retType.Class.Base, $(if ($retType.Class.Ref) { ' (reference)' } else { '' }))
    }

    $paramSigs = New-Object System.Collections.Generic.List[string]
    for ($p = 0; $p -lt $pc; $p++) {
      $pn = $pnames.Items[$p].Tokens[0].Value
      $label = 'parameter {0} ({1})' -f $p, (ConvertTo-Display $pn)
      $ty = Resolve-TypeItem $types.Items[$p] $consts
      $dimItem = $dims.Items[$p]
      $dv = $null
      if ($null -ne $dimItem.Code) { $dv = [int]$dimItem.Code }
      elseif ($dimItem.Tokens.Count -eq 1 -and ($dimItem.Tokens[0].Kind -ceq 'num' -or $dimItem.Tokens[0].Kind -ceq 'chr')) { $dv = [int]$dimItem.Tokens[0].Value }
      else { $sigOk = $false; Add-Fail $res 11 $ln $ri $name ('{0}: the dimension must be an integer constant; found: {1}' -f $label, (Get-FieldText $dimItem.Tokens)) }
      if ($null -ne $dv -and $dv -ne 0 -and $dv -ne 1 -and $dv -ne 2) {
        Add-Fail $res 11 $ln $ri $name ('{0}: dimension {1} is not allowed, only 0, 1 or 2' -f $label, $dv)
      }
      if ($null -ne $ty.Problem) { $sigOk = $false; Add-Fail $res 9 $ln $ri $name ('{0}: {1}' -f $label, $ty.Problem); continue }
      $cls = $ty.Class
      if ($null -ne $dv) {
        if ($ArrayOnlyTypes.IndexOf($cls.Base, [System.StringComparison]::Ordinal) -ge 0 -and $dv -eq 0) {
          Add-Fail $res 10 $ln $ri $name ('{0}: type {1} is only valid for arrays, its dimension must not be 0' -f $label, $cls.Base)
        }
        if ($cls.Ref -and $dv -ne 0) {
          Add-Fail $res 10 $ln $ri $name ('{0}: a reference parameter cannot have a dimension' -f $label)
        }
      }
      else { $sigOk = $false }
      if ($cls.Base -ceq 'V') { Add-Fail $res 10 $ln $ri $name ('{0}: V (void) is not valid as a parameter type' -f $label) }
      $tn = $TypeNames[$cls.Base]
      if ($cls.Ref) { $tn = $tn + '&' }
      $suffix = ''
      if ($null -ne $dv -and $dv -ge 1) { $suffix = '[]' }
      if ($null -ne $dv -and $dv -ge 2) { $suffix = '[][]' }
      $paramSigs.Add(('{0} {1}{2}' -f $tn, (ConvertTo-Display $pn), $suffix))
    }

    if ($pc -ge 1) {
      $rowSeen = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
      for ($p = 0; $p -lt $pc; $p++) {
        $pn = $pnames.Items[$p].Tokens[0].Value
        $label = 'parameter {0}' -f $p
        if ($pn.Length -eq 0) { Add-Fail $res 12 $ln $ri $name ('{0}: the name is empty' -f $label) }
        elseif (-not [regex]::IsMatch($pn, '^[A-Za-z_][A-Za-z0-9_]*$')) { Add-Fail $res 12 $ln $ri $name ('{0}: "{1}" is not a valid identifier' -f $label, (Format-Escaped $pn)) }
        elseif (-not $rowSeen.Add($pn)) { Add-Fail $res 12 $ln $ri $name ('{0}: the name "{1}" is used twice in this row' -f $label, $pn) }
      }
    }

    if ($sigOk -and $null -ne $retType.Class) {
      $res.Rows.Add([pscustomobject]@{
          Index = $ri
          Category = $category
          Description = $desc
          Signature = ('{0} {1}({2})' -f $TypeNames[$retType.Class.Base], $name, ($paramSigs -join ', '))
        })
    }
  }
  return $res
}

function Get-Markdown($res) {
  $order = New-Object System.Collections.Generic.List[string]
  # Ordinal comparer: categories group as whole, case-sensitive strings.
  $byCat = [System.Collections.Generic.Dictionary[string, object]]::new([System.StringComparer]::Ordinal)
  foreach ($row in $res.Rows) {
    if (-not $byCat.ContainsKey($row.Category)) {
      $byCat[$row.Category] = New-Object System.Collections.Generic.List[object]
      $order.Add($row.Category)
    }
    $byCat[$row.Category].Add($row)
  }
  $sb = New-Object System.Text.StringBuilder
  [void]$sb.Append("Row 0 is the reserved ``CDLL_VERSION`` marker.`n")
  foreach ($cat in $order) {
    [void]$sb.Append("`n#### $cat`n`n")
    [void]$sb.Append("| Row | Signature | Description |`n|---|---|---|`n")
    foreach ($row in $byCat[$cat]) {
      $d = ($row.Description -replace '\|', '\|') -replace '[\r\n]+', ' '
      [void]$sb.Append(('| {0} | `{1}` | {2} |' -f $row.Index, $row.Signature, $d) + "`n")
    }
  }
  return $sb.ToString()
}

function Get-FailedChecks($res) {
  return @($res.Failures | ForEach-Object { $_.Check } | Sort-Object -Unique)
}

function Write-Usage([string]$msg) {
  [Console]::Error.WriteLine($msg)
  [Console]::Error.WriteLine('usage: list-export-table.ps1 -Path <exports.cpp> [-MarkdownPath <file>] | -FixtureDir <dir>')
}

$utf8 = New-Object System.Text.UTF8Encoding($false)
$hasPath = -not [string]::IsNullOrEmpty($Path)
$hasDir = -not [string]::IsNullOrEmpty($FixtureDir)

if ($hasPath -eq $hasDir) {
  Write-Usage 'exactly one of -Path and -FixtureDir is required'
  exit 2
}
if ($hasDir -and -not [string]::IsNullOrEmpty($MarkdownPath)) {
  Write-Usage '-MarkdownPath can only be used with -Path'
  exit 2
}

if ($hasDir) {
  $dirFull = Resolve-FullPath $FixtureDir
  if (-not [System.IO.Directory]::Exists($dirFull)) {
    Write-Usage ('fixture directory not found: {0}' -f $FixtureDir)
    exit 2
  }
  $files = [string[]][System.IO.Directory]::GetFiles($dirFull, '*.fixture')
  if ($files.Length -eq 0) {
    Write-Usage ('no *.fixture files in {0}' -f $FixtureDir)
    exit 2
  }
  $leaves = [string[]]@($files | ForEach-Object { [System.IO.Path]::GetFileName($_) })
  [System.Array]::Sort($leaves, [System.StringComparer]::Ordinal)
  $passed = 0
  $failed = 0
  foreach ($leaf in $leaves) {
    $display = Join-Path $FixtureDir $leaf
    try {
      $text = [System.IO.File]::ReadAllText((Join-Path $dirFull $leaf), $utf8)
    }
    catch {
      Write-Usage ('cannot read {0}: {1}' -f $display, $_.Exception.Message)
      exit 2
    }
    $first = $text.Split("`n")[0].TrimEnd("`r")
    $m = [regex]::Match($first, '^// expect: (pass|fail( [0-9]+)+)$')
    if (-not $m.Success) {
      Write-Output ('FAIL {0}: first line is not a valid directive: {1}' -f $display, $first)
      $failed++
      continue
    }
    $expected = 'pass'
    if ($m.Groups[1].Value -cne 'pass') {
      $nums = @($m.Groups[1].Value.Substring(5).Trim().Split(' ') | ForEach-Object { [int]$_ } | Sort-Object -Unique)
      $expected = 'fail ' + ($nums -join ' ')
    }
    $res = Invoke-ExportTableCheck $text
    $checks = @(Get-FailedChecks $res)
    $actual = 'pass'
    if ($checks.Count -gt 0) { $actual = 'fail ' + ($checks -join ' ') }
    if ($actual -ceq $expected) {
      Write-Output ('PASS {0} ({1})' -f $display, $actual)
      $passed++
    }
    else {
      Write-Output ('FAIL {0}: expected {1}, got {2}' -f $display, $expected, $actual)
      $failed++
    }
  }
  Write-Output ('fixtures: {0} passed, {1} failed of {2}' -f $passed, $failed, $leaves.Length)
  if ($failed -gt 0) { exit 1 }
  exit 0
}

$pathFull = Resolve-FullPath $Path
if (-not [System.IO.File]::Exists($pathFull)) {
  Write-Usage ('file not found: {0}' -f $Path)
  exit 2
}
try {
  $text = [System.IO.File]::ReadAllText($pathFull, $utf8)
}
catch {
  Write-Usage ('cannot read {0}: {1}' -f $Path, $_.Exception.Message)
  exit 2
}

$res = Invoke-ExportTableCheck $text
$onActions = -not [string]::IsNullOrEmpty($env:GITHUB_ACTIONS)
foreach ($fl in $res.Failures) {
  $rowText = 'row -'
  if ($null -ne $fl.Row) {
    $nm = '-'
    if ($null -ne $fl.Name) { $nm = $fl.Name }
    $rowText = 'row {0} {1}' -f $fl.Row, $nm
  }
  Write-Output ('{0}:{1}: [check {2}] {3}: {4}' -f $Path, $fl.Line, $fl.Check, $rowText, $fl.Message)
  if ($onActions) {
    $annot = ('[check {0}] {1}: {2}' -f $fl.Check, $rowText, $fl.Message).Replace('%', '%25').Replace("`r", '%0D').Replace("`n", '%0A')
    Write-Output ('::error file={0},line={1}::{2}' -f $Path, $fl.Line, $annot)
  }
}

if (-not [string]::IsNullOrEmpty($MarkdownPath) -and $res.Parsed) {
  try {
    $mdFull = Resolve-FullPath $MarkdownPath
    [void][System.IO.Directory]::CreateDirectory([System.IO.Path]::GetDirectoryName($mdFull))
    [System.IO.File]::WriteAllText($mdFull, (Get-Markdown $res), $utf8)
  }
  catch {
    Write-Usage ('cannot write {0}: {1}' -f $MarkdownPath, $_.Exception.Message)
    exit 2
  }
}

$bad = @(Get-FailedChecks $res)
if ($bad.Count -eq 0) {
  $catSet = [System.Collections.Generic.HashSet[string]]::new([System.StringComparer]::Ordinal)
  foreach ($row in $res.Rows) { [void]$catSet.Add($row.Category) }
  Write-Output ('export-table passed: {0} functions in {1} categories, checks 1-12 clean.' -f $res.FunctionCount, $catSet.Count)
  exit 0
}
Write-Output ('export-table: {0} of 12 checks failed.' -f $bad.Count)
exit 1
