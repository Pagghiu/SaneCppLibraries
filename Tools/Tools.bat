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

@REM Save the arguments for later use
set "TOOL_ARGS=%*"

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
set START_TIME=%TIME%
nmake build /nologo /f "Makefile" CONFIG=Debug "TOOL=%TOOL%" "TOOL_SOURCE_DIR=%TOOL_SOURCE_DIR%" "TOOL_OUTPUT_DIR=%TOOL_OUTPUT_DIR%"
call :get_total_centisecs start_centisecs "%START_TIME%"
call :get_total_centisecs end_centisecs "%TIME%"
set /A total_centisecs = end_centisecs - start_centisecs
if !total_centisecs! lss 0 set /A total_centisecs += 8640000
if !total_centisecs! gtr 8640000 set /A total_centisecs -= 8640000
call :calculate_and_print_time total_centisecs "compile" "%TOOL_NAME%"

IF not %ERRORLEVEL% == 0 (
    @rem It could have failed because of moved files, let's re-try after cleaning
    set RE_START_TIME=%TIME%
    nmake clean /nologo /f "Makefile" CONFIG=Debug "TOOL=%TOOL%" "TOOL_SOURCE_DIR=%TOOL_SOURCE_DIR%" "TOOL_OUTPUT_DIR=%TOOL_OUTPUT_DIR%"
    timeout /t 1 /nobreak >nul
    nmake build /nologo /f "Makefile" CONFIG=Debug "TOOL=%TOOL%" "TOOL_SOURCE_DIR=%TOOL_SOURCE_DIR%" "TOOL_OUTPUT_DIR=%TOOL_OUTPUT_DIR%"
    call :get_total_centisecs re_start_centisecs "%RE_START_TIME%"
    call :get_total_centisecs re_end_centisecs "%TIME%"
    set /A re_total_centisecs = re_end_centisecs - re_start_centisecs
    if !re_total_centisecs! lss 0 set /A re_total_centisecs += 8640000
    if !re_total_centisecs! gtr 8640000 set /A re_total_centisecs -= 8640000
    call :calculate_and_print_time re_total_centisecs "re-compile" "%TOOL_NAME%" " after clean"
)

IF not %ERRORLEVEL% == 0 (
    exit /b %ERRORLEVEL%
)

cd /d "%TOOL_OUTPUT_DIR%/Windows"
@echo Starting %TOOL%
"%TOOL%.exe" !TOOL_ARGS!

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

:get_total_centisecs
for /f "tokens=1-4 delims=:.," %%a in ("%~2") do (
    set temp_h=%%a
    set temp_m=%%b
    set temp_s=%%c
    set temp_mi=%%d
)
set temp_h=00!temp_h!
set temp_h=!temp_h:~-2!
set temp_m=00!temp_m!
set temp_m=!temp_m:~-2!
set temp_s=00!temp_s!
set temp_s=!temp_s:~-2!
set temp_mi=00!temp_mi!
set temp_mi=!temp_mi:~-2!
set /A "%~1 = temp_h * 360000"
set /A "%~1 = %~1 + temp_m * 6000"
set /A "%~1 = %~1 + temp_s * 100"
set /A "%~1 = %~1 + temp_mi"
exit /b

:calculate_and_print_time
set /A build_time_temp = %~1 / 100
set /A build_time_frac_temp = (%~1 %% 100) * 10
set frac_temp=000!build_time_frac_temp!
set frac_temp=!frac_temp:~-3!
echo Time to %~2 "%~3" tool%~4: !build_time_temp!.!frac_temp! seconds
exit /b

:after

endlocal
