#!/usr/bin/env bash
# run-server.sh - Start the OAuth2Server binary (Linux/macOS)
set -euo pipefail

source "$(dirname "$0")/env_common.sh"

BUILD_TYPE="Release"

for arg in "$@"; do
    case "$arg" in
        --debug|-debug) BUILD_TYPE="Debug" ;;
        --release|-release) BUILD_TYPE="Release" ;;
    esac
done

# On Linux, single-config generators put the binary directly in the build dir
EXE_PATH="$PROJECT_DIR/build/OAuth2Server/OAuth2Server"

# Fallback: multi-config layout
if [ ! -f "$EXE_PATH" ]; then
    EXE_PATH="$PROJECT_DIR/build/OAuth2Server/$BUILD_TYPE/OAuth2Server"
fi

if [ ! -f "$EXE_PATH" ]; then
    echo "[Error] OAuth2Server binary not found."
    echo "Searched:"
    echo "  $PROJECT_DIR/build/OAuth2Server/OAuth2Server"
    echo "  $PROJECT_DIR/build/OAuth2Server/$BUILD_TYPE/OAuth2Server"
    echo "Please run build.sh first."
    exit 1
fi

echo "Starting OAuth2Server ($BUILD_TYPE)"
cd "$(dirname "$EXE_PATH")"
exec "$EXE_PATH"
