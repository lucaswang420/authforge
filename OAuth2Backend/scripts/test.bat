@echo off
setlocal enabledelayedexpansion

REM ========================================
REM OAuth2 Test Script
REM ========================================

REM Store the script directory and change to parent (OAuth2Backend)
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%.."
set PROJECT_DIR=%CD%
echo Current work directory is "%PROJECT_DIR%"
echo.

REM Default build type
set BUILD_TYPE=Release
set VERBOSE=--verbose

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
if /i "%1"=="-q" (
    set VERBOSE=
    shift
    goto parse_args
)
if /i "%1"=="-h" goto usage
if /i "%1"=="--help" goto usage
echo Unknown option: %1
goto usage

:usage
echo Usage: %0 [-debug^|-release] [-q] [-h]
echo   -debug     Test debug version
echo   -release   Test release version (default)
echo   -q         Quiet mode (disable verbose output)
echo   -h, --help Show this help message
exit /b 1

:end_parse

echo ========================================
echo Running OAuth2 Tests
echo ========================================
echo Build Type: %BUILD_TYPE%
echo.

REM Check if build directory exists
if not exist build (
    echo Error: Build directory not found!
    echo Please run build.bat first to compile the project.
    cd "%SCRIPT_DIR%"
    exit /b 1
)

cd build

REM Run tests with proper configuration
echo Running ctest for %BUILD_TYPE% configuration...
echo.
ctest -C %BUILD_TYPE% --output-on-failure %VERBOSE%

if %errorlevel% equ 0 (
    echo.
    echo ========================================
    echo All tests passed!
    echo ========================================
) else (
    echo.
    echo ========================================
    echo Tests failed!
    echo ========================================
    cd "%SCRIPT_DIR%"
    exit /b 1
)

cd "%SCRIPT_DIR%"
endlocal
