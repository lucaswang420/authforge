# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Project Overview

**authforge** — Production-grade OAuth2.0/OIDC authorization server built with Drogon (C++17), PostgreSQL, Redis, and Vue.js frontends. Supports RFC 6749, RFC 7662, RFC 7009, RFC 8414, RFC 8628, RFC 7591.

## Build & Run Commands

### Backend (C++)

```bash
# Unified management scripts (recommended)
./manage.sh build-backend          # Linux/macOS (Release)
./manage.sh build-backend -debug   # Linux/macOS (Debug)
./manage.ps1 build-backend         # Windows
./manage.ps1 build-backend -debug  # Windows

# Direct scripts
scripts/backend/build.sh [--debug] [--build-drogon] [--sanitizer=thread|address]  # Linux/macOS
scripts/backend/build.bat [-debug]                                                 # Windows (uses Conan)
```

### Run Server

```bash
./manage.sh run-backend    # Linux/macOS
./manage.ps1 run-backend   # Windows
# Or directly: build/OAuth2Server/{Debug|Release}/OAuth2Server -c config.json
```

### Tests

```bash
# C++ unit/integration tests (ctest)
./manage.sh test-backend          # Linux/macOS
./manage.ps1 test-backend         # Windows
cd build && ctest --output-on-failure   # Direct ctest

# Run specific test categories
ctest -R Unit          # Unit tests only
ctest -R Integration   # Integration tests only
ctest -R E2E           # End-to-end tests
ctest -R Security      # Security tests
ctest -R Performance   # Performance/benchmark tests

# Backend API endpoint tests (PowerShell)
scripts/backend/test-admin-endpoints.ps1    # Admin API (37 tests)
scripts/backend/test-oauth2-endpoints.ps1   # OAuth2 core (17 tests)

# Full cycle (build + unit tests + API tests)
./manage.sh full-test     # Linux/macOS
./manage.ps1 full-test    # Windows
```

### Frontend

```bash
# Admin console (OAuth2Admin)
cd OAuth2Admin && npm install && npm run dev          # Dev server at localhost:5174/admin/
cd OAuth2Admin && npx playwright test                  # E2E tests

# User frontend (OAuth2Frontend)
cd OAuth2Frontend && npm install && npm run dev        # Dev server at localhost:5173
```

### Docker

```bash
./manage.sh docker-up     # Full stack: backend + PostgreSQL + Redis + frontends
./manage.sh docker-down
# Ports: frontend :8080, admin :5174, backend API :5555
```

## Architecture

### Repository Layout

```text
authforge/
├── OAuth2Plugin/       # Core OAuth2 library (OBJECT lib, reusable by third parties)
│   ├── include/oauth2/ # Public headers
│   │   ├── storage/    # IOAuth2Storage interface + 4 implementations
│   │   ├── services/   # TokenService, ClientService, IdentityService
│   │   ├── plugin/     # OAuth2Plugin (Drogon plugin, DI container)
│   │   ├── filters/    # OAuth2AuthFilter, AuthorizationFilter, RequestValidationFilter
│   │   ├── error/      # Error envelope system (ErrorCatalog, ErrorHandler, ErrorResponder)
│   │   ├── utils/      # CryptoUtils, JwkManager, PasswordHasher, TotpUtils, EmailService
│   │   ├── config/     # ConfigManager with env var overrides
│   │   ├── types/      # OAuth2Types.h (DTO structs)
│   │   └── validation/ # RuleEngine for request validation
│   └── src/            # Implementations (mirrors include/ structure)
├── OAuth2Server/       # Executable server
│   ├── main.cc         # Server bootstrap, config loading, plugin registration
│   ├── controllers/    # HTTP controllers (thin, delegate to Plugin/Service)
│   ├── AuthService.*   # User authentication service
│   └── SchemaManager.* # Database schema setup
├── OAuth2Admin/        # Admin console (Vue 3 + Pinia + TailwindCSS + Playwright)
├── OAuth2Frontend/     # User-facing frontend (Vue 3)
└── scripts/            # Build/test/deploy scripts (Windows + Linux)
```

### Layered Data Flow

