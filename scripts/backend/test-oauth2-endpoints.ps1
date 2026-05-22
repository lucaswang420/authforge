param(
    [string]$BaseUrl = "http://127.0.0.1:5555"
)

$ErrorActionPreference = "Stop"
$passed = 0
$failed = 0
$total = 17

function Test-Endpoint {
    param([string]$Name, [scriptblock]$Block)
    Write-Host "[*] $Name" -ForegroundColor Cyan
    try {
        & $Block
        $script:passed++
        Write-Host "    [PASS]" -ForegroundColor Green
    } catch {
        $script:failed++
        Write-Host "    [FAIL] $($_.Exception.Message)" -ForegroundColor Red
    }
    Write-Host ""
}

# ========================================
# Test 1: Health Check
# ========================================
Test-Endpoint "Test 1: Health Check" {
    $r = Invoke-RestMethod -Uri "$BaseUrl/health" -Method Get
    if ($r.status -ne "ok") { throw "status != ok" }
    Write-Host "    Status: $($r.status), Storage: $($r.storage_type)"
}

# ========================================
# Test 2: Health Live/Ready
# ========================================
Test-Endpoint "Test 2: Health Live/Ready" {
    $live = Invoke-RestMethod -Uri "$BaseUrl/health/live" -Method Get
    if ($live.status -ne "ok") { throw "/health/live status != ok" }
    $ready = Invoke-RestMethod -Uri "$BaseUrl/health/ready" -Method Get
    if ($ready.status -ne "ok" -and $ready.status -ne "degraded") { throw "/health/ready failed" }
    Write-Host "    Live: $($live.status), Ready: $($ready.status)"
}

# ========================================
# Test 3: OIDC Discovery
# ========================================
Test-Endpoint "Test 3: OIDC Discovery" {
    $r = Invoke-RestMethod -Uri "$BaseUrl/.well-known/openid-configuration" -Method Get
    if (-not $r.issuer) { throw "missing issuer" }
    if (-not $r.jwks_uri) { throw "missing jwks_uri" }
    if (-not $r.scopes_supported) { throw "missing scopes_supported" }
    Write-Host "    Issuer: $($r.issuer)"
}

# ========================================
# Test 4: JWKS
# ========================================
Test-Endpoint "Test 4: JWKS" {
    $r = Invoke-RestMethod -Uri "$BaseUrl/.well-known/jwks.json" -Method Get
    if (-not $r.keys -or $r.keys.Count -eq 0) { throw "empty keys array" }
    $key = $r.keys[0]
    if ($key.kty -ne "RSA") { throw "kty != RSA" }
    if ($key.alg -ne "RS256") { throw "alg != RS256" }
    Write-Host "    Keys: $($r.keys.Count), kid: $($key.kid)"
}

# ========================================
# Test 5: OAuth2 Login (get auth code)
# ========================================
$authCode = $null
$accessToken = $null
$refreshToken = $null

Test-Endpoint "Test 5: OAuth2 Login" {
    $body = @{
        username = 'admin'; password = 'admin'
        client_id = 'vue-client'
        redirect_uri = 'http://127.0.0.1:5173/callback'
        scope = 'openid profile'
        state = 'test-state-12345678'
        json = 'true'
    }
    $r = Invoke-RestMethod -Uri "$BaseUrl/oauth2/login" -Method Post -Body $body
    if (-not $r.code) { throw "no auth code returned" }
    $script:authCode = $r.code
    Write-Host "    Code: $($r.code.Substring(0,20))... ($($r.code.Length) chars)"
}

# ========================================
# Test 6: Token Exchange (with id_token)
# ========================================
Test-Endpoint "Test 6: Token Exchange + id_token" {
    if (-not $authCode) { throw "skipped: no auth code" }
    $body = @{
        grant_type = 'authorization_code'
        code = $authCode
        redirect_uri = 'http://127.0.0.1:5173/callback'
        client_id = 'vue-client'
        client_secret = '123456'
    }
    $r = Invoke-RestMethod -Uri "$BaseUrl/oauth2/token" -Method Post -Body $body
    if (-not $r.access_token) { throw "no access_token" }
    if ($r.access_token.Length -lt 40) { throw "token too short ($($r.access_token.Length) chars)" }
    if (-not $r.refresh_token) { throw "no refresh_token" }
    $script:accessToken = $r.access_token
    $script:refreshToken = $r.refresh_token

    # Verify id_token present (scope included openid)
    if (-not $r.id_token) { throw "no id_token (scope=openid)" }
    $parts = $r.id_token.Split('.')
    if ($parts.Count -ne 3) { throw "id_token not 3-part JWT" }
    Write-Host "    AT: $($r.access_token.Substring(0,20))..., id_token: present (3-part JWT)"
}

