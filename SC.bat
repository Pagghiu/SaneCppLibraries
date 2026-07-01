@echo off
setlocal enabledelayedexpansion enableextensions

set "SCRIPT_DIR=%~dp0"
@REM Remove the trailing slash
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

@REM CMD cannot use a UNC current directory. pushd assigns one a temporary drive letter.
set "BOOTSTRAP_PUSHED_UNC=0"
if "%SCRIPT_DIR:~0,2%"=="\\" (
    pushd "%SCRIPT_DIR%"
    if !errorlevel! neq 0 (
        echo Failed to map UNC bootstrap directory
        set "BOOTSTRAP_EXIT=1"
        goto bootstrap_end
    )
    set "BOOTSTRAP_PUSHED_UNC=1"
) else (
    cd /d "%SCRIPT_DIR%"
)

@REM Set up MSVC environment
set "vswhere_path=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%vswhere_path%" (
    echo Error: Visual Studio Locator not found.
    set "BOOTSTRAP_EXIT=1"
    goto bootstrap_end
)

set vcvarsall_path=""
for /f "usebackq delims=" %%i in (`"%vswhere_path%" -latest -property installationPath`) do (
        set "vcvarsall_path=%%i\VC\Auxiliary\Build\vcvarsall.bat"
    )

set __VSCMD_ARG_NO_LOGO=1
set VSCMD_SKIP_SENDTELEMETRY=1
set VCPKG_KEEP_ENV_VARS=VSCMD_SKIP_SENDTELEMETRY
set vcvarsall_called=0
if defined VSCMD_VER (
    set vcvarsall_called=1
) else if exist "%vcvarsall_path%" (
    call "%vcvarsall_path%" x86_amd64
    set vcvarsall_called=1
) else (
    echo Error: vcvarsall.bat not found.
    set "BOOTSTRAP_EXIT=1"
    goto bootstrap_end
)

set "BOOTSTRAP_ROOT=%CD%"
set "BOOTSTRAP_ALIAS_ROOT="
if not "%SCRIPT_DIR:~200,1%"=="" (
    set "BOOTSTRAP_ALIAS_ROOT=%TEMP%\SC-bootstrap-root-%RANDOM%-%RANDOM%"
    rmdir "!BOOTSTRAP_ALIAS_ROOT!" 2>nul
    mklink /J "!BOOTSTRAP_ALIAS_ROOT!" "%SCRIPT_DIR%" >nul 2>nul
    if !errorlevel! neq 0 (
        rmdir "!BOOTSTRAP_ALIAS_ROOT!" 2>nul
        mklink /D "!BOOTSTRAP_ALIAS_ROOT!" "%SCRIPT_DIR%" >nul
    )
    if !errorlevel! neq 0 (
        echo Failed to create bootstrap path alias
        set "BOOTSTRAP_EXIT=1"
        goto bootstrap_end
    )
    set "BOOTSTRAP_ROOT=!BOOTSTRAP_ALIAS_ROOT!"
)

set "BOOTSTRAP_EXE=%BOOTSTRAP_ROOT%\_Build\_Tools\Windows\ToolsBootstrap.exe"

@REM Create platform Tools directory if needed
mkdir "%BOOTSTRAP_ROOT%\_Build\_Tools\Windows" 2>nul

@REM Check if ToolsBootstrap needs to be built
if not exist "%BOOTSTRAP_EXE%" (
    set build_bootstrap=1
) else (
    for %%f in ("%BOOTSTRAP_ROOT%\Tools\ToolsBootstrap.c") do set source_time=%%~tf
    for %%f in ("%BOOTSTRAP_EXE%") do set exe_time=%%~tf
    if "!source_time!" gtr "!exe_time!" (set build_bootstrap=1) else (set build_bootstrap=0)
)

if !build_bootstrap! equ 1 (
    set SRC_FILE=!BOOTSTRAP_ROOT!\Tools\ToolsBootstrap.c
    set OBJ_FILE=!BOOTSTRAP_ROOT!\_Build\_Tools\Windows\ToolsBootstrap.obj
    cl.exe /nologo /MTd /Zi /Od /D_DEBUG=1 /Fo"!OBJ_FILE!" /c "!SRC_FILE!" 2>&1
    if !errorlevel! neq 0 (
        echo Failed to build ToolsBootstrap
        set "BOOTSTRAP_EXIT=1"
        goto bootstrap_end
    )
    link /nologo /DEBUG /MANIFEST:EMBED /MANIFESTINPUT:"!BOOTSTRAP_ROOT!\Tools\LongPathAware.manifest" /OUT:"!BOOTSTRAP_EXE!" "!OBJ_FILE!" Shell32.lib 2>&1
    if !errorlevel! neq 0 (
        echo Failed to link ToolsBootstrap
        set "BOOTSTRAP_EXIT=1"
        goto bootstrap_end
    )
)

@REM Execute ToolsBootstrap with original args
"%BOOTSTRAP_EXE%" "%SCRIPT_DIR%" "%SCRIPT_DIR%\Tools" "%SCRIPT_DIR%\_Build" "%SCRIPT_DIR%" %*
set "BOOTSTRAP_EXIT=!errorlevel!"

:bootstrap_end
if defined BOOTSTRAP_ALIAS_ROOT rmdir "%BOOTSTRAP_ALIAS_ROOT%" 2>nul
if "%BOOTSTRAP_PUSHED_UNC%"=="1" popd
endlocal & exit /b %BOOTSTRAP_EXIT%
