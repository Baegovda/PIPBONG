$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not (Test-Path "build\CMakeCache.txt")) {
    cmake --preset default
}

cmake --build build --config Release --target deploy-qt
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

Write-Host 'OK: build\Release\PIPBONG.exe (DLLs updated)' -ForegroundColor Green
