<script setup lang="ts">
import { ref, onMounted } from 'vue'
import axios from 'axios'

const health = ref<any>(null)
const loading = ref(true)

onMounted(async () => {
  try {
    const resp = await axios.get('/health/ready')
    health.value = resp.data
  } catch (e) {
    health.value = { status: 'error', message: 'Failed to fetch health status' }
  } finally {
    loading.value = false
  }
})
</script>

<template>
  <div>
    <h2 class="text-2xl font-bold text-gray-900 mb-6">Dashboard</h2>

    <!-- Health Status -->
    <div class="grid grid-cols-1 md:grid-cols-3 gap-6 mb-8">
      <div class="bg-white rounded-lg shadow p-6">
        <div class="flex items-center">
          <div class="w-3 h-3 rounded-full mr-3" :class="health?.status === 'ok' ? 'bg-green-500' : 'bg-red-500'"></div>
          <h3 class="text-sm font-medium text-gray-500">System Status</h3>
        </div>
        <p class="mt-2 text-2xl font-semibold" :class="health?.status === 'ok' ? 'text-green-600' : 'text-red-600'">
          {{ loading ? '...' : (health?.status === 'ok' ? 'Healthy' : 'Unhealthy') }}
        </p>
      </div>

      <div class="bg-white rounded-lg shadow p-6">
        <h3 class="text-sm font-medium text-gray-500">Database</h3>
        <p class="mt-2 text-2xl font-semibold text-gray-900">
          {{ loading ? '...' : (health?.database || 'N/A') }}
        </p>
      </div>

      <div class="bg-white rounded-lg shadow p-6">
        <h3 class="text-sm font-medium text-gray-500">Redis</h3>
        <p class="mt-2 text-2xl font-semibold text-gray-900">
          {{ loading ? '...' : (health?.redis || 'N/A') }}
        </p>
      </div>
    </div>

    <!-- Quick Links -->
    <div class="bg-white rounded-lg shadow p-6">
      <h3 class="text-lg font-medium text-gray-900 mb-4">Quick Actions</h3>
      <div class="grid grid-cols-2 md:grid-cols-4 gap-4">
        <router-link to="/applications" class="p-4 border rounded-lg hover:border-indigo-500 hover:bg-indigo-50 transition-colors text-center">
          <span class="text-2xl">📱</span>
          <p class="mt-2 text-sm font-medium text-gray-700">Applications</p>
        </router-link>
        <router-link to="/users" class="p-4 border rounded-lg hover:border-indigo-500 hover:bg-indigo-50 transition-colors text-center">
          <span class="text-2xl">👥</span>
          <p class="mt-2 text-sm font-medium text-gray-700">Users</p>
        </router-link>
        <router-link to="/logs" class="p-4 border rounded-lg hover:border-indigo-500 hover:bg-indigo-50 transition-colors text-center">
          <span class="text-2xl">📋</span>
          <p class="mt-2 text-sm font-medium text-gray-700">Audit Logs</p>
        </router-link>
        <router-link to="/settings" class="p-4 border rounded-lg hover:border-indigo-500 hover:bg-indigo-50 transition-colors text-center">
          <span class="text-2xl">⚙️</span>
          <p class="mt-2 text-sm font-medium text-gray-700">Settings</p>
        </router-link>
      </div>
    </div>
  </div>
</template>
