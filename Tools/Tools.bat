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

rem Set default command if not provided
set "COMMAND=%1"
if %COMMAND% == "" set "COMMAND=build"

@REM Directory where "Libraries" exists
set "LIBRARY_DIR=%2"
@REM Directory where SC-COMMAND.cpp file exists
set "COMMAND_DIR=%3"
@REM Directory where Command outputs will be placed
set "OUTPUT_DIR=%4\_Build"
@REM Additional parameters to the command
set "CUSTOM_PARAMETERS=%5"

@REM Directory where the build system executable will be generated
set "OUTPUT_COMMANDS_DIR=%OUTPUT_DIR%\_Commands"

mkdir  "%OUTPUT_COMMANDS_DIR%" 2>nul
@REM We set this as current directory because cl.exe doesn't allow overriding
@REM intermediates paths when multiple files are specified as input.
cd /d "%OUTPUT_COMMANDS_DIR%"

if "%6" == "" (
    goto :DoCompile
)

if exist "SC-%COMMAND%.exe" (
    goto :SkipCompiling
)

REM /permissive- allows usage of (and, or) operators with C++14
:DoCompile
echo Building SC-%COMMAND%.cpp...
cl.exe /nologo /std:c++14 /permissive- /EHsc /Fe"SC-%COMMAND%.exe" "%LIBRARY_DIR%/Tools/Tools.cpp" "%COMMAND_DIR%/SC-%COMMAND%.cpp" /link Advapi32.lib Shell32.lib
:SkipCompiling

call "SC-%COMMAND%.exe" "%LIBRARY_DIR%" "%OUTPUT_DIR%" %6 %7 %8 %9
endlocal
