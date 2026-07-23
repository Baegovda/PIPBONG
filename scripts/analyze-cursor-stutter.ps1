# Summarize cursor-stutter/latest.md for AI / human review.
$ErrorActionPreference = "Stop"
$root = Split-Path $PSScriptRoot -Parent
$logPath = Join-Path $root "cursor-stutter\latest.md"
if (-not (Test-Path $logPath)) {
    foreach ($rel in @("cursor-stutter\latest.md")) {
        $alt = Join-Path (Get-Location) $rel
        if (Test-Path $alt) { $logPath = $alt; break }
    }
}
if (-not (Test-Path $logPath)) {
    throw "Report not found at cursor-stutter\latest.md. Enable 커서 스터터 진단, reproduce, then exit PIPBONG."
}

$content = Get-Content -Raw -Encoding UTF8 $logPath
Write-Host "=== cursor-stutter report ===" -ForegroundColor Cyan
Write-Host $logPath
Write-Host ""

if ($content -match '(?ms)## Auto diagnosis\s*\r?\n(.*?)(?:\r?\n## |\z)') {
    Write-Host "## Auto diagnosis" -ForegroundColor Yellow
    Write-Host $Matches[1].TrimEnd()
    Write-Host ""
}

$jumpLines = Select-String -InputObject $content -Pattern '^\| cursor_jump ' -AllMatches
if ($jumpLines) {
    Write-Host "Recent cursor_jump rows (last 15):" -ForegroundColor Yellow
    $jumpLines.Matches | Select-Object -Last 15 | ForEach-Object { Write-Host $_.Line }
}

Write-Host ""
Write-Host "AI: read cursor-stutter/latest.md — Auto diagnosis + set_cursor / mouse_hook_snap tables."
