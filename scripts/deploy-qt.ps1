$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

if (-not (Test-Path 'build\CMakeCache.txt')) {
    Write-Host 'Configure first: cmake --preset default' -ForegroundColor Yellow
    cmake --preset default
}

Write-Host 'Deploying Qt/OpenCV DLLs to build\Release\ ...' -ForegroundColor Cyan
cmake --build build --config Release --target deploy-qt

Write-Host ''
Write-Host 'OK: build\Release\SuckbongMachine.exe (DLLs updated)' -ForegroundColor Green
