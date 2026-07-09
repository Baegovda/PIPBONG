# One-click recovery when CMake Tools / vcpkg lock / stuck configure breaks the IDE build path.
$ErrorActionPreference = "Stop"
. (Join-Path $PSScriptRoot "build-common.ps1")

$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

Write-Host "Recovering IDE build workflow..." -ForegroundColor Cyan

Stop-StuckConfigureProcesses -IncludeMsbuild
Start-Sleep -Milliseconds 500
Clear-StaleVcpkgLock -RepoRoot $repoRoot | Out-Null

& (Join-Path $PSScriptRoot "fix-pipbong-cursor-f5.ps1")

$vscodeFiles = @(
    ".vscode\settings.json",
    ".vscode\tasks.json",
    ".vscode\launch.json",
    ".vscode\extensions.json",
    ".vscode\c_cpp_properties.json"
)
foreach ($rel in $vscodeFiles) {
    if (Test-Path $rel) {
        git checkout -- $rel 2>$null
    }
}

Write-Host ""
Write-Host "Done. Next steps:" -ForegroundColor Green
Write-Host "  1. Cursor: Developer -> Reload Window"
Write-Host "  2. Do NOT click CMake Tools [Configure] / [Build] in the status bar"
Write-Host "  3. Build with Ctrl+Shift+B or .\scripts\build-release.ps1"
Write-Host "  4. Run with F5 -> task Build and Run (terminal must show Started PIPBONG.exe)"
Write-Host "  5. Do NOT use Run and Debug play for daily runs (launch.json has no debug configs)"
Write-Host ""
Write-Host "If F5 still opens a blank CodeLLDB terminal after build:" -ForegroundColor Yellow
Write-Host "  .\scripts\fix-pipbong-cursor-f5.ps1"
Write-Host "  Then Developer -> Reload Window (required once so F5 leaves the debugger)"
Write-Host ""
Write-Host "If DLLs are missing at runtime: .\scripts\deploy-qt.ps1 (once)"
