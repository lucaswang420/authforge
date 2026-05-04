@echo off
setlocal

REM ========================================================================
REM OAuth Plugin - ORM Model Generation Script
REM ========================================================================
REM
REM WARNING: Models are currently in use by the service layer!
REM
REM Only regenerate models if:
REM 1. Database schema has changed
REM 2. You need to add new models
REM 3. Models are corrupted or missing
REM
REM After regeneration, you may need to update service files to use
REM the new model interfaces.
REM
REM Usage:
REM   generate_models.bat         - Interactive mode (with confirmation)
REM   generate_models.bat -y      - Non-interactive mode (auto-confirm)
REM   generate_models.bat --force - Same as -y
REM
REM ========================================================================

REM Parse command line arguments
set AUTO_MODE=0
if "%1"=="-y" set AUTO_MODE=1
if "%1"=="--force" set AUTO_MODE=1
if "%1"=="-Y" set AUTO_MODE=1
if "%1"=="-yes" set AUTO_MODE=1

echo.
echo ========================================
echo OAuth Plugin Model Generation
echo ========================================
echo.

if %AUTO_MODE%==0 (
  echo WARNING: This will regenerate ORM models currently in use!
  echo.
  echo Press Ctrl+C to cancel, or
  pause
  echo.
) else (
  echo Auto-confirmed mode: skipping user prompts
  echo.
)

cd /d "%~dp0.."

REM Check if drogon_ctl is available
where drogon_ctl >nul 2>&1
if %errorlevel% neq 0 (
  echo Error: drogon_ctl not found in PATH
  echo Please install Drogon framework or add it to PATH
  exit /b 1
)

REM Backup existing models
if exist "models" (
  echo Backing up existing models to models_backup...
  if exist "models_backup" rmdir /s /q models_backup
  xcopy /e /i /y models models_backup >nul
)

echo Generating ORM models from model.json...
echo.

if %AUTO_MODE%==1 (
  echo y | drogon_ctl create model models
) else (
  drogon_ctl create model models
)
if %errorlevel% neq 0 (
  echo.
  echo Error: drogon_ctl failed
  echo Restoring backup...
  if exist "models_backup" (
    rmdir /s /q models
    move models_backup models
  )
  exit /b 1
)

echo.
echo ========================================
echo Model generation complete!
echo ========================================
echo.
echo Generated models: models/
echo Backup saved to: models_backup/
echo.
echo NOTE: If model interfaces changed, you may need to update:
echo   - services/*.cc
echo   - controllers/*.cc
echo   - plugins/*.cc
echo.
echo Please review the changes and rebuild the project.
echo.
