import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import axios from 'axios'

export const useAuthStore = defineStore('auth', () => {
  const accessToken = ref<string | null>(null)
  const user = ref<any>(null)

  const isAuthenticated = computed(() => !!accessToken.value)

  // PKCE helpers
  function generateCodeVerifier(): string {
    const array = new Uint8Array(32)
    crypto.getRandomValues(array)
    return btoa(String.fromCharCode(...array))
      .replace(/\+/g, '-').replace(/\//g, '_').replace(/=/g, '')
  }

  async function generateCodeChallenge(verifier: string): Promise<string> {
    const encoder = new TextEncoder()
    const data = encoder.encode(verifier)
    const hash = await crypto.subtle.digest('SHA-256', data)
    return btoa(String.fromCharCode(...new Uint8Array(hash)))
      .replace(/\+/g, '-').replace(/\//g, '_').replace(/=/g, '')
  }

  async function startLogin() {
    const verifier = generateCodeVerifier()
    const challenge = await generateCodeChallenge(verifier)
    const state = crypto.randomUUID()

    // Store for callback
    sessionStorage.setItem('pkce_verifier', verifier)
    sessionStorage.setItem('oauth_state', state)

    const params = new URLSearchParams({
      response_type: 'code',
      client_id: 'admin-console',
      redirect_uri: window.location.origin + '/admin/callback',
      scope: 'openid profile admin',
      state,
      code_challenge: challenge,
      code_challenge_method: 'S256',
    })

    // Redirect to login page (use /oauth2/login with json=false for redirect)
    window.location.href = `/oauth2/authorize?${params.toString()}`
  }

  async function handleCallback(code: string, state: string) {
    const savedState = sessionStorage.getItem('oauth_state')
    if (state !== savedState) {
      throw new Error('State mismatch - possible CSRF attack')
    }

    const verifier = sessionStorage.getItem('pkce_verifier')
    sessionStorage.removeItem('pkce_verifier')
    sessionStorage.removeItem('oauth_state')

    // Exchange code for token
    const response = await axios.post('/oauth2/token', new URLSearchParams({
      grant_type: 'authorization_code',
      code,
      redirect_uri: window.location.origin + '/admin/callback',
      client_id: 'admin-console',
      code_verifier: verifier || '',
    }), {
      headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
    })

    accessToken.value = response.data.access_token

    // Fetch user info
    await fetchUserInfo()
  }

  async function fetchUserInfo() {
    if (!accessToken.value) return
    try {
      const response = await axios.get('/oauth2/userinfo', {
        headers: { Authorization: `Bearer ${accessToken.value}` },
      })
      user.value = response.data
    } catch {
      logout()
    }
  }

  function logout() {
    accessToken.value = null
    user.value = null
  }

  // Axios interceptor for auto-attaching token
  axios.interceptors.request.use((config) => {
    if (accessToken.value && !config.headers.Authorization) {
      config.headers.Authorization = `Bearer ${accessToken.value}`
    }
    return config
  })

  return {
    accessToken,
    user,
    isAuthenticated,
    startLogin,
    handleCallback,
    fetchUserInfo,
    logout,
  }
})
