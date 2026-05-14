@echo off
setlocal enabledelayedexpansion

call "%~dp0\env_common.bat"
if %errorlevel% neq 0 exit /b 1

set "PROJECT_DIR=%~dp0..\.."
set BUILD_TYPE=Release
set VERBOSE=--output-on-failure

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
shift
goto parse_args
:end_parse

echo ========================================
echo Running OAuth2 Tests
echo ========================================
echo Build Type: %BUILD_TYPE%

if not exist "%PROJECT_DIR%\build" (
    echo [Error] Build directory not found. Please run build.bat first.
    exit /b 1
)

cd /d "%PROJECT_DIR%\build"

REM Run CTest with proper configuration
echo Running test with configuration: %BUILD_TYPE%
ctest -C %BUILD_TYPE% %VERBOSE%

set TEST_RESULT=%errorlevel%

echo.
echo ========================================
echo Test Result: %TEST_RESULT%
echo ========================================

if %TEST_RESULT% neq 0 (
    echo [Error] Tests failed.
    exit /b 1
)

echo All tests passed!
endlocal
exit /b 0
