<script setup lang="ts">
import { ref, onMounted } from 'vue'
import axios from 'axios'

const clients = ref<any[]>([])
const loading = ref(true)

onMounted(async () => {
  try {
    const resp = await axios.get('/api/admin/clients')
    clients.value = resp.data.clients || []
  } catch (e) {
    console.error('Failed to fetch clients:', e)
  } finally {
    loading.value = false
  }
})
</script>

<template>
  <div>
    <div class="flex justify-between items-center mb-6">
      <h2 class="text-2xl font-bold text-gray-900">Applications</h2>
      <button class="px-4 py-2 bg-indigo-600 text-white rounded-md hover:bg-indigo-700 text-sm font-medium">
        + Create Application
      </button>
    </div>

    <div v-if="loading" class="text-center py-12 text-gray-500">Loading...</div>

    <div v-else-if="clients.length === 0" class="text-center py-12">
      <p class="text-gray-500">No applications registered yet</p>
    </div>

    <div v-else class="bg-white shadow rounded-lg overflow-hidden">
      <table class="min-w-full divide-y divide-gray-200">
        <thead class="bg-gray-50">
          <tr>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Name</th>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Client ID</th>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Type</th>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Grant Types</th>
          </tr>
        </thead>
        <tbody class="bg-white divide-y divide-gray-200">
          <tr v-for="client in clients" :key="client.client_id" class="hover:bg-gray-50">
            <td class="px-6 py-4 text-sm font-medium text-gray-900">{{ client.name || client.client_id }}</td>
            <td class="px-6 py-4 text-sm text-gray-500 font-mono">{{ client.client_id }}</td>
            <td class="px-6 py-4">
              <span class="px-2 py-1 text-xs rounded-full" :class="client.client_type === 'PUBLIC' ? 'bg-blue-100 text-blue-800' : 'bg-purple-100 text-purple-800'">
                {{ client.client_type }}
              </span>
            </td>
            <td class="px-6 py-4 text-sm text-gray-500">{{ client.allowed_grant_types }}</td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>
</template>
