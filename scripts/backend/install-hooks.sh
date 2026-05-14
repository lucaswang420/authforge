#!/bin/bash
# Install git hooks for OpenAPI documentation validation

set -e

echo "[INFO] Installing OpenAPI documentation hooks..."

# Colors for output
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

# Find the project root directory
SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
PROJECT_ROOT="$(cd "$SCRIPT_DIR/.." && pwd)"
HOOKS_DIR="$PROJECT_ROOT/.git/hooks"

# Create hooks directory if it doesn't exist
mkdir -p "$HOOKS_DIR"

# Copy the pre-commit hook
PRE_COMMIT_SOURCE="$PROJECT_ROOT/scripts/validate-openapi.sh"
PRE_COMMIT_TARGET="$HOOKS_DIR/pre-commit"

if [ -f "$PRE_COMMIT_SOURCE" ]; then
    # Make the script executable
    chmod +x "$PRE_COMMIT_SOURCE"

    # Create symlink or copy the hook
    if [ -L "$PRE_COMMIT_TARGET" ]; then
        echo "Removing existing pre-commit hook symlink..."
        rm "$PRE_COMMIT_TARGET"
    elif [ -f "$PRE_COMMIT_TARGET" ]; then
        echo "Backing up existing pre-commit hook..."
        mv "$PRE_COMMIT_TARGET" "$PRE_COMMIT_TARGET.backup"
    fi

    ln -s "$PRE_COMMIT_SOURCE" "$PRE_COMMIT_TARGET"
    chmod +x "$PRE_COMMIT_TARGET"

    echo -e "${GREEN}[PASS] Pre-commit hook installed${NC}"
else
    echo -e "${YELLOW}[WARNING] Pre-commit hook script not found at $PRE_COMMIT_SOURCE${NC}"
    exit 1
fi

# Create .hooks directory for additional hook management
HOOKS_CONFIG_DIR="$PROJECT_ROOT/.hooks"
mkdir -p "$HOOKS_CONFIG_DIR"

# Create a configuration file for hooks
cat > "$HOOKS_CONFIG_DIR/config.json" <<EOF
{
  "hooks": {
    "pre-commit": {
      "enabled": true,
      "description": "Validate OpenAPI specification before commit",
      "script": "scripts/validate-openapi.sh"
    },
    "pre-push": {
      "enabled": false,
      "description": "Run full CI validation before push",
      "script": "scripts/validate-openapi.sh"
    }
  },
  "validation": {
    "min_description_coverage": 80,
    "min_example_coverage": 50,
    "required_fields": ["openapi", "info", "paths", "servers"]
  }
}
EOF

echo -e "${GREEN}[PASS] Hooks configuration created at $HOOKS_CONFIG_DIR/config.json${NC}"

# Display hook status
echo ""
echo "=== Hook Installation Summary ==="
echo "============================="
echo "Pre-commit hook: ${GREEN}Enabled${NC}"
echo "  - Validates OpenAPI specification before each commit"
echo "  - Runs OpenAPI tests and full validation"
echo "  - Checks JSON structure, required fields, and coverage"
echo ""
echo "To disable hooks temporarily:"
echo "  git commit --no-verify"
echo ""
echo "To uninstall hooks:"
echo "  rm .git/hooks/pre-commit"
echo ""
echo -e "${GREEN}[PASS] Hook installation completed successfully${NC}"
