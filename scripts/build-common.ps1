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

function Wait-ForRepoVcpkgLock {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot,
        [int]$TimeoutSeconds = 120
    )

    $lock = Join-Path $RepoRoot "build\vcpkg_installed\vcpkg\vcpkg-running.lock"
    if (-not (Test-Path $lock)) {
        return $true
    }

    if (Clear-StaleVcpkgLock -RepoRoot $RepoRoot) {
        return $true
    }

    $deadline = (Get-Date).AddSeconds($TimeoutSeconds)
    Write-Host "PIPBONG vcpkg lock present — waiting up to ${TimeoutSeconds}s (will not kill other projects' cmake/vcpkg)..." -ForegroundColor Yellow

    while ((Get-Date) -lt $deadline) {
        Start-Sleep -Seconds 2
        if (-not (Test-Path $lock)) {
            return $true
        }
        if (Clear-StaleVcpkgLock -RepoRoot $RepoRoot) {
            return $true
        }
    }

    Write-Host "Timed out waiting for vcpkg lock. Let the other project's configure finish, then retry." -ForegroundColor Red
    Write-Host "Nuclear recovery (kills global cmake/vcpkg/msbuild): .\scripts\recover-ide-build.ps1 -KillGlobalBuildProcesses" -ForegroundColor Yellow
    return $false
}

function Prepare-IncrementalBuildEnvironment {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $lock = Join-Path $RepoRoot "build\vcpkg_installed\vcpkg\vcpkg-running.lock"
    if (Test-Path $lock) {
        if (-not (Wait-ForRepoVcpkgLock -RepoRoot $RepoRoot)) {
            exit 1
        }
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

function Test-CMakeCachePathMismatch {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $cache = Join-Path $RepoRoot "build\CMakeCache.txt"
    if (-not (Test-Path $cache)) {
        return $false
    }

    $normalizedRoot = ($RepoRoot -replace '\\', '/')
    foreach ($line in Get-Content $cache) {
        if ($line -match '^PIPBONG_SOURCE_DIR:STATIC=(.+)$') {
            $cachedSource = $Matches[1].Trim()
            return $cachedSource -ne $normalizedRoot
        }
    }

    return $false
}

function Clear-StaleCMakeBuildTree {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    $buildDir = Join-Path $RepoRoot "build"
    if (-not (Test-Path $buildDir)) {
        return
    }

    Write-Host "CMake cache points at a different source path (folder moved or renamed) — removing build/ for a clean configure..." -ForegroundColor Yellow
    Remove-Item $buildDir -Recurse -Force
}

function Ensure-BuildTreeConfigured {
    param(
        [Parameter(Mandatory = $true)]
        [string]$RepoRoot
    )

    if (Test-CMakeCachePathMismatch -RepoRoot $RepoRoot) {
        Clear-StaleCMakeBuildTree -RepoRoot $RepoRoot
    }

    $cache = Join-Path $RepoRoot "build\CMakeCache.txt"
    if (Test-Path $cache) {
        return
    }

    Ensure-VcpkgRoot

    if (-not (Wait-ForRepoVcpkgLock -RepoRoot $RepoRoot -TimeoutSeconds 300)) {
        exit 1
    }

    Write-Host "Configuring default preset (first time only — build/CMakeCache.txt missing)..."
    cmake --preset default
    if ($LASTEXITCODE -ne 0) {
        exit $LASTEXITCODE
    }
}
