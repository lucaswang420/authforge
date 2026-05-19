@echo off
setlocal

call "%~dp0\env_common.bat"
if errorlevel 1 exit /b 1

REM Check for PostgreSQL client
where psql >nul 2>&1
if errorlevel 1 (
    echo [Error] psql not found in PATH.
    exit /b 1
)

set PROJECT_DIR=%~dp0..\..
set MIGRATIONS_DIR=%PROJECT_DIR%\OAuth2Server\sql\migrations
set SEED_DIR=%PROJECT_DIR%\OAuth2Server\sql\seed
set LEGACY_SQL_DIR=%PROJECT_DIR%\OAuth2Server\sql

echo Setting up oauth_test database...

set PGPASSWORD=123456
set PGCLIENTENCODING=UTF8

echo Dropping existing database...
psql -U test -d postgres -c "DROP DATABASE IF EXISTS oauth_test;" >nul 2>&1

echo Creating new database...
psql -U test -d postgres -c "CREATE DATABASE oauth_test;" >nul 2>&1

REM Apply migrations (new structure)
if exist "%MIGRATIONS_DIR%" (
    echo Applying migrations from %MIGRATIONS_DIR%...
    for %%f in ("%MIGRATIONS_DIR%\V*.sql") do (
        echo   Applying %%~nxf...
        psql -U test -d oauth_test -f "%%f"
        if errorlevel 1 (
            echo [Error] Failed to apply %%~nxf
            exit /b 1
        )
    )
) else (
    REM Fallback to legacy flat structure
    echo Applying SQL schemas from %LEGACY_SQL_DIR%...
    for %%f in ("%LEGACY_SQL_DIR%\*.sql") do (
        echo   Applying %%~nxf...
        psql -U test -d oauth_test -f "%%f"
        if errorlevel 1 (
            echo [Error] Failed to apply %%~nxf
            exit /b 1
        )
    )
)

REM Apply seed data (dev/test only)
if exist "%SEED_DIR%" (
    echo Applying seed data from %SEED_DIR%...
    for %%f in ("%SEED_DIR%\*.sql") do (
        echo   Applying %%~nxf...
        psql -U test -d oauth_test -f "%%f"
        if errorlevel 1 (
            echo [Error] Failed to apply seed %%~nxf
            exit /b 1
        )
    )
)

echo Database setup complete!
endlocal
exit /b 0
