# Verify PIPBONG dev isolation for dual-Cursor / parallel-project workflows.
# Run once per machine after opening this repo in its own Cursor window.
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "build-common.ps1")

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

Write-Host "PIPBONG dev isolation check ($repoRoot)" -ForegroundColor Cyan

$ok = $true

function Test-Setting {
    param([string]$Label, [bool]$Pass)
    if ($Pass) {
        Write-Host "  OK  $Label" -ForegroundColor Green
    } else {
        Write-Host "  FAIL  $Label" -ForegroundColor Red
        $script:ok = $false
    }
}

$settingsPath = Join-Path $repoRoot ".vscode\settings.json"
$settings = Get-Content $settingsPath -Raw | ConvertFrom-Json
Test-Setting "cmake.enabled is false" ($settings.'cmake.enabled' -eq $false)
Test-Setting "pipbong.f5BuildAndRun is true" ($settings.'pipbong.f5BuildAndRun' -eq $true)

$launchPath = Join-Path $repoRoot ".vscode\launch.json"
$launch = Get-Content $launchPath -Raw | ConvertFrom-Json
Test-Setting "launch.json has no debug configs" ($launch.configurations.Count -eq 0)

Test-Setting "CMake cache path matches this folder" (-not (Test-CMakeCachePathMismatch -RepoRoot $repoRoot))

$qwindows = Join-Path $repoRoot "build\Release\platforms\qwindows.dll"
if (-not (Test-Path $qwindows)) {
    Write-Host "  Deploying Qt runtime (platforms/qwindows.dll missing)..." -ForegroundColor Yellow
    & (Join-Path $PSScriptRoot "deploy-qt.ps1")
    if ($LASTEXITCODE -ne 0) { $ok = $false }
} else {
    Test-Setting "Qt platform plugin deployed" $true
}

Write-Host ""
Write-Host "Applying PIPBONG-only F5 keybindings (config.pipbong.f5BuildAndRun)..." -ForegroundColor Cyan
& (Join-Path $PSScriptRoot "fix-pipbong-cursor-f5.ps1")

Write-Host ""
Write-Host "Dual Cursor checklist:" -ForegroundColor Cyan
Write-Host "  - Open PIPBONG and the other project in SEPARATE Cursor windows (File -> Open Folder each)."
Write-Host "  - Build PIPBONG here: Ctrl+Shift+B or F5 (Build and Run task only)."
Write-Host "  - Do not click CMake Tools [Configure] in either window during parallel work."
Write-Host "  - recover-ide-build.ps1: default is local-only; use -KillGlobalBuildProcesses only when alone."
Write-Host "  - First configure in both projects at the same time can still contend on vcpkg — stagger if needed."
Write-Host ""
if ($ok) {
    Write-Host "Isolation setup OK. Reload Window in this Cursor window, then F5." -ForegroundColor Green
} else {
    Write-Host "Some checks failed — fix above, or run .\scripts\recover-ide-build.ps1" -ForegroundColor Red
    exit 1
}
