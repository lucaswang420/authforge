@echo off
setlocal enabledelayedexpansion

echo Checking for running OAuth2Server processes...
taskkill /F /IM OAuth2Server.exe >nul 2>&1

set SCRIPT_DIR=%~dp0
cd /d "%SCRIPT_DIR%..\.."
set PROJECT_DIR=%CD%

set BUILD_TYPE=Release

:parse_args
if "%1"=="" goto end_parse
if /i "%1"=="-debug" (
    set BUILD_TYPE=Debug
    shift
    goto parse_args
)
if /i "%1"=="-release" (
    set BUILD_TYPE=Release
    shift
    goto parse_args
)
shift
goto parse_args
:end_parse

echo Building Project with configuration: %BUILD_TYPE%

if exist build (
    echo Cleaning existing build directory...
    rmdir /s /q build
)
mkdir build
cd build

echo Installing dependencies with Conan...
conan profile detect --force
conan install .. -s compiler="msvc" -s compiler.version=194 -s compiler.cppstd=20 -s build_type=%BUILD_TYPE% --output-folder . --build=missing

echo Configuring CMake...
cmake .. -DCMAKE_BUILD_TYPE=%BUILD_TYPE% -DCMAKE_CXX_STANDARD=20 -DCMAKE_TOOLCHAIN_FILE="conan_toolchain.cmake" -DCMAKE_POLICY_DEFAULT_CMP0091=NEW 

echo Building...
cmake --build . --parallel --config %BUILD_TYPE%

echo Copying config files...
robocopy ..\OAuth2Server OAuth2Server\%BUILD_TYPE% config.json /NFL /NDL /NJH /NJS /NP
robocopy ..\OAuth2Server OAuth2Server\test\%BUILD_TYPE% config.json /NFL /NDL /NJH /NJS /NP

echo Build completed successfully!
cd ..
endlocal
