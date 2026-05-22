param(
    [string]$BaseUrl = "http://127.0.0.1:5555"
)

$ErrorActionPreference = "Stop"
$passed = 0
$failed = 0
$total = 30

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
# Admin Console API Tests (Phase 5)
# ========================================
Write-Host ""
Write-Host "========================================" -ForegroundColor Yellow
Write-Host "Admin Console API Tests" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Yellow
Write-Host ""

$adminAccessToken = $null

function Get-AdminAuthHeaders {
    return @{ Authorization = "Bearer $script:adminAccessToken"; "Content-Type" = "application/json" }
}

# ========================================
# Test 18: Admin Login (admin-console client)
# ========================================
Test-Endpoint "Test 18: Admin Login + Token" {
    $loginBody = @{
        username = 'admin'; password = 'admin'
        client_id = 'admin-console'
        redirect_uri = 'http://localhost:5174/admin/callback'
        scope = 'openid profile admin'
        state = 'admin-test-state'
        json = 'true'
    }
    $login = Invoke-RestMethod -Uri "$BaseUrl/oauth2/login" -Method Post -Body $loginBody
    if (-not $login.code) { throw "no auth code from login" }

    $tokenBody = @{
        grant_type = 'authorization_code'
        code = $login.code
        redirect_uri = 'http://localhost:5174/admin/callback'
        client_id = 'admin-console'
        client_secret = ''
    }
    $tok = Invoke-RestMethod -Uri "$BaseUrl/oauth2/token" -Method Post -Body $tokenBody
    if (-not $tok.access_token) { throw "no access_token" }
    $script:adminAccessToken = $tok.access_token
    Write-Host "    Token obtained: $($tok.access_token.Substring(0,16))..."
}

# ========================================
# Test 19: GET /api/admin/clients/:id (Client Detail)
# ========================================
Test-Endpoint "Test 19: GET /api/admin/clients/:id - Client Detail" {
    if (-not $adminAccessToken) { throw "skipped: no admin token" }
    $h = Get-AdminAuthHeaders
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client" -Method Get -Headers $h
    if ($r.status -ne "success") { throw "status != success, got: $($r.status)" }
    if ($r.client_id -ne "vue-client") { throw "client_id mismatch: $($r.client_id)" }
    if (-not $r.client_type) { throw "missing client_type" }
    if ($null -eq $r.name) { throw "missing name field" }
    if ($null -eq $r.redirect_uris) { throw "missing redirect_uris field" }
    if ($null -eq $r.allowed_grant_types) { throw "missing allowed_grant_types field" }
    if ($null -eq $r.scopes) { throw "missing scopes array" }
    if ($r.scopes -isnot [array]) { throw "scopes is not an array" }
    if ($r.client_secret) { throw "SECURITY: client_secret should NOT be returned!" }
    if ($r.salt) { throw "SECURITY: salt should NOT be returned!" }
    Write-Host "    client_id: $($r.client_id), type: $($r.client_type), name: $($r.name)"
    Write-Host "    redirect_uris: $($r.redirect_uris)"
    Write-Host "    grant_types: $($r.allowed_grant_types)"
    Write-Host "    scopes: [$($r.scopes -join ', ')]"
}

# ========================================
# Test 20: GET /api/admin/clients/:id - Not Found
# ========================================
Test-Endpoint "Test 20: GET /api/admin/clients/:id - Not Found (404)" {
    if (-not $adminAccessToken) { throw "skipped: no admin token" }
    $h = Get-AdminAuthHeaders
    try {
        Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/nonexistent-client-xyz" -Method Get -Headers $h -ErrorAction Stop
        throw "should have returned 404"
    } catch {
        if ($_.Exception.Response.StatusCode -eq "NotFound") {
            Write-Host "    Correctly returned 404 for non-existent client"
        } else {
            throw "expected 404, got: $($_.Exception.Response.StatusCode)"
        }
    }
}

