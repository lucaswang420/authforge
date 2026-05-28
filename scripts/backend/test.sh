#!/usr/bin/env bash
# test.sh - Run backend tests (Linux/macOS equivalent of test.bat)
set -euo pipefail

source "$(dirname "$0")/env_common.sh"

BUILD_TYPE="Release"
VERBOSE="--output-on-failure"

for arg in "$@"; do
    case "$arg" in
        --debug|-debug) BUILD_TYPE="Debug" ;;
        --release|-release) BUILD_TYPE="Release" ;;
        -q|--quiet) VERBOSE="" ;;
    esac
done

echo "========================================"
echo "Running OAuth2 Tests"
echo "========================================"
echo "Build Type: $BUILD_TYPE"

if [ ! -d "$PROJECT_DIR/build" ]; then
    echo "[Error] Build directory not found. Please run build.sh first."
    exit 1
fi

cd "$PROJECT_DIR/build"

# Run 1: Standard config.json
echo ""
echo "[1/2] Running tests with standard config.json..."
ctest --build-config "$BUILD_TYPE" $VERBOSE
echo "[PASS] Standard config tests successful."

# Run 2: config.ci.json
echo ""
echo "[2/2] Running tests with config.ci.json..."
CI_CONFIG="$PROJECT_DIR/OAuth2Server/config.ci.json"
TEST_WORK_DIR="$PROJECT_DIR/build/OAuth2Server/test"

if [ ! -f "$CI_CONFIG" ]; then
    echo "[SKIP] config.ci.json not found, skipping second run."
    exit 0
fi

if [ -d "$TEST_WORK_DIR" ]; then
    cp "$TEST_WORK_DIR/config.json" "$TEST_WORK_DIR/config.json.bak"
    cp "$CI_CONFIG" "$TEST_WORK_DIR/config.json"

    CI_EXIT=0
    ctest --build-config "$BUILD_TYPE" $VERBOSE || CI_EXIT=$?

    # Restore original config
    mv "$TEST_WORK_DIR/config.json.bak" "$TEST_WORK_DIR/config.json"

    if [ $CI_EXIT -ne 0 ]; then
        echo "[FAIL] Tests failed with config.ci.json"
        exit 1
    fi
    echo "[PASS] CI config tests successful."
fi

echo ""
echo "========================================"
echo "All test runs completed successfully"
echo "========================================"
