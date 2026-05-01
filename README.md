# Drogon OAuth2.0 Provider & Vue Client Demo

![Linux CI](https://github.com/lucaswang420/OAuth2-plugin-example/workflows/ci-linux.yml/badge.svg)
![Windows CI](https://github.com/lucaswang420/OAuth2-plugin-example/workflows/ci-windows.yml/badge.svg)
![macOS CI](https://github.com/lucaswang420/OAuth2-plugin-example/workflows/ci-macos.yml/badge.svg)

This project demonstrates how to implement a fully functional OAuth2.0 Provider (Server) using the [Drogon C++ Web Framework](https://github.com/drogonframework/drogon) and a modern Client Application using [Vue.js](https://vuejs.org/).

It implements the **Authorization Code Grant** flow and supports:

1. **Local Authentication**: Login with credentials stored in the Drogon backend.
2. **External Provider Integration**: A "Login with WeChat" flow demonstrating server-side integration with the WeChat Open Platform API.

## Project Structure

```
OAuth2Test/
|-- OAuth2Backend/      # C++ OAuth2 Provider (Drogon)
|   |-- controllers/    # API verify (OAuth2, WeChat)
|   |-- filters/        # Middleware (Token Validation)
|   |-- plugins/        # Core OAuth2 Logic Plugin
|   |-- views/          # Server-side Login Pages (CSP)
|   `-- config.json     # App Configuration
`-- OAuth2Frontend/     # Vue.js Client Application
    |-- src/views/      # Login & Callback Pages
    `-- ...
```

## Prerequisites

### Platform Requirements

- **Linux (Ubuntu 20.04+, Debian 11+)**:
  - GCC 7.5+ or Clang 6.0+
  - CMake 3.20+
  - PostgreSQL 14+ (optional, for persistence)
  - Redis 7+ (optional, for caching)

- **Windows (10/11, Server 2019/2022)**:
  - Visual Studio 2019/2022 (MSVC v19.2+)
  - CMake 3.20+
  - Conan 1.50+ (package manager)
  - Git for Windows

- **macOS (11+ Big Sur, including Apple Silicon)**:
  - Xcode 12.2+ or Command Line Tools
  - Clang 11.0+
  - CMake 3.20+
  - Homebrew (for dependencies)

- **Frontend** (All platforms):
  - [Node.js](https://nodejs.org/) 16+ & npm 8+

- **Docker** (Optional, for cross-platform development):
  - Docker Desktop 4.0+ (Windows/macOS)
  - Docker Engine 20.10+ (Linux)

## CI/CD

This project uses comprehensive multi-platform CI/CD to ensure code quality across all major platforms:

### Platforms

- **Linux (Ubuntu 22.04)**: GCC with system package management
  - Full testing with PostgreSQL and Redis containers
  - No caching for consistent builds
  
- **Windows (Server 2022)**: MSVC 2022 with Conan package management
  - Full testing with memory storage (no database servers)
  - CI-optimized configuration for faster builds
  
- **macOS (14)**: Clang with Homebrew, ARM64 architecture
  - Build-only verification (tests disabled due to framework compatibility)
  - Pure C++17 enforcement to avoid codecvt_utf8_utf16 issues

### Features

- [x] Full integration testing with PostgreSQL and Redis (Linux)
- [x] Platform-specific optimizations and dependency management
- [x] Automatic artifact collection and test log collection on failure
- [x] Detailed platform diagnostics for debugging
- [x] Memory storage testing on Windows for faster CI cycles

### Known Issues

- **macOS Runtime Issue**: Tests disabled due to Drogon framework compatibility issue with C++17/20 on macOS. Builds succeed but runtime crashes occur during test execution. This is a framework-level issue, not a code issue.

### Testing Coverage

- [x] **18/18** Security tests (100%) - SQL injection, XSS, CORS, rate limiting, etc.
- [x] **21/21** Functional tests (100%) - OAuth2 flow, UTF-8, RBAC, token lifecycle
- Unit tests for OAuth2 core logic
- Integration tests for PostgreSQL persistence (Linux)
- Integration tests for Redis caching (Linux)
- Memory storage tests (Windows/Linux)
- RBAC permission system tests
- End-to-end OAuth2 authorization flow tests

**Security & Quality Status**: **Production Ready**
- All 10 critical security vulnerabilities fixed
- 18 bugs resolved (51% completion rate)
- 17 remaining bugs are low-priority technical debt
- 1 bug confirmed as false positive (DB connection leak)

See test reports (local documentation):
- [Security Test Report](reports/bug-fix-2026-04-21/SECURITY_TEST_REPORT.md) - Comprehensive security testing results
- [Functional Test Report](reports/bug-fix-2026-04-21/FUNCTIONAL_TEST_REPORT.md) - Complete functional testing results
- [Remaining Bugs Analysis](reports/bug-fix-2026-04-21/REMAINING_BUGS_ANALYSIS.md) - Priority analysis for remaining bugs
- [Remaining Bugs List](reports/bug-fix-2026-04-21/REMAINING_BUGS.md) - Detailed bug status and risk assessment

See individual workflow files for detailed configuration:
- [.github/workflows/ci-linux.yml](.github/workflows/ci-linux.yml)
- [.github/workflows/ci-windows.yml](.github/workflows/ci-windows.yml)
- [.github/workflows/ci-macos.yml](.github/workflows/ci-macos.yml)

## 1. Backend Setup (OAuth2Backend)

The backend handles OAuth2 requests, issues tokens, and validates API access.

### Platform-Specific Installation

#### Windows

```powershell
cd OAuth2Backend
# Install dependencies and configure CMake
./build.bat

# Run the server
cd build
Release/OAuth2Server.exe
```

#### Linux

```bash
cd OAuth2Backend
# Install system dependencies
sudo apt-get update
sudo apt-get install -y cmake g++ libjsoncpp-dev libpq-dev libhiredis-dev

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
make -j$(nproc)

# Run the server
./OAuth2Server
```

#### macOS

```bash
cd OAuth2Backend
# Install dependencies via Homebrew
brew install cmake jsoncpp ossp-uuid openssl@1.1

# Build
mkdir build && cd build
cmake .. -DCMAKE_BUILD_TYPE=Release -DOPENSSL_ROOT_DIR=$(brew --prefix openssl@1.1)
make -j$(sysctl -n hw.ncpu)

# Run the server
./OAuth2Server
```

#### Docker (All Platforms)

```bash
# Build and run with Docker Compose
docker-compose up -d

# Or build manually
docker build -t oauth2-backend:latest .
docker run -p 5555:5555 oauth2-backend:latest
```

The server listens on `http://localhost:5555` on all platforms.

### Configuration (Optional: WeChat)

To enable real WeChat login, edit `controllers/WeChatController.cc` and replace `YOUR_WECHAT_APPID` / `YOUR_WECHAT_SECRET` with your actual credentials.

## 2. Frontend Setup (OAuth2Frontend)

The frontend is a Single Page Application (SPA) acting as the OAuth2 Client.

### Installation

```bash
cd OAuth2Frontend
npm install
```

### Running the Client

```bash
npm run dev
```

The client runs on `http://localhost:5173`.

### Configuration (Optional: WeChat)

To enable real WeChat login, edit `src/views/Login.vue` and set `APPID` and `REDIRECT_URI` (Must match your domain).

## Storage & Persistence

The project supports pluggable storage backends for OAuth2 data.

### Supported Backends

1. **Memory** (Default): Fast, volatile storage. Best for testing.
2. **PostgreSQL**: Persistent, SQL-based storage.
3. **Redis**: High-performance, persistent Key-Value storage.

### Configuration

Edit `OAuth2Backend/config.json`:

```json
{
  "oauth2": {
    "storage_type": "postgres" // Options: "memory", "postgres", "redis"
  },
  "redis": {
    "host": "127.0.0.1",
    "port": 6379,
    "passwd": "your_password"
  }
}
```

### Security Hardening

Client Secrets are securely stored using **SHA256 Hashing with Salt**.

### Persistence & Storage

This project uses a flexible persistence layer supporting **PostgreSQL** (Production) and **Redis** (High Performance).

For detailed architecture, supported backends, and schema designs, please refer to:
See **[Data Persistence Guide](OAuth2Backend/docs/data_persistence.md)**

### Data Consistency & Security

We implement **Atomic Consume** operations and **SHA256 Hashing** to ensure high security and consistency.

For implementation details (Lua Scripts, Threat Models, Token Lifecycle):
See **[Data Consistency Guide](OAuth2Backend/docs/data_consistency.md)**
See **[Security Architecture](OAuth2Backend/docs/security_architecture.md)**

### Observability

Production-ready monitoring with Prometheus Metrics and Structured Audit Logs.
See **[Observability Guide](OAuth2Backend/docs/observability.md)**

### Security Hardening

We implement Rate Limiting and Security Headers to protect against attacks.
See **[Security Hardening Guide](OAuth2Backend/docs/security_hardening.md)**

**Verified Security Features** (as of 2026-04-21):
- [x] SQL injection protection (parameterized queries)
- [x] XSS attack prevention (input validation + CSP headers)
- [x] Command injection prevention
- [x] DoS protection (input length limits: username 100 chars, password 200 chars)
- [x] Rate limiting (brute force protection)
- [x] CORS policy (domain whitelist)
- [x] Token revocation mechanism
- [x] Complete security HTTP headers
- [x] HSTS (HTTPS-only configuration)
- [x] Sensitive data protection (POST body credential transmission)

### Configuration & Deployment (New)

Full guide on Environment Variables and Docker deployment.
See **[Configuration Guide](OAuth2Backend/docs/configuration_guide.md)**

### RBAC Permission System

Role-Based Access Control using `AuthorizationFilter` and `rbac_rules` configuration.
Matches URL patterns to required roles (e.g. `/api/admin/.*` -> `["admin"]`).
See **[RBAC Guide](OAuth2Backend/docs/rbac_guide.md)**

### Multi-Platform Compatibility

This project provides **full cross-platform support** with platform-specific optimizations and validated workflows.

#### Platform Matrix

| Platform | Build System | Package Manager | Testing | Production | Status |
|----------|-------------|-----------------|---------|------------|--------|
| **Linux** | CMake + Make | System (apt/yum) | Full (PostgreSQL + Redis) | Docker / Systemd | Stable |
| **Windows** | CMake + MSBuild | Conan | Full (Memory Storage) | Service / EXE | Stable |
| **macOS** | CMake + Make | Homebrew | Build-only | Development | Limited* |

*macOS is recommended for development and build verification only. Runtime testing limited due to Drogon framework libc++ compatibility issues on ARM64.

#### Platform-Specific Features

**Linux**:
- Systemd service integration
- Native PostgreSQL and Redis support
- Production deployment with Docker
- Comprehensive testing (39/39 tests passing)

**Windows**:
- MSVC 2022 optimization
- Conan package management
- Windows Service integration
- Memory storage for fast CI cycles
- Teardown crash protection (std::_Exit fix)

**macOS**:
- Apple Silicon (ARM64) support
- Homebrew dependency management
- Build verification for cross-platform compatibility
- Development and testing environment

#### Docker Support (All Platforms)

```bash
# Development
docker-compose up -d

# Production deployment
docker build -t oauth2-backend:v1.9.12 .
docker run -d -p 5555:5555 --name oauth2-server oauth2-backend:v1.9.12
```

#### Platform-Specific Fixes

**Linux Teardown Crash Fix** (2026-04-22):
- [x] Fixed SegFault during program exit
- `OAuth2CleanupService` with `stopped_` flag prevents duplicate cleanup
- Tests exit cleanly without `std::_Exit(0)`

**Windows Teardown Crash Fix** (2026-04-22):
- [x] Fixed SegFault in `thr.join()` during teardown
- Uses `std::_Exit(0)` for successful tests to bypass framework bugs
- Proper `queueInLoop(quit())` handling

**macOS Compatibility** (2026-04-15):
- [x] Fixed C++17/20 compatibility issues
- [x] ARM64 (Apple Silicon) support
- [!] Tests disabled (build-only verification)

**Verification**:
```bash
# Linux/macOS
docker build --no-cache -f Dockerfile.debug.cn -t oauth2-backend-debug:v1.9.12 .
docker-compose -f docker-compose.debug.yml run --rm debug-env bash /app/docker-quick-verify-debug.sh

# Expected result:
# assertions: 46 | 46 passed | 0 failed
# test cases: 11 | 11 passed | 0 failed
# SUCCESS: No crash during teardown!
```

For detailed debugging and verification instructions, see:
- [Docker Debug Verification Guide](docs/docker-debug-verification.md)
- [Docker Standardization Guide](docs/docker-standardization.md)

## Features & Endpoints

> **OpenAPI Specification**: [openapi.yaml](OAuth2Backend/openapi.yaml)

| Feature | Endpoint / Description |
|---------|------------------------|
| **Authorize** | `GET /oauth2/authorize` - Logic to handle Authorization requests. |
| **Token** | `POST /oauth2/token` - Exchange Auth Code for Access Token. |
| **User Info** | `GET /oauth2/userinfo` - Protected Endpoint (Requires Bearer Token). |
| **WeChat Login** | `POST /api/wechat/login` - Server-side exchange of WeChat code for Session. |
| **Persistence** | Support for Redis/Postgres backends via Strategy Pattern. |
| **Expiration** | Auto-cleanup of expired tokens (Hourly) via Scheduler. |

## Usage Guide

1. **Start Backend & Frontend** using the commands above.
2. Open **<http://localhost:5173>**.
3. **Local Login**:
    - Click "Login with Drogon".
    - Credentials: `admin` / `admin`.
    - Observe successful redirect and user info display.
4. **WeChat Login**:
    - Requires valid AppID Configuration.
    - Click "Login with WeChat", scan QR code, and verify login.

## License

This project is licensed under the [MIT License](LICENSE).
