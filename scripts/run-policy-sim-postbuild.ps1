# Invoked from CMake POST_BUILD after PIPBONG links (and from run-policy-sim.ps1).
param(
    [Parameter(Mandatory = $true)]
    [string]$SimExe,

    [Parameter(Mandatory = $true)]
    [string]$ReportPath
)

$ErrorActionPreference = 'Stop'

if ($env:PIPBONG_SKIP_POLICY_SIM -eq '1') {
    Write-Host 'Skipped policy sim (PIPBONG_SKIP_POLICY_SIM=1)' -ForegroundColor Yellow
    exit 0
}

if (-not (Test-Path -LiteralPath $SimExe)) {
    Write-Error "PIPBONGPolicySim.exe not found: $SimExe"
}

Write-Host 'Running SessionRunPolicySim...'
& $SimExe --report=$ReportPath
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Policy sim failed — fix SessionRunPolicy or scenarios before shipping.' -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host "Policy sim OK (report: $ReportPath)" -ForegroundColor Green
exit 0
