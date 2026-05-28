#!/usr/bin/env bash
# docker-postgres-stop.sh - Stop PostgreSQL in Docker (Linux/macOS)
set -euo pipefail

source "$(dirname "$0")/env_common.sh"

echo "Stopping PostgreSQL in Docker..."

cd "$PROJECT_DIR"
docker-compose down

echo "[SUCCESS] PostgreSQL stopped"
echo ""
echo "To remove data volumes as well, run:"
echo "  docker-compose down -v"
