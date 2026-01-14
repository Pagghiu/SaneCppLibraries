@echo off
set "SCRIPT_DIR=%~dp0"
if not exist "%SCRIPT_DIR%\_Build" mkdir "%SCRIPT_DIR%\_Build"
pushd "%SCRIPT_DIR%\_Build"
set "vswhere_path=C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe"
for /f "usebackq delims=" %%i in (`"%vswhere_path%" -latest -property installationPath`) do ( set "vcvarsall_path=%%i\VC\Auxiliary\Build\vcvarsall.bat" )
set __VSCMD_ARG_NO_LOGO=1
set VSCMD_SKIP_SENDTELEMETRY=1
set VCPKG_KEEP_ENV_VARS=VSCMD_SKIP_SENDTELEMETRY
if exist "%vcvarsall_path%" (call "%vcvarsall_path%" x86_amd64)
echo Building and running AsyncWebServer example (DEBUG)...
cl ..\AsyncWebServer.cpp ..\..\..\SC.cpp /Fe"AsyncWebServer.exe" /nologo /std:c++14 /permissive- /EHsc /DEBUG /D_DEBUG /MTd /Od Advapi32.lib Shell32.lib && cd .. && _Build\AsyncWebServer.exe %*
popd
