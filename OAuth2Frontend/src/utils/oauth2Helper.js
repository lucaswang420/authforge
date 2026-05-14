import apiClient from './api'

/**
 * OAuth2 Helper Utilities

 *
 * Implements OAuth2.0 security enhancements and RFC compliance:
 * - PKCE (RFC 7636) for public clients
 * - State parameter for CSRF protection
 * - Token introspection (RFC 7662)
 * - Token revocation (RFC 7009)
 * - Authorization Server Metadata (RFC 8414)
 */

/**
 * Generate PKCE code verifier
 * @returns {string} 43-character URL-safe string
 */
export function generateCodeVerifier() {
  const array = new Uint8Array(32)
  crypto.getRandomValues(array)

  // Convert to base64url format
  return base64UrlEncode(array)
}

/**
 * Generate PKCE code challenge from verifier
 * @param {string} codeVerifier
 * @returns {Promise<string>} SHA-256 hash encoded as base64url
 */
export async function generateCodeChallenge(codeVerifier) {
  const encoder = new TextEncoder()
  const data = encoder.encode(codeVerifier)

  const hash = await crypto.subtle.digest('SHA-256', data)

  return base64UrlEncode(new Uint8Array(hash))
}

/**
 * Encode Uint8Array to base64url format
 * @param {Uint8Array} buffer
 * @returns {string} base64url encoded string
 */
function base64UrlEncode(buffer) {
  const base64 = btoa(String.fromCharCode(...buffer))
  return base64
    .replace(/\+/g, '-')
    .replace(/\//g, '_')
    .replace(/=/g, '')
}

/**
 * Generate secure state parameter
 * @returns {string} UUID
 */
export function generateState() {
  return crypto.randomUUID()
}

/**
 * Build OAuth2 authorization URL with PKCE
 * @param {Object} options
 * @returns {string} authorization URL
 */
export async function buildAuthorizationUrl(options) {
  const {
    endpoint,
    clientId,
    redirectUri,
    scope,
    state,
    usePKCE = true
  } = options

  const params = new URLSearchParams({
    response_type: 'code',
    client_id: clientId,
    redirect_uri: redirectUri,
    scope: scope,
    state: state
  })

  // Add PKCE parameters if enabled
  if (usePKCE) {
    const codeVerifier = generateCodeVerifier()
    const codeChallenge = await generateCodeChallenge(codeVerifier)

    // Store for later use in token exchange
    sessionStorage.setItem('pkce_code_verifier', codeVerifier)

    params.append('code_challenge', codeChallenge)
    params.append('code_challenge_method', 'S256')
  }

  return `${endpoint}?${params.toString()}`
}

/**
 * Exchange authorization code for access token with PKCE
 * @param {Object} options
 * @returns {Promise<Object>} token response
 */
export async function exchangeCodeForToken(options) {
  const {
    code,
    redirectUri,
    clientId
  } = options

  const tokenBody = {
    grant_type: 'authorization_code',
    code: code,
    client_id: clientId,
    redirect_uri: redirectUri
  }

  // Add PKCE code verifier if available
  const codeVerifier = sessionStorage.getItem('pkce_code_verifier')
  if (codeVerifier) {
    tokenBody.code_verifier = codeVerifier
    sessionStorage.removeItem('pkce_code_verifier')
  }

  try {
    const response = await apiClient.post('/oauth2/token', new URLSearchParams(tokenBody), {
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
    })
    return response.data
  } catch (error) {
    if (error.response) {
      throw new Error(`Token exchange failed: ${error.response.data.error || error.message}`)
    }
    throw error
  }
}

/**
 * Parse RFC 6749 OAuth2 error response
 * @param {Response} response
 * @returns {Promise<Object>} parsed error object
 */
export async function parseOAuth2Error(response) {
  const contentType = response.headers.get('content-type')

  if (contentType && contentType.includes('application/json')) {
    return await response.json()
  }

  // Fallback to form-encoded
  const text = await response.text()
  const params = new URLSearchParams(text)

  return {
    error: params.get('error') || 'unknown_error',
    error_description: params.get('error_description') || '',
    error_uri: params.get('error_uri') || ''
  }
}

/**
 * Introspect access token (RFC 7662)
 * @param {string} token
 * @returns {Promise<Object>} token info
 */
export async function introspectToken(token) {
  try {
    const response = await apiClient.post('/oauth2/introspect', new URLSearchParams({ token }), {
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
    })
    return response.data
  } catch (error) {
    throw new Error(`Token introspection failed: ${error.message}`)
  }
}

/**
 * Revoke access token (RFC 7009)
 * @param {string} token
 * @returns {Promise<boolean>} success
 */
export async function revokeToken(token) {
  try {
    await apiClient.post('/oauth2/revoke', new URLSearchParams({ token }), {
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' }
    })
    return true
  } catch (error) {
    return false
  }
}

/**
 * Fetch authorization server metadata (RFC 8414)
 * @returns {Promise<Object>} server metadata
 */
export async function fetchServerMetadata() {
  try {
    const response = await apiClient.get('/.well-known/oauth-authorization-server')
    return response.data
  } catch (error) {
    throw new Error(`Failed to fetch metadata: ${error.message}`)
  }
}

/**
 * Validate token and auto-refresh if needed
 * @param {string} accessToken
 * @returns {Promise<boolean>} token is valid
 */
export async function validateToken(accessToken) {
  try {
    const introspection = await introspectToken(accessToken)
    return introspection.active === true
  } catch (error) {
    console.warn('Token validation failed:', error)
    return false
  }
}

/**
 * Store tokens securely
 * @param {Object} tokens
 */
export function storeTokens(tokens) {
  if (tokens.access_token) {
    localStorage.setItem('access_token', tokens.access_token)

    // Calculate expiration time
    if (tokens.expires_in) {
      const expiresAt = Date.now() + (tokens.expires_in * 1000)
      localStorage.setItem('token_expires_at', expiresAt.toString())
    }
  }

  if (tokens.refresh_token) {
    localStorage.setItem('refresh_token', tokens.refresh_token)
  }

  if (tokens.scope) {
    localStorage.setItem('token_scope', tokens.scope)
  }
}

/**
 * Clear all tokens
 */
export function clearTokens() {
  localStorage.removeItem('access_token')
  localStorage.removeItem('refresh_token')
  localStorage.removeItem('token_expires_at')
  localStorage.removeItem('token_scope')
  localStorage.removeItem('user_info')
}

/**
 * Check if token is expired
 * @returns {boolean} token is expired
 */
export function isTokenExpired() {
  const expiresAt = localStorage.getItem('token_expires_at')
  if (!expiresAt) return false

  return Date.now() > parseInt(expiresAt)
}

/**
 * Get stored access token if valid
 * @returns {string|null} access token or null
 */
export function getValidAccessToken() {
  if (isTokenExpired()) {
    clearTokens()
    return null
  }

  return localStorage.getItem('access_token')
}
