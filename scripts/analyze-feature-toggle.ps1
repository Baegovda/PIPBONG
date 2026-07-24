# Summarize feature-toggle/latest.md for AI / human review (사용 ON/OFF).
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

function Find-FeatureToggleReport {
    param([string]$Base)
    $candidates = @(
        (Join-Path $Base "feature-toggle\latest.md"),
        (Join-Path $env:LOCALAPPDATA "PIPBONG\PIPBONG\feature-toggle\latest.md")
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) { return (Resolve-Path $path).Path }
    }
    $dir = $Base
    for ($i = 0; $i -lt 8; $i++) {
        $cmake = Join-Path $dir "CMakeLists.txt"
        if (Test-Path $cmake) {
            $p = Join-Path $dir "feature-toggle\latest.md"
            if (Test-Path $p) { return (Resolve-Path $p).Path }
        }
        $parent = Split-Path $dir -Parent
        if (-not $parent -or $parent -eq $dir) { break }
        $dir = $parent
    }
    return $null
}

$logPath = Find-FeatureToggleReport -Base $root
if (-not $logPath) {
    $logPath = Find-FeatureToggleReport -Base (Get-Location).Path
}
if (-not $logPath) {
    throw @"
Report not found. Expected:
  <repo>\feature-toggle\latest.md
  or %LOCALAPPDATA%\PIPBONG\PIPBONG\feature-toggle\latest.md

Enable **프로그램 설정 → 진단 → 기능 사용 토글 진단** (or PIPBONG_FEATURE_TOGGLE_PROFILE=1),
toggle **사용** ON/OFF 3-5 times, then re-run this script.
"@
}

$content = Get-Content -Raw -Encoding UTF8 $logPath
Write-Host "=== feature-toggle report (사용 ON/OFF) ===" -ForegroundColor Cyan
Write-Host $logPath
Write-Host ""

if ($content -match '(?ms)^feature_id:\s*(.+)$') {
    Write-Host "feature_id: $($Matches[1])" -ForegroundColor Gray
}
if ($content -match '(?ms)^enabled:\s*(\S+)') {
    Write-Host "enabled: $($Matches[1])" -ForegroundColor Gray
}
if ($content -match '(?ms)^total_ms:\s*(\d+)') {
    Write-Host "total_ms: $($Matches[1])" -ForegroundColor Gray
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

Write-Host "AI: read full report — undo_copy_profiles / hotkeys_* / configure_row are common lag sources."
