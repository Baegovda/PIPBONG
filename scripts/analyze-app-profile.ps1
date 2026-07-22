# Summarize PIPBONG app-profile/latest.md (Markdown v5) for AI / human review.
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
    foreach ($rel in @("app-profile\latest.md", "workflow-profile\latest.md")) {
        $candidate = Join-Path $repoRoot $rel
        if (Test-Path -LiteralPath $candidate) {
            return $candidate
        }
    }

    throw "Profile log not found at app-profile\latest.md. Enable profiling, reproduce, then exit PIPBONG."
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
$endReason = Get-FrontMatterValue -Lines $lines -Key "end_reason"
$tracePath = Join-Path (Split-Path -Parent $resolvedPath) "trace.json"
$tsvLines = Get-TsvEventLines -Lines $lines
$events = Parse-ProfileEvents -TsvLines $tsvLines
$autoDiagnosis = Get-MarkdownSection -Lines $lines -Heading "## Auto diagnosis"

Write-Host "PIPBONG app profile"
Write-Host "file: $resolvedPath"
if ($formatVersion) { Write-Host "format_version: $formatVersion" }
if ($profilingDepth) { Write-Host "profiling_depth: $profilingDepth" }
if ($featureName) { Write-Host "feature: $featureName ($runMode)" }
if ($endReason) { Write-Host "end_reason: $endReason" }
if (Test-Path -LiteralPath $tracePath) { Write-Host "chrome_trace: $tracePath" }
if ($blockCount) { Write-Host "workflow_blocks: $blockCount" }
if ($startSource) { Write-Host "start_source: $startSource" }
Write-Host "events: $($events.Count)"
Write-Host ""

if ($autoDiagnosis.Count -gt 0) {
    Write-Host "=== Auto diagnosis ==="
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

function Show-Percentiles {
    param([int64[]]$Values, [string]$Label)
    if (-not $Values -or $Values.Count -eq 0) {
        Write-Host "$Label : (no samples)"
        return
    }
    $sorted = $Values | Sort-Object
    $p50 = $sorted[[int]([math]::Floor(($sorted.Count - 1) * 0.50))]
    $p95 = $sorted[[int]([math]::Floor(($sorted.Count - 1) * 0.95))]
    $max = $sorted[-1]
    Write-Host ("{0}: count={1} p50={2:N3}ms p95={3:N3}ms max={4:N3}ms" -f `
        $Label, $sorted.Count, ($p50/1000.0), ($p95/1000.0), ($max/1000.0))
}

Write-Host "=== Event percentiles ==="
Show-Percentiles -Values @($loopEnds.DurUs) -Label "loop_duration"
Show-Percentiles -Values @($uiFlushes.DurUs) -Label "ui_fast_repeat_flush"
Show-Percentiles -Values @($clicks.DurUs) -Label "synthetic_mouse_click"

Write-Host ""
Write-Host "AI: read app-profile/latest.md — Auto diagnosis + snapshot tables."
