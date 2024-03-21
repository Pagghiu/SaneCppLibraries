@echo off
setlocal enabledelayedexpansion
set "SCRIPT_DIR=%~dp0"
@REM Remove the trailing slash
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
call "!SCRIPT_DIR!\Tools\Tools.bat" "%SCRIPT_DIR%" "%SCRIPT_DIR%\Tools" "%SCRIPT_DIR%\_Build" %*
endlocal
