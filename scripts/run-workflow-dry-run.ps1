# Build and run PIPBONGWorkflowDryRunSim (AGENTS.md §8.21 R6.4).
$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
. (Join-Path $root 'scripts\build-common.ps1')
Prepare-IncrementalBuildEnvironment
Ensure-BuildTreeConfigured -Root $root

$buildDir = Join-Path $root 'build'
cmake --build $buildDir --config Release --target PIPBONGWorkflowDryRunSim
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$sim = Join-Path $buildDir 'Release\PIPBONGWorkflowDryRunSim.exe'
if (-not (Test-Path $sim)) {
    Write-Error "Missing $sim"
}
& $sim
exit $LASTEXITCODE
