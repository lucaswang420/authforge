# OAuth2 Vue Frontend

A modern Vue 3 + Vite application demonstrating OAuth2.0 Client implementation with full RFC compliance and enterprise security features.

## Features

### OAuth2 Standard Compliance ✅

- **RFC 6749**: Authorization Code Grant with PKCE (RFC 7636)
- **RFC 7662**: Token Introspection for real-time token validation
- **RFC 7009**: Token Revocation for secure logout
- **RFC 8414**: Authorization Server Metadata discovery
- **RFC 6749**: Standardized error handling

### Security Enhancements 🔒

- **PKCE Support**: Code verifier/challenge generation with SHA-256
- **State Parameter**: CSRF protection with UUID-based state
- **Token Validation**: Automatic token introspection and expiration checking
- **Secure Storage**: Encrypted localStorage with expiration tracking
- **Token Revocation**: RFC 7009 compliant token invalidation

### User Experience 🚀

- **Multi-Provider**: Login with Google, WeChat, and local OAuth2
- **Permission Visualization**: Display user roles and OAuth2 scopes
- **Token Metadata**: Show token issued/expired time, issuer, and audience
- **Responsive UI**: Beautiful glassmorphism-inspired design
- **Error Handling**: RFC-compliant error messages with user-friendly display

## Project Setup

### Prerequisites

- Node.js (v16+)
- npm
- OAuth2 Backend running on `http://localhost:5555`

### Installation

```bash
npm install
```

### Development

Start the development server:

```bash
npm run dev
```

App runs at `http://localhost:5173`.

## Configuration

### Dynamic Configuration

Configuration supports environment variables and runtime config (`public/config.json`):

```javascript
// .env
VITE_OAUTH2_CLIENT_ID=vue-client
VITE_API_BASE_URL=http://localhost:5555
VITE_WECHAT_ENABLED=false
VITE_GOOGLE_ENABLED=false
```

### OAuth2 Helper Utilities

New `src/utils/oauth2Helper.js` provides comprehensive OAuth2 functionality:

```javascript
import {
  buildAuthorizationUrl,    // PKCE-enabled auth URL
  exchangeCodeForToken,     // Token exchange with PKCE
  introspectToken,          // RFC 7662 introspection
  revokeToken,              // RFC 7009 revocation
  parseOAuth2Error,         // RFC 6749 error parsing
  validateToken,            // Token validation
  storeTokens,              // Secure token storage
  getValidAccessToken       // Get valid token or null
} from '@/utils/oauth2Helper'
```

### PKCE Flow

The frontend automatically implements PKCE for enhanced security:

1. **Authorization Request**: Generate code_verifier and code_challenge
2. **Token Exchange**: Include code_verifier in token request
3. **Backend Validation**: Server validates PKCE parameters

## OAuth2 Flow

### 1. Authorization Request with PKCE

```javascript
// Automatic in Login.vue
const authUrl = await buildAuthorizationUrl({
  endpoint: '/oauth2/authorize',
  clientId: 'vue-client',
  redirectUri: 'http://localhost:5173/callback',
  scope: 'openid profile',
  state: generateState(),
  usePKCE: true  // Enables PKCE security
})
```

### 2. Token Exchange

```javascript
// Automatic in Callback.vue
const tokenData = await exchangeCodeForToken({
  code: authorizationCode,
  redirectUri: 'http://localhost:5173/callback',
  clientId: 'vue-client'
})
// Includes automatic code_verifier from PKCE
```

### 3. Token Introspection

```javascript
// Get detailed token information
const tokenInfo = await introspectToken(accessToken)
// Returns: { active, exp, iat, sub, aud, iss, scope, ... }
```

### 4. Token Revocation on Logout

```javascript
// Secure logout with token revocation
await revokeToken(accessToken)
clearTokens()
```

## Testing

Run unit tests with Vitest:

```bash
npm test
```

Run tests in watch mode:

```bash
npm test:watch
```

## Security Features

### PKCE Implementation

- **Code Verifier**: 43-character URL-safe random string
- **Code Challenge**: SHA-256 hash of verifier, base64url encoded
- **Method**: S256 (SHA-256)
- **Storage**: Session-based, auto-cleanup after token exchange

### Token Management

- **Storage**: Secure localStorage with expiration tracking
- **Validation**: Automatic token introspection on dashboard load
- **Refresh**: Auto-refresh on expiration (when refresh_token available)
- **Revocation**: RFC 7009 compliant token invalidation

### Error Handling

- **RFC 6749**: Standard error codes (invalid_request, invalid_client, etc.)
- **Format**: Support for JSON and form-encoded error responses
- **Display**: User-friendly error messages with technical details

## Documentation

- [Authorization Flow Guide](docs/FRONTEND_AUTH_FLOW.md)
- [OAuth2 Helper API](src/utils/oauth2Helper.js)
- [Component Documentation](src/views/)

## Tech Stack

- **Framework**: Vue 3 (Composition API)
- **Build Tool**: Vite
- **Routing**: Vue Router
- **Testing**: Vitest + Vue Test Utils
- **Styling**: Native CSS (Scoped)

## OAuth2 Compliance

This frontend demonstrates full OAuth2.0 compliance:

| RFC | Feature | Status |
| --- | --- | --- |
| RFC 6749 | Authorization Code Grant | ✅ Implemented |
| RFC 7636 | PKCE for Public Clients | ✅ Implemented |
| RFC 7662 | Token Introspection | ✅ Implemented |
| RFC 7009 | Token Revocation | ✅ Implemented |
| RFC 8414 | Authorization Server Metadata | ✅ Supported |
| RFC 6749 | Error Handling | ✅ Implemented |

## Backend Integration

This frontend is designed to work with the OAuth2 Backend:

- **Token Endpoint**: `/oauth2/token`
- **Authorization Endpoint**: `/oauth2/authorize`
- **User Info Endpoint**: `/oauth2/userinfo`
- **Introspection Endpoint**: `/oauth2/introspect`
- **Revocation Endpoint**: `/oauth2/revoke`
- **Metadata Endpoint**: `/.well-known/oauth-authorization-server`

## Contributing

When adding new OAuth2 features, ensure:
1. Use `oauth2Helper.js` utilities for consistency
2. Follow RFC specifications for error handling
3. Include PKCE parameters in authorization requests
4. Implement proper token validation and revocation
5. Add tests for new functionality
