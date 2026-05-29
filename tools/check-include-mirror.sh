#!/usr/bin/env bash
# tools/check-include-mirror.sh — verify Property 7: public include layout strictly mirrors src/
#
# Asserts (per design.md §5, Property 7):
#   1. Each declared subdir exists under OAuth2Plugin/include/oauth2/
#   2. The include/oauth2/ root contains NO flat *.h files (only subdirectories) after P11
#
# Exit 0 when the mirror invariant holds; exit 1 (with diagnostics) otherwise.
set -euo pipefail

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
INCLUDE_ROOT="$REPO_ROOT/OAuth2Plugin/include/oauth2"

# Declared subdirs that MUST exist under include/oauth2/ (design §5 / Property 7)
SUBDIRS=(config error utils validation services storage filters controllers models plugin types observability)

FAIL=0

echo "========================================"
echo "Include Mirror Check (Property 7)"
echo "========================================"

if [ ! -d "$INCLUDE_ROOT" ]; then
    echo "[check-include-mirror] ERROR: include root not found: $INCLUDE_ROOT"
    exit 1
fi

# 1. Each declared subdir must exist
for subdir in "${SUBDIRS[@]}"; do
    if [ ! -d "$INCLUDE_ROOT/$subdir" ]; then
        echo "[FAIL] missing required subdir: include/oauth2/$subdir/"
        FAIL=1
    fi
done

# 2. Root must contain no flat *.h files (shims removed in P11)
flat_headers=$(find "$INCLUDE_ROOT" -maxdepth 1 -type f -name '*.h' 2>/dev/null || true)
if [ -n "$flat_headers" ]; then
    echo "[FAIL] flat .h headers still present in include/oauth2/ root (P11 should have removed all shims):"
    echo "$flat_headers" | sed "s|$REPO_ROOT/|  |"
    FAIL=1
fi

if [ "$FAIL" -ne 0 ]; then
    echo ""
    echo "[check-include-mirror] FAILED: include layout does not strictly mirror src/."
    exit 1
fi

echo "[check-include-mirror] OK: ${#SUBDIRS[@]} mirrored subdirs present; 0 flat .h headers in root."
