# Runs SessionRunPolicySim (policy regression scenarios for trigger/hold/repeat sessions).
$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
. (Join-Path $repoRoot 'scripts\build-common.ps1')
Prepare-IncrementalBuildEnvironment
Ensure-BuildTreeConfigured

$buildDir = Join-Path $repoRoot 'build'
cmake --build $buildDir --config Release --target PIPBONGPolicySim
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$sim = Join-Path $buildDir 'Release\PIPBONGPolicySim.exe'
if (-not (Test-Path $sim)) {
    Write-Error "PIPBONGPolicySim.exe not found at $sim"
}
& $sim
exit $LASTEXITCODE
