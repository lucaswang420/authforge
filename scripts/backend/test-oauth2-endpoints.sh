#!/usr/bin/env bash
# test-oauth2-endpoints.sh - OAuth2 endpoint tests (Linux/macOS)
# Equivalent of test-oauth2-endpoints.ps1
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
source "$SCRIPT_DIR/common-test-functions.sh"

BASE_URL="${1:-http://127.0.0.1:5555}"
ACCESS_TOKEN=""
REFRESH_TOKEN=""
AUTH_CODE=""

TOTAL=17

echo "========================================"
echo "Pre-test Setup"
echo "========================================"
reset_admin_account
echo ""

echo "========================================"
echo "OAuth2 Endpoints Tests ($TOTAL tests)"
echo "========================================"
echo "Base URL: $BASE_URL"
echo ""

# Test 1: Health Check
test_1() {
    local r
    r=$(curl -s "$BASE_URL/health")
    assert_json_field "$r" "status" "ok" || return 1
    echo "    Status: $(echo "$r" | jq -r '.status')"
}
run_test "Test 1: Health Check" test_1

# Test 2: Health Live/Ready
test_2() {
    local live ready
    live=$(curl -s "$BASE_URL/health/live")
    assert_json_field "$live" "status" "ok" || return 1
    ready=$(curl -s "$BASE_URL/health/ready")
    local ready_status
    ready_status=$(echo "$ready" | jq -r '.status')
    [ "$ready_status" = "ok" ] || [ "$ready_status" = "degraded" ] || { echo "    /health/ready failed"; return 1; }
    echo "    Live: ok, Ready: $ready_status"
}
run_test "Test 2: Health Live/Ready" test_2

# Test 3: OIDC Discovery
test_3() {
    local r
    r=$(curl -s "$BASE_URL/.well-known/openid-configuration")
    assert_json_exists "$r" "issuer" || return 1
    assert_json_exists "$r" "jwks_uri" || return 1
    assert_json_exists "$r" "scopes_supported" || return 1
    echo "    Issuer: $(echo "$r" | jq -r '.issuer')"
}
run_test "Test 3: OIDC Discovery" test_3

# Test 4: JWKS
test_4() {
    local r
    r=$(curl -s "$BASE_URL/.well-known/jwks.json")
    local count
    count=$(echo "$r" | jq '.keys | length')
    [ "$count" -gt 0 ] || { echo "    empty keys array"; return 1; }
    local kty alg
    kty=$(echo "$r" | jq -r '.keys[0].kty')
    alg=$(echo "$r" | jq -r '.keys[0].alg')
    [ "$kty" = "RSA" ] || { echo "    kty != RSA"; return 1; }
    [ "$alg" = "RS256" ] || { echo "    alg != RS256"; return 1; }
    echo "    Keys: $count, kid: $(echo "$r" | jq -r '.keys[0].kid')"
}
run_test "Test 4: JWKS" test_4

# Test 5: OAuth2 Login
test_5() {
    local r
    r=$(curl -s -X POST "$BASE_URL/oauth2/login" \
        -d "username=admin&password=admin&client_id=vue-client&redirect_uri=http://127.0.0.1:5173/callback&scope=openid+profile&state=test-state-12345678&json=true")
    AUTH_CODE=$(echo "$r" | jq -r '.code')
    [ -n "$AUTH_CODE" ] && [ "$AUTH_CODE" != "null" ] || { echo "    no auth code returned"; return 1; }
    echo "    Code: ${AUTH_CODE:0:20}... (${#AUTH_CODE} chars)"
}
run_test "Test 5: OAuth2 Login" test_5

# Test 6: Token Exchange + id_token
test_6() {
    [ -n "$AUTH_CODE" ] || { echo "    skipped: no auth code"; return 1; }
    local r
    r=$(curl -s -X POST "$BASE_URL/oauth2/token" \
        -d "grant_type=authorization_code&code=$AUTH_CODE&redirect_uri=http://127.0.0.1:5173/callback&client_id=vue-client&client_secret=123456")
    ACCESS_TOKEN=$(echo "$r" | jq -r '.access_token')
    REFRESH_TOKEN=$(echo "$r" | jq -r '.refresh_token')
    [ -n "$ACCESS_TOKEN" ] && [ "$ACCESS_TOKEN" != "null" ] || { echo "    no access_token"; return 1; }
    [ -n "$REFRESH_TOKEN" ] && [ "$REFRESH_TOKEN" != "null" ] || { echo "    no refresh_token"; return 1; }
    local id_token
    id_token=$(echo "$r" | jq -r '.id_token')
    [ -n "$id_token" ] && [ "$id_token" != "null" ] || { echo "    no id_token"; return 1; }
    echo "    AT: ${ACCESS_TOKEN:0:20}..., id_token: present"
}
run_test "Test 6: Token Exchange + id_token" test_6

