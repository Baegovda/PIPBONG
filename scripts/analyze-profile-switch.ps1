# Summarize profile-switch/latest.md for AI / human review (manual + auto switch).
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

function Find-ProfileSwitchReport {
    param([string]$Base)
    $candidates = @(
        (Join-Path $Base "profile-switch\latest.md"),
        (Join-Path $env:LOCALAPPDATA "PIPBONG\PIPBONG\profile-switch\latest.md")
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) { return (Resolve-Path $path).Path }
    }
    $dir = $Base
    for ($i = 0; $i -lt 8; $i++) {
        $cmake = Join-Path $dir "CMakeLists.txt"
        if (Test-Path $cmake) {
            $p = Join-Path $dir "profile-switch\latest.md"
            if (Test-Path $p) { return (Resolve-Path $p).Path }
        }
        $parent = Split-Path $dir -Parent
        if (-not $parent -or $parent -eq $dir) { break }
        $dir = $parent
    }
    return $null
}

$logPath = Find-ProfileSwitchReport -Base $root
if (-not $logPath) {
    $logPath = Find-ProfileSwitchReport -Base (Get-Location).Path
}
if (-not $logPath) {
    throw @"
Report not found. Expected:
  <repo>\profile-switch\latest.md
  or %LOCALAPPDATA%\PIPBONG\PIPBONG\profile-switch\latest.md

Enable **프로그램 설정 → 진단 → 프로필 전환 진단** (or PIPBONG_PROFILE_SWITCH_PROFILE=1),
switch profiles manually 2-3 times, then re-run this script.
"@
}

$content = Get-Content -Raw -Encoding UTF8 $logPath
Write-Host "=== profile-switch report ===" -ForegroundColor Cyan
Write-Host $logPath
Write-Host ""

if ($content -match '(?ms)^switch_mode:\s*(\S+)') {
    $mode = $Matches[1]
    $modeKo = if ($mode -eq 'manual') { '수동' } elseif ($mode -eq 'auto') { '자동' } else { $mode }
    Write-Host "switch_mode: $mode ($modeKo)" -ForegroundColor Gray
}
if ($content -match '(?ms)^total_ms:\s*(\d+)') {
    Write-Host "total_ms: $($Matches[1])" -ForegroundColor Gray
}
if ($content -match '(?ms)^end_reason:\s*(\S+)') {
    Write-Host "end_reason: $($Matches[1])" -ForegroundColor Gray
}
Write-Host ""

if ($content -match '(?ms)## Auto diagnosis\s*\r?\n(.*?)(?:\r?\n## |\z)') {
    Write-Host "## Auto diagnosis" -ForegroundColor Yellow
    Write-Host $Matches[1].TrimEnd()
    Write-Host ""
}

Write-Host "Slowest phases (duration ms):" -ForegroundColor Yellow
$phaseRows = [regex]::Matches($content, '\|\s*\d+\s*\|\s*([^\|]+)\|\s*(\d+)\s*\|')
$timed = @()
foreach ($m in $phaseRows) {
    $name = $m.Groups[1].Value.Trim()
    $ms = [int]$m.Groups[2].Value
    if ($name -and $ms -ge 0) {
        $timed += [pscustomobject]@{ Phase = $name; Ms = $ms }
    }
}
$timed | Sort-Object Ms -Descending | Select-Object -First 8 | Format-Table -AutoSize

Write-Host "AI: read full report at path above — Auto diagnosis + Phases table for manual-switch lag fixes."
