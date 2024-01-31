@echo off
setlocal

@REM Action ("configure" / "build")
set "ACTION=%1"

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
@REM /permissive- allows usage of (and, or) operators with C++14
if %ACTION% == "configure" (
echo Configuring by building SCBuild.cpp...

@REM Directory where the build system executable will be generated
set "GENERATOR_DIR=%2"
@REM Directory where to generate projects
set "PROJECTS_DIR=%3"
@REM Directory where SCBuild.cpp file exists
set "SCRIPT_DIR=%4"
@REM Directory where "Libraries" exists
set "SOURCES_DIR=%5"

mkdir  "%GENERATOR_DIR%" 2>nul
@REM We set this as current directory because cl.exe doesn't allow overriding
@REM intermediates paths when multiple files are specified as input.
cd /d "%GENERATOR_DIR%"

cl.exe /nologo /std:c++14 /permissive- /EHsc /Fe"SCBuild.exe" "%SCRIPT_DIR%/Support/Build/BuildBootstrap.cpp" "%SCRIPT_DIR%/SCBuild.cpp" /link Advapi32.lib Shell32.lib
) else if %ACTION% == "build" (
echo Building Generated Project...
msbuild %2 %3 %4 
)
if %errorlevel% neq 0 (
    echo Error: Compilation failed.
    exit /b 1
)

call SCBuild.exe --target "%PROJECTS_DIR%" --sources "%SCRIPT_DIR%"

endlocal
