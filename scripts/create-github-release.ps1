# Packages dynamic Release build and publishes a GitHub Release for the current CMake version.
param(
    [string]$Notes = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$cmakeVersionLine = Select-String -Path (Join-Path $repoRoot "CMakeLists.txt") -Pattern 'project\(PIPBONG VERSION ' | Select-Object -First 1
if (-not $cmakeVersionLine) {
    throw "Could not read project version from CMakeLists.txt"
}
$version = ($cmakeVersionLine.Line -replace '.*VERSION\s+([0-9.]+).*', '$1').Trim()
$tag = "v$version"

Write-Host "Packaging dynamic Release for $tag..."
taskkill /IM PIPBONG.exe /F 2>$null | Out-Null
& (Join-Path $repoRoot "scripts\package-release.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$zipPath = Join-Path $repoRoot "dist\PIPBONG-win64.zip"
if (-not (Test-Path $zipPath)) {
    throw "Missing $zipPath"
}

if (-not $Notes) {
    $Notes = "PIPBONG $tag — extract the ZIP and run PIPBONG.exe (includes Qt/OpenCV DLLs)."
}

$gh = Get-Command gh -ErrorAction SilentlyContinue
if (-not $gh) {
    throw "GitHub CLI (gh) is not installed. Install it, run 'gh auth login', then rerun this script."
}

$releaseRepo = "Baegovda/PIPBONG-releases"

Write-Host "Creating GitHub release $tag on $releaseRepo..."
gh release create $tag $zipPath --repo $releaseRepo --title "PIPBONG $tag" --notes $Notes
Write-Host "Done: https://github.com/$releaseRepo/releases/tag/$tag"
