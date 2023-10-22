@echo off
setlocal

set "SCRIPT_DIR=%~dp0"
@REM Remove the trailing slash
set "SCRIPT_DIR=%SCRIPT_DIR:~0,-1%"

set "BUILD_DIR=../../_Build/Build"
cd /d "%BUILD_DIR%"

echo Building SCBuild.cpp...
@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
@REM /permissive- allows usage of (and, or) operators with C++14
mkdir  ..\..\_Build\Build 2>nul
cl.exe /nologo /std:c++14 /permissive- /EHsc /Fe"scbuild.exe" "%SCRIPT_DIR%/../../Bindings/cpp/SC.cpp" "%SCRIPT_DIR%/SCBuild.cpp" /link Advapi32.lib Shell32.lib
if %errorlevel% neq 0 (
    echo Error: Compilation failed.
    exit /b 1
)

scbuild.exe --target "%SCRIPT_DIR%/Build" --sources "%SCRIPT_DIR%/../.."

endlocal
