import { defineStore } from 'pinia'
import { ref, computed } from 'vue'
import { authService } from '../services/authService'
import { userService } from '../services/userService'
import { getAccessToken, setTokens as httpSetTokens, clearTokens as httpClearTokens } from '../services/http'
import type { User, LoginResult } from '../types'

export const useAuthStore = defineStore('auth', () => {
  const user = ref<User | null>(null)
  const loading = ref(false)
  const error = ref('')
  const tokenPresent = ref(!!getAccessToken())

  const isAuthenticated = computed(() => tokenPresent.value)

  function markAuthenticated() { tokenPresent.value = true }
  function markUnauthenticated() { tokenPresent.value = false }

  async function login(username: string, password: string): Promise<LoginResult> {
    error.value = ''
    loading.value = true
    try {
      const result = await authService.login(username, password)
      if (result.success) {
        markAuthenticated()
        await fetchUser()
      }
      return result
    } catch (e: any) {
      error.value = e.response?.data?.error_description || e.response?.data?.error || e.message || 'Login failed'
      return { error: error.value }
    } finally {
      loading.value = false
    }
  }

  async function verifyMfa(mfaToken: string, code: string): Promise<LoginResult> {
    error.value = ''
    loading.value = true
    try {
      const result = await authService.verifyMfa(mfaToken, code)
      if (result.success) {
        markAuthenticated()
        await fetchUser()
      }
      return result
    } catch (e: any) {
      error.value = e.response?.data?.error_description || 'Verification failed'
      return { error: error.value }
    } finally {
      loading.value = false
    }
  }

  async function exchangeCode(code: string) {
    await authService.exchangeCode(code)
    markAuthenticated()
    await fetchUser()
  }

  async function fetchUser() {
    try {
      user.value = await userService.getUserInfo()
    } catch {
      user.value = null
    }
  }

  async function logout() {
    await authService.logout()
    user.value = null
    httpClearTokens()
    markUnauthenticated()
  }

  // Initialize
  if (getAccessToken()) {
    fetchUser()
  }

  return { user, loading, error, isAuthenticated, login, verifyMfa, exchangeCode, fetchUser, logout }
})