```text
HTTP Request
  → OAuth2Server/controllers/*.cc (thin HTTP layer, format validation)
    → OAuth2Plugin/filters/ (auth, authorization, request validation middleware)
      → OAuth2Plugin/services/ (business logic: TokenService, ClientService, IdentityService)
        → OAuth2Plugin/storage/ (data access via IOAuth2Storage interface)
          → PostgresOAuth2Storage | RedisOAuth2Storage | MemoryOAuth2Storage
            ↑ CachedOAuth2Storage wraps any storage with Redis L2 cache
```

### Key Patterns

**Plugin-based DI**: `OAuth2Plugin` is a Drogon `HttpPlugin<>` singleton. `initAndStart()` creates storage, services (TokenService, ClientService, IdentityService), JwkManager, and CleanupService. Other code accesses them via `drogon::app().getPlugin<OAuth2Plugin>()`.

**Storage Strategy pattern**: `IOAuth2Storage` is the abstract interface. Config `storage_type` selects the backend:
- `postgres` → `CachedOAuth2Storage(PostgresOAuth2Storage + Redis)` (production)
- `redis` → `RedisOAuth2Storage` (cache-only)
- `memory` → `MemoryOAuth2Storage` (testing, no external deps)

**Async callback chains**: All storage/service methods are async with `std::function<void(...)> &&callback`. Services hold `shared_ptr<IOAuth2Storage>` for lifetime safety. Async continuations capture `auto self = shared_from_this()` to prevent use-after-free.

**Error system**: `Error` struct → `ErrorCatalog` (single source of truth for error codes/messages) → `ErrorResponder` renders JSON error envelopes. All errors use stable string codes (e.g., `AUTH_INVALID_CREDENTIALS`), not integers.

## Critical Rules

- **Never modify ORM model classes** (`OAuth2Plugin/include/oauth2/models/*.h`). Regenerate with `./manage.sh generate-models` using `drogon_ctl`.
- **Never use raw SQL** except for `UPDATE ... RETURNING`, DDL, or batch operations with documented justification. Use `Mapper::findBy`/`insert`/`update` instead.
- **Never use coroutines** (`CoroMapper` is strictly forbidden). Use async callbacks.
- **Lambda captures**: Always capture `[sharedCb]` (shared_ptr to callback). Never capture raw `[this]` or `[&var]` in async contexts. Use `shared_from_this()` or `weak_ptr` for self-capture.
- **No emoji in code or output**. Use ASCII markers: `[+]` allowed, `[-]` forbidden, `[!]` warning.
- **`git push` is forbidden** — commit is allowed, push requires human review.
- **Debug code must be removed** after fixing issues. Use `LOG_DEBUG` for conditional logging.

## Configuration

- `OAuth2Server/config.json` — main config (storage type, DB, Redis, token TTLs, OIDC)
- `config.{dev,ci,prod}.json` — environment overrides
- Sensitive values via env vars: `OAUTH2_DB_PASSWORD`, `OAUTH2_REDIS_PASSWORD`
- Build option `-DOAUTH2_MEMORY_TESTS_ONLY=ON` builds tests that need no external databases

## Test Architecture

Tests live in `OAuth2Server/test/` organized by category:

| Directory | CTest label | Description |
| ---------- | ----------- | ----------- |
| `unit/` | Unit | No external deps, pure logic |
| `integration/` | Integration | Multi-component with DB/Redis |
| `integration/concurrency/` | Integration | Race condition and use-after-free tests |
| `e2e/` | E2E | Full OAuth2 flows from client perspective |
| `security/` | Security | Injection, bypass, vulnerability tests |
| `performance/` | Performance | Benchmarks |

Test base: `TestBase.h` provides `TestTransaction` (RAII rollback wrapper). Categories/priorities defined in `test_categories.h`.

CI runs on three platforms: Linux (Ubuntu 22.04, GCC, PostgreSQL+Redis), Windows (MSVC 2022, memory storage), macOS (Clang, ARM64 build verification).

## Conventions

- **C++17**, Google C++ Style Guide, 100-char line limit, `clang-format` enforced
- Namespace: `oauth2::` for plugin code, `common::error` / `common::config` for cross-cutting
- Async callback parameter order: inputs first, `std::function<void(result)> &&callback` last
- `enable_shared_from_this` on all classes that capture `self` in async callbacks (TokenService, CleanupService, storage implementations)
- Build output: `build/OAuth2Server/{Debug|Release}/` for the server, `build/OAuth2Server/test/{Debug|Release}/` for tests
