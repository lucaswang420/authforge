@echo off
REM Project environment configuration
set DROGON_VERSION=v1.9.12
set BUILD_DIR=build
set CONFIG_FILE=config.json

REM Check if conan is installed
where conan >nul 2>&1
if %errorlevel% neq 0 (
    echo [Error] Conan not found. Please install Conan.
    exit /b 1
)

REM Check if cmake is installed
where cmake >nul 2>&1
if %errorlevel% neq 0 (
    echo [Error] CMake not found. Please install CMake.
    exit /b 1
)
