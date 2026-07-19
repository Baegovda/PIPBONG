# Incremental Release build, then start PIPBONG.exe (detached). Used by F5 task — no launch.json / debugger.
$ErrorActionPreference = "Stop"

$repoRoot = Split-Path -Parent $PSScriptRoot
& (Join-Path $PSScriptRoot "build-release.ps1")
if ($LASTEXITCODE -ne 0) {
    exit $LASTEXITCODE
}

$exe = Join-Path $repoRoot "build\Release\PIPBONG.exe"
$cwd = Join-Path $repoRoot "build\Release"
if (-not (Test-Path $exe)) {
    Write-Error "Missing $exe"
    exit 1
}

$qwindows = Join-Path $cwd "platforms\qwindows.dll"
if (-not (Test-Path $qwindows)) {
    Write-Host "Qt platform plugin missing — running deploy-qt (once after configure or folder move)..." -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot "deploy-qt.ps1")
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}

Start-Process -FilePath $exe -WorkingDirectory $cwd
Write-Host "Started PIPBONG.exe" -ForegroundColor Green
