#!/usr/bin/env bash
# generate-models.sh - Generate Drogon ORM models (Linux/macOS)
set -euo pipefail

source "$(dirname "$0")/env_common.sh"

# Check for drogon_ctl
if ! command -v drogon_ctl &>/dev/null; then
    echo "[Error] drogon_ctl not found in PATH."
    exit 1
fi

MODELS_SRC_DIR="$PROJECT_DIR/OAuth2Plugin/src/models"
MODELS_INC_DIR="$PROJECT_DIR/OAuth2Plugin/include/oauth2/models"
MODELS_BACKUP="$PROJECT_DIR/OAuth2Plugin/models_backup"
MODEL_JSON_DIR="$PROJECT_DIR/OAuth2Server"

echo ""
echo "========================================"
echo "OAuth2 Plugin Model Generation"
echo "========================================"
echo ""

AUTO_MODE=0
for arg in "$@"; do
    case "$arg" in
        -y|--force) AUTO_MODE=1 ;;
    esac
done

if [ $AUTO_MODE -eq 0 ]; then
    echo "WARNING: This will regenerate ORM models in $MODELS_SRC_DIR"
    read -rp "Press Enter to continue or Ctrl+C to cancel..."
fi

# Backup existing models
if [ -d "$MODELS_SRC_DIR" ]; then
    echo "Backing up existing models to $MODELS_BACKUP..."
    rm -rf "$MODELS_BACKUP"
    mkdir -p "$MODELS_BACKUP"
    cp -r "$MODELS_SRC_DIR"/* "$MODELS_BACKUP/" 2>/dev/null || true
    if ls "$MODELS_INC_DIR"/*.h &>/dev/null; then
        cp "$MODELS_INC_DIR"/*.h "$MODELS_BACKUP/" 2>/dev/null || true
    fi
fi

echo "Generating ORM models..."
mkdir -p "$MODELS_SRC_DIR"

cd "$MODEL_JSON_DIR"
if [ $AUTO_MODE -eq 1 ]; then
    echo "y" | drogon_ctl create model "../OAuth2Plugin/src/models"
else
    drogon_ctl create model "../OAuth2Plugin/src/models"
fi

echo "Moving header files to $MODELS_INC_DIR..."
mkdir -p "$MODELS_INC_DIR"
# Remove old headers
rm -f "$MODELS_INC_DIR"/*.h
# Move generated headers
if ls "$MODELS_SRC_DIR"/*.h &>/dev/null; then
    mv "$MODELS_SRC_DIR"/*.h "$MODELS_INC_DIR/"
fi

echo ""
echo "========================================"
echo "Model generation complete!"
echo "========================================"
