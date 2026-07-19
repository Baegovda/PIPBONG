# Build PIPBONGPolicySim and run regression checks (manual or CI).
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
if ([string]::IsNullOrWhiteSpace($ReportPath)) {
    $ReportPath = Join-Path $buildDir 'policy-sim-report.txt'
}

& (Join-Path $repoRoot 'scripts\run-policy-sim-postbuild.ps1') -SimExe $sim -ReportPath $ReportPath
exit $LASTEXITCODE
