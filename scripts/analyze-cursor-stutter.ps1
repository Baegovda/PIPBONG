# Summarize cursor-stutter/latest.md for AI / human review.
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent

function Find-CursorStutterReport {
    param([string]$Base)
    $candidates = @(
        (Join-Path $Base "cursor-stutter\latest.md"),
        (Join-Path $env:LOCALAPPDATA "PIPBONG\PIPBONG\cursor-stutter\latest.md")
    )
    foreach ($path in $candidates) {
        if (Test-Path $path) { return (Resolve-Path $path).Path }
    }
    $dir = $Base
    for ($i = 0; $i -lt 8; $i++) {
        $cmake = Join-Path $dir "CMakeLists.txt"
        if (Test-Path $cmake) {
            $p = Join-Path $dir "cursor-stutter\latest.md"
            if (Test-Path $p) { return (Resolve-Path $p).Path }
        }
        $parent = Split-Path $dir -Parent
        if (-not $parent -or $parent -eq $dir) { break }
        $dir = $parent
    }
    return $null
}

$logPath = Find-CursorStutterReport -Base $root
if (-not $logPath) {
    $logPath = Find-CursorStutterReport -Base (Get-Location).Path
}
if (-not $logPath) {
    throw @"
Report not found. Expected:
  <repo>\cursor-stutter\latest.md
  or %LOCALAPPDATA%\PIPBONG\PIPBONG\cursor-stutter\latest.md

Run PIPBONG (F5), reproduce cursor jump with Hold Q/W/E/R, exit app — sampler runs by default.
"@
}

$content = Get-Content -Raw -Encoding UTF8 $logPath
Write-Host "=== cursor-stutter report ===" -ForegroundColor Cyan
Write-Host $logPath
Write-Host ""

if ($content -match '(?ms)^sampler_ticks:\s*(\d+)') {
    Write-Host "sampler_ticks: $($Matches[1])" -ForegroundColor Gray
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

$summary = Select-String -InputObject $content -Pattern '^\| (cursor_jump|micro_jump|snap_back|sampler_overrun|sampler_ticks) ' -AllMatches
if ($summary) {
    Write-Host "Summary counters:" -ForegroundColor Yellow
    $summary.Matches | ForEach-Object { Write-Host $_.Line }
    Write-Host ""
}

Write-Host "AI: read full report at path above — Auto diagnosis + jump tables + event TSV."
