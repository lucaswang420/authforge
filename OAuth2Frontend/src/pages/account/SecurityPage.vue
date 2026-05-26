<script setup lang="ts">
import { ref, onMounted } from 'vue'
import axios from 'axios'

const loading = ref(true)
const profile = ref<any>(null)
const success = ref('')
const error = ref('')

// Password change
const oldPassword = ref('')
const newPassword = ref('')
const confirmNewPassword = ref('')
const changingPassword = ref(false)

// MFA
const mfaSetupData = ref<any>(null)
const mfaVerifyCode = ref('')
const settingUpMfa = ref(false)
const disablingMfa = ref(false)
const disablePassword = ref('')

async function fetchProfile() {
  try {
    const resp = await axios.get('/api/me')
    profile.value = resp.data
  } catch {} finally { loading.value = false }
}

function showSuccess(msg: string) { success.value = msg; error.value = ''; setTimeout(() => { success.value = '' }, 4000) }
function showError(msg: string) { error.value = msg; success.value = '' }

async function changePassword() {
  if (newPassword.value !== confirmNewPassword.value) { showError('Passwords do not match'); return }
  if (newPassword.value.length < 6) { showError('Password must be at least 6 characters'); return }
  changingPassword.value = true
  try {
    await axios.put('/api/me/password', { old_password: oldPassword.value, new_password: newPassword.value }, { headers: { 'Content-Type': 'application/json' } })
    showSuccess('Password changed successfully')
    oldPassword.value = ''; newPassword.value = ''; confirmNewPassword.value = ''
  } catch (e: any) {
    showError(e.response?.data?.message || 'Failed to change password')
  } finally { changingPassword.value = false }
}

async function setupMfa() {
  settingUpMfa.value = true
  try {
    const resp = await axios.post('/api/me/mfa/setup')
    mfaSetupData.value = resp.data
  } catch (e: any) {
    showError(e.response?.data?.message || 'Failed to start MFA setup')
    settingUpMfa.value = false
  }
}

async function verifyMfaSetup() {
  try {
    await axios.post('/api/me/mfa/verify', new URLSearchParams({ code: mfaVerifyCode.value }))
    showSuccess('MFA enabled successfully!')
    mfaSetupData.value = null
    settingUpMfa.value = false
    mfaVerifyCode.value = ''
    await fetchProfile()
  } catch (e: any) {
    showError(e.response?.data?.message || 'Invalid code')
  }
}

async function disableMfa() {
  if (!disablePassword.value) { showError('Password required to disable MFA'); return }
  disablingMfa.value = true
  try {
    await axios.post('/api/me/mfa/disable', new URLSearchParams({ password: disablePassword.value }))
    showSuccess('MFA disabled')
    disablePassword.value = ''
    await fetchProfile()
  } catch (e: any) {
    showError(e.response?.data?.message || 'Failed to disable MFA')
  } finally { disablingMfa.value = false }
}

onMounted(fetchProfile)
</script>

