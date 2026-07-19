# Canonical incremental Release build — Ctrl+Shift+B, F5, AI task close.
# Never runs cmake --preset when build/ already exists (no vcpkg reinstall loop).
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "build-common.ps1")

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

Prepare-IncrementalBuildEnvironment -RepoRoot $repoRoot
Ensure-BuildTreeConfigured -RepoRoot $repoRoot

Write-Host "Building Release (incremental, target PIPBONG, no windeployqt)..."
Stop-Process -Name PIPBONG -ErrorAction SilentlyContinue
cmake --build build --config Release --target PIPBONG
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host "OK: build\Release\PIPBONG.exe" -ForegroundColor Green
if (-not (Test-Path "build\Release\Qt6Core.dll")) {
    Write-Host "DLLs missing? Run: .\scripts\deploy-qt.ps1" -ForegroundColor Yellow
}
# SessionRunPolicySim runs via CMake POST_BUILD on PIPBONG link (see PIPBONG_RUN_POLICY_SIM_ON_BUILD).
