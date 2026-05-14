@echo off
REM Common path validation for all backend scripts
if not exist "%~dp0\..\..\OAuth2Plugin" (
    echo [Error] Script must be run from 'scripts/backend' directory.
    exit /b 1
)
