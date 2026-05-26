import axios from 'axios'

const http = axios.create({
  baseURL: import.meta.env.VITE_API_BASE_URL || '',
  timeout: 15000,
  headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
})

// Token management
let accessToken: string | null = localStorage.getItem('access_token')
let refreshToken: string | null = localStorage.getItem('refresh_token')

export function setTokens(access: string, refresh: string) {
  accessToken = access
  refreshToken = refresh
  localStorage.setItem('access_token', access)
  localStorage.setItem('refresh_token', refresh)
}

export function clearTokens() {
  accessToken = null
  refreshToken = null
  localStorage.removeItem('access_token')
  localStorage.removeItem('refresh_token')
}

export function getAccessToken() { return accessToken }
export function getRefreshToken() { return refreshToken }

// Request interceptor: attach Bearer token
http.interceptors.request.use((config) => {
  if (accessToken && !config.headers.Authorization) {
    config.headers.Authorization = `Bearer ${accessToken}`
  }
  return config
})

// Response interceptor: auto-refresh on 401
http.interceptors.response.use(
  (response) => response,
  async (error) => {
    const originalRequest = error.config
    if (error.response?.status === 401 && refreshToken && !originalRequest._retry) {
      originalRequest._retry = true
      try {
        const CLIENT_ID = import.meta.env.VITE_CLIENT_ID || 'vue-client'
        const CLIENT_SECRET = import.meta.env.VITE_CLIENT_SECRET || '123456'
        const resp = await axios.post('/oauth2/token', new URLSearchParams({
          grant_type: 'refresh_token',
          refresh_token: refreshToken,
          client_id: CLIENT_ID,
          client_secret: CLIENT_SECRET,
        }))
        setTokens(resp.data.access_token, resp.data.refresh_token)
        originalRequest.headers.Authorization = `Bearer ${resp.data.access_token}`
        return http(originalRequest)
      } catch {
        clearTokens()
        window.location.href = '/login'
      }
    }
    return Promise.reject(error)
  }
)

export default http
