# Shared helpers for incremental Release builds — stale vcpkg lock / stuck configure recovery.
$ErrorActionPreference = "Stop"

function Clear-StaleVcpkgLock {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $lock = Join-Path $RepoRoot "build\vcpkg_installed\vcpkg\vcpkg-running.lock"
    if (-not (Test-Path $lock)) {
        return $false
    }

    $holding = Get-Process cmake, vcpkg -ErrorAction SilentlyContinue
    if ($holding) {
        return $false
    }

    Remove-Item $lock -Force -ErrorAction SilentlyContinue
    Write-Host "Cleared stale vcpkg-running.lock" -ForegroundColor Yellow
    return $true
}

function Stop-StuckConfigureProcesses {
    param(
        [switch]$IncludeMsbuild
    )

    $names = @("cmake", "vcpkg")
    if ($IncludeMsbuild) {
        $names += "msbuild"
    }

    foreach ($name in $names) {
        Stop-Process -Name $name -Force -ErrorAction SilentlyContinue
    }
}

function Prepare-IncrementalBuildEnvironment {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $lock = Join-Path $RepoRoot "build\vcpkg_installed\vcpkg\vcpkg-running.lock"
    if (Test-Path $lock) {
        Write-Host "vcpkg lock present — stopping stuck cmake/vcpkg (not msbuild)..." -ForegroundColor Yellow
        Stop-StuckConfigureProcesses
        Start-Sleep -Milliseconds 300
        Clear-StaleVcpkgLock -RepoRoot $RepoRoot | Out-Null
    }
}

function Ensure-VcpkgRoot {
    if ($env:VCPKG_ROOT -and (Test-Path (Join-Path $env:VCPKG_ROOT "scripts\buildsystems\vcpkg.cmake"))) {
        return
    }

    $candidates = @(
        "C:\Users\Revaptor_FX\PIP2.0\third_party\vcpkg",
        "C:\vcpkg",
        (Join-Path $env:USERPROFILE "vcpkg"),
        (Join-Path $env:USERPROFILE "source\repos\vcpkg")
    )

    foreach ($root in $candidates) {
        $toolchain = Join-Path $root "scripts\buildsystems\vcpkg.cmake"
        if (Test-Path $toolchain) {
            $env:VCPKG_ROOT = ($root -replace '\\', '/')
            Write-Host "Using VCPKG_ROOT=$($env:VCPKG_ROOT)" -ForegroundColor DarkGray
            return
        }
    }

    Write-Host "VCPKG_ROOT is not set. Set the environment variable or copy CMakeUserPresets.json.example to CMakeUserPresets.json." -ForegroundColor Red
    exit 1
}

function Ensure-BuildTreeConfigured {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $cache = Join-Path $RepoRoot "build\CMakeCache.txt"
    if (Test-Path $cache) {
        return
    }

    Ensure-VcpkgRoot

    Write-Host "Configuring default preset (first time only — build/CMakeCache.txt missing)..."
    cmake --preset default
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
