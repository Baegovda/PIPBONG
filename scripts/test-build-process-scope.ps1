$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'build-common.ps1')
$taskRepo = Split-Path -Parent $PSScriptRoot
$taskExe = Join-Path $taskRepo 'build\Release\PIPBONG.exe'
$script:taskStopped = @()
$script:taskRefuse = $false
function Get-Process {
    [CmdletBinding()] param([string]$Name)
    if ($Name -ne 'PIPBONG') { throw 'Unexpected process enumeration' }
    $script:taskProcesses
}
function Stop-Process {
    [CmdletBinding()] param([object]$InputObject, [switch]$Force)
    if (-not $Force) { throw 'Expected explicit noninteractive termination' }
    if ($script:taskRefuse) { throw 'simulated termination failure' }
    $script:taskStopped += $InputObject.Id
}
$taskUnknown = [pscustomobject]@{Id=7}
$taskUnknown | Add-Member -MemberType ScriptProperty -Name Path -Value { throw 'simulated access denied' }
$script:taskProcesses = @(
    [pscustomobject]@{Id=1;Path=$taskExe},
    [pscustomobject]@{Id=2;Path=$taskExe.ToUpperInvariant()},
    [pscustomobject]@{Id=3;Path=(Join-Path $taskRepo '..\another-checkout\build\Release\PIPBONG.exe')},
    [pscustomobject]@{Id=4;Path=(Join-Path $taskRepo 'dist\PIPBONG\PIPBONG.exe')},
    [pscustomobject]@{Id=5;Path=($taskExe + '-another')},
    [pscustomobject]@{Id=6;Path=$null},
    $taskUnknown,
    [pscustomobject]@{Id=8;Path=(Join-Path $taskRepo 'build\Release\..\Release\PIPBONG.exe')}
)
Stop-RepoReleaseProcess -RepoRoot $taskRepo
if (($script:taskStopped -join ',') -ne '1,2,8') { throw 'Incorrect executable termination scope' }
$script:taskProcesses = @();$script:taskStopped = @()
Stop-RepoReleaseProcess -RepoRoot $taskRepo
if ($script:taskStopped.Count -ne 0) { throw 'Empty snapshot stopped a process' }
$script:taskProcesses = @([pscustomobject]@{Id=1;Path=$taskExe});$script:taskRefuse = $true
$taskFailed = $false
try { Stop-RepoReleaseProcess -RepoRoot $taskRepo } catch { $taskFailed = $true }
if (-not $taskFailed) { throw 'Termination failure was suppressed' }
foreach ($taskScript in @('build-common.ps1','build-release.ps1','deploy-qt.ps1')) {
    $taskErrors = $null;$taskTokens = $null
    [void][Management.Automation.Language.Parser]::ParseFile((Join-Path $PSScriptRoot $taskScript),[ref]$taskTokens,[ref]$taskErrors)
    if ($taskErrors) { throw "Parse failure: $taskScript" }
}
Write-Host 'Build process scope: exact normalized/case-insensitive executable only; other checkout/portable/prefix/unknown paths retained, empty snapshot and stop failure OK (mock process APIs; no processes terminated)'
