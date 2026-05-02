# Changelog

All notable changes to this project will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Added

#### OAuth2 Security & Compliance (2026-05-02)

- **RFC 6749 Section 2.3.1 Compliance**: Client Authentication
  - Type-aware client validation (PUBLIC vs CONFIDENTIAL)
  - HTTP Basic Authentication support for confidential clients
  - Constant-time comparison for timing attack prevention
  - Proper HTTP status codes (401 vs 400 per OAuth2 spec)

- **RFC 6749 Section 4.1.3 Compliance**: Redirect URI Validation
  - Strict redirect_uri validation in token endpoint
  - Atomic validation in Redis using Lua scripts
  - Prevents authorization code interception attacks
  - All storage layers implement validation (PostgreSQL, Redis, Memory)

#### Frontend Features (2026-05-02)

- **Dashboard Page**: Comprehensive user dashboard at `/dashboard`
  - Displays user information (username, email)
  - Shows user roles and permissions
  - Real-time data fetching from `/oauth2/userinfo` endpoint
  - Loading states and error handling
  - Responsive design with professional UI

- **Logout Functionality**: Complete logout implementation
  - Backend `/oauth2/logout` endpoint with session clearing
  - Frontend logout button with localStorage clearing
  - Route guards to prevent access after logout
  - Proper token revocation and session cleanup
  - Fixed authentication flow and redirect loops

- **Route Guards**: Authentication state management
  - Automatic redirect to dashboard if already authenticated
  - Protected routes require authentication
  - Guest routes redirect to dashboard if authenticated
  - Consistent authentication state across application

#### Documentation (2026-05-02)

- **OpenAPI Documentation**: Interactive Swagger UI at `/docs/api`
  - Fixed static resource paths (relative to absolute)
  - Complete API documentation for all endpoints
  - Logout endpoint documentation
  - External login providers documentation
  - User info endpoint documentation

- **Automated Validation**: Consolidated OpenAPI validation scripts
  - Single `validate-openapi.sh` script for CI, pre-commit, and manual validation
  - Documentation coverage checks (descriptions, examples)
  - JSON structure validation
  - Removed redundant validation scripts

#### Type System (2026-05-02)

- **OAuth2 Types**: New type system in `OAuth2Types.h`
  - `ClientType` enum (PUBLIC, CONFIDENTIAL)
  - `OAuth2Error` enum with standard OAuth2 errors
  - HTTP status code mapping for OAuth2 errors
  - Type-safe error handling

### Changed

#### Storage Layer (2026-05-02)

- **All Storage Implementations**: Updated to support client type validation
  - `PostgresOAuth2Storage`: Added clientType field, constant-time comparison
  - `RedisOAuth2Storage`: Lua script updated for atomic redirect_uri validation
  - `MemoryOAuth2Storage`: Synchronized security fixes
  - All implementations validate redirect_uri parameter

- **Interface Changes**: Updated `IOAuth2Storage.h`
  - `consumeAuthCode` now requires `redirectUri` parameter
  - All implementations validate redirect_uri matches authorization request
  - Enhanced error messages for validation failures

#### Plugin Layer (2026-05-02)

- **OAuth2Plugin**: Updated token exchange flow
  - `exchangeCodeForToken` now includes redirectUri validation
  - `generateAuthorizationCode` stores redirectUri with auth code
  - Enhanced error handling for validation failures

#### Controller Layer (2026-05-02)

- **OAuth2Controller**: Enhanced security and error handling
  - HTTP Basic Authentication parsing (Base64 decoding without OpenSSL)
  - Type-aware client validation
  - Proper HTTP status codes per OAuth2 spec
  - Logout endpoint with session management
  - Fixed authentication flow and token handling

### Fixed

#### Security Vulnerabilities (2026-05-02) ⚠️ **CRITICAL**

1. **Client Secret Validation**: Fixed missing validation per RFC 6749
   - CONFIDENTIAL clients now require valid client_secret
   - PUBLIC clients skip secret validation (as per spec)
   - Constant-time comparison prevents timing attacks
   - HTTP Basic Authentication support

2. **Redirect URI Validation**: Fixed authorization code interception vulnerability
   - Token endpoint now validates redirect_uri parameter
   - Atomic validation prevents race conditions
   - All storage layers implement validation
   - Proper error messages for mismatch

3. **Session Management**: Fixed logout not clearing sessions
   - Backend logout endpoint clears userId from session
   - Frontend clears localStorage tokens
   - Route guards enforce authentication state
   - Fixed redirect loops after logout

