#!/usr/bin/env bash
# tools/check-dockerignore-sync.sh — verify root .dockerignore == deploy/docker/.dockerignore
set -euo pipefail
REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
if ! diff -q "$REPO_ROOT/.dockerignore" "$REPO_ROOT/deploy/docker/.dockerignore" >/dev/null 2>&1; then
    echo "[check-dockerignore-sync] ERROR: .dockerignore and deploy/docker/.dockerignore differ"
    diff "$REPO_ROOT/.dockerignore" "$REPO_ROOT/deploy/docker/.dockerignore" || true
    exit 1
fi
echo "[check-dockerignore-sync] OK: files are identical"