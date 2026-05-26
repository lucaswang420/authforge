<script setup lang="ts">
import { ref } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import { useAuthStore } from '../../stores/auth'
import AppInput from '../../components/ui/AppInput.vue'
import AppButton from '../../components/ui/AppButton.vue'
import AppAlert from '../../components/ui/AppAlert.vue'

const auth = useAuthStore()
const router = useRouter()
const route = useRoute()

const username = ref('')
const password = ref('')
const mfaCode = ref('')
const mfaToken = ref('')
const showMfa = ref(false)

async function handleLogin() {
  const result = await auth.login(username.value, password.value)
  if (result.mfaRequired) {
    mfaToken.value = result.mfaToken!
    showMfa.value = true
  } else if (result.success) {
    router.push((route.query.redirect as string) || '/')
  }
}

async function handleMfa() {
  const result = await auth.verifyMfa(mfaToken.value, mfaCode.value)
  if (result.success) {
    router.push((route.query.redirect as string) || '/')
  }
}
</script>

<template>
  <div>
    <div class="mb-8">
      <h1 class="text-2xl font-bold text-gray-900">Sign in to your account</h1>
      <p class="mt-2 text-sm text-gray-500">
        Or <router-link to="/register" class="text-indigo-600 font-medium hover:text-indigo-500">create a new account</router-link>
      </p>
    </div>

    <AppAlert v-if="auth.error" type="error" class="mb-6">{{ auth.error }}</AppAlert>

    <!-- MFA Challenge -->
    <form v-if="showMfa" @submit.prevent="handleMfa" class="space-y-6">
      <div class="text-center py-4">
        <div class="w-14 h-14 bg-indigo-100 rounded-full flex items-center justify-center mx-auto mb-4">
          <span class="text-2xl">🔐</span>
        </div>
        <h2 class="text-lg font-semibold text-gray-900">Two-Factor Authentication</h2>
        <p class="text-sm text-gray-500 mt-1">Enter the 6-digit code from your authenticator app</p>
      </div>
      <div>
        <input v-model="mfaCode" type="text" inputmode="numeric" maxlength="6" autocomplete="one-time-code"
          class="block w-full px-4 py-4 text-center text-2xl tracking-[0.5em] font-mono border border-gray-300 rounded-lg focus:ring-2 focus:ring-indigo-500/20 focus:border-indigo-500"
          placeholder="000000" />
      </div>
      <AppButton type="submit" :loading="auth.loading" :disabled="mfaCode.length !== 6" class="w-full">
        Verify Code
      </AppButton>
      <button type="button" @click="showMfa = false; mfaCode = ''" class="w-full text-sm text-gray-500 hover:text-gray-700">
        ← Back to login
      </button>
    </form>

    <!-- Login Form -->
    <form v-else @submit.prevent="handleLogin" class="space-y-5">
      <AppInput v-model="username" label="Username" placeholder="Enter your username" required autocomplete="username" />
      <AppInput v-model="password" label="Password" type="password" placeholder="Enter your password" required autocomplete="current-password" />

      <div class="flex justify-end">
        <router-link to="/forgot-password" class="text-sm text-indigo-600 hover:text-indigo-500 font-medium">
          Forgot password?
        </router-link>
      </div>

      <AppButton type="submit" :loading="auth.loading" class="w-full">
        Sign In
      </AppButton>
    </form>
  </div>
</template>
