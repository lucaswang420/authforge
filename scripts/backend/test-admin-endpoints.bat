@echo off
setlocal enabledelayedexpansion

REM ========================================
REM Admin Endpoints Testing Script
REM ========================================

set BASE_URL=http://127.0.0.1:5555
set SHOULD_PAUSE=1

:parse_args
if "%~1"=="" goto end_parse
if /i "%~1"=="-BaseUrl" ( set BASE_URL=%~2& shift& shift& goto parse_args )
if /i "%~1"=="-NoPause" ( set SHOULD_PAUSE=0& shift& goto parse_args )
shift
goto parse_args
:end_parse

if not "%CI%"=="" set SHOULD_PAUSE=0
if not "%GITHUB_ACTIONS%"=="" set SHOULD_PAUSE=0

echo ========================================
echo Admin Endpoints Testing
echo ========================================
echo Base URL: %BASE_URL%
echo.

set PASSED=0
set FAILED=0

REM Run all tests via a single PowerShell script for reliability
set "PS_SCRIPT=%~dp0test-admin-endpoints.ps1"
if not exist "%PS_SCRIPT%" (
    echo ERROR: PowerShell script not found at %PS_SCRIPT%
    exit /b 1
)
powershell -NoProfile -ExecutionPolicy Bypass -File "%PS_SCRIPT%" -BaseUrl "%BASE_URL%"
if %ERRORLEVEL% neq 0 (
    echo.
    echo === Testing Failed ===
    if %SHOULD_PAUSE%==1 pause
    exit /b 1
)

echo.
echo === All Tests Passed ===
if %SHOULD_PAUSE%==1 pause
exit /b 0
