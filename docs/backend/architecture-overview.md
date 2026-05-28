# OAuth2 System Architecture Overview

This document summarizes the Drogon OAuth2 plugin example from the service,
storage, request-flow, and deployment perspectives.

## 1. Technology Stack

| Layer | Technology | Purpose |
|---|---|---|
| Web framework | Drogon | High-performance asynchronous C++ HTTP service |
| Primary database | PostgreSQL 15 | Users, roles, clients, auth codes, and tokens |
| Cache / KV store | Redis | Token cache, auth-code cache, and rate-limit data |
| Frontend | Vue 3 + Vite | SPA client for OAuth2 authorization-code flow |
| Deployment | Docker Compose | Local and production-like full-stack deployment |
| Observability | Prometheus | Metrics collection through Drogon PromExporter |

## 2. Module Layout

The project has been refactored into a core Plugin Library and a Demo Server to ensure true pluggability.

```text
HTTP request
  |
  |-- OAuth2Server (Demo Server Application)
  |   |-- App Controllers: OAuth2Controller (login UI/API), AdminController, GoogleController
  |   `-- Services: AuthService (local app authentication)
  |
  `-- OAuth2Plugin (Standalone CMake Library)
      |-- Plugin Core
      |   `-- OAuth2Plugin: initialization and lifecycle manager
      |
      |-- Protocol Controllers & Filters (Auto-registered)
      |   |-- OAuth2StandardController: handles /oauth2/authorize, /token, /userinfo
      |   `-- Filters: OAuth2Middleware, ValidationFilter, AuthorizationFilter
      |
      |-- Service Layer (Core Business Logic)
      |   |-- TokenService: PKCE, code/token generation and exchange
      |   |-- ClientService: client credentials and redirect URI validation
      |   `-- IdentityService: RBAC, subject mapping, and user consent
      |
      `-- Storage Layer
          |-- IOAuth2Storage: asynchronous storage interface
          |-- MemoryOAuth2Storage: in-process test storage
          |-- PostgresOAuth2Storage: persistent storage
          |-- RedisOAuth2Storage: Redis-only storage
          `-- CachedOAuth2Storage: PostgreSQL storage with L1 Memory & L2 Redis Cache
```

## 3. Authorization-Code Flow

```text
Vue SPA                 OAuth2Server (App)        OAuth2Plugin (Core)        Storage
  |                          |                            |                     |
  | GET /oauth2/authorize    |                            |                     |
  |------------------------------------------------------>| validate client     |
  |                          |                            |-------------------->|
  |                          |                            |<--------------------|
  |<------------------------------------------------------| 302 to App /login   |
  |                          |                            |                     |
  | POST /api/login          |                            |                     |
  |------------------------->| AuthService::validateUser  |                     |
  |                          |------------------------------------------------->|
  |                          |<-------------------------------------------------|
  |<-------------------------| 302 /callback?code=...     |                     |
  |                          |                            |                     |
  | POST /oauth2/token       |                            |                     |
  |------------------------------------------------------>| consume auth code   |
  |                          |                            |-------------------->|
  |                          |                            | save access token   |
  |                          |                            |-------------------->|
  |<------------------------------------------------------| access_token JSON   |
  |                          |                            |                     |
  | GET /oauth2/userinfo     |                            |                     |
  |------------------------------------------------------>| validate token      |
  |                          |                            |-------------------->|
  |                          |                            |<--------------------|
  |<------------------------------------------------------| user info JSON      |
```

## 4. Storage Strategy

`OAuth2Plugin` selects the backend through `storage_type` in `config.json`.

| storage_type | Implementation | Typical use |
|---|---|---|
| memory | MemoryOAuth2Storage | Unit tests and quick local demos |
| redis | RedisOAuth2Storage | Fast ephemeral token storage |
| postgres | PostgresOAuth2Storage | Durable production storage |
| postgres with cache | CachedOAuth2Storage | PostgreSQL source of truth with L1 Memory and L2 Redis Cache |

## 5. Frontend and Backend Integration

The frontend starts the OAuth2 authorization-code flow from the login page,
stores the CSRF `state` in localStorage, handles `/callback`, exchanges the
code for tokens, and then calls `/oauth2/userinfo`.

For third-party providers, the frontend receives the external authorization
code and sends it to the backend endpoints:

- `/api/google/login`
- `/api/wechat/login`

The backend performs the provider token exchange server-side so provider
secrets are not exposed to the browser.

## 6. Deployment Notes

The provided Docker Compose stack starts:

- `oauth2-frontend` on port `8080`
- `oauth2-backend` on port `5555`
- PostgreSQL on host port `5433`
- Redis on host port `6380`
- Prometheus on port `9090`

In production, terminate TLS at a reverse proxy and proxy API requests to the
Drogon backend.
