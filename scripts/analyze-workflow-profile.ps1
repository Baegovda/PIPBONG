# Summarize PIPBONG workflow run profile logs (Markdown v4/v5 + embedded TSV) for human/AI spike review.
param(
    [string]$Path = "",
    [int]$Top = 20
)

$ErrorActionPreference = "Stop"

function Resolve-ProfilePath {
    param([string]$InputPath)
    if ($InputPath -and (Test-Path -LiteralPath $InputPath)) {
        return (Resolve-Path -LiteralPath $InputPath).Path
    }

    $repoRoot = Split-Path -Parent $PSScriptRoot
    $latestMd = Join-Path $repoRoot "workflow-profile\latest.md"
    if (Test-Path -LiteralPath $latestMd) {
        return $latestMd
    }

    $legacyTxt = Join-Path $repoRoot "workflow-profile\latest.txt"
    if (Test-Path -LiteralPath $legacyTxt) {
        return $legacyTxt
    }

    throw "Profile log not found at $latestMd. Enable profiling, run a feature session, then stop/exit."
}

function Get-FrontMatterValue {
    param([string[]]$Lines, [string]$Key)
    foreach ($line in $Lines) {
        if ($line -match "^$([regex]::Escape($Key)):\s*(.+)$") {
            return $Matches[1].Trim()
        }
        if ($line -eq "---") { break }
    }
    return ""
}

function Get-MarkdownSection {
    param([string[]]$Lines, [string]$Heading)

    $capture = $false
    $section = [System.Collections.Generic.List[string]]::new()
    foreach ($line in $Lines) {
        if ($line -eq $Heading) {
            $capture = $true
            continue
        }
        if ($capture) {
            if ($line -match '^## ') { break }
            $section.Add($line)
        }
    }
    return ,$section.ToArray()
}

function Get-TsvEventLines {
    param([string[]]$Lines)

    $inFence = $false
    $tsvLines = [System.Collections.Generic.List[string]]::new()

    foreach ($line in $Lines) {
        if ($line -match '^```tsv\s*$') {
            $inFence = $true
            continue
        }
        if ($inFence -and $line -match '^```\s*$') {
            break
        }
        if ($inFence) {
            if ($line -match '^rel_us\t') { continue }
            $tsvLines.Add($line)
            continue
        }

        if ($line.StartsWith("#") -or [string]::IsNullOrWhiteSpace($line)) { continue }
        if ($line.StartsWith("---")) { break }
        $tsvLines.Add($line)
    }

    return ,$tsvLines.ToArray()
}

function Parse-ProfileEvents {
    param([string[]]$TsvLines)

    $events = @()
    foreach ($line in $TsvLines) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }

        $parts = $line -split "`t", 5
        if ($parts.Count -lt 4) { continue }

        $dur = -1
        if ($parts.Count -ge 5 -and $parts[4]) {
            [void][int64]::TryParse($parts[4], [ref]$dur)
        }

        $events += [pscustomobject]@{
            RelUs = [int64]$parts[0]
            Thread = $parts[1]
            Event = $parts[2]
            Detail = $parts[3]
            DurUs = $dur
        }
    }
    return $events
}

$resolvedPath = Resolve-ProfilePath -InputPath $Path
$lines = Get-Content -LiteralPath $resolvedPath -Encoding UTF8
$formatVersion = Get-FrontMatterValue -Lines $lines -Key "format_version"
$profilingDepth = Get-FrontMatterValue -Lines $lines -Key "profiling_depth"
$featureName = Get-FrontMatterValue -Lines $lines -Key "feature_name"
$runMode = Get-FrontMatterValue -Lines $lines -Key "run_mode"
$blockCount = Get-FrontMatterValue -Lines $lines -Key "workflow_block_count"
$startSource = Get-FrontMatterValue -Lines $lines -Key "session_start_source"
$tracePath = Join-Path (Split-Path -Parent $resolvedPath) "trace.json"
$tsvLines = Get-TsvEventLines -Lines $lines
$events = Parse-ProfileEvents -TsvLines $tsvLines
$autoDiagnosis = Get-MarkdownSection -Lines $lines -Heading "## Auto diagnosis"

