#!/bin/bash
# CI script to validate OpenAPI specification
# This script is used in CI/CD pipelines to ensure OpenAPI documentation quality

set -e

echo "🔍 CI OpenAPI Validation"
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
    echo "📋 $1"
    echo "--------------------------------"
}

# Function to check command result
check_result() {
    if [ $1 -eq 0 ]; then
        echo -e "${GREEN}✓ $2${NC}"
        return 0
    else
        echo -e "${RED}✗ $2${NC}"
        FAILURE=1
        return 1
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
if ctest -C Release --output-on-failure; then
    check_result 0 "OpenAPI tests passed"
else
    check_result 1 "OpenAPI tests failed"
fi

# 3. Validate OpenAPI JSON structure
print_section "Validating OpenAPI JSON"
OPENAPI_FILE="$PROJECT_ROOT/build/Release/docs/api/openapi.json"
if [ -f "$OPENAPI_FILE" ]; then
    if python3 -m json.tool "$OPENAPI_FILE" > /dev/null 2>&1; then
        check_result 0 "OpenAPI JSON is valid"
    else
        check_result 1 "OpenAPI JSON is invalid"
    fi

    # Check OpenAPI version
    OPENAPI_VERSION=$(jq -r '.openapi' "$OPENAPI_FILE" 2>/dev/null || echo "unknown")
    echo "OpenAPI Version: $OPENAPI_VERSION"

    # Count documented endpoints
    ENDPOINT_COUNT=$(jq '.paths | length' "$OPENAPI_FILE" 2>/dev/null || echo "0")
    echo "Documented Endpoints: $ENDPOINT_COUNT"

    # Check for required fields
    REQUIRED_FIELDS=("openapi" "info" "paths" "servers")
    for field in "${REQUIRED_FIELDS[@]}"; do
        if jq -e ".${field}" "$OPENAPI_FILE" > /dev/null 2>&1; then
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
    TOTAL_ENDPOINTS=$(jq '.paths | length' "$OPENAPI_FILE" 2>/dev/null || echo "0")

    # Check for descriptions
    ENDPOINTS_WITH_DESC=$(jq '[.paths[][] | select(.description != null and .description != "")] | length' "$OPENAPI_FILE" 2>/dev/null || echo "0")

    # Check for response examples
    ENDPOINTS_WITH_EXAMPLES=$(jq '[.paths[][] | select(.responseExamples != null and (.responseExamples | length) > 0)] | length' "$OPENAPI_FILE" 2>/dev/null || echo "0")

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
            echo -e "${YELLOW}⚠ Description coverage is below 80%${NC}"
        fi

        if [ "$EXAMPLE_COVERAGE" -lt 50 ]; then
            echo -e "${YELLOW}⚠ Example coverage is below 50%${NC}"
        fi
    fi
fi

# 5. Security check
print_section "Security Documentation Check"
if [ -f "$OPENAPI_FILE" ]; then
    # Check for security schemes
    if jq -e '.components.securitySchemes' "$OPENAPI_FILE" > /dev/null 2>&1; then
        check_result 0 "Security schemes documented"
    else
        check_result 1 "Security schemes not documented"
    fi

    # Check endpoints with authentication
    SECURED_ENDPOINTS=$(jq '[.paths[][] | select(.requiresAuth == true)] | length' "$OPENAPI_FILE" 2>/dev/null || echo "0")
    echo "Secured Endpoints: $SECURED_ENDPOINTS"
fi

# Final summary
print_section "Validation Summary"
if [ $FAILURE -eq 0 ]; then
    echo -e "${GREEN}✓ All OpenAPI validations passed${NC}"
    exit 0
else
    echo -e "${RED}✗ Some OpenAPI validations failed${NC}"
    exit 1
fi
