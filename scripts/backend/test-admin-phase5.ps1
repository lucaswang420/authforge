param(
    [string]$BaseUrl = "http://127.0.0.1:5555"
)

$ErrorActionPreference = "Stop"
$passed = 0
$failed = 0
$total = 12

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
# Setup: Get admin access token
# ========================================
Write-Host "========================================" -ForegroundColor Yellow
Write-Host "Admin Console Phase 5 API Tests ($total tests)" -ForegroundColor Yellow
Write-Host "========================================" -ForegroundColor Yellow
Write-Host "Base URL: $BaseUrl"
Write-Host ""

$accessToken = $null

Test-Endpoint "Setup: Admin Login + Token" {
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
    $script:accessToken = $tok.access_token
    Write-Host "    Token obtained: $($tok.access_token.Substring(0,16))..."
}

$headers = $null
function Get-AuthHeaders {
    return @{ Authorization = "Bearer $script:accessToken"; "Content-Type" = "application/json" }
}

# ========================================
# Test 1: GET /api/admin/clients/:id (Client Detail)
# ========================================
Test-Endpoint "Test 1: GET /api/admin/clients/:id - Client Detail" {
    if (-not $accessToken) { throw "skipped: no token" }
    $h = Get-AuthHeaders
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client" -Method Get -Headers $h
    # Verify all expected fields
    if ($r.status -ne "success") { throw "status != success, got: $($r.status)" }
    if ($r.client_id -ne "vue-client") { throw "client_id mismatch: $($r.client_id)" }
    if (-not $r.client_type) { throw "missing client_type" }
    if ($null -eq $r.name) { throw "missing name field" }
    if ($null -eq $r.redirect_uris) { throw "missing redirect_uris field" }
    if ($null -eq $r.allowed_grant_types) { throw "missing allowed_grant_types field" }
    if ($null -eq $r.scopes) { throw "missing scopes array" }
    if ($r.scopes -isnot [array]) { throw "scopes is not an array" }
    # Ensure secret is NOT returned
    if ($r.client_secret) { throw "SECURITY: client_secret should NOT be returned!" }
    if ($r.salt) { throw "SECURITY: salt should NOT be returned!" }
    Write-Host "    client_id: $($r.client_id), type: $($r.client_type), name: $($r.name)"
    Write-Host "    redirect_uris: $($r.redirect_uris)"
    Write-Host "    grant_types: $($r.allowed_grant_types)"
    Write-Host "    scopes: [$($r.scopes -join ', ')]"
}

