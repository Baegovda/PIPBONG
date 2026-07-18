# Runs SessionRunPolicySim (manual scenarios + invariant grid + fuzz).
param(
    [string]$ReportPath = ""
)

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $repoRoot 'scripts\build-common.ps1')
Prepare-IncrementalBuildEnvironment -RepoRoot $repoRoot
Ensure-BuildTreeConfigured -RepoRoot $repoRoot

$buildDir = Join-Path $repoRoot 'build'
cmake --build $buildDir --config Release --target PIPBONGPolicySim
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$sim = Join-Path $buildDir 'Release\PIPBONGPolicySim.exe'
if (-not (Test-Path $sim)) {
    Write-Error "PIPBONGPolicySim.exe not found at $sim"
}

if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $ReportPath = Join-Path $buildDir 'policy-sim-report.txt'
}

& $sim --report=$ReportPath
$code = $LASTEXITCODE
if ($code -eq 0) {
    Write-Host "Report: $ReportPath" -ForegroundColor Green
}
exit $code
