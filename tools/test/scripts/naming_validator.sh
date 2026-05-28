#!/bin/bash
# Naming validator for DROGON_TEST
# Usage: ./naming_validator.sh <directory_to_scan>

TARGET_DIR=${1:-"OAuth2Backend/test"}

echo "Scanning directory: $TARGET_DIR for naming violations..."

# Find all DROGON_TEST macro usages, extract the name inside parentheses
invalid_names=$(grep -rh "DROGON_TEST(" "$TARGET_DIR" --include="*.cc" | \
    sed 's/.*DROGON_TEST(\([^)]*\)).*/\1/' | \
    grep -vE '^(Unit|Integration|E2E|Performance|Security|API|Database|Acceptance)_P[0-3]_')

if [ -n "$invalid_names" ]; then
    echo ""
    echo "[ERROR] 命名规范检查失败! 发现以下不合规测试 (Naming violation detected!):"
    echo "--------------------------------------------------------"
    echo "$invalid_names"
    echo "--------------------------------------------------------"
    echo "期望格式 (Expected format): [Category]_[Priority]_[Module]_[Feature]_[Scenario]"
    echo "  Category : Unit, Integration, E2E, Performance, Security, API, Database, Acceptance"
    echo "  Priority : P0, P1, P2, P3"
    echo "  Example  : Unit_P0_Validator_ClientId_InvalidFormat_ReturnsError"
    exit 1
else
    echo "[PASS] 命名规范检查通过 (All test names follow convention!)"
    exit 0
fi
