$ErrorActionPreference = 'Stop'

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

Stop-Process -Name SuckbongMachine -ErrorAction SilentlyContinue

if (-not (Test-Path 'build\CMakeCache.txt')) {
    Write-Host 'First configure: cmake --preset default' -ForegroundColor Cyan
    cmake --preset default
}

Write-Host 'Building Release (incremental, no windeployqt)...' -ForegroundColor Cyan
cmake --build build --config Release

Write-Host ''
Write-Host 'OK: build\Release\SuckbongMachine.exe' -ForegroundColor Green
Write-Host 'DLLs missing? Run: .\scripts\deploy-qt.ps1' -ForegroundColor DarkGray