# Test 7: UserInfo
test_7() {
    [ -n "$ACCESS_TOKEN" ] || { echo "    skipped: no token"; return 1; }
    local r
    r=$(curl -s -H "Authorization: Bearer $ACCESS_TOKEN" "$BASE_URL/oauth2/userinfo")
    assert_json_exists "$r" "sub" || return 1
    local sub name
    sub=$(echo "$r" | jq -r '.sub')
    name=$(echo "$r" | jq -r '.name // empty')
    [ ${#sub} -eq 36 ] || { echo "    sub not UUID format (len=${#sub})"; return 1; }
    echo "    Sub: $sub, Name: $name"
}
run_test "Test 7: UserInfo" test_7

# Test 8: Admin Dashboard
test_8() {
    [ -n "$ACCESS_TOKEN" ] || { echo "    skipped: no token"; return 1; }
    local r
    r=$(curl -s -H "Authorization: Bearer $ACCESS_TOKEN" "$BASE_URL/api/admin/dashboard")
    assert_json_field "$r" "status" "success" || return 1
}
run_test "Test 8: Admin Dashboard" test_8

# Test 9: Token Refresh
test_9() {
    [ -n "$REFRESH_TOKEN" ] || { echo "    skipped: no refresh_token"; return 1; }
    local r
    r=$(curl -s -X POST "$BASE_URL/oauth2/token" \
        -d "grant_type=refresh_token&refresh_token=$REFRESH_TOKEN&client_id=vue-client&client_secret=123456")
    local new_at new_rt
    new_at=$(echo "$r" | jq -r '.access_token')
    new_rt=$(echo "$r" | jq -r '.refresh_token')
    [ -n "$new_at" ] && [ "$new_at" != "null" ] || { echo "    no new access_token"; return 1; }
    [ "$new_at" != "$ACCESS_TOKEN" ] || { echo "    same access_token (not rotated)"; return 1; }
    ACCESS_TOKEN="$new_at"
    REFRESH_TOKEN="$new_rt"
    echo "    New AT: ${ACCESS_TOKEN:0:20}..."
}
run_test "Test 9: Token Refresh" test_9

# Test 10: Client Credentials
test_10() {
    local r
    r=$(curl -s -X POST "$BASE_URL/oauth2/token" \
        -d "grant_type=client_credentials&client_id=backend-svc&client_secret=test-secret&scope=read")
    local at rt scope
    at=$(echo "$r" | jq -r '.access_token')
    rt=$(echo "$r" | jq -r '.refresh_token // empty')
    scope=$(echo "$r" | jq -r '.scope')
    [ -n "$at" ] && [ "$at" != "null" ] || { echo "    no access_token"; return 1; }
    [ -z "$rt" ] || [ "$rt" = "null" ] || { echo "    client_credentials should NOT have refresh_token"; return 1; }
    echo "    AT: ${at:0:20}..., Scope: $scope"
}
run_test "Test 10: Client Credentials" test_10

# Test 11: Token Introspection
test_11() {
    [ -n "$ACCESS_TOKEN" ] || { echo "    skipped: no token"; return 1; }
    local r
    r=$(curl -s -X POST -H "Authorization: Bearer $ACCESS_TOKEN" "$BASE_URL/oauth2/introspect" \
        -d "token=$ACCESS_TOKEN&client_id=vue-client&client_secret=123456")
    local active
    active=$(echo "$r" | jq -r '.active')
    [ "$active" = "true" ] || { echo "    active != true"; return 1; }
    echo "    Active: $active, Sub: $(echo "$r" | jq -r '.sub')"
}
run_test "Test 11: Token Introspection" test_11

# Test 12: Token Revocation
test_12() {
    [ -n "$ACCESS_TOKEN" ] || { echo "    skipped: no token"; return 1; }
    curl -s -X POST -H "Authorization: Bearer $ACCESS_TOKEN" "$BASE_URL/oauth2/revoke" \
        -d "token=$ACCESS_TOKEN&client_id=vue-client&client_secret=123456" >/dev/null
    # Verify revoked
    local code
    code=$(curl -s -o /dev/null -w '%{http_code}' -H "Authorization: Bearer $ACCESS_TOKEN" "$BASE_URL/oauth2/userinfo")
    [ "$code" = "401" ] || { echo "    token should be revoked but userinfo returned $code"; return 1; }
    echo "    Revoked and verified: userinfo returns 401"
    ACCESS_TOKEN=""
}
run_test "Test 12: Token Revocation" test_12

# Test 13: User Registration
test_13() {
    local ts
    ts=$(date +%s)
    local r
    r=$(curl -s -X POST "$BASE_URL/api/register" \
        -d "username=testuser_$ts&password=TestPass123&email=test_${ts}@example.com")
    assert_json_exists "$r" "message" || return 1
    echo "    Registered: testuser_$ts"
}
run_test "Test 13: User Registration" test_13

# Test 14: User Profile
test_14() {
    # Need a fresh token (previous was revoked)
    local login_resp
    login_resp=$(curl -s -X POST "$BASE_URL/oauth2/login" \
        -d "username=admin&password=admin&client_id=vue-client&redirect_uri=http://127.0.0.1:5173/callback&scope=openid+profile&state=test-state-12345678&json=true")
    local code
    code=$(echo "$login_resp" | jq -r '.code')
    local tok_resp
    tok_resp=$(curl -s -X POST "$BASE_URL/oauth2/token" \
        -d "grant_type=authorization_code&code=$code&redirect_uri=http://127.0.0.1:5173/callback&client_id=vue-client&client_secret=123456")
    ACCESS_TOKEN=$(echo "$tok_resp" | jq -r '.access_token')

    local r
    r=$(curl -s -H "Authorization: Bearer $ACCESS_TOKEN" "$BASE_URL/api/me")
    assert_json_exists "$r" "username" || return 1
    echo "    Username: $(echo "$r" | jq -r '.username')"
}
run_test "Test 14: User Profile" test_14

# Test 15: Password Reset Request
test_15() {
    local r
    r=$(curl -s -X POST -H "Content-Type: application/json" \
        -d '{"email":"admin@example.com"}' "$BASE_URL/api/password-reset/request")
    assert_json_exists "$r" "message" || return 1
    echo "    Response: $(echo "$r" | jq -r '.message')"
}
run_test "Test 15: Password Reset Request" test_15

# Test 16: Password Reset (non-existent)
test_16() {
    local r
    r=$(curl -s -X POST -H "Content-Type: application/json" \
        -d '{"email":"nobody@nowhere.com"}' "$BASE_URL/api/password-reset/request")
    assert_json_exists "$r" "message" || return 1
    echo "    Anti-enumeration: same response for non-existent email"
}
run_test "Test 16: Password Reset (non-existent)" test_16

# Test 17: Password Change
test_17() {
    [ -n "$ACCESS_TOKEN" ] || { echo "    skipped: no token"; return 1; }
    local r
    r=$(curl -s -X PUT -H "Authorization: Bearer $ACCESS_TOKEN" -H "Content-Type: application/json" \
        -d '{"old_password":"admin","new_password":"NewPass123!"}' "$BASE_URL/api/me/password")
    assert_json_exists "$r" "message" || return 1
    echo "    $(echo "$r" | jq -r '.message')"

    # Restore password
    local login_resp
    login_resp=$(curl -s -X POST "$BASE_URL/oauth2/login" \
        -d "username=admin&password=NewPass123!&client_id=vue-client&redirect_uri=http://127.0.0.1:5173/callback&scope=openid&state=restore-pw-state1&json=true" 2>/dev/null) || true
    local restore_code
    restore_code=$(echo "$login_resp" | jq -r '.code // empty')
    if [ -n "$restore_code" ] && [ "$restore_code" != "null" ]; then
        local tok_resp
        tok_resp=$(curl -s -X POST "$BASE_URL/oauth2/token" \
            -d "grant_type=authorization_code&code=$restore_code&redirect_uri=http://127.0.0.1:5173/callback&client_id=vue-client&client_secret=123456")
        local restore_token
        restore_token=$(echo "$tok_resp" | jq -r '.access_token')
        curl -s -X PUT -H "Authorization: Bearer $restore_token" -H "Content-Type: application/json" \
            -d '{"old_password":"NewPass123!","new_password":"admin"}' "$BASE_URL/api/me/password" >/dev/null
        echo "    Password restored to 'admin'"
    else
        echo "    NOTE: Could not restore password (run setup-database.sh to reset)"
    fi
}
run_test "Test 17: Password Change" test_17

# Post-test Cleanup
echo ""
echo "========================================"
echo "Post-test Cleanup"
echo "========================================"
reset_admin_account
echo ""

print_summary $TOTAL
