# Invoked from CMake POST_BUILD after PIPBONG links (optional; skip with PIPBONG_SKIP_WORKFLOW_DRY_RUN=1).
param(
    [Parameter(Mandatory = $true)]
    [string]$SimExe
)

$ErrorActionPreference = 'Stop'

if ($env:PIPBONG_SKIP_WORKFLOW_DRY_RUN -eq '1') {
    Write-Host 'Skipped workflow dry-run sim (PIPBONG_SKIP_WORKFLOW_DRY_RUN=1)' -ForegroundColor Yellow
    exit 0
}

if (-not (Test-Path -LiteralPath $SimExe)) {
    Write-Error "PIPBONGWorkflowDryRunSim.exe not found: $SimExe"
}

Write-Host 'Running PIPBONGWorkflowDryRunSim...'
& $SimExe
if ($LASTEXITCODE -ne 0) {
    Write-Host 'Workflow dry-run sim failed — fix WorkflowRunner scenarios before shipping.' -ForegroundColor Red
    exit $LASTEXITCODE
}

Write-Host 'Workflow dry-run sim OK' -ForegroundColor Green
exit 0