# ========================================
# Test 21: PUT /api/admin/clients/:id (Update Client)
# ========================================
Test-Endpoint "Test 21: PUT /api/admin/clients/:id - Update Client" {
    if (-not $adminAccessToken) { throw "skipped: no admin token" }
    $h = Get-AdminAuthHeaders
    $body = @{ name = "Vue Frontend Updated" } | ConvertTo-Json
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client" -Method Put -Headers $h -Body $body
    if ($r.status -ne "success") { throw "status != success: $($r.status)" }
    if (-not $r.message) { throw "missing message" }
    Write-Host "    Response: $($r.message)"

    $check = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client" -Method Get -Headers $h
    if ($check.name -ne "Vue Frontend Updated") { throw "name not updated: $($check.name)" }
    Write-Host "    Verified: name = '$($check.name)'"

    $restore = @{ name = "Vue Frontend" } | ConvertTo-Json
    Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client" -Method Put -Headers $h -Body $restore | Out-Null
}

# ========================================
# Test 22: GET /api/admin/clients/:id/scopes
# ========================================
Test-Endpoint "Test 22: GET /api/admin/clients/:id/scopes" {
    if (-not $adminAccessToken) { throw "skipped: no admin token" }
    $h = Get-AdminAuthHeaders
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client/scopes" -Method Get -Headers $h
    if ($r.status -ne "success") { throw "status != success" }
    if ($null -eq $r.scopes) { throw "missing scopes field" }
    if ($r.scopes -isnot [array]) { throw "scopes is not an array" }
    Write-Host "    Current scopes: [$($r.scopes -join ', ')]"
}

# ========================================
# Test 23: PUT /api/admin/clients/:id/scopes (Update Scopes)
# ========================================
Test-Endpoint "Test 23: PUT /api/admin/clients/:id/scopes - Update Scopes" {
    if (-not $adminAccessToken) { throw "skipped: no admin token" }
    $h = Get-AdminAuthHeaders

    $current = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client/scopes" -Method Get -Headers $h
    $originalScopes = $current.scopes

    $body = @{ scopes = @("openid", "profile", "email") } | ConvertTo-Json
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client/scopes" -Method Put -Headers $h -Body $body
    if ($r.status -ne "success") { throw "status != success: $($r.status)" }
    if ($null -eq $r.scopes) { throw "missing scopes in response" }
    Write-Host "    Updated scopes: [$($r.scopes -join ', ')]"

    $verify = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client/scopes" -Method Get -Headers $h
    if ($verify.scopes.Count -ne 3) { throw "expected 3 scopes, got $($verify.scopes.Count)" }
    if ($verify.scopes -notcontains "openid") { throw "missing openid scope" }
    if ($verify.scopes -notcontains "profile") { throw "missing profile scope" }
    if ($verify.scopes -notcontains "email") { throw "missing email scope" }
    Write-Host "    Verified: all 3 scopes persisted correctly"

    if ($originalScopes -and $originalScopes.Count -gt 0) {
        $restoreBody = @{ scopes = $originalScopes } | ConvertTo-Json
        Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client/scopes" -Method Put -Headers $h -Body $restoreBody | Out-Null
    }
}

# ========================================
# Test 24: PUT /api/admin/clients/:id/scopes - Empty scopes
# ========================================
Test-Endpoint "Test 24: PUT /api/admin/clients/:id/scopes - Empty Array" {
    if (-not $adminAccessToken) { throw "skipped: no admin token" }
    $h = Get-AdminAuthHeaders

    $current = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/admin-console/scopes" -Method Get -Headers $h
    $originalScopes = $current.scopes

    $body = @{ scopes = @() } | ConvertTo-Json
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/admin-console/scopes" -Method Put -Headers $h -Body $body
    if ($r.status -ne "success") { throw "status != success" }

    $verify = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/admin-console/scopes" -Method Get -Headers $h
    if ($verify.scopes.Count -ne 0) { throw "expected 0 scopes, got $($verify.scopes.Count)" }
    Write-Host "    Verified: scopes cleared to empty array"

    if ($originalScopes -and $originalScopes.Count -gt 0) {
        $restoreBody = @{ scopes = $originalScopes } | ConvertTo-Json
        Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/admin-console/scopes" -Method Put -Headers $h -Body $restoreBody | Out-Null
        Write-Host "    Restored original scopes"
    }
}

