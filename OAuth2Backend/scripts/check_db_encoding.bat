@echo off
setlocal

cd /d "%~dp0.."
set PGPASSWORD=123456
set PGCLIENTENCODING=UTF8
chcp 65001 >nul 2>&1

echo Checking PostgreSQL database encoding...
echo.

echo 1. Database encoding:
psql -U test -d postgres -c "SELECT datname, encoding, encoding FROM pg_database WHERE datname = 'oauth_test';"

echo.
echo 2. Server encoding:
psql -U test -d postgres -c "SHOW server_encoding;"

echo.
echo 3. Client encoding:
psql -U test -d postgres -c "SHOW client_encoding;"

endlocal
