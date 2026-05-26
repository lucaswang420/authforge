<script setup lang="ts">
import { onMounted, ref } from 'vue'
import { useRouter, useRoute } from 'vue-router'
import axios from 'axios'

const router = useRouter()
const route = useRoute()
const error = ref('')

onMounted(async () => {
  const code = route.query.code as string
  if (!code) {
    error.value = 'No authorization code from GitHub'
    return
  }

  try {
    const resp = await axios.post('/api/github/login', { code }, {
      headers: { 'Content-Type': 'application/json' },
    })
    // GitHub login returns user info — for now show it
    // In a full implementation, the backend would create/link a local account
    // and return an OAuth2 token
    if (resp.data.login) {
      // TODO: Backend should return access_token after account linking
      error.value = `GitHub user "${resp.data.login}" authenticated. Account linking not yet implemented.`
    }
  } catch (e: any) {
    error.value = e.response?.data?.error || 'GitHub login failed'
  }
})
</script>

<template>
  <div class="min-h-screen flex items-center justify-center">
    <div class="text-center max-w-md">
      <div v-if="error" class="p-6 bg-yellow-50 border border-yellow-200 rounded-lg">
        <p class="text-yellow-800 font-medium">GitHub Login</p>
        <p class="text-yellow-700 text-sm mt-2">{{ error }}</p>
        <router-link to="/login" class="mt-4 inline-block text-indigo-600 hover:text-indigo-800">Back to Login</router-link>
      </div>
      <div v-else>
        <div class="animate-spin w-8 h-8 border-4 border-indigo-600 border-t-transparent rounded-full mx-auto"></div>
        <p class="mt-4 text-gray-600">Signing in with GitHub...</p>
      </div>
    </div>
  </div>
</template>
