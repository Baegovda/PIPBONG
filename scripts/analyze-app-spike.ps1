# Summarize app-spike/latest.md for AI / human review.
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

function Find-AppSpikeReport {
    param([string]$Base)
    $candidates = @(
        (Join-Path $Base "app-spike\latest.md"),
        (Join-Path $env:LOCALAPPDATA "PIPBONG\PIPBONG\app-spike\latest.md")
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) { return (Resolve-Path $path).Path }
    }
    $dir = $Base
    for ($i = 0; $i -lt 8; $i++) {
        $cmake = Join-Path $dir "CMakeLists.txt"
        if (Test-Path $cmake) {
            $p = Join-Path $dir "app-spike\latest.md"
            if (Test-Path $p) { return (Resolve-Path $p).Path }
        }
        $parent = Split-Path $dir -Parent
        if (-not $parent -or $parent -eq $dir) { break }
        $dir = $parent
    }
    return $null
}

$logPath = Find-AppSpikeReport -Base $root
if (-not $logPath) {
    $logPath = Find-AppSpikeReport -Base (Get-Location).Path
}
if (-not $logPath) {
    throw @"
Report not found. Expected:
  <repo>\app-spike\latest.md
  or %LOCALAPPDATA%\PIPBONG\PIPBONG\app-spike\latest.md

Enable **프로그램 설정 → 진단 → 앱 스파이크 진단** (or PIPBONG_APP_SPIKE_PROFILE=1), reproduce lag, exit app.
"@
}

$content = Get-Content -Raw -Encoding UTF8 $logPath
Write-Host "=== app-spike report ===" -ForegroundColor Cyan
Write-Host $logPath
Write-Host ""

if ($content -match '(?ms)^session_seconds:\s*([\d.]+)') {
    Write-Host "session_seconds: $($Matches[1])" -ForegroundColor Gray
}
if ($content -match '(?ms)^foreground_at_end:\s*(.+)$') {
    Write-Host "foreground_at_end: $($Matches[1])" -ForegroundColor Gray
}
Write-Host ""

if ($content -match '(?ms)## Auto diagnosis\s*\r?\n(.*?)(?:\r?\n## |\z)') {
    Write-Host "## Auto diagnosis" -ForegroundColor Yellow
    Write-Host $Matches[1].TrimEnd()
    Write-Host ""
}

$summary = Select-String -InputObject $content -Pattern '^\| (gui_pulse_count|max_gui_stall_ms|gui_stall_|cpu_spike|peak_)' -AllMatches
if ($summary) {
    Write-Host "Summary counters:" -ForegroundColor Yellow
    $summary.Matches | ForEach-Object { Write-Host $_.Line }
    Write-Host ""
}

Write-Host "AI: read full report at path above — Auto diagnosis + summary table + event TSV."
