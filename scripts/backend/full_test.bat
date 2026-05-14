@echo off
setlocal enabledelayedexpansion

REM ========================================
REM One-Click Build and Test Script
REM ========================================
REM This script performs a complete build and test cycle:
REM 1. Reinitialize database
REM 2. Regenerate ORM models
REM 3. Rebuild project
REM 4. Run tests
REM 5. Start server
REM 6. Test OAuth2 endpoints
REM 7. Stop server
REM ========================================

echo.
echo ========================================
echo One-Click Build and Test
echo ========================================
echo.

REM Store the script directory
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%.."
set PROJECT_DIR=%CD%
echo Project directory: %PROJECT_DIR%
echo.

REM Track server PID for cleanup
set SERVER_PID=0
set BUILD_TYPE=-release

REM Parse command line arguments
:parse_args
if "%1"=="" goto end_parse
if /i "%1"=="-debug" (
    set BUILD_TYPE=-debug
    shift
    goto parse_args
)
if /i "%1"=="-release" (
    set BUILD_TYPE=-release
    shift
    goto parse_args
)

:end_parse

REM ========================================
REM Step 1: Reinitialize Database
REM ========================================
echo ========================================
echo Step 1: Reinitializing oauth_test database
echo ========================================
call "%SCRIPT_DIR%setup_database.bat"
if %ERRORLEVEL% neq 0 (
    echo.
    echo [FAILED] Database initialization failed
    goto cleanup_and_exit
)
echo [SUCCESS] Database initialized
echo.

REM ========================================
REM Step 2: Regenerate ORM Models
REM ========================================
echo ========================================
echo Step 2: Regenerating ORM models
echo ========================================
call "%SCRIPT_DIR%generate_models.bat" -y
if %ERRORLEVEL% neq 0 (
    echo.
    echo [FAILED] ORM model generation failed
    goto cleanup_and_exit
)
echo [SUCCESS] ORM models regenerated
echo.

REM ========================================
REM Step 3: Rebuild Project
REM ========================================
echo ========================================
echo Step 3: Rebuilding project
echo ========================================
call "%SCRIPT_DIR%build.bat" %BUILD_TYPE%
if %ERRORLEVEL% neq 0 (
    echo.
    echo [FAILED] Build failed
    goto cleanup_and_exit
)
echo [SUCCESS] Project built
echo.

REM ========================================
REM Step 4: Run Tests
REM ========================================
echo ========================================
echo Step 4: Running tests
echo ========================================
call "%SCRIPT_DIR%test.bat" %BUILD_TYPE%
if %ERRORLEVEL% neq 0 (
    echo.
    echo [FAILED] Tests failed
    goto cleanup_and_exit
)
echo [SUCCESS] All tests passed
echo.

REM ========================================
REM Step 5: Start Server
REM ========================================
echo ========================================
echo Step 5: Starting OAuth2 server
echo ========================================

@echo off
setlocal enabledelayedexpansion

call "%~dp0\env_common.bat"
if %errorlevel% neq 0 exit /b 1

set "PROJECT_DIR=%~dp0..\.."
set BUILD_TYPE=Release
set BUILD_ARG=-release

:parse_args
if "%1"=="" goto end_parse
if /i "%1"=="-debug" (
    set BUILD_TYPE=Debug
    set BUILD_ARG=-debug
    shift
    goto parse_args
)
shift
goto parse_args
:end_parse

REM ... (keep Step 1, 2, 3, 4 unchanged) ...

REM ========================================
REM Step 5: Start Server
REM ========================================
echo ========================================
echo Step 5: Starting OAuth2 server
echo ========================================

set "SERVER_EXE=%PROJECT_DIR%\build\OAuth2Server\%BUILD_TYPE%\OAuth2Server.exe"
if exist "%SERVER_EXE%" (
    echo Starting server: %SERVER_EXE%
    start "" "%SERVER_EXE%" -c "%PROJECT_DIR%\OAuth2Server\config.json"
) else (
    echo [FAILED] Server executable not found at %SERVER_EXE%
    goto cleanup_and_exit
)


REM Wait for server to start
echo Waiting for server to start...
timeout /t 3 /nobreak >nul

REM Check if server is running
tasklist /FI "IMAGENAME eq OAuth2Server.exe" 2>NUL | find /I /N "OAuth2Server.exe">NUL
if "%ERRORLEVEL%"=="0" (
    echo [SUCCESS] Server started
) else (
    tasklist /FI "IMAGENAME eq OAuth2Backend.exe" 2>NUL | find /I /N "OAuth2Backend.exe">NUL
    if "%ERRORLEVEL%"=="0" (
        echo [SUCCESS] Server started
    ) else (
        echo [FAILED] Server failed to start
        goto cleanup_and_exit
    )
)
echo.

REM ========================================
REM Step 6: Test OAuth2 Endpoints
REM ========================================
echo ========================================
echo Step 6: Testing OAuth2 endpoints
echo ========================================
call "%SCRIPT_DIR%test-oauth2-endpoints.bat" -NoPause
if %ERRORLEVEL% neq 0 (
    echo.
    echo [FAILED] OAuth2 endpoint tests failed
    goto cleanup_and_exit
)
echo [SUCCESS] OAuth2 endpoint tests passed
echo.

REM ========================================
REM Step 7: Stop Server
REM ========================================
echo ========================================
echo Step 7: Stopping OAuth2 server
echo ========================================

REM Try to stop OAuth2Server.exe
tasklist /FI "IMAGENAME eq OAuth2Server.exe" 2>NUL | find /I /N "OAuth2Server.exe">NUL
if "%ERRORLEVEL%"=="0" (
    taskkill /F /IM OAuth2Server.exe >nul 2>&1
    echo Stopped OAuth2Server.exe
)

REM Try to stop OAuth2Backend.exe
tasklist /FI "IMAGENAME eq OAuth2Backend.exe" 2>NUL | find /I /N "OAuth2Backend.exe">NUL
if "%ERRORLEVEL%"=="0" (
    taskkill /F /IM OAuth2Backend.exe >nul 2>&1
    echo Stopped OAuth2Backend.exe
)

echo [SUCCESS] Server stopped
echo.

REM ========================================
REM Success Summary
REM ========================================
echo ========================================
echo ALL STEPS COMPLETED SUCCESSFULLY!
echo ========================================
echo.
echo Summary:
echo   [1/7] Database initialization    - PASS
echo   [2/7] ORM model generation       - PASS
echo   [3/7] Project build              - PASS
echo   [4/7] Unit tests                 - PASS
echo   [5/7] Server startup             - PASS
echo   [6/7] OAuth2 endpoint tests      - PASS
echo   [7/7] Server shutdown            - PASS
echo.
echo ========================================
echo Build and Test Cycle Complete
echo ========================================
goto cleanup_and_exit

REM ========================================
REM Cleanup and Exit
REM ========================================
:cleanup_and_exit

REM Ensure server is stopped even on failure
echo.
echo Ensuring server is stopped...
tasklist /FI "IMAGENAME eq OAuth2Server.exe" 2>NUL | find /I /N "OAuth2Server.exe">NUL
if "%ERRORLEVEL%"=="0" (
    taskkill /F /IM OAuth2Server.exe >nul 2>&1
)

tasklist /FI "IMAGENAME eq OAuth2Backend.exe" 2>NUL | find /I /N "OAuth2Backend.exe">NUL
if "%ERRORLEVEL%"=="0" (
    taskkill /F /IM OAuth2Backend.exe >nul 2>&1
)

REM Pause before exit
echo.
echo Press any key to exit...
pause >nul

endlocal
exit /b 0
