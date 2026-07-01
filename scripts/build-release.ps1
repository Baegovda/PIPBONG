$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not (Test-Path "build\CMakeCache.txt")) {
    Write-Host "Configuring default preset (first time)..."
    cmake --preset default
}

Write-Host "Building Release (incremental, no windeployqt)..."
Stop-Process -Name PIPBONG -ErrorAction SilentlyContinue
cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host 'OK: build\Release\PIPBONG.exe' -ForegroundColor Green
if (-not (Test-Path "build\Release\Qt6Core.dll")) {
    Write-Host 'DLLs missing? Run: .\scripts\deploy-qt.ps1' -ForegroundColor Yellow
}
