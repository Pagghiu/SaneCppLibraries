@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
@REM Remove the trailing slash
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"
set "PROJECTS_DIR=%SCRIPT_DIR%\_Build\Projects"
set "GENERATOR_DIR=%SCRIPT_DIR%\_Build\Generator"
set "SOURCES_DIR=%SCRIPT_DIR%"

call "!SCRIPT_DIR!\Support\Build\Build.bat" "!GENERATOR_DIR!" "!PROJECTS_DIR!" "!SCRIPT_DIR!" "!SOURCES_DIR!"

endlocal


@echo off
setlocal
