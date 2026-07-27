# Tail the always-on PIPBONG live session log (응답없음 / force-kill analysis).
param(
    [int] $Tail = 200
)

$repoRoot = Split-Path -Parent $PSScriptRoot
$paths = @(
    (Join-Path $repoRoot "live-session\latest.log")
    "$env:LOCALAPPDATA\PIPBONG\PIPBONG\live-session\latest.log"
)

foreach ($p in $paths) {
    if (Test-Path -LiteralPath $p) {
        Write-Host "=== $p ===" -ForegroundColor Cyan
        Get-Content -LiteralPath $p -Tail $Tail -Encoding UTF8
        exit 0
    }
}

Write-Host "No live-session/latest.log found (run PIPBONG from build/Release first)." -ForegroundColor Yellow
exit 1
