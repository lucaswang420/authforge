#!/usr/bin/env bash
# full-test.sh - One-click build + unit test + API test cycle (Linux/macOS)
set -euo pipefail

source "$(dirname "$0")/env_common.sh"

BUILD_TYPE="Release"
BUILD_ARG="--release"

for arg in "$@"; do
    case "$arg" in
        --debug|-debug) BUILD_TYPE="Debug"; BUILD_ARG="--debug" ;;
        --release|-release) BUILD_TYPE="Release"; BUILD_ARG="--release" ;;
    esac
done

FINAL_RESULT=0
SERVER_PID=""

cleanup() {
    if [ -n "$SERVER_PID" ] && kill -0 "$SERVER_PID" 2>/dev/null; then
        echo "Stopping server (PID $SERVER_PID)..."
        kill "$SERVER_PID" 2>/dev/null || true
        wait "$SERVER_PID" 2>/dev/null || true
    fi
}
trap cleanup EXIT

echo ""
echo "========================================"
echo "One-Click Build and Test ($BUILD_TYPE)"
echo "========================================"
echo ""

# Step 1: Reinitialize Database
echo "========================================"
echo "Step 1: Reinitializing oauth2_db database"
echo "========================================"
bash "$SCRIPT_DIR/setup-database.sh"
echo "[SUCCESS] Database initialized"
echo ""

# Step 2: Regenerate ORM Models
echo "========================================"
echo "Step 2: Regenerating ORM models"
echo "========================================"
bash "$SCRIPT_DIR/generate-models.sh" -y
echo "[SUCCESS] ORM models regenerated"
echo ""

# Step 3: Rebuild Project
echo "========================================"
echo "Step 3: Rebuilding project"
echo "========================================"
bash "$SCRIPT_DIR/build.sh" "$BUILD_ARG"
echo "[SUCCESS] Project built"
echo ""

# Step 4: Run Tests
echo "========================================"
echo "Step 4: Running tests"
echo "========================================"
bash "$SCRIPT_DIR/test.sh" "$BUILD_ARG"
echo "[SUCCESS] All tests passed"
echo ""

# Step 5: Start Server
echo "========================================"
echo "Step 5: Starting OAuth2 server"
echo "========================================"
EXE_PATH="$PROJECT_DIR/build/OAuth2Server/OAuth2Server"
if [ ! -f "$EXE_PATH" ]; then
    EXE_PATH="$PROJECT_DIR/build/OAuth2Server/$BUILD_TYPE/OAuth2Server"
fi
if [ ! -f "$EXE_PATH" ]; then
    echo "[FAILED] Server executable not found"
    exit 1
fi

cd "$(dirname "$EXE_PATH")"
"$EXE_PATH" &
SERVER_PID=$!
echo "Server started (PID $SERVER_PID), waiting for startup..."
sleep 8

if ! kill -0 "$SERVER_PID" 2>/dev/null; then
    echo "[FAILED] Server failed to start or crashed"
    exit 1
fi
echo "[SUCCESS] Server started"
echo ""

# Step 6: Test OAuth2 Endpoints
echo "========================================"
echo "Step 6: Testing OAuth2 endpoints"
echo "========================================"
bash "$SCRIPT_DIR/test-oauth2-endpoints.sh" || FINAL_RESULT=1
if [ $FINAL_RESULT -eq 0 ]; then
    echo "[SUCCESS] OAuth2 endpoint tests passed"
fi
echo ""

# Step 7: Test Admin Endpoints
if [ $FINAL_RESULT -eq 0 ]; then
    echo "========================================"
    echo "Step 7: Testing Admin endpoints"
    echo "========================================"
    bash "$SCRIPT_DIR/test-admin-endpoints.sh" || FINAL_RESULT=1
    if [ $FINAL_RESULT -eq 0 ]; then
        echo "[SUCCESS] Admin endpoint tests passed"
    fi
    echo ""
fi

# Step 8: Stop Server
echo "========================================"
echo "Step 8: Stopping OAuth2 server"
echo "========================================"
kill "$SERVER_PID" 2>/dev/null || true
wait "$SERVER_PID" 2>/dev/null || true
SERVER_PID=""
echo "[SUCCESS] Server stopped"
echo ""

if [ $FINAL_RESULT -ne 0 ]; then
    echo "========================================"
    echo "FULL TEST FAILED - see errors above"
    echo "========================================"
    exit 1
fi

echo "========================================"
echo "ALL STEPS COMPLETED SUCCESSFULLY!"
echo "========================================"
