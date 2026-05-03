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

REM Set test working directory
set TEST_WORK_DIR=test\%BUILD_TYPE%

REM ========================================
REM First Test Run: Default Configuration
REM ========================================
echo Running first test with default configuration...
echo.
ctest -C %BUILD_TYPE% --output-on-failure %VERBOSE%

set FIRST_RESULT=%errorlevel%

if %FIRST_RESULT% equ 0 (
    echo First test: PASSED
) else (
    echo First test: FAILED
)

REM ========================================
REM Second Test Run: CI Configuration
REM ========================================
set CI_CONFIG_EXISTS=0
if exist "%PROJECT_DIR%\config.ci.json" set CI_CONFIG_EXISTS=1

if %CI_CONFIG_EXISTS% equ 0 (
    echo config.ci.json not found, skipping CI test.
    set SECOND_RESULT=0
    goto skip_second_test
)

echo.
echo Running second test with CI configuration...

REM Backup and replace config
move /Y "%TEST_WORK_DIR%\config.json" "%TEST_WORK_DIR%\config.json.bak" >nul 2>&1
copy /Y "%PROJECT_DIR%\config.ci.json" "%TEST_WORK_DIR%\config.json" >nul 2>&1

ctest -C %BUILD_TYPE% --output-on-failure %VERBOSE%
set SECOND_RESULT=%errorlevel%

REM Restore config
move /Y "%TEST_WORK_DIR%\config.json.bak" "%TEST_WORK_DIR%\config.json" >nul 2>&1

if %SECOND_RESULT% equ 0 (
    echo Second test: PASSED
) else (
    echo Second test: FAILED
)

:skip_second_test

echo.
echo ========================================
echo Test Results Summary
echo ========================================
echo First test (default config):  Status=%FIRST_RESULT%
echo Second test (CI config):      Status=%SECOND_RESULT%
echo ========================================

REM Exit with error if any test failed
if %FIRST_RESULT% neq 0 (
    cd "%SCRIPT_DIR%"
    exit /b 1
)
if %SECOND_RESULT% neq 0 (
    cd "%SCRIPT_DIR%"
    exit /b 1
)

echo.
echo ========================================
echo All tests passed!
echo ========================================

cd "%SCRIPT_DIR%"
endlocal
