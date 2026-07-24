# Summarize multi-hold/latest.md for AI / human review (simultaneous Hold hotkeys).
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

function Find-MultiHoldReport {
    param([string]$Base)
    $candidates = @(
        (Join-Path $Base "multi-hold\latest.md"),
        (Join-Path $env:LOCALAPPDATA "PIPBONG\PIPBONG\multi-hold\latest.md")
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) { return (Resolve-Path $path).Path }
    }
    $dir = $Base
    for ($i = 0; $i -lt 8; $i++) {
        $cmake = Join-Path $dir "CMakeLists.txt"
        if (Test-Path $cmake) {
            $p = Join-Path $dir "multi-hold\latest.md"
            if (Test-Path $p) { return (Resolve-Path $p).Path }
        }
        $parent = Split-Path $dir -Parent
        if (-not $parent -or $parent -eq $dir) { break }
        $dir = $parent
    }
    return $null
}

$logPath = Find-MultiHoldReport -Base $root
if (-not $logPath) {
    $logPath = Find-MultiHoldReport -Base (Get-Location).Path
}
if (-not $logPath) {
    throw @"
Report not found. Expected:
  <repo>\multi-hold\latest.md
  or %LOCALAPPDATA%\PIPBONG\PIPBONG\multi-hold\latest.md

Enable **프로그램 설정 → 진단 → 홀드 동시 입력 진단** (or PIPBONG_MULTI_HOLD_PROFILE=1),
press/release 2+ Hold hotkeys together 10 times, then re-run this script.
"@
}

$content = Get-Content -Raw -Encoding UTF8 $logPath
Write-Host "=== multi-hold report (Hold burst) ===" -ForegroundColor Cyan
Write-Host $logPath
Write-Host ""

if ($content -match '(?ms)^reason:\s*(.+)$') {
    Write-Host "reason: $($Matches[1])" -ForegroundColor Gray
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
$timed | Sort-Object Ms -Descending | Select-Object -First 12 | Format-Table -AutoSize

Write-Host "AI: compare hold_burst_prep_ms / hold_start_ui_ms / hold_end_cleanup_ms / apply_run_ui_ms."
Write-Host "Pair with app-spike/latest.md max_gui_stall_ms for full picture."
