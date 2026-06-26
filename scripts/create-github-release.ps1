# Builds static dist/ binaries and publishes a GitHub Release for the current CMake version.
param(
    [string]$Notes = ""
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

$cmakeVersionLine = Select-String -Path (Join-Path $repoRoot "CMakeLists.txt") -Pattern 'project\(SuckbongMachine VERSION ' | Select-Object -First 1
if (-not $cmakeVersionLine) {
    throw "Could not read project version from CMakeLists.txt"
}
$version = ($cmakeVersionLine.Line -replace '.*VERSION\s+([0-9.]+).*', '$1').Trim()
$tag = "v$version"

Write-Host "Building static Release for $tag..."
taskkill /IM SuckbongMachine.exe /F 2>$null | Out-Null
cmake --preset static
cmake --build build-static --config Release

$mainExe = Join-Path $repoRoot "dist\SuckbongMachine.exe"
$updaterExe = Join-Path $repoRoot "dist\SuckbongMachineUpdater.exe"
if (-not (Test-Path $mainExe)) {
    throw "Missing $mainExe"
}
if (-not (Test-Path $updaterExe)) {
    throw "Missing $updaterExe"
}

if (-not $Notes) {
    $Notes = "SuckbongMachine $tag"
}

$gh = Get-Command gh -ErrorAction SilentlyContinue
if (-not $gh) {
    throw "GitHub CLI (gh) is not installed. Install it, run 'gh auth login', then rerun this script."
}

Write-Host "Creating GitHub release $tag..."
gh release create $tag $mainExe $updaterExe --title "SuckbongMachine $tag" --notes $Notes
Write-Host "Done: https://github.com/Baegovda/sbm/releases/tag/$tag"
