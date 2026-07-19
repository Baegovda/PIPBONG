# PIPBONG: F5 must run Build and Run task (Start-Process) — never CodeLLDB.
# Bindings are gated on config.pipbong.f5BuildAndRun (set in this repo's .vscode/settings.json)
# so a second Cursor window on another project keeps its own F5 / debug behavior.
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

$whenPipbong = 'config.pipbong.f5BuildAndRun'
$existing = @(Read-KeybindingsArray -Path $keybindingsPath)

$kept = @($existing | Where-Object {
    $w = if ($null -eq $_.when) { '' } else { [string]$_.when }
    if ($w -eq $whenPipbong) { return $false }
    if ($w -like '*Sbm1*' -or $w -like '*pipbong*') { return $false }
    return $true
})

$pipbong = @(
    [pscustomobject]@{ key = 'f5'; command = '-workbench.action.debug.start'; when = $whenPipbong },
    [pscustomobject]@{ key = 'f5'; command = '-workbench.action.debug.selectandstart'; when = $whenPipbong },
    [pscustomobject]@{ key = 'f5'; command = '-workbench.action.debug.run'; when = $whenPipbong },
    [pscustomobject]@{ key = 'ctrl+f5'; command = '-workbench.action.debug.run'; when = $whenPipbong },
    [pscustomobject]@{ key = 'f5'; command = 'workbench.action.tasks.test'; when = $whenPipbong },
    [pscustomobject]@{ key = 'ctrl+shift+b'; command = '-glass.openBrowserTab'; when = $whenPipbong },
    [pscustomobject]@{ key = 'ctrl+shift+b'; command = 'workbench.action.tasks.build'; when = $whenPipbong }
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
Write-Host "  F5 -> tasks.test ONLY when config.pipbong.f5BuildAndRun (this workspace)" -ForegroundColor Cyan
Write-Host "  Other Cursor windows are unchanged — dual-project safe" -ForegroundColor Cyan
Write-Host "REQUIRED once: Command Palette -> Developer: Reload Window" -ForegroundColor Yellow
Write-Host "PIPBONG window F5: terminal must print 'Started PIPBONG.exe'." -ForegroundColor Yellow