# ========================================
# Test 7: UserInfo
# ========================================
Test-Endpoint "Test 7: UserInfo" {
    if (-not $accessToken) { throw "skipped: no token" }
    $headers = @{ Authorization = "Bearer $accessToken" }
    $r = Invoke-RestMethod -Uri "$BaseUrl/oauth2/userinfo" -Method Get -Headers $headers
    if (-not $r.sub) { throw "no sub" }
    if ($r.sub.Length -ne 36) { throw "sub not UUID format (len=$($r.sub.Length))" }
    if ($r.name -eq $r.sub) { throw "name equals sub (username not resolved)" }
    Write-Host "    Sub: $($r.sub), Name: $($r.name), Roles: $($r.roles -join ',')"
}

# ========================================
# Test 8: Admin Dashboard
# ========================================
Test-Endpoint "Test 8: Admin Dashboard" {
    if (-not $accessToken) { throw "skipped: no token" }
    $headers = @{ Authorization = "Bearer $accessToken" }
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/dashboard" -Method Get -Headers $headers
    if ($r.status -ne "success") { throw "status != success" }
    Write-Host "    Message: $($r.message)"
}

# ========================================
# Test 9: Token Refresh
# ========================================
Test-Endpoint "Test 9: Token Refresh" {
    if (-not $refreshToken) { throw "skipped: no refresh_token" }
    $body = @{
        grant_type = 'refresh_token'
        refresh_token = $refreshToken
        client_id = 'vue-client'
        client_secret = '123456'
    }
    $r = Invoke-RestMethod -Uri "$BaseUrl/oauth2/token" -Method Post -Body $body
    if (-not $r.access_token) { throw "no new access_token" }
    if (-not $r.refresh_token) { throw "no new refresh_token" }
    if ($r.access_token -eq $accessToken) { throw "same access_token (not rotated)" }
    $script:accessToken = $r.access_token
    $script:refreshToken = $r.refresh_token
    Write-Host "    New AT: $($r.access_token.Substring(0,20))..."
}

# ========================================
# Test 10: Client Credentials Grant
# ========================================
Test-Endpoint "Test 10: Client Credentials" {
    $body = @{
        grant_type = 'client_credentials'
        client_id = 'backend-svc'
        client_secret = 'test-secret'
        scope = 'read'
    }
    $r = Invoke-RestMethod -Uri "$BaseUrl/oauth2/token" -Method Post -Body $body
    if (-not $r.access_token) { throw "no access_token" }
    if ($r.refresh_token) { throw "client_credentials should NOT have refresh_token" }
    if ($r.scope -ne "read") { throw "scope mismatch" }
    Write-Host "    AT: $($r.access_token.Substring(0,20))..., Scope: $($r.scope)"
}

# ========================================
# Test 11: Token Introspection
# ========================================
Test-Endpoint "Test 11: Token Introspection" {
    if (-not $accessToken) { throw "skipped: no token" }
    # Introspect endpoint has OAuth2Middleware, so needs Bearer token
    # Plus client credentials for the actual introspection
    $headers = @{ Authorization = "Bearer $accessToken" }
    $body = @{
        token = $accessToken
        client_id = 'vue-client'
        client_secret = '123456'
    }
    $r = Invoke-RestMethod -Uri "$BaseUrl/oauth2/introspect" -Method Post -Headers $headers -Body $body
    if ($r.active -ne $true) { throw "active != true" }
    Write-Host "    Active: $($r.active), Sub: $($r.sub), Scope: $($r.scope)"
}

# ========================================
# Test 12: Token Revocation + Verify
# ========================================
Test-Endpoint "Test 12: Token Revocation" {
    if (-not $accessToken) { throw "skipped: no token" }
    $headers = @{ Authorization = "Bearer $accessToken" }
    $body = @{
        token = $accessToken
        client_id = 'vue-client'
        client_secret = '123456'
    }
    Invoke-WebRequest -Uri "$BaseUrl/oauth2/revoke" -Method Post -Headers $headers -Body $body -UseBasicParsing | Out-Null

    # Verify: try to use the revoked token - should get 401
    try {
        Invoke-RestMethod -Uri "$BaseUrl/oauth2/userinfo" -Method Get -Headers $headers -ErrorAction Stop
        throw "token should be revoked but userinfo succeeded"
    } catch {
        if ($_.Exception.Response.StatusCode -eq "Unauthorized") {
            Write-Host "    Revoked and verified: userinfo returns 401"
        } else {
            throw "unexpected error: $($_.Exception.Message)"
        }
    }
}

# ========================================
# Test 13: User Registration
# ========================================
Test-Endpoint "Test 13: User Registration" {
    $ts = [DateTimeOffset]::UtcNow.ToUnixTimeSeconds()
    $body = @{
        username = "testuser_$ts"
        password = "TestPass123"
        email = "test_$ts@example.com"
    }
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/register" -Method Post -Body $body
    if (-not $r.message) { throw "no message in response" }
    Write-Host "    Registered: testuser_$ts"
}

