#!/usr/bin/env bash
# reset-account-lockout.sh - Reset account lockout counters (Linux/macOS)
set -euo pipefail

source "$(dirname "$0")/env_common.sh"
source "$(dirname "$0")/common-test-functions.sh"

USERNAME="${1:-}"

echo "========================================"
echo "Reset Account Lockout"
echo "========================================"
echo ""

if [ -n "$USERNAME" ]; then
    echo "Resetting lockout for user: $USERNAME"
    QUERY="UPDATE users SET failed_login_count = 0, locked_until = 0 WHERE username='$USERNAME';"
    CHECK_QUERY="SELECT username, failed_login_count, locked_until FROM users WHERE username='$USERNAME';"
else
    echo "Resetting lockout for ALL users"
    QUERY="UPDATE users SET failed_login_count = 0, locked_until = 0;"
    CHECK_QUERY="SELECT username, failed_login_count, locked_until FROM users ORDER BY username;"
fi

if run_psql "$QUERY" >/dev/null 2>&1; then
    echo ""
    echo "Current status:"
    run_psql "$CHECK_QUERY" || true
else
    echo "[Error] Failed to reset account lockout"
    exit 1
fi

echo ""
echo "Account lockout reset completed successfully"
