@echo off
setlocal

cd /d "%~dp0.."
echo Setting up oauth_test database...

set PGPASSWORD=123456

echo Dropping existing database...
psql -U test -d postgres -c "DROP DATABASE IF EXISTS oauth_test;" >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Failed to drop database
    endlocal
    exit /b 1
)

echo Creating new database...
psql -U test -d postgres -c "CREATE DATABASE oauth_test;" >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Failed to create database
    endlocal
    exit /b 1
)

echo Applying OAuth2 core schema...
psql -U test -d oauth_test -f sql/001_oauth2_core.sql >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Failed to apply OAuth2 core schema
    endlocal
    exit /b 1
)

echo Creating users table...
psql -U test -d oauth_test -f sql/002_users_table.sql >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Failed to create users table
    endlocal
    exit /b 1
)

echo Applying RBAC schema...
psql -U test -d oauth_test -f sql/003_rbac_schema.sql >nul 2>&1
if %errorlevel% neq 0 (
    echo Error: Failed to apply RBAC schema
    endlocal
    exit /b 1
)

echo Database setup complete!
endlocal
exit /b 0
