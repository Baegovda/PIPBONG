# Builds a staged Release folder and ZIP for distribution (dynamic DLL layout).
param(
    [switch]$SkipBuild
)

$ErrorActionPreference = "Stop"
$repoRoot = Split-Path -Parent $PSScriptRoot
Set-Location $repoRoot

function Get-ProjectVersion {
    $cmakeVersionLine = Select-String -Path (Join-Path $repoRoot "CMakeLists.txt") -Pattern 'project\(PIPBONG VERSION ' | Select-Object -First 1
    if (-not $cmakeVersionLine) {
        throw "Could not read project version from CMakeLists.txt"
    }
    return ($cmakeVersionLine.Line -replace '.*VERSION\s+([0-9.]+).*', '$1').Trim()
}

function Remove-ReleaseJunk {
    param([string]$StageRoot)

    $unusedDllPatterns = @(
        'libpq*.dll',
        'libecpg*.dll',
        'libpgtypes*.dll',
        'libecpg_compat*.dll',
        'dbus-1*.dll',
        'opencv_calib3d4.dll',
        'opencv_dnn4.dll',
        'opencv_features2d4.dll',
        'opencv_flann4.dll',
        'opencv_highgui4.dll',
        'opencv_ml4.dll',
        'opencv_objdetect4.dll',
        'opencv_photo4.dll',
        'opencv_stitching4.dll',
        'opencv_video4.dll',
        'opencv_videoio4.dll',
        'libprotobuf*.dll',
        'libprotoc*.dll',
        'abseil_dll.dll',
        'Qt6DBus.dll',
        'Qt6OpenGL.dll',
        'Qt6OpenGLWidgets.dll',
        'Qt6PrintSupport.dll',
        'Qt6Sql.dll',
        'Qt6Test.dll',
        'Qt6Xml.dll',
        'Qt6Concurrent.dll',
        'harfbuzz-gpu.dll',
        'harfbuzz-raster.dll',
        'harfbuzz-subset.dll',
        'harfbuzz-vector.dll',
        'icuio*.dll',
        'icutu*.dll',
        'pcre2-32.dll',
        'pcre2-8.dll',
        'pcre2-posix.dll',
        'legacy.dll',
        'brotlienc.dll',
        'libssl-3-x64.dll',
        'turbojpeg.dll',
        'sqlite3.dll',
        'libexpat.dll',
        'md4c-html.dll',
        'lz4.dll'
    )

    foreach ($pattern in $unusedDllPatterns) {
        Get-ChildItem -Path $StageRoot -Filter $pattern -File -ErrorAction SilentlyContinue |
            Remove-Item -Force -ErrorAction SilentlyContinue
    }

    $translationsDir = Join-Path $StageRoot "translations"
    if (Test-Path $translationsDir) {
        $hasTranslation = Get-ChildItem -Path $translationsDir -File -ErrorAction SilentlyContinue
        if (-not $hasTranslation) {
            Remove-Item $translationsDir -Recurse -Force -ErrorAction SilentlyContinue
        }
    }
}

function Write-ReleaseReadme {
    param(
        [string]$StageRoot,
        [string]$Version
    )

    $readme = @"
PIPBONG v$Version
=================

Windows 64비트 포터블 배포판 (PoE2 자동화 유틸리티)

시작하기
--------
1. PIPBONG 폴더 전체를 원하는 위치에 압축 해제합니다 (예: 바탕화면\PIPBONG).
2. PIPBONG.exe 또는 "PIPBONG 실행.bat"을 실행합니다.

필요 환경
---------
- Windows 10 또는 11 (64비트)

폴더 구성
---------
PIPBONG.exe             메인 프로그램
PIPBONGUpdater.exe      앱 내 업데이트용 (삭제하지 마세요)
PIPBONG 실행.bat        실행 바로가기
VERSION.txt             빌드 버전
README.txt              이 안내 파일
platforms/              Qt 플랫폼 플러그인 (필수)
imageformats/           Qt 이미지 플러그인 (필수)
styles/                 Qt 스타일 플러그인 (필수)
tls/                    Qt TLS 백엔드 (필수)
generic/                Qt 플러그인 (필수)
networkinformation/     Qt 네트워크 플러그인 (필수)
*.dll                   런타임 라이브러리 (exe와 같은 폴더에 유지)

업데이트
--------
프로그램 **파일 → 업데이트** 메뉴를 사용하세요.
PIPBONG.exe만 다른 폴더로 옮기지 마세요. DLL과 plugins 폴더가 함께 있어야 합니다.

링크
----
소스:   https://github.com/Baegovda/PIPBONG
릴리즈: https://github.com/Baegovda/PIPBONG-releases
"@

    Set-Content -Path (Join-Path $StageRoot "README.txt") -Value $readme -Encoding UTF8
}

function Write-ReleaseLauncher {
    param([string]$StageRoot)

    $launcher = @'
@echo off
cd /d "%~dp0"
start "" "PIPBONG.exe"
'@

    $launcherName = 'PIPBONG ' + [char]0xC2E4 + [char]0xD589 + '.bat'
    $launcherPath = Join-Path $StageRoot $launcherName
    [System.IO.File]::WriteAllText($launcherPath, $launcher, [System.Text.Encoding]::ASCII)
}

$version = Get-ProjectVersion

if (-not $SkipBuild) {
    Write-Host "Building Release v$version..."
    & (Join-Path $repoRoot "scripts\build-release.ps1")
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

Write-Host "Deploying runtime DLLs..."
& (Join-Path $repoRoot "scripts\deploy-qt.ps1")
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

$releaseDir = Join-Path $repoRoot "build\Release"
$mainExe = Join-Path $releaseDir "PIPBONG.exe"
$updaterExe = Join-Path $releaseDir "PIPBONGUpdater.exe"
if (-not (Test-Path $mainExe)) {
    throw "Missing $mainExe"
}
if (-not (Test-Path $updaterExe)) {
    throw "Missing $updaterExe"
}

$distDir = Join-Path $repoRoot "dist"
$stageRoot = Join-Path $distDir "PIPBONG"
if (Test-Path $stageRoot) {
    Remove-Item $stageRoot -Recurse -Force
}
New-Item -ItemType Directory -Force -Path $stageRoot | Out-Null

Write-Host "Staging release folder..."
robocopy $releaseDir $stageRoot /E /NFL /NDL /NJH /NJS /NC /NS /NP | Out-Null
if ($LASTEXITCODE -ge 8) {
    throw "robocopy failed with exit code $LASTEXITCODE"
}

Remove-ReleaseJunk -StageRoot $stageRoot
Write-ReleaseReadme -StageRoot $stageRoot -Version $version
Write-ReleaseLauncher -StageRoot $stageRoot
Set-Content -Path (Join-Path $stageRoot "VERSION.txt") -Value $version -Encoding ASCII -NoNewline

$zipName = "PIPBONG-win64.zip"
$zipPath = Join-Path $distDir $zipName
if (Test-Path $zipPath) {
    Remove-Item $zipPath -Force
}

Write-Host "Creating $zipPath..."
Compress-Archive -Path $stageRoot -DestinationPath $zipPath -CompressionLevel Optimal

$dllCount = (Get-ChildItem $stageRoot -Filter "*.dll" -File).Count
Write-Host "OK: $zipPath ($dllCount runtime DLLs, root folder PIPBONG/)" -ForegroundColor Green
Write-Host "Run from: $stageRoot\PIPBONG.exe" -ForegroundColor Green
exit 0
