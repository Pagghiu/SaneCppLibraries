@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
@REM Remove the trailing slash
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

set "ACTION=configure"
if not "%1" == "" (
    set "ACTION=%1"
)
echo ACTION=%ACTION%
if "%ACTION%" == "build" (
echo "!SCRIPT_DIR!\_Build\Projects\VisualStudio2022\SCTest.sln"
call "!SCRIPT_DIR!\Support\Build\Build.bat" "build" "!SCRIPT_DIR!\_Build\Projects\VisualStudio2022\SCTest.sln" "/p:Configuration=Debug" "/p:Platform=x64"
) else (
set "PROJECTS_DIR=%SCRIPT_DIR%\_Build\Projects"
set "GENERATOR_DIR=%SCRIPT_DIR%\_Build\Generator"
set "SOURCES_DIR=%SCRIPT_DIR%"
call "!SCRIPT_DIR!\Support\Build\Build.bat" "configure" "!GENERATOR_DIR!" "!PROJECTS_DIR!" "!SCRIPT_DIR!" "!SOURCES_DIR!"
)
endlocal


@echo off
setlocal
