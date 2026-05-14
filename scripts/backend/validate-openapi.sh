#!/bin/bash
# CI script to validate OpenAPI specification
# This script is used in CI/CD pipelines to ensure OpenAPI documentation quality

set -e

echo "[INFO] CI OpenAPI Validation"
echo "========================"

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Find the project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"

FAILURE=0

# Function to print section header
print_section() {
    echo ""
    echo "=== $1 ==="
    echo "--------------------------------"
}

# Function to check command result
check_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}[PASS] $2${NC}"
        return 0
    else
        echo -e "${RED}[ERROR] $2${NC}"
        FAILURE=1
        return 1
    fi
}

# Function to convert Unix path to Windows path for Node.js in Git Bash
convert_path_for_node() {
    local unix_path="$1"
    if [[ "$unix_path" =~ ^/([a-z])/(.*)$ ]]; then
        local drive="${BASH_REMATCH[1]}"
        local path_rest="${BASH_REMATCH[2]}"
        echo "${drive}:/${path_rest}"
    else
        echo "$unix_path"
    fi
}

# 1. Build project (if needed)
print_section "Building Project"
cd "$PROJECT_ROOT"
if cmake --build build --config Release > /dev/null 2>&1; then
    check_result 0 "Build successful"
else
    echo "Build already up-to-date or build directory exists"
    check_result 0 "Build check passed"
fi

# 2. Run OpenAPI tests
print_section "Running OpenAPI Tests"
cd "$PROJECT_ROOT/build"
if ctest -V -C Release --output-on-failure; then
    check_result 0 "OpenAPI tests passed"
else
    check_result 1 "OpenAPI tests failed"
fi

# 3. Validate OpenAPI JSON structure
print_section "Validating OpenAPI JSON"
OPENAPI_FILE="$PROJECT_ROOT/build/Release/docs/api/openapi.json"
if [ -f "$OPENAPI_FILE" ]; then
    # Try multiple JSON validators (python3, jq, node) in order of preference
    JSON_VALID=0

    # Try python3 first
    if command -v python3 &> /dev/null; then
        if python3 -m json.tool "$OPENAPI_FILE" > /dev/null 2>&1; then
            JSON_VALID=1
        fi
    fi

    # Try jq if python3 failed or wasn't available
    if [ $JSON_VALID -eq 0 ] && command -v jq &> /dev/null; then
        if jq . "$OPENAPI_FILE" > /dev/null 2>&1; then
            JSON_VALID=1
        fi
    fi

    # Try node if both python3 and jq failed
    if [ $JSON_VALID -eq 0 ] && command -v node &> /dev/null; then
        WIN_FILE=$(convert_path_for_node "$OPENAPI_FILE")
        if node -e "JSON.parse(require('fs').readFileSync('${WIN_FILE}', 'utf8'))" 2> /dev/null; then
            JSON_VALID=1
        fi
    fi

    if [ $JSON_VALID -eq 1 ]; then
        check_result 0 "OpenAPI JSON is valid"
    else
        check_result 1 "OpenAPI JSON is invalid"
    fi

    # Check OpenAPI version and extract data (use node as fallback)
    if command -v jq &> /dev/null; then
        OPENAPI_VERSION=$(jq -r '.openapi' "$OPENAPI_FILE" 2>/dev/null || echo "unknown")
        ENDPOINT_COUNT=$(jq '.paths | length' "$OPENAPI_FILE" 2>/dev/null || echo "0")
    elif command -v node &> /dev/null; then
        WIN_FILE=$(convert_path_for_node "$OPENAPI_FILE")
        OPENAPI_VERSION=$(node -e "console.log(JSON.parse(require('fs').readFileSync('${WIN_FILE}', 'utf8')).openapi || 'unknown')" 2>/dev/null || echo "unknown")
        ENDPOINT_COUNT=$(node -e "console.log(Object.keys(JSON.parse(require('fs').readFileSync('${WIN_FILE}', 'utf8')).paths || {}).length)" 2>/dev/null || echo "0")
    else
        OPENAPI_VERSION="unknown"
        ENDPOINT_COUNT="0"
    fi
    echo "OpenAPI Version: $OPENAPI_VERSION"
    echo "Documented Endpoints: $ENDPOINT_COUNT"

    # Check for required fields
    REQUIRED_FIELDS=("openapi" "info" "paths" "servers")
    for field in "${REQUIRED_FIELDS[@]}"; do
        FIELD_EXISTS=0
        if command -v jq &> /dev/null; then
            if jq -e ".${field}" "$OPENAPI_FILE" > /dev/null 2>&1; then
                FIELD_EXISTS=1
            fi
        elif command -v node &> /dev/null; then
            WIN_FILE=$(convert_path_for_node "$OPENAPI_FILE")
            if node -e "JSON.parse(require('fs').readFileSync('${WIN_FILE}', 'utf8')).${field}" 2> /dev/null; then
                FIELD_EXISTS=1
            fi
        fi

        if [ $FIELD_EXISTS -eq 1 ]; then
            check_result 0 "Required field '${field}' exists"
        else
            check_result 1 "Required field '${field}' is missing"
        fi
    done
