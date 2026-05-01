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

```text
HTTP request
  |
  |-- Drogon filter/plugin layer
  |   |-- Hodor: request rate limiting
  |   |-- OAuth2Middleware: bearer token validation
  |   `-- AuthorizationFilter: RBAC checks
  |
  |-- Controller layer
  |   |-- OAuth2Controller: authorize, login, token, userinfo, register, health
  |   |-- AdminController: protected admin sample API
  |   |-- GoogleController: Google OAuth callback exchange
  |   `-- WeChatController: WeChat OAuth callback exchange
  |
  |-- Service layer
  |   |-- AuthService: user registration and password validation
  |   `-- OAuth2CleanupService: expired OAuth2 data cleanup
  |
  |-- Plugin layer
  |   |-- OAuth2Plugin: core auth-code and token lifecycle
  |   `-- OAuth2Metrics: Prometheus metrics helpers
  |
  `-- Storage layer
      |-- IOAuth2Storage: asynchronous storage interface
      |-- MemoryOAuth2Storage: in-process test storage
      |-- PostgresOAuth2Storage: persistent storage
      |-- RedisOAuth2Storage: Redis-only storage
      `-- CachedOAuth2Storage: PostgreSQL storage with Redis L2 cache
```

## 3. Authorization-Code Flow

```text
Vue SPA                 OAuth2Backend                 Storage
  |                          |                            |
  | GET /oauth2/authorize    |                            |
  |------------------------->| validate client + redirect |
  |                          |--------------------------->|
  |                          |<---------------------------|
  |<-------------------------| render login page or 302   |
  |                          |                            |
  | POST /oauth2/login       |                            |
  |------------------------->| AuthService::validateUser  |
  |                          |--------------------------->|
  |                          |<---------------------------|
  |<-------------------------| 302 /callback?code=...     |
  |                          |                            |
  | POST /oauth2/token       |                            |
  |------------------------->| consume auth code          |
  |                          |--------------------------->|
  |                          | save access/refresh token  |
  |                          |--------------------------->|
  |<-------------------------| access_token + refresh     |
  |                          |                            |
  | GET /oauth2/userinfo     |                            |
  |------------------------->| validate bearer token      |
  |                          |--------------------------->|
  |                          |<---------------------------|
  |<-------------------------| user info JSON             |
```

## 4. Storage Strategy

`OAuth2Plugin` selects the backend through `storage_type` in `config.json`.

| storage_type | Implementation | Typical use |
|---|---|---|
| memory | MemoryOAuth2Storage | Unit tests and quick local demos |
| redis | RedisOAuth2Storage | Fast ephemeral token storage |
| postgres | PostgresOAuth2Storage | Durable production storage |
| postgres with cache | CachedOAuth2Storage | PostgreSQL source of truth with Redis cache |

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
