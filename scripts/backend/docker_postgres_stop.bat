@echo off
setlocal

REM ========================================
REM Stop PostgreSQL in Docker
REM ========================================

echo Stopping PostgreSQL in Docker...

cd /d "%~dp0../.."

docker-compose down

if %ERRORLEVEL% equ 0 (
    echo [SUCCESS] PostgreSQL stopped
) else (
    echo [FAILED] Failed to stop PostgreSQL
)

echo.
echo To remove data volumes as well, run:
echo   docker-compose down -v

endlocal
exit /b 0
