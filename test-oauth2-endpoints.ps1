# OAuth2 Endpoints Testing Script for PowerShell
# Usage:
#   .\test-oauth2-endpoints.ps1
#   .\test-oauth2-endpoints.ps1 -BaseUrl "http://127.0.0.1:5555" -Pause
#   .\test-oauth2-endpoints.ps1 -NoPause

param(
    [string]$BaseUrl = "http://127.0.0.1:5555",
    [switch]$Pause,
    [switch]$NoPause
)

$results = @()

function Add-Result {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        [ValidateSet("PASS", "FAIL", "SKIP")]
        [string]$Status,
        [string]$Message = ""
    )

    $script:results += [pscustomobject]@{
        Name = $Name
        Status = $Status
        Message = $Message
    }
}

function Write-TestError {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [Parameter(Mandatory = $true)]
        $ErrorRecord
    )

    Write-Host "[-] $Name failed: $($ErrorRecord.Exception.Message)" -ForegroundColor Red
    if ($ErrorRecord.ErrorDetails) {
        Write-Host "   Details: $($ErrorRecord.ErrorDetails.Message)" -ForegroundColor Red
    }

    Add-Result $Name "FAIL" $ErrorRecord.Exception.Message
}

function Require-Value {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Name,
        [AllowNull()]
        [AllowEmptyString()]
        [string]$Value,
        [Parameter(Mandatory = $true)]
        [string]$Reason
    )

    if ([string]::IsNullOrWhiteSpace($Value)) {
        Write-Host "[~] $Name skipped: $Reason" -ForegroundColor DarkYellow
        Add-Result $Name "SKIP" $Reason
        return $false
    }

    return $true
}

function Test-CiEnvironment {
    return (
        $env:CI -eq "true" -or
        $env:GITHUB_ACTIONS -eq "true" -or
        $env:TF_BUILD -eq "True" -or
        $env:BUILD_BUILDID
    )
}

function Wait-IfRequested {
    $shouldPause = $Pause -or (-not $NoPause -and -not (Test-CiEnvironment))
    if ($shouldPause) {
        Write-Host ""
        Write-Host "Press any key to exit..." -ForegroundColor Gray
        $null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")
    }
}

Write-Host "=== OAuth2 Endpoints Testing ===" -ForegroundColor Cyan
Write-Host "Base URL: $BaseUrl" -ForegroundColor Gray
Write-Host ""

$serverReady = $false
$authCode = $null
$accessToken = $null

# Test 1: Health Check
$testName = "Health Check"
Write-Host "[*] Test 1: $testName" -ForegroundColor Yellow
try {
    $response = Invoke-RestMethod -Uri "$BaseUrl/health" -Method Get
    Write-Host "[+] Health check successful" -ForegroundColor Green
    Write-Host "   Status: $($response.status)" -ForegroundColor Gray
    Write-Host "   Service: $($response.service)" -ForegroundColor Gray

    if ($response.storage_type) {
        Write-Host "   Storage: $($response.storage_type)" -ForegroundColor Gray
    }

    $serverReady = $true
    Add-Result $testName "PASS"
} catch {
    Write-TestError $testName $_
    Write-Host ""
    Write-Host "[!] Make sure the OAuth2 server is running:" -ForegroundColor Yellow
    Write-Host "   cd OAuth2Backend/build" -ForegroundColor Gray
    Write-Host "   ./OAuth2Backend -c ../config.json" -ForegroundColor Gray
}
Write-Host ""

# Test 2: Login
$testName = "OAuth2 Login"
if ($serverReady) {
    Write-Host "[*] Test 2: $testName" -ForegroundColor Yellow
    try {
        $body = @{
            username = "admin"
            password = "admin"
            client_id = "vue-client"
            redirect_uri = "http://127.0.0.1:5173/callback"
            json = "true"
        }

        $response = Invoke-RestMethod -Uri "$BaseUrl/oauth2/login" -Method Post -Body $body
        Write-Host "[+] Login successful" -ForegroundColor Green
        Write-Host "   Code: $($response.code)" -ForegroundColor Gray
        Write-Host "   Location: $($response.location)" -ForegroundColor Gray

        $authCode = $response.code
        if ([string]::IsNullOrWhiteSpace($authCode)) {
            Write-Host "[-] Login response did not include an authorization code" -ForegroundColor Red
            Add-Result $testName "FAIL" "Login response did not include an authorization code"
        } else {
            Add-Result $testName "PASS"
        }
    } catch {
        Write-TestError $testName $_
    }
} else {
    Require-Value $testName $null "blocked because health check failed" | Out-Null
}
Write-Host ""

