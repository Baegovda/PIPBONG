$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot
$releaseSource = Get-Content -Raw -LiteralPath (Join-Path $PSScriptRoot 'create-github-release.ps1')
$tail = $releaseSource.Substring($releaseSource.IndexOf('$releaseRepo ='))
$releaseSteps = [scriptblock]::Create($tail)
$zipPath = Join-Path $repoRoot ("build/release-policy-" + [guid]::NewGuid().ToString('N') + '.zip')
[IO.File]::WriteAllBytes($zipPath, [byte[]](1,2,3))
$tag = 'v-test'
$Notes = 'test'
function gh {
    $script:taskCalls += ($args -join ' ')
    if ($args[0] -ne 'release' -or $args[1] -notin @('create','view')) {
        throw 'Unexpected GitHub mutation in release policy test.'
    }
    if ($args[1] -eq 'create' -and $script:taskScenario -eq 'create-failure') {
        $global:LASTEXITCODE = 1
        return
    }
    if ($args[1] -eq 'view' -and $script:taskScenario -eq 'verify-failure') {
        $global:LASTEXITCODE = 1
        return
    }
    $global:LASTEXITCODE = 0
    if ($args[1] -eq 'view') { '{"assets":[{"name":"PIPBONG-win64.zip","size":3}]}' }
}
try {
    foreach ($script:taskScenario in @('create-failure','verify-failure','success')) {
        $script:taskCalls = @()
        $threw = $false
        try { & $releaseSteps } catch { $threw = $true }
        if ($threw -ne ($script:taskScenario -ne 'success')) {
            throw "Wrong outcome for $script:taskScenario"
        }
        $expectedCalls = if ($script:taskScenario -eq 'create-failure') { 1 } else { 2 }
        if ($script:taskCalls.Count -ne $expectedCalls) { throw 'Release continued after failure.' }
    }
    Write-Host 'Release policy: create failure / verification failure / success OK (mock GitHub only)'
} finally {
    Remove-Item -LiteralPath $zipPath -ErrorAction SilentlyContinue
}
exit 0
