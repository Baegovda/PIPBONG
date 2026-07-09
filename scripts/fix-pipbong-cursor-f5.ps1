# PIPBONG: F5 must run Build and Run task (Start-Process) — never CodeLLDB.
#
# Root cause of blank-terminal hang: F5 fell through to Debug: Start Debugging
# because when-clauses like `workspaceFolder =~ /Sbm1\.0/i` often do NOT match in
# Cursor, so unbind+rebind were ignored. Fix: unconditional F5 -> tasks.test, plus
# config.pipbong.f5BuildAndRun gated bindings as a second layer.
$ErrorActionPreference = "Stop"

$cursorUser = Join-Path $env:APPDATA "Cursor\User"
$keybindingsPath = Join-Path $cursorUser "keybindings.json"

if (-not (Test-Path $cursorUser)) {
    Write-Host "Cursor user folder not found: $cursorUser" -ForegroundColor Yellow
    exit 1
}

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

$existing = @(Read-KeybindingsArray -Path $keybindingsPath)

$kept = @($existing | Where-Object {
    $k = [string]$_.key
    if ($k -in @('f5', 'ctrl+f5')) { return $false }
    if ($k -eq 'ctrl+shift+b') {
        $w = if ($null -eq $_.when) { '' } else { [string]$_.when }
        $c = [string]$_.command
        if ($w -like '*Sbm1*' -or $w -like '*pipbong*' -or
            $c -eq '-glass.openBrowserTab' -or $c -eq 'workbench.action.tasks.build') {
            return $false
        }
    }
    return $true
})

# Layer 1: unconditional (always wins over default F5 debug).
# Layer 2: config.pipbong.f5BuildAndRun (set in .vscode/settings.json) for clarity.
$pipbong = @(
    [pscustomobject]@{ key = 'f5'; command = '-workbench.action.debug.start' },
    [pscustomobject]@{ key = 'f5'; command = '-workbench.action.debug.selectandstart' },
    [pscustomobject]@{ key = 'f5'; command = '-workbench.action.debug.run' },
    [pscustomobject]@{ key = 'ctrl+f5'; command = '-workbench.action.debug.run' },
    [pscustomobject]@{ key = 'f5'; command = 'workbench.action.tasks.test' },
    [pscustomobject]@{ key = 'f5'; command = '-workbench.action.debug.start'; when = 'config.pipbong.f5BuildAndRun' },
    [pscustomobject]@{ key = 'f5'; command = 'workbench.action.tasks.test'; when = 'config.pipbong.f5BuildAndRun' },
    [pscustomobject]@{ key = 'ctrl+shift+b'; command = '-glass.openBrowserTab'; when = 'config.pipbong.f5BuildAndRun' },
    [pscustomobject]@{ key = 'ctrl+shift+b'; command = 'workbench.action.tasks.build'; when = 'config.pipbong.f5BuildAndRun' }
)

$merged = @($kept) + @($pipbong)

$sb = [System.Text.StringBuilder]::new()
[void]$sb.AppendLine('[')
for ($i = 0; $i -lt $merged.Count; $i++) {
    $b = $merged[$i]
    [void]$sb.AppendLine('  {')
    [void]$sb.AppendLine(('    "key": "{0}",' -f $b.key))
    $cmdEsc = ([string]$b.command).Replace('\', '\\').Replace('"', '\"')
    if ($null -ne $b.when -and [string]$b.when -ne '') {
        [void]$sb.AppendLine(('    "command": "{0}",' -f $cmdEsc))
        $whenJson = ([string]$b.when).Replace('\', '\\').Replace('"', '\"')
        [void]$sb.AppendLine(('    "when": "{0}"' -f $whenJson))
    } else {
        [void]$sb.AppendLine(('    "command": "{0}"' -f $cmdEsc))
    }
    if ($i -lt $merged.Count - 1) {
        [void]$sb.AppendLine('  },')
    } else {
        [void]$sb.AppendLine('  }')
    }
}
[void]$sb.AppendLine(']')

[System.IO.File]::WriteAllText($keybindingsPath, $sb.ToString(), [System.Text.UTF8Encoding]::new($false))

Write-Host "Updated Cursor keybindings ($keybindingsPath)" -ForegroundColor Green
Write-Host "  F5 -> workbench.action.tasks.test (NO debugger, unconditional)" -ForegroundColor Cyan
Write-Host "  launch.json has no debug configs — F5 cannot attach CodeLLDB" -ForegroundColor Cyan
Write-Host "REQUIRED once: Command Palette -> Developer: Reload Window" -ForegroundColor Yellow
Write-Host "Then F5: terminal must print 'Started PIPBONG.exe' (not a blank pane)." -ForegroundColor Yellow
