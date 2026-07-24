# Summarize the latest profile auto-switch profiler report.
param(
    [string]$ReportPath = ""
)

$ErrorActionPreference = "Stop"

if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $local = Join-Path $env:LOCALAPPDATA "PIPBONG\PIPBONG\profile-switch\latest.md"
    $repo = Join-Path (Split-Path $PSScriptRoot -Parent) "profile-switch\latest.md"
    if (Test-Path $local) {
        $ReportPath = $local
    } elseif (Test-Path $repo) {
        $ReportPath = $repo
    } else {
        Write-Host "No profile-switch report found."
        Write-Host "Enable program/profileSwitchProfiling or set PIPBONG_PROFILE_SWITCH_PROFILE=1, then reproduce a switch."
        exit 1
    }
}

if (-not (Test-Path $ReportPath)) {
    Write-Host "Report not found: $ReportPath"
    exit 1
}

Write-Host "Profile switch report: $ReportPath"
Write-Host ""
Get-Content -Path $ReportPath -Encoding UTF8