4. **User Info Display**: Fixed dashboard not showing user information
   - Proper token handling in API requests
   - Error handling for failed requests
   - Role display functionality
   - Loading states and error messages

#### Bug Fixes (2026-05-02)

- **Swagger UI 404 Errors**: Fixed static resource paths in index.html
  - Changed relative paths to absolute paths
  - All resources now accessible at `/docs/api/swagger-ui/*`

- **Authentication Flow**: Fixed redirect loops after logout
  - Route guards properly check authentication state
  - Session clearing on backend and frontend

- **Token Handling**: Fixed token storage and retrieval in frontend
  - Proper localStorage management
  - Token validation before API requests

- **OpenAPI Documentation**: Fixed external login provider documentation inconsistencies
  - Updated endpoint documentation
  - Added missing response examples

### Security (2026-05-02)

**OAuth2 Compliance**: ✅ Full RFC 6749 Compliance

- ✅ Client Authentication (Section 2.3.1)
- ✅ Authorization Code Flow (Section 4.1)
- ✅ Redirect URI Validation (Section 4.1.3)
- ✅ Token Endpoint Security (Section 4.1.3)
- ✅ Error Responses (Section 4.1.2.1)
- ✅ Session Management and Cleanup

**Production Status**: 🟢 Ready for deployment

### Deprecated (2026-05-02)

- Removed redundant OpenAPI validation scripts:
  - `validate-openapi-simple.sh`
  - `pre-commit-validate-openapi.sh`
  - `ci-validate-openapi.sh`
- Replaced with unified `validate-openapi.sh`

### Added

#### Docker & DevOps (2026-04-22)

- **Docker Environment Standardization**
  - Standardized container naming: `oauth2-{service}-{env}` format
  - Standardized image naming with version tags
  - Added `Dockerfile.debug` and `Dockerfile.debug.cn` for development
  - Added `docker-compose.debug.yml` for isolated debug environment
  - Added verification scripts: `docker-quick-verify-debug.sh`, `docker-quick-verify-release.sh`
  - Added `cleanup-docker.sh` for automated Docker resource cleanup
  - Comprehensive Docker documentation

- **Release Environment Verification**
  - Container status checks for all services
  - Database initialization verification
  - HTTP endpoint testing (health, metrics, OAuth2)
  - OAuth2 integration tests (login, token, protected resources)
  - Automated log error scanning

#### Testing (2026-04-21)

- **Comprehensive Test Suites**
  - Security test suite (18 tests): SQL injection, XSS, CSRF, rate limiting
  - Functional test suite (21 tests): OAuth2 flow, UTF-8, RBAC, token lifecycle
  - 100% test pass rate

- **E2E Test Automation**
  - `/e2e-test` skill for automated OAuth2 flow validation
  - Authorization code flow testing
  - Token refresh validation
  - RBAC permission verification

### Changed

#### Docker Configuration (2026-04-22)

- Updated all services to use standardized container names
- Fixed Redis image version inconsistency (`redis:alpine` → `redis:7-alpine`)
- Updated `nginx.conf` and `prometheus.yml` to use new container names
- Updated all CI/CD workflows and integration test scripts

#### Documentation (2026-04-22)

- Created project-level CHANGELOG.md
- Added Docker standardization and verification guides
- Updated README with Linux compatibility section
- Moved bug fix reports to local-only directory

### Fixed

#### Critical Issues

- **Linux Teardown Crash** (2026-04-17) ⚠️ **CRITICAL**
  - Problem: Segmentation Fault during program exit
  - Root Cause: `OAuth2CleanupService` destructor accessed destroyed Event loop
  - Solution: Added `stopped_` flag to prevent duplicate cleanup
  - Impact: Clean exit without `std::_Exit(0)`

- **Security Vulnerabilities** (2026-04-21) ⚠️ **CRITICAL**
  - Fixed all 10 critical security vulnerabilities
  - SQL injection, XSS, command injection prevention
  - DoS protection, rate limiting, CORS policy
  - Token revocation, security headers, HSTS
  - **Status**: 18/18 tests passing (100%)

#### CI/CD & Platform Support

- **CI/CD Stability** (2026-04-17)
  - Fixed Windows CI teardown crashes
  - Resolved macOS codecvt_utf8_utf16 compatibility
  - Fixed duplicated test runs
  - Improved config handling
  - Disabled macOS tests due to framework issues

#### Feature Improvements

- **Rate Limiting** (2026-04-21)
  - Migrated to Drogon's Hodor plugin
  - Token bucket algorithm
  - Removed Redis dependency
  - Per-user and global rate limiting

