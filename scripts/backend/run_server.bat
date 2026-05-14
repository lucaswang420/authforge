@echo off
setlocal enabledelayedexpansion

REM Store the script directory
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%.."
set PROJECT_DIR=%CD%
echo Current work directory is "%PROJECT_DIR%"

REM Default build type
set BUILD_TYPE=

REM Parse command line arguments
:parse_args
if "%1"=="" goto end_parse
if /i "%1"=="-debug" (
    set BUILD_TYPE=Debug
    shift
    goto parse_args
)
if /i "%1"=="-release" (
    set BUILD_TYPE=Release
    shift
    goto parse_args
)
echo Unknown option: %1
echo Usage: %0 [-debug|-release]
echo   -debug     Run debug version
echo   -release   Run release version
echo If no option is specified, defaults to Release then Debug.
exit /b 1
:end_parse

REM Check for Conan environment script
if exist "build\conanrun.bat" (
    call "build\conanrun.bat"
) else (
    echo Warning: build\conanrun.bat not found. DLL lookup might fail.
)

REM If build type specified, run that version
if not "%BUILD_TYPE%"=="" (
    if exist "build\%BUILD_TYPE%\OAuth2Server.exe" (
        echo Starting OAuth2Server (%BUILD_TYPE%)
        cd build\%BUILD_TYPE%
        OAuth2Server.exe
        goto :eof
    )
    echo Error: OAuth2Server.exe not found in build\%BUILD_TYPE%.
    echo Please run build.bat -%BUILD_TYPE% first.
    exit /b 1
)

REM Try to run Release version first (default behavior)
if exist "build\Release\OAuth2Server.exe" (
    echo Starting OAuth2Server (Release)
    cd build\Release
    OAuth2Server.exe
    goto :eof
)

REM Try Debug version
if exist "build\Debug\OAuth2Server.exe" (
    echo Starting OAuth2Server (Debug)
    cd build\Debug
    OAuth2Server.exe
    goto :eof
)

echo Error: OAuth2Server.exe not found in build\Release or build\Debug.
echo Please run build.bat first.
exit /b 1