Write-Host "PIPBONG workflow profile analysis"
Write-Host "file: $resolvedPath"
if ($formatVersion) { Write-Host "format_version: $formatVersion" }
if ($profilingDepth) { Write-Host "profiling_depth: $profilingDepth" }
if ($featureName) { Write-Host "feature: $featureName ($runMode)" }
if (Test-Path -LiteralPath $tracePath) { Write-Host "chrome_trace: $tracePath" }
if ($blockCount) { Write-Host "workflow_blocks: $blockCount" }
if ($startSource) { Write-Host "start_source: $startSource" }
Write-Host "events: $($events.Count)"
Write-Host ""

if ($autoDiagnosis.Count -gt 0) {
    Write-Host "=== Auto diagnosis (from log) ==="
    foreach ($line in $autoDiagnosis) {
        if ([string]::IsNullOrWhiteSpace($line)) { continue }
        Write-Host $line
    }
    Write-Host ""
}

$loopEnds = $events | Where-Object { $_.Event -eq "loop_end" -and $_.DurUs -ge 0 }
$uiFlushes = $events | Where-Object { $_.Event -eq "ui_fast_repeat_flush" -and $_.DurUs -ge 0 }
$clicks = $events | Where-Object {
    ($_.Event -eq "mouse_click" -or $_.Event -eq "synthetic_mouse_click") -and $_.DurUs -ge 0
}
$imageFindPolls = $events | Where-Object { $_.Event -eq "imagefind_poll" }
$physicalKeys = $events | Where-Object { $_.Event -eq "user_physical_key" }
$foregroundChanges = $events | Where-Object { $_.Event -eq "foreground_change" }

function Show-Percentiles {
    param([int64[]]$Values, [string]$Label)
    if (-not $Values -or $Values.Count -eq 0) {
        Write-Host "$Label : (no samples)"
        return
    }
    $sorted = $Values | Sort-Object
    $p50 = $sorted[[int]([math]::Floor(($sorted.Count - 1) * 0.50))]
    $p95 = $sorted[[int]([math]::Floor(($sorted.Count - 1) * 0.95))]
    $p99 = $sorted[[int]([math]::Floor(($sorted.Count - 1) * 0.99))]
    $max = $sorted[-1]
    Write-Host ("{0}: count={1} p50={2:N3}ms p95={3:N3}ms p99={4:N3}ms max={5:N3}ms" -f `
        $Label, $sorted.Count, ($p50/1000.0), ($p95/1000.0), ($p99/1000.0), ($max/1000.0))
}

Write-Host "=== Event percentiles ==="
Show-Percentiles -Values @($loopEnds.DurUs) -Label "loop_duration"
Show-Percentiles -Values @($uiFlushes.DurUs) -Label "ui_fast_repeat_flush"
Show-Percentiles -Values @($clicks.DurUs) -Label "synthetic_mouse_click"

if ($imageFindPolls.Count -gt 0) {
    Write-Host "imagefind_poll events: $($imageFindPolls.Count)"
}
if ($physicalKeys.Count -gt 0) {
    Write-Host "user_physical_key events: $($physicalKeys.Count)"
}
if ($foregroundChanges.Count -gt 0) {
    Write-Host "foreground_change events: $($foregroundChanges.Count)"
}

$gapEvents = $events | Where-Object { $_.Event -eq "spike_loop_gap" -and $_.DurUs -ge 0 }
if ($gapEvents.Count -gt 0) {
    Write-Host ""
    Write-Host "Top loop_gap spikes:"
    $gapEvents | Sort-Object DurUs -Descending | Select-Object -First $Top | ForEach-Object {
        Write-Host ("  rel={0:N3}ms gap={1:N3}ms {2}" -f ($_.RelUs/1000.0), ($_.DurUs/1000.0), $_.Detail)
    }
}

$uiSpikes = $events | Where-Object { $_.Event -eq "spike_ui_flush" -and $_.DurUs -ge 0 }
if ($uiSpikes.Count -gt 0) {
    Write-Host ""
    Write-Host "Top UI flush spikes:"
    $uiSpikes | Sort-Object DurUs -Descending | Select-Object -First $Top | ForEach-Object {
        Write-Host ("  rel={0:N3}ms dur={1:N3}ms {2}" -f ($_.RelUs/1000.0), ($_.DurUs/1000.0), $_.Detail)
    }
}

Write-Host ""
Write-Host "Hint: open workflow-profile/latest.md — Feature settings / Workflow blocks / Auto diagnosis need no manual workflow description."
