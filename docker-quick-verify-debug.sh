#!/bin/bash
# Quick verification script for teardown fix
# This script runs inside the pre-built debug container

set -e

echo "========================================"
echo "Teardown Fix Quick Verification"
echo "========================================"
echo ""

# Check if Drogon is pre-installed
echo "[0/4] Verifying environment..."
if [ -f "/usr/local/include/drogon/drogon.h" ] && [ -f "/usr/local/lib/libdrogon.a" ]; then
  echo "✓ Drogon found (headers and library installed)"
else
  echo "✗ ERROR: Drogon not found!"
  echo "Missing files:"
  [ ! -f "/usr/local/include/drogon/drogon.h" ] && echo "  - /usr/local/include/drogon/drogon.h"
  [ ! -f "/usr/local/lib/libdrogon.a" ] && echo "  - /usr/local/lib/libdrogon.a"
  echo ""
  echo "Please ensure you built the image with: docker build -f Dockerfile.debug -t oauth2-backend-debug:v1.9.12 ."
  exit 1
fi

# Wait for databases
echo ""
echo "[1/4] Waiting for databases..."
for i in {1..30}; do
  if pg_isready -h postgres -U test >/dev/null 2>&1; then
    echo "✓ PostgreSQL is ready"
    break
  fi
  if [ $i -eq 30 ]; then
    echo "✗ ERROR: PostgreSQL not ready after 30 seconds"
    exit 1
  fi
  sleep 1
done

for i in {1..30}; do
  if redis-cli -h redis ping >/dev/null 2>&1; then
    echo "✓ Redis is ready"
    break
  fi
  if [ $i -eq 30 ]; then
    echo "✗ ERROR: Redis not ready after 30 seconds"
    exit 1
  fi
  sleep 1
done

# Initialize database
echo ""
echo "[2/4] Initializing database..."
export PGPASSWORD=123456
if psql -h postgres -U test -d oauth_test -c "SELECT 1 FROM oauth2_clients LIMIT 1;" >/dev/null 2>&1; then
  echo "✓ Database already initialized"
else
  echo "Initializing database schema..."
  cd /app/OAuth2Backend
  psql -h postgres -U test -d oauth_test -f sql/001_oauth2_core.sql >/dev/null 2>&1
  psql -h postgres -U test -d oauth_test -f sql/002_users_table.sql >/dev/null 2>&1
  psql -h postgres -U test -d oauth_test -f sql/003_rbac_schema.sql >/dev/null 2>&1
  psql -h postgres -U test -d oauth_test -f sql/004_oauth2_scopes.sql >/dev/null 2>&1
  echo "✓ Database initialized"
fi

# Build project
echo ""
echo "[3/4] Building OAuth2Backend..."
cd /app/OAuth2Backend

# Clean build directory with Windows compatibility
if [ -d "build" ]; then
  echo "  Cleaning build directory..."
  # Try normal removal first
  if ! rm -rf build 2>/dev/null; then
    echo "  Warning: Some files locked, trying alternative cleanup..."
    # Remove specific problematic files first
    find build -type f -name "*.log" -delete 2>/dev/null || true
    find build -type f -name "*.exe" -delete 2>/dev/null || true
    rm -rf build/test 2>/dev/null || true
    # Try again
    rm -rf build || {
      # Last resort: rename and create new
      mv build "build.old.$(date +%s)" 2>/dev/null || true
    }
  fi
fi

mkdir -p build && cd build

echo "  Running CMake..."
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_CXX_STANDARD=17 >/dev/null

echo "  Building (this may take 1-2 minutes)..."
time cmake --build . --config Release --parallel $(nproc)

# Verify test executable was built
echo ""
echo "Verifying build..."
if [ ! -f "./test/OAuth2Test_test" ] && [ ! -f "./test/Debug/OAuth2Test_test" ]; then
  echo "ERROR: Test executable not found after build!"
  echo ""
  echo "Build directory contents:"
  ls -la test/ || true
  echo ""
  echo "Full build directory:"
  ls -la || true
  exit 1
fi

# Verify config.json was copied
if [ ! -f "./config.json" ]; then
  echo "WARNING: config.json not found in build directory"
  echo "This may cause test failures!"
  ls -la config.json 2>/dev/null || true
fi

echo "✓ Test executable found"
[ -f "./config.json" ] && echo "✓ config.json found" || echo "⚠ config.json missing"

# Run test
echo ""
echo "[4/4] Running test..."
echo "========================================"
export OAUTH2_DB_HOST="postgres"
export OAUTH2_REDIS_HOST="redis"
export OAUTH2_REDIS_PASSWORD="123456"

# Linux uses test/OAuth2Test_test, Windows uses test/Debug/OAuth2Test_test
if [ -f "./test/OAuth2Test_test" ]; then
  ./test/OAuth2Test_test
elif [ -f "./test/Debug/OAuth2Test_test" ]; then
  ./test/Debug/OAuth2Test_test
else
  echo "ERROR: Test executable not found!"
  echo "Looking for:"
  echo "  - ./test/OAuth2Test_test (Linux)"
  echo "  - ./test/Debug/OAuth2Test_test (Windows)"
  echo ""
  echo "Build directory contents:"
  ls -la test/ || true
  exit 1
fi
EXIT_CODE=$?

echo ""
echo "========================================"
if [ $EXIT_CODE -eq 0 ]; then
  echo "✅ SUCCESS: No crash during teardown!"
  echo "The fix is working correctly."
else
  echo "❌ FAILED: Exit code $EXIT_CODE"
  echo "Please check the output above for errors."
fi
echo "========================================"