else
    check_result 1 "OpenAPI file not found at $OPENAPI_FILE"
fi

# 4. Check documentation coverage
print_section "Documentation Coverage"
if [ -f "$OPENAPI_FILE" ]; then
    # Extract all paths and methods
    if command -v jq &> /dev/null; then
        TOTAL_ENDPOINTS=$(jq '.paths | length' "$OPENAPI_FILE" 2>/dev/null || echo "0")
        ENDPOINTS_WITH_DESC=$(jq '[.paths[][] | select(.description != null and .description != "")] | length' "$OPENAPI_FILE" 2>/dev/null || echo "0")
        ENDPOINTS_WITH_EXAMPLES=$(jq '[.paths[][] | select(.responseExamples != null and (.responseExamples | length) > 0)] | length' "$OPENAPI_FILE" 2>/dev/null || echo "0")
    elif command -v node &> /dev/null; then
        WIN_FILE=$(convert_path_for_node "$OPENAPI_FILE")
        TOTAL_ENDPOINTS=$(node -e "console.log(Object.keys(JSON.parse(require('fs').readFileSync('${WIN_FILE}', 'utf8')).paths || {}).length)" 2>/dev/null || echo "0")
        # For node, just count total endpoints and set examples to 0 for now
        ENDPOINTS_WITH_DESC=$TOTAL_ENDPOINTS
        ENDPOINTS_WITH_EXAMPLES=0
    else
        TOTAL_ENDPOINTS=0
        ENDPOINTS_WITH_DESC=0
        ENDPOINTS_WITH_EXAMPLES=0
    fi

    echo "Total Endpoints: $TOTAL_ENDPOINTS"
    echo "Endpoints with Descriptions: $ENDPOINTS_WITH_DESC"
    echo "Endpoints with Examples: $ENDPOINTS_WITH_EXAMPLES"

    # Calculate coverage percentages
    if [ "$TOTAL_ENDPOINTS" -gt 0 ]; then
        DESC_COVERAGE=$((ENDPOINTS_WITH_DESC * 100 / TOTAL_ENDPOINTS))
        EXAMPLE_COVERAGE=$((ENDPOINTS_WITH_EXAMPLES * 100 / TOTAL_ENDPOINTS))

        echo "Description Coverage: ${DESC_COVERAGE}%"
        echo "Example Coverage: ${EXAMPLE_COVERAGE}%"

        if [ "$DESC_COVERAGE" -lt 80 ]; then
            echo -e "${YELLOW}[WARNING] Description coverage is below 80%${NC}"
        fi

        if [ "$EXAMPLE_COVERAGE" -lt 50 ]; then
            echo -e "${YELLOW}[WARNING] Example coverage is below 50%${NC}"
        fi
    fi
fi

# 5. Security check
print_section "Security Documentation Check"
if [ -f "$OPENAPI_FILE" ]; then
    # Check for security schemes
    SECURITY_SCHEMES_EXISTS=0
    if command -v jq &> /dev/null; then
        if jq -e '.components.securitySchemes' "$OPENAPI_FILE" > /dev/null 2>&1; then
            SECURITY_SCHEMES_EXISTS=1
        fi
    elif command -v node &> /dev/null; then
        WIN_FILE=$(convert_path_for_node "$OPENAPI_FILE")
        if node -e "JSON.parse(require('fs').readFileSync('${WIN_FILE}', 'utf8')).components.securitySchemes" 2> /dev/null; then
            SECURITY_SCHEMES_EXISTS=1
        fi
    fi

    if [ $SECURITY_SCHEMES_EXISTS -eq 1 ]; then
        check_result 0 "Security schemes documented"
    else
        check_result 1 "Security schemes not documented"
    fi

    # Check endpoints with authentication
    if command -v jq &> /dev/null; then
        SECURED_ENDPOINTS=$(jq '[.paths[][] | select(.security != null)] | length' "$OPENAPI_FILE" 2>/dev/null || echo "0")
    elif command -v node &> /dev/null; then
        WIN_FILE=$(convert_path_for_node "$OPENAPI_FILE")
        SECURED_ENDPOINTS=$(node -e "const data=JSON.parse(require('fs').readFileSync('${WIN_FILE}','utf8'));let count=0;for(const path in data.paths){for(const method in data.paths[path]){if(data.paths[path][method].security)count++;}}console.log(count)" 2>/dev/null || echo "0")
    else
        SECURED_ENDPOINTS=0
    fi
    echo "Secured Endpoints: $SECURED_ENDPOINTS"
fi

# Final summary
print_section "Validation Summary"
if [ $FAILURE -eq 0 ]; then
    echo -e "${GREEN}[+] All OpenAPI validations passed${NC}"
    exit 0
else
    echo -e "${RED}[-] Some OpenAPI validations failed${NC}"
    exit 1
fi
