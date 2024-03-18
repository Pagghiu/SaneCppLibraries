@echo off
setlocal

set "vcvarsall_path1=C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Auxiliary\Build\vcvarsall.bat"
set "vcvarsall_path2=C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat"

@echo off
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

mkdir  "%TOOL_OUTPUT_DIR%" 2>nul
@REM We set this as current directory because cl.exe doesn't allow overriding
@REM intermediates paths when multiple files are specified as input.
cd /d "%TOOL_OUTPUT_DIR%"

if "%5" == "" (
    goto :DoCompile
)

if exist "SC-%TOOL%.exe" (
    goto :SkipCompiling
)

REM /permissive- allows usage of (and, or) operators with C++14
:DoCompile
echo Building SC-%TOOL%.cpp...
cl.exe /nologo /std:c++14 /permissive- /EHsc /Fe"SC-%TOOL%.exe" "%LIBRARY_DIR%/Tools/Tools.cpp" "%TOOL_SOURCE_DIR%/SC-%TOOL%.cpp" /link Advapi32.lib Shell32.lib
:SkipCompiling

IF %ERRORLEVEL% == 0 (
echo Running SC-%TOOL%.cpp...
call "SC-%TOOL%.exe" %*
) 
endlocal
