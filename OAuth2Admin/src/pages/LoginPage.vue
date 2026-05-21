<script setup lang="ts">
import { useAuthStore } from '../stores/auth'

const auth = useAuthStore()
</script>

<template>
  <div class="min-h-screen flex items-center justify-center bg-gray-900">
    <div class="max-w-md w-full bg-white rounded-lg shadow-xl p-8">
      <div class="text-center mb-8">
        <h1 class="text-2xl font-bold text-gray-900">OAuth2 Admin Console</h1>
        <p class="mt-2 text-sm text-gray-600">Sign in with your administrator account</p>
      </div>
      <button
        @click="auth.startLogin()"
        class="w-full flex justify-center py-3 px-4 border border-transparent rounded-md shadow-sm text-sm font-medium text-white bg-indigo-600 hover:bg-indigo-700 focus:outline-none focus:ring-2 focus:ring-offset-2 focus:ring-indigo-500 transition-colors"
      >
        Sign in with OAuth2
      </button>
      <p class="mt-4 text-center text-xs text-gray-500">
        Requires admin role to access the management console
      </p>
    </div>
  </div>
</template>
