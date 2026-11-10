$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'build-common.ps1')
foreach($case in @(@(32,67108864,8),@(2,2097152,1),@(16,3145728,2),@(0,0,1))) {
    if((Get-SafeMsvcCompileCount -LogicalProcessors $case[0] -FreeMemoryKiB $case[1]) -ne $case[2]){throw 'CPU/memory budget mismatch'}
}
$priorCl=[Environment]::GetEnvironmentVariable('CL','Process')
$priorTail=[Environment]::GetEnvironmentVariable('_CL_','Process')
$script:resourceReads=0
function Get-CimInstance {
    [CmdletBinding()]param([string]$ClassName)
    if($ClassName -ne 'Win32_OperatingSystem'){throw 'Unexpected resource query'}
    ++$script:resourceReads
    [pscustomobject]@{FreePhysicalMemory=524288}
}
try {
    $env:CL='/DKEEP=1';$env:_CL_=$null
    Enable-BoundedMsvcCompilation
    if($env:CL -ne '/DKEEP=1 /MP1 /FS' -or $script:resourceReads -ne 1){throw 'Low-memory auto budget or existing flag retention failed'}
    foreach($options in @('/MP3 /DKEEP=1','/showIncludes','/Ycpch.h','/EP','/E','/Gm','/link /DEBUG')) {
        $env:CL=$options;Enable-BoundedMsvcCompilation
        if($env:CL -ne $options -or $script:resourceReads -ne 1){throw 'Explicit/incompatible option was changed'}
    }
    $env:CL='/DKEEP=1';$env:_CL_='/MP4';Enable-BoundedMsvcCompilation
    if($env:CL -ne '/DKEEP=1' -or $env:_CL_ -ne '/MP4' -or $script:resourceReads -ne 1){throw 'Explicit tail option was changed'}
}finally{
    [Environment]::SetEnvironmentVariable('CL',$priorCl,'Process')
    [Environment]::SetEnvironmentVariable('_CL_',$priorTail,'Process')
}
Write-Host 'Build parallelism: CPU/memory cap, low-memory fallback, explicit flags and incompatible modes preserved OK'
