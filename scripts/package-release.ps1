# Builds a portable Release folder and ZIP for distribution (dynamic DLL layout).
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$cmakeVersionLine = Select-String -Path (Join-Path $repoRoot "CMakeLists.txt") -Pattern 'project\(PIPBONG VERSION ' | Select-Object -First 1
if (-not $cmakeVersionLine) {
    throw "Could not read project version from CMakeLists.txt"
}
$version = ($cmakeVersionLine.Line -replace '.*VERSION\s+([0-9.]+).*', '$1').Trim()

if (-not $SkipBuild) {
    Write-Host "Building Release v$version..."
    & (Join-Path $repoRoot "scripts\build-release.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "Deploying runtime DLLs..."
& (Join-Path $repoRoot "scripts\deploy-qt.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$releaseDir = Join-Path $repoRoot "build\Release"
$mainExe = Join-Path $releaseDir "PIPBONG.exe"
$updaterExe = Join-Path $releaseDir "PIPBONGUpdater.exe"
if (-not (Test-Path $mainExe)) {
    throw "Missing $mainExe"
}
if (-not (Test-Path $updaterExe)) {
    throw "Missing $updaterExe"
}

$distDir = Join-Path $repoRoot "dist"
New-Item -ItemType Directory -Force -Path $distDir | Out-Null

$zipName = "PIPBONG-win64.zip"
$zipPath = Join-Path $distDir $zipName
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Write-Host "Creating $zipPath..."
Compress-Archive -Path (Join-Path $releaseDir "*") -DestinationPath $zipPath -CompressionLevel Optimal

Write-Host "OK: $zipPath" -ForegroundColor Green
Write-Host "Run from: $releaseDir\PIPBONG.exe" -ForegroundColor Green
