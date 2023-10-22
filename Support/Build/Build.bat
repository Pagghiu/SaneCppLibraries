@echo off
setlocal

@REM Directory where the build system executable will be generated
set "GENERATOR_DIR=%1"
@REM Directory where to generate projects
set "PROJECTS_DIR=%2"
@REM Directory where SCBuild.cpp file exists
set "SCRIPT_DIR=%3"
@REM Directory where "Libraries" exists
set "SOURCES_DIR=%4"

mkdir  "%GENERATOR_DIR%" 2>nul
@REM We set this as current directory because cl.exe doesn't allow overriding
@REM intermediates paths when multiple files are specified as input.
cd /d "%GENERATOR_DIR%"

echo Building SCBuild.cpp...
@echo off
call "C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Auxiliary\Build\vcvarsall.bat" x86_amd64
@REM /permissive- allows usage of (and, or) operators with C++14
cl.exe /nologo /std:c++14 /permissive- /EHsc /Fe"SCBuild.exe" "%SCRIPT_DIR%/Support/Build/BuildBootstrap.cpp" "%SCRIPT_DIR%/SCBuild.cpp" /link Advapi32.lib Shell32.lib

if %errorlevel% neq 0 (
    echo Error: Compilation failed.
    exit /b 1
)

call SCBuild.exe --target "%PROJECTS_DIR%" --sources "%SCRIPT_DIR%"

endlocal