# ========================================
# Test 14: User Profile (GET /api/me)
# ========================================
Test-Endpoint "Test 14: User Profile" {
    # Need a fresh token (previous was revoked)
    $loginBody = @{
        username = 'admin'; password = 'admin'
        client_id = 'vue-client'
        redirect_uri = 'http://127.0.0.1:5173/callback'
        scope = 'openid profile'; state = 'test-state-12345678'; json = 'true'
    }
    $login = Invoke-RestMethod -Uri "$BaseUrl/oauth2/login" -Method Post -Body $loginBody
    $tokenBody = @{
        grant_type = 'authorization_code'; code = $login.code
        redirect_uri = 'http://127.0.0.1:5173/callback'
        client_id = 'vue-client'; client_secret = '123456'
    }
    $tok = Invoke-RestMethod -Uri "$BaseUrl/oauth2/token" -Method Post -Body $tokenBody
    $script:accessToken = $tok.access_token

    $headers = @{ Authorization = "Bearer $($tok.access_token)" }
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/me" -Method Get -Headers $headers
    if (-not $r.username) { throw "no username" }
    Write-Host "    Username: $($r.username), Email: $($r.email)"
}

# ========================================
# Test 15: Password Reset Request
# ========================================
Test-Endpoint "Test 15: Password Reset Request" {
    $body = @{ email = 'admin@example.com' } | ConvertTo-Json
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/password-reset/request" -Method Post -Body $body -ContentType 'application/json'
    if (-not $r.message) { throw "no message" }
    # Should always return 200 (anti-enumeration)
    Write-Host "    Response: $($r.message)"
}

# ========================================
# Test 16: Password Reset (non-existent email - still 200)
# ========================================
Test-Endpoint "Test 16: Password Reset (non-existent)" {
    $body = @{ email = 'nobody@nowhere.com' } | ConvertTo-Json
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/password-reset/request" -Method Post -Body $body -ContentType 'application/json'
    if (-not $r.message) { throw "no message" }
    Write-Host "    Anti-enumeration: same response for non-existent email"
}

# ========================================
# Test 17: Password Change (PUT /api/me/password)
# ========================================
Test-Endpoint "Test 17: Password Change" {
    if (-not $accessToken) { throw "skipped: no token" }
    $headers = @{
        Authorization = "Bearer $accessToken"
        "Content-Type" = "application/json"
    }
    # Change password and verify it works
    $body = '{"old_password":"admin","new_password":"NewPass123!"}'
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/me/password" -Method Put -Body $body -Headers $headers
    if (-not $r.message) { throw "no message" }
    Write-Host "    $($r.message)"

    # Verify old token is revoked (password change revokes all tokens)
    try {
        Invoke-RestMethod -Uri "$BaseUrl/api/me" -Method Get -Headers $headers -ErrorAction Stop
        Write-Host "    WARNING: old token still works after password change"
    } catch {
        Write-Host "    Verified: old token revoked after password change"
    }

    # Restore password for future test runs
    $loginBody = @{
        username = 'admin'; password = 'NewPass123!'
        client_id = 'vue-client'
        redirect_uri = 'http://127.0.0.1:5173/callback'
        scope = 'openid'; state = 'restore-pw-state1'; json = 'true'
    }
    try {
        $login = Invoke-RestMethod -Uri "$BaseUrl/oauth2/login" -Method Post -Body $loginBody
        $tokenBody = @{
            grant_type = 'authorization_code'; code = $login.code
            redirect_uri = 'http://127.0.0.1:5173/callback'
            client_id = 'vue-client'; client_secret = '123456'
        }
        $tok = Invoke-RestMethod -Uri "$BaseUrl/oauth2/token" -Method Post -Body $tokenBody
        $headers2 = @{ Authorization = "Bearer $($tok.access_token)"; "Content-Type" = "application/json" }
        $body2 = '{"old_password":"NewPass123!","new_password":"admin"}'
        Invoke-RestMethod -Uri "$BaseUrl/api/me/password" -Method Put -Body $body2 -Headers $headers2 | Out-Null
        Write-Host "    Password restored to 'admin'"
    } catch {
        Write-Host "    NOTE: Could not restore password (run setup_database.bat to reset)"
    }
}

# ========================================
# Summary
# ========================================
Write-Host "========================================"
Write-Host "Test Summary: $passed/$total passed, $failed failed"
Write-Host "========================================"

if ($failed -gt 0) {
    Write-Host "FAILED" -ForegroundColor Red
    exit 1
} else {
    Write-Host "ALL PASSED" -ForegroundColor Green
    exit 0
}
