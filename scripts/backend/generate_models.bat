@echo off
setlocal

call "%~dp0\env_common.bat"
if %errorlevel% neq 0 exit /b 1

REM Check for drogon_ctl
where drogon_ctl >nul 2>&1
if %errorlevel% neq 0 (
    echo [Error] drogon_ctl not found in PATH.
    exit /b 1
)

set PROJECT_DIR=%~dp0..\..
set MODELS_DIR=%PROJECT_DIR%\OAuth2Plugin\src\models
set MODELS_BACKUP=%PROJECT_DIR%\OAuth2Plugin\src\models_backup
set MODEL_JSON=%PROJECT_DIR%\OAuth2Server\model.json

echo.
echo ========================================
echo OAuth2 Plugin Model Generation
echo ========================================
echo.

REM Parse arguments
set AUTO_MODE=0
if "%1"=="-y" set AUTO_MODE=1
if "%1"=="--force" set AUTO_MODE=1

if %AUTO_MODE%==0 (
  echo WARNING: This will regenerate ORM models in %MODELS_DIR%
  pause
)

REM Backup existing models
if exist "%MODELS_DIR%" (
  echo Backing up existing models...
  if exist "%MODELS_BACKUP%" rmdir /s /q "%MODELS_BACKUP%"
  mkdir "%MODELS_BACKUP%"
  xcopy /e /i /y "%MODELS_DIR%" "%MODELS_BACKUP%" >nul
)

echo Generating ORM models from %MODEL_JSON%...
cd /d "%PROJECT_DIR%"
if %AUTO_MODE%==1 (
  echo y | drogon_ctl create model "%MODELS_DIR%" "%MODEL_JSON%"
) else (
  drogon_ctl create model "%MODELS_DIR%" "%MODEL_JSON%"
)

if %errorlevel% neq 0 (
  echo [Error] Model generation failed.
  exit /b 1
)

echo.
echo ========================================
echo Model generation complete!
echo ========================================
endlocal
exit /b 0
