@echo off
cd /d "%~dp0.."

set PGPASSWORD=123456

echo ========================================
echo Diagnosing users table...
echo ========================================
echo.

echo 1. All users in database:
psql -U test -d oauth_test -c "SELECT id, username, email, created_at FROM users ORDER BY id;"

echo.
echo 2. Check for empty usernames:
psql -U test -d oauth_test -c "SELECT * FROM users WHERE username = '' OR username IS NULL;"

echo.
echo 3. Check database connection activity:
psql -U test -d oauth_test -c "SELECT datname, usename, application_name, state FROM pg_stat_activity WHERE datname = 'oauth_test';"

echo.
echo ========================================
echo Diagnosis complete
echo ========================================
