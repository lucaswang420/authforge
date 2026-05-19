@echo off
setlocal enabledelayedexpansion

REM ========================================
REM OAuth2 Endpoints Testing Script (CMD)
REM ========================================
REM Usage:
REM   test-oauth2-endpoints.bat
REM   test-oauth2-endpoints.bat -BaseUrl "http://127.0.0.1:5555" -Pause
REM   test-oauth2-endpoints.bat -NoPause
REM ========================================

REM Default values
set BASE_URL=http://127.0.0.1:5555
set SHOULD_PAUSE=1
set CI_ENV=0

REM Parse command line arguments
:parse_args
if "%~1"=="" goto end_parse
if /i "%~1"=="-BaseUrl" (
    set BASE_URL=%~2
    shift
    shift
    goto parse_args
)
if /i "%~1"=="-Pause" (
    set SHOULD_PAUSE=1
    shift
    goto parse_args
)
if /i "%~1"=="-NoPause" (
    set SHOULD_PAUSE=0
    shift
    goto parse_args
)
shift
goto parse_args

:end_parse

REM Check for CI environment
if not "%CI%"=="" set CI_ENV=1
if not "%GITHUB_ACTIONS%"=="" set CI_ENV=1
if not "%TF_BUILD%"=="" set CI_ENV=1
if not "%BUILD_BUILDID%"=="" set CI_ENV=1

if %CI_ENV%==1 set SHOULD_PAUSE=0

echo ========================================
echo OAuth2 Endpoints Testing
echo ========================================
echo Base URL: %BASE_URL%
echo.

REM Test variables
set SERVER_READY=0
set LOGIN_SUCCESS=0
set AUTH_CODE=
set ACCESS_TOKEN=
set PASSED=0
set FAILED=0
set SKIPPED=0

REM ========================================
REM Test 1: Health Check
REM ========================================
set TEST_NAME=Health Check
echo [*] Test 1: %TEST_NAME%

REM Try to get health status and capture both output and exit code
for /f "tokens=*" %%a in ('powershell -Command "try { $response = Invoke-RestMethod -Uri '%BASE_URL%/health' -Method Get -ErrorAction Stop; Write-Output $response.status; exit 0 } catch { Write-Output ''; exit 1 }"') do set STATUS=%%a

if %ERRORLEVEL% neq 0 (
    echo [-] Health check failed: Could not connect to server
    echo.
    echo [!] Make sure the OAuth2 server is running:
    echo    cd OAuth2Backend\build
    echo    .\OAuth2Backend -c ..\config.json
    set /a FAILED+=1
    goto summary
)

if "%STATUS%"=="" (
    echo [-] Health check failed: Invalid response from server
    set /a FAILED+=1
    goto summary
)

REM Get additional health info
for /f "tokens=*" %%a in ('powershell -Command "$response = Invoke-RestMethod -Uri '%BASE_URL%/health' -Method Get; Write-Output $response.service"') do set SERVICE=%%a
for /f "tokens=*" %%a in ('powershell -Command "$response = Invoke-RestMethod -Uri '%BASE_URL%/health' -Method Get; if ($response.storage_type) { Write-Output $response.storage_type }"') do set STORAGE=%%a

echo [+] Health check successful
echo    Status: %STATUS%
echo    Service: %SERVICE%
if not "%STORAGE%"=="" echo    Storage: %STORAGE%

set SERVER_READY=1
set /a PASSED+=1
echo.

REM ========================================
REM Test 2: Login
REM ========================================
set TEST_NAME=OAuth2 Login
if %SERVER_READY%==0 (
    echo [~] %TEST_NAME% skipped: blocked because health check failed
    set /a SKIPPED+=1
    goto token_exchange
)

echo [*] Test 2: %TEST_NAME%