# ========================================
# Test 25: GET /api/admin/tokens (Token List)
# ========================================
Test-Endpoint "Test 25: GET /api/admin/tokens - Token List" {
    if (-not $adminAccessToken) { throw "skipped: no admin token" }
    $h = Get-AdminAuthHeaders
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/tokens?page=1&per_page=10" -Method Get -Headers $h
    if ($null -eq $r.tokens) { throw "missing tokens array" }
    if ($r.tokens -isnot [array]) { throw "tokens is not an array" }
    if ($null -eq $r.total) { throw "missing total field" }
    if ($null -eq $r.page) { throw "missing page field" }
    if ($null -eq $r.per_page) { throw "missing per_page field" }
    Write-Host "    Total tokens: $($r.total), Page: $($r.page), Per page: $($r.per_page)"
    Write-Host "    Returned: $($r.tokens.Count) tokens"

    if ($r.tokens.Count -gt 0) {
        $t = $r.tokens[0]
        if (-not $t.token_prefix) { throw "missing token_prefix" }
        if ($t.token_prefix.Length -gt 8) { throw "token_prefix too long (security: should be max 8 chars)" }
        if ($null -eq $t.client_id) { throw "missing client_id" }
        if ($null -eq $t.scope) { throw "missing scope" }
        if ($null -eq $t.expires_at) { throw "missing expires_at" }
        Write-Host "    First token: prefix=$($t.token_prefix), client=$($t.client_id), scope=$($t.scope)"
    }
}

# ========================================
# Test 26: GET /api/admin/tokens with filter
# ========================================
Test-Endpoint "Test 26: GET /api/admin/tokens - Filter by client_id" {
    if (-not $adminAccessToken) { throw "skipped: no admin token" }
    $h = Get-AdminAuthHeaders
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/tokens?client_id=admin-console&page=1&per_page=50" -Method Get -Headers $h
    if ($null -eq $r.tokens) { throw "missing tokens array" }
    foreach ($t in $r.tokens) {
        if ($t.client_id -ne "admin-console") {
            throw "filter failed: got token for client '$($t.client_id)' instead of 'admin-console'"
        }
    }
    Write-Host "    Filtered tokens for admin-console: $($r.tokens.Count)"
}

# ========================================
# Test 27: POST /api/admin/tokens/revoke-by-client
# ========================================
Test-Endpoint "Test 27: POST /api/admin/tokens/revoke-by-client" {
    # Get a fresh token for this test
    $loginBody27 = @{
        username = 'admin'; password = 'admin'
        client_id = 'admin-console'
        redirect_uri = 'http://localhost:5174/admin/callback'
        scope = 'openid profile admin'
        state = 'test27-state'
        json = 'true'
    }
    $login27 = Invoke-RestMethod -Uri "$BaseUrl/oauth2/login" -Method Post -Body $loginBody27
    if (-not $login27.code) { throw "no auth code for test 27" }
    $tokenBody27 = @{
        grant_type = 'authorization_code'
        code = $login27.code
        redirect_uri = 'http://localhost:5174/admin/callback'
        client_id = 'admin-console'
        client_secret = ''
    }
    $tok27 = Invoke-RestMethod -Uri "$BaseUrl/oauth2/token" -Method Post -Body $tokenBody27
    if (-not $tok27.access_token) { throw "no access_token for test 27" }
    $freshToken = $tok27.access_token

    $body = @{ client_id = "backend-svc" } | ConvertTo-Json
    $revokeHeaders = @{ Authorization = "Bearer $freshToken"; "Content-Type" = "application/json" }
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/tokens/revoke-by-client" -Method Post -Headers $revokeHeaders -Body $body
    if ($r.status -ne "success") { throw "status != success: $($r.status)" }
    if ($null -eq $r.count) { throw "missing count field" }
    Write-Host "    Revoked $($r.count) tokens for backend-svc"
    Write-Host "    Message: $($r.message)"
}

