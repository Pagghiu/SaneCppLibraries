@echo off
setlocal enabledelayedexpansion

set "SCRIPT_DIR=%~dp0"
for %%I in ("%SCRIPT_DIR%..\..") do set "REPO_ROOT=%%~fI"
set "TEST_FILE=%REPO_ROOT%\Support\CompileTests\StdCppHeaderNoLinkProbe.cpp"
set "TMP_DIR=%TEMP%\SCStdCppHeaderNoLink-%RANDOM%%RANDOM%"
mkdir "%TMP_DIR%" || exit /b 1

set "FAILED=0"

where cl.exe >nul 2>nul
if not errorlevel 1 (
    echo Testing cl.exe
    cl.exe /nologo /std:c++20 /EHs-c- /GR- /MT "%TEST_FILE%" /Fe:"%TMP_DIR%\probe-msvc.exe" /link /NODEFAULTLIB:libcpmt /NODEFAULTLIB:libcpmtd /NODEFAULTLIB:msvcprt /NODEFAULTLIB:msvcprtd /NODEFAULTLIB:msvcp140 /NODEFAULTLIB:msvcp140d || set "FAILED=1"
    if "!FAILED!"=="0" (
        dumpbin /dependents "%TMP_DIR%\probe-msvc.exe" > "%TMP_DIR%\probe-msvc.dependencies" || set "FAILED=1"
        type "%TMP_DIR%\probe-msvc.dependencies"
        findstr /i "msvcp libcpm msvcprt" "%TMP_DIR%\probe-msvc.dependencies" >nul && set "FAILED=1"
    )
)

where clang-cl.exe >nul 2>nul
if not errorlevel 1 (
    echo Testing clang-cl.exe
    clang-cl.exe /nologo /std:c++20 /EHs-c- /GR- /MT "%TEST_FILE%" /Fe:"%TMP_DIR%\probe-clangcl.exe" /link /NODEFAULTLIB:libcpmt /NODEFAULTLIB:libcpmtd /NODEFAULTLIB:msvcprt /NODEFAULTLIB:msvcprtd /NODEFAULTLIB:msvcp140 /NODEFAULTLIB:msvcp140d || set "FAILED=1"
    if "!FAILED!"=="0" (
        dumpbin /dependents "%TMP_DIR%\probe-clangcl.exe" > "%TMP_DIR%\probe-clangcl.dependencies" || set "FAILED=1"
        type "%TMP_DIR%\probe-clangcl.dependencies"
        findstr /i "msvcp libcpm msvcprt" "%TMP_DIR%\probe-clangcl.dependencies" >nul && set "FAILED=1"
    )
)

if "%FAILED%"=="0" (
    rmdir /s /q "%TMP_DIR%"
    echo Standard C++ header without C++ runtime link check passed.
    exit /b 0
)

echo Standard C++ header without C++ runtime link check failed.
exit /b 1
