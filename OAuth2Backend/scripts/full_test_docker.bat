@echo off
setlocal enabledelayedexpansion

REM ========================================
REM One-Click Build and Test Script (Docker)
REM ========================================
REM This script performs a complete build and test cycle using Docker:
REM 1. Start PostgreSQL in Docker container
REM 2. Wait for database to be ready
REM 3. Reinitialize database
REM 4. Regenerate ORM models
REM 5. Rebuild project
REM 6. Run tests
REM 7. Start server
REM 8. Test OAuth2 endpoints
REM 9. Stop server and cleanup
REM ========================================

echo.
echo ========================================
echo One-Click Build and Test (Docker)
echo ========================================
echo.

REM Store the script directory
set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%.."
set PROJECT_DIR=%CD%
echo Project directory: %PROJECT_DIR%
echo.

REM ========================================
REM Prerequisites Check
REM ========================================
echo Checking prerequisites...

REM Check if Docker is installed
where docker >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Docker is not installed or not in PATH
    echo Please install Docker Desktop for Windows.
    goto cleanup_and_exit
)

REM Check if Docker is running
docker ps >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Docker is not running
    echo Please start Docker Desktop.
    goto cleanup_and_exit
)

REM Check if docker-compose is available
where docker-compose >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [FAILED] docker-compose is not installed or not in PATH
    goto cleanup_and_exit
)

echo [OK] All prerequisites found
echo.

REM ========================================
REM Step 1: Start PostgreSQL in Docker
REM ========================================
echo ========================================
echo Step 1: Starting PostgreSQL in Docker
echo ========================================

REM Stop any existing containers
echo Stopping existing containers...
docker-compose -f "%PROJECT_DIR%\docker-compose.yml" down >nul 2>&1

REM Start PostgreSQL container
echo Starting PostgreSQL container...
docker-compose -f "%PROJECT_DIR%\docker-compose.yml" up -d postgres
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Failed to start PostgreSQL container
    goto cleanup_and_exit
)

REM Wait for PostgreSQL to be ready
echo Waiting for PostgreSQL to be ready...
set MAX_WAIT=30
set WAIT_COUNT=0

:wait_postgres
docker exec oauth2-postgres pg_isready -U test >nul 2>&1
if %ERRORLEVEL% equ 0 (
    echo [SUCCESS] PostgreSQL is ready
    goto postgres_ready
)

set /a WAIT_COUNT+=1
if %WAIT_COUNT% geq %MAX_WAIT% (
    echo [FAILED] PostgreSQL did not become ready in %MAX_WAIT% seconds
    goto cleanup_and_exit
)

echo Waiting... (%WAIT_COUNT%/%MAX_WAIT%)
timeout /t 1 /nobreak >nul
goto wait_postgres

:postgres_ready
echo.

REM ========================================
REM Step 2: Reinitialize Database
REM ========================================
echo ========================================
echo Step 2: Reinitializing oauth_test database
echo ========================================

REM Drop and recreate database using docker exec
echo Dropping existing database...
docker exec oauth2-postgres psql -U test -d postgres -c "DROP DATABASE IF EXISTS oauth_test;" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Failed to drop database
    goto cleanup_and_exit
)

echo Creating new database...
docker exec oauth2-postgres psql -U test -d postgres -c "CREATE DATABASE oauth_test;" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Failed to create database
    goto cleanup_and_exit
)

echo Applying OAuth2 core schema...
docker exec -i oauth2-postgres psql -U test -d oauth_test < "%PROJECT_DIR%\sql\001_oauth2_core.sql" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Failed to apply OAuth2 core schema
    goto cleanup_and_exit
)

echo Creating users table...
docker exec -i oauth2-postgres psql -U test -d oauth_test < "%PROJECT_DIR%\sql\002_users_table.sql" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Failed to create users table
    goto cleanup_and_exit
)

echo Applying RBAC schema...
docker exec -i oauth2-postgres psql -U test -d oauth_test < "%PROJECT_DIR%\sql\003_rbac_schema.sql" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Failed to apply RBAC schema
    goto cleanup_and_exit
)