# ========================================
# Test 2: GET /api/admin/clients/:id - Not Found
# ========================================
Test-Endpoint "Test 2: GET /api/admin/clients/:id - Not Found (404)" {
    if (-not $accessToken) { throw "skipped: no token" }
    $h = Get-AuthHeaders
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
# Test 3: PUT /api/admin/clients/:id (Update Client)
# ========================================
Test-Endpoint "Test 3: PUT /api/admin/clients/:id - Update Client" {
    if (-not $accessToken) { throw "skipped: no token" }
    $h = Get-AuthHeaders
    $body = @{ name = "Vue Frontend Updated" } | ConvertTo-Json
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client" -Method Put -Headers $h -Body $body
    if ($r.status -ne "success") { throw "status != success: $($r.status)" }
    if (-not $r.message) { throw "missing message" }
    Write-Host "    Response: $($r.message)"

    # Verify the update persisted
    $check = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client" -Method Get -Headers $h
    if ($check.name -ne "Vue Frontend Updated") { throw "name not updated: $($check.name)" }
    Write-Host "    Verified: name = '$($check.name)'"

    # Restore original name
    $restore = @{ name = "Vue Frontend" } | ConvertTo-Json
    Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client" -Method Put -Headers $h -Body $restore | Out-Null
}

# ========================================
# Test 4: GET /api/admin/clients/:id/scopes
# ========================================
Test-Endpoint "Test 4: GET /api/admin/clients/:id/scopes" {
    if (-not $accessToken) { throw "skipped: no token" }
    $h = Get-AuthHeaders
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client/scopes" -Method Get -Headers $h
    if ($r.status -ne "success") { throw "status != success" }
    if ($null -eq $r.scopes) { throw "missing scopes field" }
    if ($r.scopes -isnot [array]) { throw "scopes is not an array" }
    Write-Host "    Current scopes: [$($r.scopes -join ', ')]"
}

# ========================================
# Test 5: PUT /api/admin/clients/:id/scopes (Update Scopes)
# ========================================
Test-Endpoint "Test 5: PUT /api/admin/clients/:id/scopes - Update Scopes" {
    if (-not $accessToken) { throw "skipped: no token" }
    $h = Get-AuthHeaders

    # First get current scopes to restore later
    $current = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client/scopes" -Method Get -Headers $h
    $originalScopes = $current.scopes

    # Update to new scopes
    $body = @{ scopes = @("openid", "profile", "email") } | ConvertTo-Json
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client/scopes" -Method Put -Headers $h -Body $body
    if ($r.status -ne "success") { throw "status != success: $($r.status)" }
    if ($null -eq $r.scopes) { throw "missing scopes in response" }
    Write-Host "    Updated scopes: [$($r.scopes -join ', ')]"

    # Verify persistence
    $verify = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client/scopes" -Method Get -Headers $h
    if ($verify.scopes.Count -ne 3) { throw "expected 3 scopes, got $($verify.scopes.Count)" }
    $hasOpenid = $verify.scopes -contains "openid"
    $hasProfile = $verify.scopes -contains "profile"
    $hasEmail = $verify.scopes -contains "email"
    if (-not $hasOpenid) { throw "missing openid scope" }
    if (-not $hasProfile) { throw "missing profile scope" }
    if (-not $hasEmail) { throw "missing email scope" }
    Write-Host "    Verified: all 3 scopes persisted correctly"

    # Restore original scopes
    if ($originalScopes -and $originalScopes.Count -gt 0) {
        $restoreBody = @{ scopes = $originalScopes } | ConvertTo-Json
        Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/vue-client/scopes" -Method Put -Headers $h -Body $restoreBody | Out-Null
    }
}

# ========================================
# Test 6: PUT /api/admin/clients/:id/scopes - Empty scopes
# ========================================
Test-Endpoint "Test 6: PUT /api/admin/clients/:id/scopes - Empty Array" {
    if (-not $accessToken) { throw "skipped: no token" }
    $h = Get-AuthHeaders

    # Save current
    $current = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/admin-console/scopes" -Method Get -Headers $h
    $originalScopes = $current.scopes

    # Set empty
    $body = @{ scopes = @() } | ConvertTo-Json
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/admin-console/scopes" -Method Put -Headers $h -Body $body
    if ($r.status -ne "success") { throw "status != success" }

    # Verify empty
    $verify = Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/admin-console/scopes" -Method Get -Headers $h
    if ($verify.scopes.Count -ne 0) { throw "expected 0 scopes, got $($verify.scopes.Count)" }
    Write-Host "    Verified: scopes cleared to empty array"

    # Restore
    if ($originalScopes -and $originalScopes.Count -gt 0) {
        $restoreBody = @{ scopes = $originalScopes } | ConvertTo-Json
        Invoke-RestMethod -Uri "$BaseUrl/api/admin/clients/admin-console/scopes" -Method Put -Headers $h -Body $restoreBody | Out-Null
        Write-Host "    Restored original scopes"
    }
}

# ========================================
# Test 7: GET /api/admin/tokens (Token List)
# ========================================
Test-Endpoint "Test 7: GET /api/admin/tokens - Token List" {
    if (-not $accessToken) { throw "skipped: no token" }
    $h = Get-AuthHeaders
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/tokens?page=1&per_page=10" -Method Get -Headers $h
    if ($null -eq $r.tokens) { throw "missing tokens array" }
    if ($r.tokens -isnot [array]) { throw "tokens is not an array" }
    if ($null -eq $r.total) { throw "missing total field" }
    if ($null -eq $r.page) { throw "missing page field" }
    if ($null -eq $r.per_page) { throw "missing per_page field" }
    Write-Host "    Total tokens: $($r.total), Page: $($r.page), Per page: $($r.per_page)"
    Write-Host "    Returned: $($r.tokens.Count) tokens"

    # Verify token fields (if any tokens exist)
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
# Test 8: GET /api/admin/tokens with filter
# ========================================
Test-Endpoint "Test 8: GET /api/admin/tokens - Filter by client_id" {
    if (-not $accessToken) { throw "skipped: no token" }
    $h = Get-AuthHeaders
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/tokens?client_id=admin-console&page=1&per_page=50" -Method Get -Headers $h
    if ($null -eq $r.tokens) { throw "missing tokens array" }
    # All returned tokens should belong to admin-console
    foreach ($t in $r.tokens) {
        if ($t.client_id -ne "admin-console") {
            throw "filter failed: got token for client '$($t.client_id)' instead of 'admin-console'"
        }
    }
    Write-Host "    Filtered tokens for admin-console: $($r.tokens.Count)"
}

# ========================================
# Test 9: POST /api/admin/tokens/revoke-by-client
# ========================================
Test-Endpoint "Test 9: POST /api/admin/tokens/revoke-by-client" {
    # Get a fresh token for this test
    $loginBody9 = @{
        username = 'admin'; password = 'admin'
        client_id = 'admin-console'
        redirect_uri = 'http://localhost:5174/admin/callback'
        scope = 'openid profile admin'
        state = 'test9-state'
        json = 'true'
    }
    $login9 = Invoke-RestMethod -Uri "$BaseUrl/oauth2/login" -Method Post -Body $loginBody9
    if (-not $login9.code) { throw "no auth code for test 9" }
    $tokenBody9 = @{
        grant_type = 'authorization_code'
        code = $login9.code
        redirect_uri = 'http://localhost:5174/admin/callback'
        client_id = 'admin-console'
        client_secret = ''
    }
    $tok9 = Invoke-RestMethod -Uri "$BaseUrl/oauth2/token" -Method Post -Body $tokenBody9
    if (-not $tok9.access_token) { throw "no access_token for test 9" }
    $freshToken = $tok9.access_token

    # Revoke all tokens for a non-critical client (backend-svc)
    $body = @{ client_id = "backend-svc" } | ConvertTo-Json
    $revokeHeaders = @{ Authorization = "Bearer $freshToken"; "Content-Type" = "application/json" }
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/tokens/revoke-by-client" -Method Post -Headers $revokeHeaders -Body $body
    if ($r.status -ne "success") { throw "status != success: $($r.status)" }
    if ($null -eq $r.count) { throw "missing count field" }
    Write-Host "    Revoked $($r.count) tokens for backend-svc"
    Write-Host "    Message: $($r.message)"
}

# ========================================
# Test 10: POST /api/admin/tokens/revoke-by-user
# ========================================
Test-Endpoint "Test 10: POST /api/admin/tokens/revoke-by-user" {
    if (-not $accessToken) { throw "skipped: no token" }
    $h = Get-AuthHeaders
    # Use a non-existent user to avoid revoking our own token
    $body = @{ user_id = "nonexistent-user-id-12345" } | ConvertTo-Json
    $revokeHeaders = @{ Authorization = "Bearer $script:accessToken"; "Content-Type" = "application/json" }
    $r = Invoke-RestMethod -Uri "$BaseUrl/api/admin/tokens/revoke-by-user" -Method Post -Headers $revokeHeaders -Body $body
    if ($r.status -ne "success") { throw "status != success: $($r.status)" }
    if ($null -eq $r.count) { throw "missing count field" }
    Write-Host "    Revoked $($r.count) tokens for nonexistent user (expected 0)"
    Write-Host "    Message: $($r.message)"
}

# ========================================
# Test 11: GET /api/admin/oidc/keys
# ========================================
Test-Endpoint "Test 11: GET /api/admin/oidc/keys - OIDC Key Info" {
    if (-not $accessToken) { throw "skipped: no token" }
    $h = Get-AuthHeaders
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
# Test 12: Unauthorized access (no token)
# ========================================
Test-Endpoint "Test 12: Unauthorized Access - All Phase 5 endpoints require auth" {
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
# Cleanup: Reset admin account lockout
# ========================================
Write-Host ""
Write-Host "Cleaning up: Resetting admin account lockout..." -ForegroundColor Cyan
try {
    # Try Docker first
    $containerName = docker ps --format "{{.Names}}" 2>$null | Select-String -Pattern "postgres"
    if ($containerName) {
        docker exec $containerName psql -U oauth2_user -d oauth2_db -c "UPDATE users SET failed_login_count = 0, locked_until = 0 WHERE username='admin';" 2>$null | Out-Null
        Write-Host "Admin account lockout reset successfully (Docker)" -ForegroundColor Green
    } else {
        # Try local PostgreSQL
        $env:PGPASSWORD = "your_password"  # 修改为你的数据库密码
        psql -U oauth2_user -d oauth2_db -h localhost -c "UPDATE users SET failed_login_count = 0, locked_until = 0 WHERE username='admin';" 2>$null | Out-Null
        if ($LASTEXITCODE -eq 0) {
            Write-Host "Admin account lockout reset successfully (Local PostgreSQL)" -ForegroundColor Green
        } else {
            Write-Host "Warning: Could not reset admin account. Please run manually:" -ForegroundColor Yellow
            Write-Host "  psql -U oauth2_user -d oauth2_db -c `"UPDATE users SET failed_login_count = 0, locked_until = 0 WHERE username='admin';`"" -ForegroundColor Yellow
        }
        $env:PGPASSWORD = $null
    }
} catch {
    Write-Host "Warning: Failed to reset admin account: $($_.Exception.Message)" -ForegroundColor Yellow
    Write-Host "Please run manually:" -ForegroundColor Yellow
    Write-Host "  psql -U oauth2_user -d oauth2_db -c `"UPDATE users SET failed_login_count = 0, locked_until = 0 WHERE username='admin';`"" -ForegroundColor Yellow
}

# ========================================
# Summary
# ========================================
Write-Host ""
Write-Host "========================================"
Write-Host "Admin Phase 5 API Tests: $passed/$total passed, $failed failed"
Write-Host "========================================"

if ($failed -gt 0) {
    Write-Host "FAILED" -ForegroundColor Red
    exit 1
} else {
    Write-Host "ALL PASSED" -ForegroundColor Green
    exit 0
}