# Test 3: Exchange Code for Token
$testName = "Token Exchange"
if (Require-Value $testName $authCode "blocked because login did not return an authorization code") {
    Write-Host "[*] Test 3: Exchange Authorization Code for Token" -ForegroundColor Yellow
    try {
        $tokenBody = @{
            grant_type = "authorization_code"
            code = $authCode
            redirect_uri = "http://127.0.0.1:5173/callback"
            client_id = "vue-client"
            client_secret = "123456"
        }

        $tokenResponse = Invoke-RestMethod -Uri "$BaseUrl/oauth2/token" -Method Post -Body $tokenBody
        Write-Host "[+] Token exchange successful" -ForegroundColor Green

        if ([string]::IsNullOrWhiteSpace($tokenResponse.access_token)) {
            Write-Host "[-] Token response did not include an access token" -ForegroundColor Red
            Add-Result $testName "FAIL" "Token response did not include an access token"
        } else {
            Write-Host "   Access Token: $($tokenResponse.access_token.Substring(0, [Math]::Min(20, $tokenResponse.access_token.Length)))..." -ForegroundColor Gray
            Write-Host "   Token Type: $($tokenResponse.token_type)" -ForegroundColor Gray
            Write-Host "   Expires In: $($tokenResponse.expires_in)s" -ForegroundColor Gray
            if ($tokenResponse.refresh_token) {
                Write-Host "   Refresh Token: $($tokenResponse.refresh_token.Substring(0, [Math]::Min(20, $tokenResponse.refresh_token.Length)))..." -ForegroundColor Gray
            }

            $accessToken = $tokenResponse.access_token
            Add-Result $testName "PASS"
        }
    } catch {
        Write-TestError $testName $_
    }
}
Write-Host ""

# Test 4: Access Protected Resource
$testName = "UserInfo"
if (Require-Value $testName $accessToken "blocked because token exchange did not return an access token") {
    Write-Host "[*] Test 4: Access Protected Resource (UserInfo)" -ForegroundColor Yellow
    try {
        $headers = @{
            Authorization = "Bearer $accessToken"
        }

        $userInfo = Invoke-RestMethod -Uri "$BaseUrl/oauth2/userinfo" -Method Get -Headers $headers
        Write-Host "[+] UserInfo access successful" -ForegroundColor Green
        Write-Host "   User ID: $($userInfo.sub)" -ForegroundColor Gray
        Write-Host "   Name: $($userInfo.name)" -ForegroundColor Gray
        Write-Host "   Email: $($userInfo.email)" -ForegroundColor Gray
        Add-Result $testName "PASS"
    } catch {
        Write-TestError $testName $_
    }
}
Write-Host ""

# Test 5: Test Admin Dashboard
$testName = "Admin Dashboard"
if (Require-Value $testName $accessToken "blocked because token exchange did not return an access token") {
    Write-Host "[*] Test 5: Access Admin Dashboard" -ForegroundColor Yellow
    try {
        $headers = @{
            Authorization = "Bearer $accessToken"
        }

        $adminData = Invoke-RestMethod -Uri "$BaseUrl/api/admin/dashboard" -Method Get -Headers $headers
        Write-Host "[+] Admin dashboard access successful" -ForegroundColor Green
        Write-Host "   Message: $($adminData.message)" -ForegroundColor Gray
        Write-Host "   Status: $($adminData.status)" -ForegroundColor Gray
        Add-Result $testName "PASS"
    } catch {
        Write-TestError $testName $_
    }
}
Write-Host ""

$passed = @($results | Where-Object { $_.Status -eq "PASS" }).Count
$failed = @($results | Where-Object { $_.Status -eq "FAIL" }).Count
$skipped = @($results | Where-Object { $_.Status -eq "SKIP" }).Count

Write-Host "=== Test Summary ===" -ForegroundColor Cyan
foreach ($result in $results) {
    $color = "Gray"
    if ($result.Status -eq "PASS") { $color = "Green" }
    elseif ($result.Status -eq "FAIL") { $color = "Red" }
    elseif ($result.Status -eq "SKIP") { $color = "DarkYellow" }

    if ([string]::IsNullOrWhiteSpace($result.Message)) {
        Write-Host ("{0,-16} {1}" -f $result.Name, $result.Status) -ForegroundColor $color
    } else {
        Write-Host ("{0,-16} {1} - {2}" -f $result.Name, $result.Status, $result.Message) -ForegroundColor $color
    }
}

Write-Host ""
Write-Host "Passed: $passed | Failed: $failed | Skipped: $skipped" -ForegroundColor Gray

if ($failed -gt 0 -or $skipped -gt 0) {
    Write-Host "=== Testing Failed ===" -ForegroundColor Red
    Wait-IfRequested
    exit 1
}

Write-Host "=== Testing Complete: all tests passed ===" -ForegroundColor Green
Wait-IfRequested
exit 0