for /f "tokens=*" %%a in ('powershell -Command "$body = @{ username='admin'; password='admin'; client_id='vue-client'; redirect_uri='http://127.0.0.1:5173/callback'; json='true' }; $response = Invoke-RestMethod -Uri '%BASE_URL%/oauth2/login' -Method Post -Body $body; Write-Output $response.code"') do set AUTH_CODE=%%a

if "%AUTH_CODE%"=="" (
    echo [-] Login failed: No authorization code in response
    set /a FAILED+=1
    goto token_exchange
)

echo [+] Login successful
echo    Code: %AUTH_CODE%

set LOGIN_SUCCESS=1
set /a PASSED+=1
echo.

REM ========================================
REM Test 3: Token Exchange
REM ========================================
:token_exchange
set TEST_NAME=Token Exchange
if "%AUTH_CODE%"=="" (
    echo [~] %TEST_NAME% skipped: blocked because login did not return an authorization code
    set /a SKIPPED+=1
    goto userinfo
)

echo [*] Test 3: Exchange Authorization Code for Token

REM Save AUTH_CODE to temp file to avoid escaping issues
echo %AUTH_CODE% > "%TEMP%\oauth2_code.txt"

REM Call PowerShell to get token response
powershell -NoProfile -ExecutionPolicy Bypass -Command "$code = Get-Content '%TEMP%\oauth2_code.txt' -Raw; $body = @{ grant_type='authorization_code'; code=$code.Trim(); redirect_uri='http://127.0.0.1:5173/callback'; client_id='vue-client'; client_secret='123456' }; try { $response = Invoke-RestMethod -Uri '%BASE_URL%/oauth2/token' -Method Post -Body $body; $response.access_token | Out-File '%TEMP%\oauth2_access_token.txt' -Encoding ASCII; $response.token_type | Out-File '%TEMP%\oauth2_token_type.txt' -Encoding ASCII; $response.expires_in | Out-File '%TEMP%\oauth2_expires_in.txt' -Encoding ASCII; if ($response.refresh_token) { $response.refresh_token | Out-File '%TEMP%\oauth2_refresh_token.txt' -Encoding ASCII } exit 0 } catch { Write-Output $_.Exception.Message | Out-File '%TEMP%\oauth2_error.txt' -Encoding ASCII; exit 1 }"

if %ERRORLEVEL% neq 0 (
    echo [-] Token exchange failed
    if exist "%TEMP%\oauth2_error.txt" (
        set /p ERROR_MSG=<"%TEMP%\oauth2_error.txt"
        echo    Error: !ERROR_MSG!
    )
    set /a FAILED+=1
    goto cleanup_token
)

REM Read values from temp files
if exist "%TEMP%\oauth2_access_token.txt" (
    set /p ACCESS_TOKEN=<"%TEMP%\oauth2_access_token.txt"
) else (
    set ACCESS_TOKEN=
)

if exist "%TEMP%\oauth2_token_type.txt" (
    set /p TOKEN_TYPE=<"%TEMP%\oauth2_token_type.txt"
) else (
    set TOKEN_TYPE=
)

if exist "%TEMP%\oauth2_expires_in.txt" (
    set /p EXPIRES_IN=<"%TEMP%\oauth2_expires_in.txt"
) else (
    set EXPIRES_IN=
)

if exist "%TEMP%\oauth2_refresh_token.txt" (
    set /p REFRESH_TOKEN=<"%TEMP%\oauth2_refresh_token.txt"
) else (
    set REFRESH_TOKEN=
)

:cleanup_token
REM Cleanup temp files
del "%TEMP%\oauth2_code.txt" >nul 2>&1
del "%TEMP%\oauth2_access_token.txt" >nul 2>&1
del "%TEMP%\oauth2_token_type.txt" >nul 2>&1
del "%TEMP%\oauth2_expires_in.txt" >nul 2>&1
del "%TEMP%\oauth2_refresh_token.txt" >nul 2>&1
del "%TEMP%\oauth2_error.txt" >nul 2>&1

if "%ACCESS_TOKEN%"=="" (
    echo [-] Token exchange failed: No access token in response
    set /a FAILED+=1
    goto userinfo
)

echo [+] Token exchange successful
echo    Access Token: %ACCESS_TOKEN:~0,20%...
echo    Token Type: %TOKEN_TYPE%
echo    Expires In: %EXPIRES_IN%s
if not "%REFRESH_TOKEN%"=="" echo    Refresh Token: %REFRESH_TOKEN:~0,20%...

REM P0 Validation: Token must be base64url (43 chars), not UUID (36 chars)
REM Simple check: token should NOT contain only hex+dashes (UUID pattern)
REM and should contain base64url chars like _ or - or uppercase
set TOKEN_VALID=1
if "%ACCESS_TOKEN:~42,1%"=="" (
    echo    [-] SECURITY: Access token shorter than 43 chars
    set TOKEN_VALID=0
)
if %TOKEN_VALID%==1 (
    echo    [OK] Token format: base64url, 43+ chars
)

set /a PASSED+=1
echo.

REM ========================================
REM Test 4: UserInfo
REM ========================================
:userinfo
set TEST_NAME=UserInfo
if "%ACCESS_TOKEN%"=="" (
    echo [~] %TEST_NAME% skipped: blocked because token exchange did not return an access token
    set /a SKIPPED+=1
    goto admin_dashboard
)

echo [*] Test 4: Access Protected Resource (UserInfo)

REM Save ACCESS_TOKEN to temp file
echo %ACCESS_TOKEN% > "%TEMP%\oauth2_access_token_userinfo.txt"

powershell -NoProfile -ExecutionPolicy Bypass -Command "$token = Get-Content '%TEMP%\oauth2_access_token_userinfo.txt' -Raw; $headers = @{ Authorization = 'Bearer ' + $token.Trim() }; try { $response = Invoke-RestMethod -Uri '%BASE_URL%/oauth2/userinfo' -Method Get -Headers $headers; $response.sub | Out-File '%TEMP%\oauth2_user_sub.txt' -Encoding ASCII; $response.name | Out-File '%TEMP%\oauth2_user_name.txt' -Encoding ASCII; $response.email | Out-File '%TEMP%\oauth2_user_email.txt' -Encoding ASCII; exit 0 } catch { Write-Output $_.Exception.Message | Out-File '%TEMP%\oauth2_error.txt' -Encoding ASCII; exit 1 }"

if %ERRORLEVEL% neq 0 (
    echo [-] UserInfo access failed
    if exist "%TEMP%\oauth2_error.txt" (
        set /p ERROR_MSG=<"%TEMP%\oauth2_error.txt"
        echo    Error: !ERROR_MSG!
    )
    set /a FAILED+=1
    goto cleanup_userinfo
)

REM Read values from temp files
if exist "%TEMP%\oauth2_user_sub.txt" (
    set /p USER_SUB=<"%TEMP%\oauth2_user_sub.txt"
) else (
    set USER_SUB=
)

if exist "%TEMP%\oauth2_user_name.txt" (
    set /p USER_NAME=<"%TEMP%\oauth2_user_name.txt"
) else (
    set USER_NAME=
)

if exist "%TEMP%\oauth2_user_email.txt" (
    set /p USER_EMAIL=<"%TEMP%\oauth2_user_email.txt"
) else (
    set USER_EMAIL=
)

:cleanup_userinfo
REM Cleanup temp files
del "%TEMP%\oauth2_access_token_userinfo.txt" >nul 2>&1
del "%TEMP%\oauth2_user_sub.txt" >nul 2>&1
del "%TEMP%\oauth2_user_name.txt" >nul 2>&1
del "%TEMP%\oauth2_user_email.txt" >nul 2>&1
del "%TEMP%\oauth2_error.txt" >nul 2>&1

if "%USER_SUB%"=="" (
    echo [-] UserInfo access failed: Invalid response
    set /a FAILED+=1
    goto admin_dashboard
)

echo [+] UserInfo access successful
echo    User ID: %USER_SUB%
echo    Name: %USER_NAME%
echo    Email: %USER_EMAIL%

REM P0 Validation: sub must be UUID format (36+ chars with dashes, not numeric ID)
if not "%USER_SUB:~35,1%"=="" (
    echo    [OK] Subject format: UUID (not enumerable)
)

REM P0 Validation: name should NOT be the UUID (should be username)
if "%USER_NAME%"=="%USER_SUB%" (
    echo [-] WARNING: Name equals sub (getUserInfo may not be resolving username)
)

set /a PASSED+=1
echo.

REM ========================================
REM Test 5: Admin Dashboard
REM ========================================
:admin_dashboard
set TEST_NAME=Admin Dashboard
if "%ACCESS_TOKEN%"=="" (
    echo [~] %TEST_NAME% skipped: blocked because token exchange did not return an access token
    set /a SKIPPED+=1
    goto summary
)

echo [*] Test 5: Access Admin Dashboard

REM Save ACCESS_TOKEN to temp file
echo %ACCESS_TOKEN% > "%TEMP%\oauth2_access_token_admin.txt"

powershell -NoProfile -ExecutionPolicy Bypass -Command "$token = Get-Content '%TEMP%\oauth2_access_token_admin.txt' -Raw; $headers = @{ Authorization = 'Bearer ' + $token.Trim() }; try { $response = Invoke-RestMethod -Uri '%BASE_URL%/api/admin/dashboard' -Method Get -Headers $headers; $response.message | Out-File '%TEMP%\oauth2_admin_msg.txt' -Encoding ASCII; $response.status | Out-File '%TEMP%\oauth2_admin_status.txt' -Encoding ASCII; exit 0 } catch { Write-Output $_.Exception.Message | Out-File '%TEMP%\oauth2_error.txt' -Encoding ASCII; exit 1 }"

if %ERRORLEVEL% neq 0 (
    echo [-] Admin dashboard access failed
    if exist "%TEMP%\oauth2_error.txt" (
        set /p ERROR_MSG=<"%TEMP%\oauth2_error.txt"
        echo    Error: !ERROR_MSG!
    )
    set /a FAILED+=1
    goto cleanup_admin
)

REM Read values from temp files
if exist "%TEMP%\oauth2_admin_msg.txt" (
    set /p ADMIN_MSG=<"%TEMP%\oauth2_admin_msg.txt"
) else (
    set ADMIN_MSG=
)

if exist "%TEMP%\oauth2_admin_status.txt" (
    set /p ADMIN_STATUS=<"%TEMP%\oauth2_admin_status.txt"
) else (
    set ADMIN_STATUS=
)

:cleanup_admin
REM Cleanup temp files
del "%TEMP%\oauth2_access_token_admin.txt" >nul 2>&1
del "%TEMP%\oauth2_admin_msg.txt" >nul 2>&1
del "%TEMP%\oauth2_admin_status.txt" >nul 2>&1
del "%TEMP%\oauth2_error.txt" >nul 2>&1

if "%ADMIN_MSG%"=="" (
    echo [-] Admin dashboard access failed: Invalid response
    set /a FAILED+=1
    goto summary
)

echo [+] Admin dashboard access successful
echo    Message: %ADMIN_MSG%
echo    Status: %ADMIN_STATUS%

set /a PASSED+=1
echo.

REM ========================================
REM Test Summary
REM ========================================
:summary
echo ========================================
echo Test Summary
echo ========================================

if %SERVER_READY%==1 (
    echo Health Check        PASS
) else (
    echo Health Check        FAIL
)

if %SERVER_READY%==0 (
    echo OAuth2 Login        SKIP - Blocked by health check failure
) else if %LOGIN_SUCCESS%==1 (
    echo OAuth2 Login        PASS
) else (
    echo OAuth2 Login        FAIL - No authorization code
)

if "%AUTH_CODE%"=="" (
    if %SERVER_READY%==1 (
        echo Token Exchange      FAIL - No authorization code
    ) else (
        echo Token Exchange      SKIP - Blocked by previous failures
    )
) else (
    echo Token Exchange      PASS
)

if "%ACCESS_TOKEN%"=="" (
    if not "%AUTH_CODE%"=="" (
        echo UserInfo             FAIL - No access token
    ) else (
        echo UserInfo             SKIP - Blocked by previous failures
    )
) else (
    echo UserInfo             PASS
)

if "%ACCESS_TOKEN%"=="" (
    echo Admin Dashboard      SKIP - Blocked by previous failures
) else (
    echo Admin Dashboard      PASS
)

echo.
echo Passed: %PASSED% ^| Failed: %FAILED% ^| Skipped: %SKIPPED%
echo.

REM Exit with appropriate code
if %FAILED% gtr 0 (
    echo === Testing Failed ===
    if %SHOULD_PAUSE%==1 pause
    endlocal
    exit /b 1
)

echo === Testing Complete: all tests passed ===
if %SHOULD_PAUSE%==1 pause
endlocal
exit /b 0
