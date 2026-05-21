import { Page } from '@playwright/test'

/**
 * Mock API responses for E2E tests.
 * Intercepts backend calls so tests run without a real OAuth2 server.
 */

export const ADMIN_USER = {
  sub: '550e8400-e29b-41d4-a716-446655440000',
  username: 'admin',
  email: 'admin@example.com',
  email_verified: true,
  roles: ['admin', 'user'],
}

export const MOCK_CLIENTS = [
  {
    client_id: 'vue-client',
    name: 'Vue Frontend',
    client_type: 'PUBLIC',
    redirect_uris: 'http://localhost:8080/callback',
    allowed_grant_types: 'authorization_code',
  },
  {
    client_id: 'api-service',
    name: 'API Service',
    client_type: 'CONFIDENTIAL',
    redirect_uris: 'https://api.example.com/callback',
    allowed_grant_types: 'client_credentials',
  },
]

export const MOCK_USERS = [
  {
    id: '550e8400-e29b-41d4-a716-446655440000',
    username: 'admin',
    email: 'admin@example.com',
    email_verified: true,
    mfa_enabled: true,
  },
  {
    id: '660e8400-e29b-41d4-a716-446655440001',
    username: 'testuser',
    email: 'test@example.com',
    email_verified: false,
    mfa_enabled: false,
  },
]

export const MOCK_SCOPES = [
  { id: 1, name: 'openid', description: 'OpenID Connect', mapped_role: null, is_default: true, requires_admin_role: false },
  { id: 2, name: 'profile', description: 'User profile', mapped_role: null, is_default: true, requires_admin_role: false },
  { id: 3, name: 'admin', description: 'Admin access', mapped_role: 'admin', is_default: false, requires_admin_role: true },
]

export const MOCK_LOGS = [
  { id: 1, action: 'login_success', actor_type: 'user', actor_id: '550e8400-e29b-41d4-a716-446655440000', outcome: 'success', ip: '127.0.0.1', timestamp: '2026-05-21T10:00:00Z' },
  { id: 2, action: 'token_issued', actor_type: 'client', actor_id: 'vue-client', outcome: 'success', ip: '127.0.0.1', timestamp: '2026-05-21T10:00:01Z' },
  { id: 3, action: 'login_failure', actor_type: 'user', actor_id: '660e8400-e29b-41d4-a716-446655440001', outcome: 'failure', ip: '192.168.1.100', timestamp: '2026-05-21T09:55:00Z' },
]

export const MOCK_CLIENT_DETAIL = {
  status: 'success',
  client_id: 'vue-client',
  name: 'Vue Frontend',
  client_type: 'PUBLIC',
  redirect_uris: 'http://localhost:8080/callback',
  allowed_grant_types: 'authorization_code,refresh_token',
  created_at: '2026-05-20T10:00:00Z',
  scopes: ['openid', 'profile'],
}

export const MOCK_TOKENS = [
  { token_prefix: 'a1b2c3d4', client_id: 'vue-client', user_id: 'admin', scope: 'openid profile', created_at: '2026-05-21T10:00:00Z', expires_at: '2026-05-21T11:00:00Z' },
  { token_prefix: 'e5f6g7h8', client_id: 'api-service', user_id: '', scope: 'admin', created_at: '2026-05-21T09:30:00Z', expires_at: '2026-05-21T10:30:00Z' },
  { token_prefix: 'i9j0k1l2', client_id: 'vue-client', user_id: 'testuser', scope: 'openid', created_at: '2026-05-21T09:00:00Z', expires_at: '2026-05-21T10:00:00Z' },
]

export const MOCK_OIDC_KEYS = {
  status: 'success',
  kid: 'default-key-1',
  kty: 'RSA',
  alg: 'RS256',
  use: 'sig',
  jwks_uri: '/.well-known/jwks.json',
  discovery_uri: '/.well-known/openid-configuration',
  key_status: 'active',
  note: 'Key rotation is not yet implemented. Single signing key in use.',
}

/**
 * Set up all API mocks for an authenticated admin session.
 */