echo Applying RBAC schema...
docker exec -i oauth2-postgres psql -U test -d oauth_test < "%PROJECT_DIR%\sql\004_oauth2_scopes.sql" >nul 2>&1
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Failed to apply OAuth2 scopes schema
    goto cleanup_and_exit
)

echo [SUCCESS] Database initialized
echo.

REM ========================================
REM Step 3: Regenerate ORM Models
REM ========================================
echo ========================================
echo Step 3: Regenerating ORM models
echo ========================================
call "%SCRIPT_DIR%generate_models.bat" -y
if %ERRORLEVEL% neq 0 (
    echo [FAILED] ORM model generation failed
    goto cleanup_and_exit
)
echo [SUCCESS] ORM models regenerated
echo.

REM ========================================
REM Step 4: Rebuild Project
REM ========================================
echo ========================================
echo Step 4: Rebuilding project
echo ========================================
call "%SCRIPT_DIR%build.bat"
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Build failed
    goto cleanup_and_exit
)
echo [SUCCESS] Project built
echo.

REM ========================================
REM Step 5: Run Tests
REM ========================================
echo ========================================
echo Step 5: Running tests
echo ========================================
call "%SCRIPT_DIR%test.bat"
if %ERRORLEVEL% neq 0 (
    echo [FAILED] Tests failed
    goto cleanup_and_exit
)
echo [SUCCESS] All tests passed
echo.

REM ========================================
REM Step 6: Start Server
REM ========================================
echo ========================================
echo Step 6: Starting OAuth2 server
echo ========================================

REM Determine server executable path
set SERVER_EXE=
if exist "%PROJECT_DIR%\build\Release\OAuth2Server.exe" (
    set SERVER_EXE=%PROJECT_DIR%\build\Release\OAuth2Server.exe
) else if exist "%PROJECT_DIR%\build\Debug\OAuth2Server.exe" (
    set SERVER_EXE=%PROJECT_DIR%\build\Debug\OAuth2Server.exe
) else if exist "%PROJECT_DIR%\build\OAuth2Backend.exe" (
    set SERVER_EXE=%PROJECT_DIR%\build\OAuth2Backend.exe
) else (
    echo [FAILED] Server executable not found
    goto cleanup_and_exit
)

echo Starting server: %SERVER_EXE%
start "" "%SERVER_EXE%" -c "%PROJECT_DIR%\config.json"

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
REM Step 7: Test OAuth2 Endpoints
REM ========================================
echo ========================================
echo Step 7: Testing OAuth2 endpoints
echo ========================================
call "%SCRIPT_DIR%test-oauth2-endpoints.bat" -NoPause
if %ERRORLEVEL% neq 0 (
    echo [FAILED] OAuth2 endpoint tests failed
    goto cleanup_and_exit
)
echo [SUCCESS] OAuth2 endpoint tests passed
echo.

REM ========================================
REM Step 8: Stop Server and Cleanup
REM ========================================
echo ========================================
echo Step 8: Stopping OAuth2 server
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
REM Step 9: Stop Docker Containers
REM ========================================
echo ========================================
echo Step 9: Stopping Docker containers
echo ========================================
docker-compose -f "%PROJECT_DIR%\docker-compose.yml" down
echo [SUCCESS] Docker containers stopped
echo.

REM ========================================
REM Success Summary
REM ========================================
echo ========================================
echo ALL STEPS COMPLETED SUCCESSFULLY!
echo ========================================
echo.
echo Summary:
echo   [1/9] PostgreSQL container startup - PASS
echo   [2/9] Database initialization       - PASS
echo   [3/9] ORM model generation          - PASS
echo   [4/9] Project build                 - PASS
echo   [5/9] Unit tests                    - PASS
echo   [6/9] Server startup                - PASS
echo   [7/9] OAuth2 endpoint tests         - PASS
echo   [8/9] Server shutdown               - PASS
echo   [9/9] Docker containers cleanup     - PASS
echo.
echo ========================================
echo Docker Build and Test Cycle Complete
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

REM Stop Docker containers
echo Stopping Docker containers...
docker-compose -f "%PROJECT_DIR%\docker-compose.yml" down >nul 2>&1

REM Pause before exit
echo.
echo Press any key to exit...
pause >nul

endlocal
exit /b 0
