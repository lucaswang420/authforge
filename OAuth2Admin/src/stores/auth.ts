import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import axios from 'axios'

export const useAuthStore = defineStore('auth', () => {
  const accessToken = ref<string | null>(null)
  const refreshToken = ref<string | null>(null)
  const user = ref<any>(null)
  const loginError = ref('')

  const isAuthenticated = computed(() => !!accessToken.value)

  /**
   * Direct login: POST credentials to /oauth2/login (json mode)
   * Then exchange the code for tokens.
   * This avoids the redirect-based flow for the admin SPA.
   */
  async function login(username: string, password: string) {
    loginError.value = ''
    try {
      // Step 1: Login and get auth code
      const loginResp = await axios.post('/oauth2/login', new URLSearchParams({
        username,
        password,
        client_id: 'admin-console',
        redirect_uri: window.location.origin + '/admin/callback',
        scope: 'openid profile admin',
        state: crypto.randomUUID(),
        json: 'true',
      }), {
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      })

      if (loginResp.data.mfa_required) {
        // MFA required - return the mfa_token for the MFA step
        return { mfaRequired: true, mfaToken: loginResp.data.mfa_token }
      }

      const code = loginResp.data.code
      if (!code) {
        throw new Error('No authorization code received')
      }

      // Step 2: Exchange code for tokens
      const tokenResp = await axios.post('/oauth2/token', new URLSearchParams({
        grant_type: 'authorization_code',
        code,
        redirect_uri: window.location.origin + '/admin/callback',
        client_id: 'admin-console',
        client_secret: '',
      }), {
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      })

      accessToken.value = tokenResp.data.access_token
      refreshToken.value = tokenResp.data.refresh_token

      // Step 3: Fetch user info
      await fetchUserInfo()

      // Verify admin role
      if (!user.value?.roles?.includes('admin')) {
        logout()
        loginError.value = 'Admin role required to access this console'
        return { error: 'forbidden' }
      }

      return { success: true }
    } catch (e: any) {
      const msg = e.response?.data?.error_description || e.response?.data || e.message || 'Login failed'
      loginError.value = typeof msg === 'string' ? msg : 'Login failed'
      return { error: loginError.value }
    }
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

  async function refreshAccessToken() {
    if (!refreshToken.value) return false
    try {
      const resp = await axios.post('/oauth2/token', new URLSearchParams({
        grant_type: 'refresh_token',
        refresh_token: refreshToken.value,
        client_id: 'admin-console',
      }), {
        headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
      })
      accessToken.value = resp.data.access_token
      refreshToken.value = resp.data.refresh_token
      return true
    } catch {
      logout()
      return false
    }
  }

  function logout() {
    accessToken.value = null
    refreshToken.value = null
    user.value = null
  }

  // Axios interceptor for auto-attaching token
  axios.interceptors.request.use((config) => {
    if (accessToken.value && !config.headers.Authorization) {
      config.headers.Authorization = `Bearer ${accessToken.value}`
    }
    return config
  })

  // Axios interceptor for 401 → auto refresh
  axios.interceptors.response.use(
    (response) => response,
    async (error) => {
      if (error.response?.status === 401 && refreshToken.value && !error.config._retry) {
        error.config._retry = true
        const refreshed = await refreshAccessToken()
        if (refreshed) {
          error.config.headers.Authorization = `Bearer ${accessToken.value}`
          return axios(error.config)
        }
      }
      return Promise.reject(error)
    }
  )

  return {
    accessToken,
    user,
    loginError,
    isAuthenticated,
    login,
    fetchUserInfo,
    refreshAccessToken,
    logout,
  }
})
