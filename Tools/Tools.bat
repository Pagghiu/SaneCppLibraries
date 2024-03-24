@echo off
setlocal

set "vcvarsall_path1=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
set "vcvarsall_path2=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"

@echo off
set __VSCMD_ARG_NO_LOGO=1
set VSCMD_SKIP_SENDTELEMETRY=1
set VCPKG_KEEP_ENV_VARS=VSCMD_SKIP_SENDTELEMETRY
if exist "%vcvarsall_path1%" (
    call "%vcvarsall_path1%" x86_amd64
) else if exist "%vcvarsall_path2%" (
    call "%vcvarsall_path2%" x86_amd64
) else (
    echo Error: vcvarsall.bat not found on either path.
    exit /b 1
)

rem Tool to execute (build by default)
set "TOOL=%4"
if "%4" == "" (
set "TOOL=build"
) else (
set "TOOL=%4"
)

@REM Directory where "Libraries" exists
set "LIBRARY_DIR=%1"
@REM Directory where SC-${TOOL}.cpp file exists
set "TOOL_SOURCE_DIR=%2"
@REM Directory where the ${TOOL} executable will be generated
set "BUILD_DIR=%3"

@REM Directory where the build system executable will be generated
set "TOOL_OUTPUT_DIR=%BUILD_DIR%\_Tools"

mkdir "%TOOL_OUTPUT_DIR%" 2>nul

cd /d "%LIBRARY_DIR%/Tools/Build/Windows"
nmake /nologo /f "Makefile" CONFIG=Debug "TOOL=SC-%TOOL%" "TOOL_SOURCE_DIR=%TOOL_SOURCE_DIR%" "TOOL_OUTPUT_DIR=%TOOL_OUTPUT_DIR%"

IF %ERRORLEVEL% == 0 (
cd /d "%TOOL_OUTPUT_DIR%/Windows"
echo Starting SC-%TOOL%...
"SC-%TOOL%.exe" %*
) 
endlocal
