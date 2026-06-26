@echo off
chcp 65001 >nul
cd /d "%~dp0"
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0scripts\build-release.ps1"
if errorlevel 1 (
    echo.
    echo 빌드 실패
    pause
    exit /b 1
)
echo.
pause
