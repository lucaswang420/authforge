#!/usr/bin/env bash
# manage-parity-check.sh - Verify manage.ps1 and manage.sh have identical command sets
# Parses switch/case patterns from both files and asserts parity.
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

PS1_FILE="$PROJECT_DIR/manage.ps1"
SH_FILE="$PROJECT_DIR/manage.sh"

# Commands to exclude from parity check (platform-specific only)
ALLOWLIST=(
    "rebuild-debug-image"
    "install-hooks"
    "validate-openapi"
)

echo "========================================"
echo "Manage Script Parity Check"
echo "========================================"
echo ""

if [ ! -f "$PS1_FILE" ]; then
    echo "[Error] manage.ps1 not found at $PS1_FILE"
    exit 1
fi

if [ ! -f "$SH_FILE" ]; then
    echo "[Error] manage.sh not found at $SH_FILE"
    exit 1
fi

# Extract commands from manage.ps1 switch cases
# Pattern: quoted strings in switch statement like "build-backend" {
ps1_commands=$(grep -oP '^\s*"([a-z0-9-]+)"\s*\{' "$PS1_FILE" | grep -oP '"[^"]+"' | tr -d '"' | sort -u)

# Extract commands from manage.sh case patterns
# Pattern: case labels like build-backend)
sh_commands=$(grep -oP '^\s*\K[a-z0-9-]+(?=\))' "$SH_FILE" | grep -v '^\*$' | sort -u)

# Filter out allowlisted commands
filter_allowlist() {
    local input="$1"
    for cmd in "${ALLOWLIST[@]}"; do
        input=$(echo "$input" | grep -v "^${cmd}$" || true)
    done
    echo "$input"
}

ps1_filtered=$(filter_allowlist "$ps1_commands")
sh_filtered=$(filter_allowlist "$sh_commands")

# Compare
MISSING_IN_SH=""
MISSING_IN_PS1=""
EXIT_CODE=0

echo "Commands in manage.ps1 (filtered):"
echo "$ps1_filtered" | sed 's/^/  /'
echo ""
echo "Commands in manage.sh (filtered):"
echo "$sh_filtered" | sed 's/^/  /'
echo ""

# Check for commands in ps1 but not in sh
while IFS= read -r cmd; do
    [ -z "$cmd" ] && continue
    if ! echo "$sh_filtered" | grep -q "^${cmd}$"; then
        MISSING_IN_SH="$MISSING_IN_SH  $cmd\n"
        EXIT_CODE=1
    fi
done <<< "$ps1_filtered"

# Check for commands in sh but not in ps1
while IFS= read -r cmd; do
    [ -z "$cmd" ] && continue
    if ! echo "$ps1_filtered" | grep -q "^${cmd}$"; then
        MISSING_IN_PS1="$MISSING_IN_PS1  $cmd\n"
        EXIT_CODE=1
    fi
done <<< "$sh_filtered"

if [ $EXIT_CODE -ne 0 ]; then
    if [ -n "$MISSING_IN_SH" ]; then
        echo "[FAIL] Commands in manage.ps1 but NOT in manage.sh:"
        echo -e "$MISSING_IN_SH"
    fi
    if [ -n "$MISSING_IN_PS1" ]; then
        echo "[FAIL] Commands in manage.sh but NOT in manage.ps1:"
        echo -e "$MISSING_IN_PS1"
    fi
    echo ""
    echo "Allowlisted (excluded from check): ${ALLOWLIST[*]}"
    exit 1
else
    echo "[PASS] manage.ps1 and manage.sh have identical command sets"
    echo "Allowlisted (excluded): ${ALLOWLIST[*]}"
fi
