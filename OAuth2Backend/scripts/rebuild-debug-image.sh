#!/bin/bash
# Rebuild script for OAuth2 debug Docker image
# Builds the image from scratch without cache

set -e

echo "========================================"
echo "Rebuilding OAuth2 Debug Image"
echo "========================================"
echo ""
echo "This will take about 10-15 minutes..."
echo ""

# Build the image
docker build --no-cache -f Dockerfile.debug.cn -t oauth2-backend-debug:v1.9.12 .

echo ""
echo "========================================"
echo "Build completed!"
echo "========================================"
echo ""
echo "Verifying Drogon installation..."
docker run --rm oauth2-backend-debug:v1.9.12 bash -c "
  echo 'Checking Drogon files:'
  ls -la /usr/local/lib/libdrogon.a && echo '  ✓ Library found'
  ls -la /usr/local/include/drogon/drogon.h && echo '  ✓ Headers found'
  echo ''
  echo 'Drogon v1.9.12 installed successfully!'
"