<template>
  <div>
    <h1 class="text-2xl font-bold text-gray-900 mb-6">Security Settings</h1>

    <div v-if="success" class="mb-4 p-3 bg-green-50 border border-green-200 text-green-700 rounded-lg text-sm">{{ success }}</div>
    <div v-if="error" class="mb-4 p-3 bg-red-50 border border-red-200 text-red-700 rounded-lg text-sm">{{ error }}</div>

    <div v-if="loading" class="text-center py-12 text-gray-500">Loading...</div>

    <div v-else class="space-y-6">
      <!-- Change Password -->
      <div class="bg-white rounded-xl shadow-sm border border-gray-200 p-6">
        <h2 class="text-lg font-semibold text-gray-900 mb-4">Change Password</h2>
        <form @submit.prevent="changePassword" class="space-y-4 max-w-md">
          <div>
            <label class="block text-sm font-medium text-gray-700 mb-1">Current Password</label>
            <input v-model="oldPassword" type="password" required autocomplete="current-password"
              class="block w-full px-3 py-2 border border-gray-300 rounded-lg text-sm focus:ring-2 focus:ring-indigo-500" />
          </div>
          <div>
            <label class="block text-sm font-medium text-gray-700 mb-1">New Password</label>
            <input v-model="newPassword" type="password" required autocomplete="new-password"
              class="block w-full px-3 py-2 border border-gray-300 rounded-lg text-sm focus:ring-2 focus:ring-indigo-500" />
          </div>
          <div>
            <label class="block text-sm font-medium text-gray-700 mb-1">Confirm New Password</label>
            <input v-model="confirmNewPassword" type="password" required autocomplete="new-password"
              class="block w-full px-3 py-2 border border-gray-300 rounded-lg text-sm focus:ring-2 focus:ring-indigo-500" />
          </div>
          <button type="submit" :disabled="changingPassword"
            class="px-4 py-2 bg-indigo-600 text-white rounded-lg text-sm hover:bg-indigo-700 disabled:opacity-50">
            {{ changingPassword ? 'Changing...' : 'Change Password' }}
          </button>
        </form>
      </div>

      <!-- MFA -->
      <div class="bg-white rounded-xl shadow-sm border border-gray-200 p-6">
        <h2 class="text-lg font-semibold text-gray-900 mb-4">Two-Factor Authentication (MFA)</h2>

        <div v-if="profile?.mfa_enabled" class="space-y-4">
          <div class="flex items-center gap-2">
            <span class="px-2 py-1 text-xs bg-green-100 text-green-700 rounded-full font-medium">Enabled</span>
            <p class="text-sm text-gray-600">Your account is protected with TOTP-based MFA.</p>
          </div>
          <div class="border-t pt-4">
            <p class="text-sm text-gray-600 mb-2">Enter your password to disable MFA:</p>
            <div class="flex gap-2 max-w-md">
              <input v-model="disablePassword" type="password" placeholder="Your password"
                class="flex-1 px-3 py-2 border border-gray-300 rounded-lg text-sm" />
              <button @click="disableMfa" :disabled="disablingMfa"
                class="px-4 py-2 bg-red-600 text-white rounded-lg text-sm hover:bg-red-700 disabled:opacity-50">
                {{ disablingMfa ? 'Disabling...' : 'Disable MFA' }}
              </button>
            </div>
          </div>
        </div>

        <div v-else-if="mfaSetupData" class="space-y-4">
          <p class="text-sm text-gray-600">Scan this QR code with your authenticator app (Google Authenticator, Authy, etc.):</p>
          <div class="bg-gray-50 p-4 rounded-lg text-center">
            <p class="text-xs text-gray-500 mb-2">Manual entry key:</p>
            <code class="text-sm font-mono bg-white px-3 py-1 rounded border select-all">{{ mfaSetupData.secret }}</code>
          </div>
          <div class="max-w-xs">
            <label class="block text-sm font-medium text-gray-700 mb-1">Verification Code</label>
            <input v-model="mfaVerifyCode" type="text" inputmode="numeric" maxlength="6"
              class="block w-full px-3 py-2 border border-gray-300 rounded-lg text-sm text-center tracking-widest" placeholder="000000" />
          </div>
          <div class="flex gap-2">
            <button @click="verifyMfaSetup" :disabled="mfaVerifyCode.length !== 6"
              class="px-4 py-2 bg-indigo-600 text-white rounded-lg text-sm hover:bg-indigo-700 disabled:opacity-50">Verify & Enable</button>
            <button @click="mfaSetupData = null; settingUpMfa = false"
              class="px-4 py-2 border border-gray-300 rounded-lg text-sm hover:bg-gray-50">Cancel</button>
          </div>
        </div>

        <div v-else>
          <p class="text-sm text-gray-600 mb-4">Add an extra layer of security to your account with time-based one-time passwords (TOTP).</p>
          <button @click="setupMfa" :disabled="settingUpMfa"
            class="px-4 py-2 bg-indigo-600 text-white rounded-lg text-sm hover:bg-indigo-700 disabled:opacity-50">
            {{ settingUpMfa ? 'Setting up...' : 'Enable MFA' }}
          </button>
        </div>
      </div>
    </div>
  </div>
</template>
