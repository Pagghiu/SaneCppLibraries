@echo off
setlocal enabledelayedexpansion enableextensions

set "SCRIPT_DIR=%~dp0"
@REM Remove the trailing slash
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

@REM Change to project directory
cd /d "%SCRIPT_DIR%"

@REM Set up MSVC environment
set "vswhere_path=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere_path%" (
    echo Error: Visual Studio Locator not found.
    exit /b 1
)

set vcvarsall_path=""
for /f "usebackq delims=" %%i in (`"%vswhere_path%" -latest -property installationPath`) do (
        set "vcvarsall_path=%%i\VC\Auxiliary\Build\vcvarsall.bat"
    )
)

set __VSCMD_ARG_NO_LOGO=1
set VSCMD_SKIP_SENDTELEMETRY=1
set VCPKG_KEEP_ENV_VARS=VSCMD_SKIP_SENDTELEMETRY
set vcvarsall_called=0
if exist "%vcvarsall_path%" (
    call "%vcvarsall_path%" x86_amd64
    set vcvarsall_called=1
) else (
    echo Error: vcvarsall.bat not found.
    exit /b 1
)

set "BOOTSTRAP_EXE=%SCRIPT_DIR%\_Build\_Tools\Windows\ToolsBootstrap.exe"

@REM Create platform Tools directory if needed
mkdir "%SCRIPT_DIR%\_Build\_Tools\Windows" 2>nul

@REM Check if ToolsBootstrap needs to be built
if not exist "%BOOTSTRAP_EXE%" (
    set build_bootstrap=1
) else (
    for %%f in ("%SCRIPT_DIR%\Tools\ToolsBootstrap.cpp") do set source_time=%%~tf
    for %%f in ("%BOOTSTRAP_EXE%") do set exe_time=%%~tf
    if "!source_time!" gtr "!exe_time!" (set build_bootstrap=1) else (set build_bootstrap=0)
)

if !build_bootstrap! equ 1 (
    set SRC_FILE=!SCRIPT_DIR!\Tools\ToolsBootstrap.cpp
    set OBJ_FILE=!SCRIPT_DIR!\_Build\_Tools\Windows\ToolsBootstrap.obj
    cl.exe /nologo /std:c++14 /MTd /Fo"!OBJ_FILE!" /c "!SRC_FILE!" 2>&1
    if !errorlevel! neq 0 (
        echo Failed to build ToolsBootstrap
        exit /b 1
    )
    link /nologo /OUT:"!BOOTSTRAP_EXE!" "!OBJ_FILE!" Shell32.lib 2>&1
    if !errorlevel! neq 0 (
        echo Failed to link ToolsBootstrap
        exit /b 1
    )
)

@REM Execute ToolsBootstrap with original args
"%BOOTSTRAP_EXE%" "%SCRIPT_DIR%" "%SCRIPT_DIR%\Tools" "%SCRIPT_DIR%\_Build" %*

endlocal
