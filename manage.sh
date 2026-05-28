#!/usr/bin/env bash
# manage.sh - Unified project management script for Linux/macOS
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
ACTION="${1:-}"
CONFIG="Release"

# Parse global options
for arg in "$@"; do
    case "$arg" in
        -debug|--debug) CONFIG="Debug" ;;
    esac
done

show_help() {
    echo "Usage: ./manage.sh <command> [options]"
    echo ""
    echo "Commands:"
    echo "  build-backend [-debug]       Build the C++ project (Plugin + Server)"
    echo "  test-backend [-debug]        Run backend tests"
    echo "  build-frontend               Build the Vue frontend"
    echo "  dev-frontend                 Run frontend in dev mode"
    echo "  build-admin                  Build the admin frontend"
    echo "  dev-admin                    Run admin frontend in dev mode"
    echo "  run-backend [-debug]         Start the OAuth2Server binary"
    echo "  setup-db                     Create database and run migrations"
    echo "  generate-models              Generate Drogon ORM models"
    echo "  reset-password               Reset admin password to default"
    echo "  reset-lockout                Reset account lockout counters"
    echo "  test-admin-endpoints         Run admin API endpoint tests"
    echo "  test-oauth2-endpoints        Run OAuth2 endpoint tests"
    echo "  e2e-admin                    Full test (build + unit + admin API)"
    echo "  e2e-frontend                 Full test with Docker"
    echo "  full-test                    Full build + test + API test cycle"
    echo "  docker-up                    Start the full stack with Docker Compose"
    echo "  docker-down                  Stop the Docker Compose stack"
    echo "  clean                        Clean build artifacts"
    echo "  help                         Show this help"
}

if [ -z "$ACTION" ]; then
    show_help
    exit 0
fi

case "$ACTION" in
    build-backend)
        bash "$SCRIPT_DIR/scripts/backend/build.sh" "--${CONFIG,,}"
        ;;
    test-backend)
        bash "$SCRIPT_DIR/scripts/backend/test.sh" "--${CONFIG,,}"
        ;;
    build-frontend)
        cd "$SCRIPT_DIR/OAuth2Frontend"
        npm install
        npm run build
        ;;
    dev-frontend)
        cd "$SCRIPT_DIR/OAuth2Frontend"
        npm install
        npm run dev
        ;;
    build-admin)
        cd "$SCRIPT_DIR/OAuth2Admin"
        npm install
        npm run build
        ;;
    dev-admin)
        cd "$SCRIPT_DIR/OAuth2Admin"
        npm install
        npm run dev
        ;;
    run-backend)
        bash "$SCRIPT_DIR/scripts/backend/run-server.sh" "--${CONFIG,,}"
        ;;
    setup-db)
        bash "$SCRIPT_DIR/scripts/backend/setup-database.sh"
        ;;
    generate-models)
        bash "$SCRIPT_DIR/scripts/backend/generate-models.sh"
        ;;
    reset-password)
        bash "$SCRIPT_DIR/scripts/backend/reset-admin-password.sh"
        ;;
    reset-lockout)
        bash "$SCRIPT_DIR/scripts/backend/reset-account-lockout.sh"
        ;;
    test-admin-endpoints)
        bash "$SCRIPT_DIR/scripts/backend/test-admin-endpoints.sh"
        ;;
    test-oauth2-endpoints)
        bash "$SCRIPT_DIR/scripts/backend/test-oauth2-endpoints.sh"
        ;;
    e2e-admin)
        bash "$SCRIPT_DIR/scripts/backend/full-test.sh" "--${CONFIG,,}"
        ;;
    e2e-frontend)
        bash "$SCRIPT_DIR/scripts/backend/full-test-docker.sh" "--${CONFIG,,}"
        ;;
    full-test)
        bash "$SCRIPT_DIR/scripts/backend/full-test.sh" "--${CONFIG,,}"
        ;;
    docker-up)
        cd "$SCRIPT_DIR"
        docker compose -f deploy/docker/docker-compose.yml --project-directory . up -d
        ;;
    docker-down)
        cd "$SCRIPT_DIR"
        docker compose -f deploy/docker/docker-compose.yml --project-directory . down
        ;;
    clean)
        rm -rf "$SCRIPT_DIR/build"
        rm -rf "$SCRIPT_DIR/OAuth2Frontend/dist"
        rm -rf "$SCRIPT_DIR/OAuth2Admin/dist"
        echo "Cleaned build artifacts."
        ;;
    help)
        show_help
        ;;
    *)
        echo "Unknown command: $ACTION"
        show_help
        exit 1
        ;;
esac