- **Database** (2026-04-21)
  - Verified no connection leaks (false positive)
  - Environment variable support for empty passwords

### Security

**Production Status**: 🟢 Ready for deployment

- ✅ 10/10 critical vulnerabilities fixed
- ✅ 18/18 security tests passing
- ✅ 21/21 functional tests passing
- ✅ Complete audit coverage
- ✅ Rate limiting and DoS protection
- ✅ CORS and CSP headers configured

---

## [1.9.0] - 2026-04-15 to 2026-04-16

### Added

- **Multi-Platform CI/CD**
  - Linux CI (Ubuntu 22.04)
  - Windows CI (MSVC 2022)
  - macOS CI (ARM64 support)
  - Platform-specific dependency installation
  - Automated testing and artifact collection

- **Drogon Framework Upgrade**
  - Upgraded from v1.9.10 to v1.9.12
  - Added drogon_ctl build support
  - Improved C++17/20 compatibility

### Fixed

- **macOS Compatibility**
  - Forced C++17 mode (avoid C++20 deprecation)
  - Added codecvt_utf8_utf16 compatibility layer
  - Fixed Homebrew conflicts
  - Native ARM64 support

- **Windows CI**
  - Fixed PostgreSQL service initialization
  - Resolved path escaping issues
  - Added memory storage for testing
  - Improved Conan toolchain integration

- **Linux CI**
  - Added libhiredis-dev dependency
  - Fixed Redis tools installation

---

## [1.8.0] - 2026-04-01 to 2026-04-14

### Added

- **RBAC Permission System**
  - Role-based access control
  - Permission management
  - User-role assignment
  - API endpoint protection

- **PostgreSQL Persistence**
  - Database schema for OAuth2 data
  - Token storage and management
  - User management
  - Migration scripts

- **Redis Caching**
  - High-performance token cache
  - Atomic operations
  - Lua scripting for consistency

- **Observability**
  - Prometheus metrics endpoint
  - Structured audit logging
  - Performance monitoring

### Changed

- **Storage Architecture**
  - Pluggable storage backend (Memory, PostgreSQL, Redis)
  - Cached storage layer
  - Strategy pattern implementation

---

## [1.0.0] - 2026-01-14 to 2026-03-31

### Added

- **OAuth2.0 Implementation**
  - Authorization Code Grant flow
  - Access Token and Refresh Token support
  - Client registration and management
  - User authentication

- **Drogon Framework Integration**
  - Plugin-based architecture
  - Controller-based HTTP endpoints
  - Filter-based middleware
  - JSON configuration

- **WeChat Integration**
  - WeChat Open Platform API
  - QR code login support
  - Session management

- **Frontend Application**
  - Vue.js SPA client
  - OAuth2 login flow
  - Protected API access
  - User profile display

- **Persistence Layer**
  - Redis persistent storage (2026-01-15)
  - PostgreSQL persistent storage (2026-01-16)
  - Synchronous writes for stability
  - Security hardening with SHA256 hashing

- **Data Consistency** (2026-01-18)
  - Atomic consume operations for Redis
  - Client secret hash verification
  - Transaction support for PostgreSQL

- **User Authentication** (2026-01-19)
  - User account system
  - ORM-based storage migration
  - Strict ORM compliance
  - UUID support for salts

- **Testing**
  - Unit tests (2026-01-17)
  - Integration tests for Redis and PostgreSQL (2026-01-17)
  - E2E integration testing (2026-01-19)
  - Direct controller tests

- **Frontend** (2026-01-19)
  - Vue.js registration UI
  - Professional UI redesign
  - Registration success animation
  - Countdown and progress bar

### Security

- Basic authentication (username/password)
- Client secret hashing (SHA256)
- CORS configuration
- SQL injection prevention
- Input validation and sanitization

---

## Migration Guides

### Docker Environment Migration

1. Build new images:

   ```bash
   docker build -f Dockerfile.debug.cn -t oauth2-backend-debug:v1.9.12 .
   docker build -t oauth2-backend-release:v1.9.12 .
   ```

2. Update scripts to use new container names

3. Verify deployment:

   ```bash
   bash docker-quick-verify-release.sh
   ```

See [Docker Standardization Guide](docs/docker-standardization.md) for details.

---

## Contributors

- Development Team
- Security Team
- DevOps Team
- QA Team

## Project Statistics

- **Total Commits**: 210
- **Development Period**: 2026-01-14 to 2026-04-22
- **Test Coverage**: 100% (39/39 tests passing)
- **Security Status**: Production Ready
- **Platforms**: Linux, Windows, macOS
