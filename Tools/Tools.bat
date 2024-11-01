@echo off
setlocal enabledelayedexpansion enableextensions

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
if exist "%vcvarsall_path%" (
    call "%vcvarsall_path%" x86_amd64
) else (
    echo Error: vcvarsall.bat not found on either path.
    exit /b 1
)

@REM Directory where "Libraries" exists
set "LIBRARY_DIR=%1"
@REM Directory where SC-%TOOL%.cpp file exists
set "TOOL_SOURCE_DIR=%2"
@REM Directory where output subdirectories must be placed
set "BUILD_DIR=%3"
@REM Tool to execute (build by default)
if "%4" == "" (
set "TOOL_NAME=build"
) else (
set "TOOL_NAME=%4"
)

set TOOL=SC-%TOOL_NAME%
if not exist "%TOOL_SOURCE_DIR%\%TOOL%.cpp" (
if not exist "%TOOL_NAME%" (
    echo Error: Tool "%TOOL_NAME%" doesn't exist
    exit /b 1
)
set FULL_NAME=%TOOL_NAME%
call :file_name_from_path TOOL !FULL_NAME!
call :dir_name_from_path TOOL_SOURCE_DIR !FULL_NAME!
SET TOOL_SOURCE_DIR="!TOOL_SOURCE_DIR:~0,-1!"
call :ext_from_path TOOL_EXT !FULL_NAME!
if "!TOOL_EXT!" neq ".cpp" (
    echo Error: !extension! Tool "!TOOL_NAME!" doesn't end with .cpp
    exit /b 1
)

)
@REM Directory where the build system executable will be generated
set TOOL_OUTPUT_DIR="%~3\_Tools"
mkdir "%TOOL_OUTPUT_DIR%" 2>nul

cd /d "%LIBRARY_DIR%/Tools/Build/Windows"

@rem Call NMAKE
nmake build /nologo /f "Makefile" CONFIG=Debug "TOOL=%TOOL%" "TOOL_SOURCE_DIR=%TOOL_SOURCE_DIR%" "TOOL_OUTPUT_DIR=%TOOL_OUTPUT_DIR%"

IF not %ERRORLEVEL% == 0 (
    @rem It could have failed because of moved files, let's re-try after cleaning
    nmake clean /nologo /f "Makefile" CONFIG=Debug "TOOL=%TOOL%" "TOOL_SOURCE_DIR=%TOOL_SOURCE_DIR%" "TOOL_OUTPUT_DIR=%TOOL_OUTPUT_DIR%"
    timeout /t 1 /nobreak >nul
    nmake build /nologo /f "Makefile" CONFIG=Debug "TOOL=%TOOL%" "TOOL_SOURCE_DIR=%TOOL_SOURCE_DIR%" "TOOL_OUTPUT_DIR=%TOOL_OUTPUT_DIR%"
)

IF not %ERRORLEVEL% == 0 (
    exit /b %ERRORLEVEL%
)

cd /d "%TOOL_OUTPUT_DIR%/Windows"
@echo Starting %TOOL%
"%TOOL%.exe" %*

goto :after

:file_name_from_path
(
    set "%~1=%~n2"
    exit /b
)

:dir_name_from_path
(
    set "%~1=%~dp2"
    exit /b
)

:ext_from_path
(
    set "%~1=%~x2"
    exit /b
)

:after

endlocal