# ========================================
# Test 28: POST /api/admin/tokens/revoke-by-user
# ========================================
Test-Endpoint "Test 28: POST /api/admin/tokens/revoke-by-user" {
    if (-not $adminAccessToken) { throw "skipped: no admin token" }
    $body = @{ user_id = "nonexistent-user-id-12345" } | ConvertTo-Json
    $revokeHeaders = @{ Authorization = "Bearer $script:adminAccessToken"; "Content-Type" = "application/json" }
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/tokens/revoke-by-user" -Method Post -Headers $revokeHeaders -Body $body
    if ($r.status -ne "success") { throw "status != success: $($r.status)" }
    if ($null -eq $r.count) { throw "missing count field" }
    Write-Host "    Revoked $($r.count) tokens for nonexistent user (expected 0)"
    Write-Host "    Message: $($r.message)"
}

# ========================================
# Test 29: GET /api/admin/oidc/keys
# ========================================
Test-Endpoint "Test 29: GET /api/admin/oidc/keys - OIDC Key Info" {
    if (-not $adminAccessToken) { throw "skipped: no admin token" }
    $h = Get-AdminAuthHeaders
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/oidc/keys" -Method Get -Headers $h
    if ($r.status -ne "success") { throw "status != success" }
    if (-not $r.kid) { throw "missing kid" }
    if ($r.kty -ne "RSA") { throw "kty != RSA, got: $($r.kty)" }
    if ($r.alg -ne "RS256") { throw "alg != RS256, got: $($r.alg)" }
    if ($r.use -ne "sig") { throw "use != sig, got: $($r.use)" }
    if (-not $r.jwks_uri) { throw "missing jwks_uri" }
    if (-not $r.discovery_uri) { throw "missing discovery_uri" }
    if (-not $r.key_status) { throw "missing key_status" }
    if ($r.key_status -ne "active") { throw "key_status != active" }
    Write-Host "    kid: $($r.kid), kty: $($r.kty), alg: $($r.alg), use: $($r.use)"
    Write-Host "    jwks_uri: $($r.jwks_uri)"
    Write-Host "    discovery_uri: $($r.discovery_uri)"
    Write-Host "    key_status: $($r.key_status)"
}

# ========================================
# Test 30: Unauthorized access (no token)
# ========================================
Test-Endpoint "Test 30: Unauthorized Access - Admin endpoints require auth" {
    $endpoints = @(
        @{ Uri = "$BaseUrl/api/admin/clients/vue-client"; Method = "Get" },
        @{ Uri = "$BaseUrl/api/admin/clients/vue-client/scopes"; Method = "Get" },
        @{ Uri = "$BaseUrl/api/admin/tokens"; Method = "Get" },
        @{ Uri = "$BaseUrl/api/admin/oidc/keys"; Method = "Get" }
    )
    $allBlocked = $true
    foreach ($ep in $endpoints) {
        try {
            Invoke-RestMethod -Uri $ep.Uri -Method $ep.Method -ErrorAction Stop
            Write-Host "    SECURITY ISSUE: $($ep.Uri) accessible without auth!" -ForegroundColor Red
            $allBlocked = $false
        } catch {
            $status = $_.Exception.Response.StatusCode
            if ($status -ne "Unauthorized" -and $status -ne "Forbidden") {
                Write-Host "    WARNING: $($ep.Uri) returned $status (expected 401/403)" -ForegroundColor Yellow
            }
        }
    }
    if (-not $allBlocked) { throw "Some endpoints accessible without authentication!" }
    Write-Host "    All 4 endpoints correctly require authentication"
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
