@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "POWERSHELL_SCRIPT=%SCRIPT_DIR%\SC-build.ps1"

if not exist "%POWERSHELL_SCRIPT%" (
    echo Error: "%POWERSHELL_SCRIPT%" not found.
    exit /b 1
)

powershell -NoProfile -ExecutionPolicy Bypass -File "%POWERSHELL_SCRIPT%" %*
exit /b %errorlevel%
