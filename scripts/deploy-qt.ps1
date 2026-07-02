$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not (Test-Path "build\CMakeCache.txt")) {
    cmake --preset default
}

cmake --build build --config Release --target deploy-qt
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$releaseDir = Join-Path $repoRoot "build\Release"
$vcpkgBin = Join-Path $repoRoot "build\vcpkg_installed\x64-windows\bin"
if (Test-Path $vcpkgBin) {
    Copy-Item (Join-Path $vcpkgBin "*.dll") $releaseDir -Force
}

Write-Host "OK: $releaseDir (Qt + runtime DLLs deployed)" -ForegroundColor Green