export async function setupAuthenticatedMocks(page: Page) {
  // Login endpoint
  await page.route('**/oauth2/login', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ code: 'mock-auth-code-12345' }),
    })
  })

  // Token exchange
  await page.route('**/oauth2/token', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({
        access_token: 'mock-access-token',
        refresh_token: 'mock-refresh-token',
        token_type: 'Bearer',
        expires_in: 3600,
      }),
    })
  })

  // UserInfo
  await page.route('**/oauth2/userinfo', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify(ADMIN_USER),
    })
  })

  // Health
  await page.route('**/health/ready', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ status: 'ok', database: 'connected', redis: 'connected' }),
    })
  })

  // Admin clients
  await page.route('**/api/admin/clients', async (route) => {
    if (route.request().method() === 'GET') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ clients: MOCK_CLIENTS }),
      })
    } else if (route.request().method() === 'POST') {
      await route.fulfill({
        status: 201,
        contentType: 'application/json',
        body: JSON.stringify({
          client_id: 'new-client-' + Date.now(),
          client_secret: 'generated-secret-abc123xyz',
          name: 'New App',
          client_type: 'CONFIDENTIAL',
        }),
      })
    } else {
      await route.continue()
    }
  })

  // Delete client / Get client detail / Update client
  await page.route('**/api/admin/clients/*', async (route) => {
    const url = route.request().url()
    // Skip if it's a sub-resource like /scopes or /reset-secret
    if (url.includes('/scopes') || url.includes('/reset-secret')) {
      await route.continue()
      return
    }
    if (route.request().method() === 'DELETE') {
      await route.fulfill({ status: 204 })
    } else if (route.request().method() === 'GET') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify(MOCK_CLIENT_DETAIL),
      })
    } else if (route.request().method() === 'PUT') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'success', message: 'Client updated successfully', client_id: 'vue-client' }),
      })
    } else {
      await route.continue()
    }
  })

  // Reset secret
  await page.route('**/api/admin/clients/*/reset-secret', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ client_secret: 'new-secret-after-reset-xyz789' }),
    })
  })

  // Admin users
  await page.route('**/api/admin/users', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ users: MOCK_USERS }),
    })
  })

  // Assign roles
  await page.route('**/api/admin/users/*/roles', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ message: 'Roles updated' }),
    })
  })

  // Scopes
  await page.route('**/api/admin/scopes', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ scopes: MOCK_SCOPES }),
    })
  })

  // Audit logs
  await page.route('**/api/admin/logs**', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ logs: MOCK_LOGS }),
    })
  })

  // Client detail (GET /api/admin/clients/:id)
  await page.route('**/api/admin/clients/*/scopes', async (route) => {
    if (route.request().method() === 'GET') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'success', scopes: ['openid', 'profile'] }),
      })
    } else if (route.request().method() === 'PUT') {
      const body = JSON.parse(route.request().postData() || '{}')
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'success', message: 'Scopes updated', scopes: body.scopes || [] }),
      })
    } else {
      await route.continue()
    }
  })

  // Tokens list
  await page.route('**/api/admin/tokens/revoke-by-client', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ status: 'success', message: 'All tokens for client revoked', count: 3 }),
    })
  })

  await page.route('**/api/admin/tokens/revoke-by-user', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify({ status: 'success', message: 'All tokens for user revoked', count: 2 }),
    })
  })

  await page.route('**/api/admin/tokens/**', async (route) => {
    if (route.request().method() === 'DELETE') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ status: 'success', message: 'Token revoked' }),
      })
    } else {
      await route.continue()
    }
  })

  await page.route('**/api/admin/tokens**', async (route) => {
    if (route.request().method() === 'GET') {
      await route.fulfill({
        status: 200,
        contentType: 'application/json',
        body: JSON.stringify({ tokens: MOCK_TOKENS, total: 3, page: 1, per_page: 50 }),
      })
    } else {
      await route.continue()
    }
  })

  // OIDC keys
  await page.route('**/api/admin/oidc/keys', async (route) => {
    await route.fulfill({
      status: 200,
      contentType: 'application/json',
      body: JSON.stringify(MOCK_OIDC_KEYS),
    })
  })
}

/**
 * Perform login through the UI.
 */
export async function loginAsAdmin(page: Page) {
  await page.goto('/admin/login')
  await page.fill('input[type="text"]', 'admin')
  await page.fill('input[type="password"]', 'admin')
  await page.click('button[type="submit"]')
  // Wait for navigation to dashboard
  await page.waitForURL('**/admin/')
}
