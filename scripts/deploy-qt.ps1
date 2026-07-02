$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not (Test-Path "build\CMakeCache.txt")) {
    cmake --preset default
}

Stop-Process -Name PIPBONG -Force -ErrorAction SilentlyContinue

cmake --build build --config Release --target deploy-qt
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

# windeployqt already pulls OpenCV and transitive DLLs from the linked exe.
# Do not copy the entire vcpkg bin folder (adds PostgreSQL, unused OpenCV modules, etc.).

Write-Host "OK: build\Release (Qt runtime DLLs deployed)" -ForegroundColor Green
