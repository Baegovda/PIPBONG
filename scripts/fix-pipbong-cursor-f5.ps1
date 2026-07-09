# PIPBONG workspace: F5 = build+run task (no debugger). Ctrl+Shift+B = build only.
# CodeLLDB attach on F5 was multi-second delay after Build Release finished; dist exe is fast.
$ErrorActionPreference = "Stop"

$whenWorkspace = "workspaceFolder =~ /Sbm1\.0/i"

$cursorUser = Join-Path $env:APPDATA "Cursor\User"
$keybindingsPath = Join-Path $cursorUser "keybindings.json"

if (-not (Test-Path $cursorUser)) {
    Write-Host "Cursor user folder not found: $cursorUser" -ForegroundColor Yellow
    exit 1
}

$desired = @(
  @{ key = "f5"; command = "-workbench.action.debug.start"; when = $whenWorkspace },
  @{ key = "f5"; command = "-workbench.action.debug.selectandstart"; when = $whenWorkspace },
  @{
    key     = "f5"
    command = "workbench.action.tasks.runTask"
    when    = $whenWorkspace
    args    = "Build and Run PIPBONG (Release)"
  },
  @{ key = "ctrl+shift+b"; command = "-glass.openBrowserTab"; when = $whenWorkspace },
  @{ key = "ctrl+shift+b"; command = "workbench.action.tasks.build"; when = $whenWorkspace }
)

function Read-KeybindingsArray {
    param([string]$Path)
    if (-not (Test-Path $Path)) { return @() }
    $raw = Get-Content -Raw -Encoding UTF8 $Path
    if ([string]::IsNullOrWhiteSpace($raw)) { return @() }
    $lines = $raw -split "`n" | ForEach-Object {
        if ($_ -match '^\s*//') { return $null }
        $idx = $_.IndexOf("//")
        if ($idx -ge 0) { $_.Substring(0, $idx).TrimEnd() } else { $_ }
    } | Where-Object { $_ -ne $null }
    $json = ($lines -join "`n").Trim()
    if (-not $json) { return @() }
    if ($json[0] -ne '[') { $json = "[$json]" }
    return @((ConvertFrom-Json $json))
}

function Binding-Exists {
    param($List, $Key, $Command, $When)
    foreach ($b in $List) {
        if ($b.key -eq $Key -and $b.command -eq $Command -and $b.when -eq $When) {
            return $true
        }
    }
    return $false
}

$existing = @(Read-KeybindingsArray -Path $keybindingsPath)
# Remove old PIPBONG F5/Ctrl+F5/Ctrl+Shift+B overrides for this workspace only.
$existing = @($existing | Where-Object {
    -not (($_.key -in @('f5', 'ctrl+f5', 'ctrl+shift+b')) -and ($null -ne $_.when -and $_.when -like "*Sbm1.0*"))
})

foreach ($entry in $desired) {
    if (-not (Binding-Exists -List $existing -Key $entry.key -Command $entry.command -When $entry.when)) {
        $obj = [ordered]@{
            key     = $entry.key
            command = $entry.command
            when    = $entry.when
        }
        if ($entry.ContainsKey('args') -and $null -ne $entry.args) {
            $obj.args = $entry.args
        }
        $existing += [pscustomobject]$obj
    }
}

$out = ($existing | ConvertTo-Json -Depth 4)
Set-Content -Path $keybindingsPath -Value $out -Encoding UTF8

Write-Host "Updated Cursor keybindings ($keybindingsPath)" -ForegroundColor Green
Write-Host "  F5 -> task Build and Run PIPBONG (Release) (no debugger; Start-Process)" -ForegroundColor Cyan
Write-Host "  Ctrl+Shift+B -> Build Release (glass browser disabled in Sbm1.0)" -ForegroundColor Cyan
Write-Host "  Optional debug: Run and Debug -> Debug PIPBONG (Release) (CodeLLDB)" -ForegroundColor Cyan
Write-Host "  Reload Window, then F5." -ForegroundColor Cyan
