<script setup lang="ts">
import { ref, onMounted } from 'vue'
import axios from 'axios'

const scopes = ref<any[]>([])
const loading = ref(true)

async function fetchScopes() {
  loading.value = true
  try {
    const resp = await axios.get('/api/admin/scopes')
    scopes.value = resp.data.scopes || []
  } catch (e) {
    console.error('Failed to fetch scopes:', e)
  } finally {
    loading.value = false
  }
}

onMounted(fetchScopes)
</script>

<template>
  <div>
    <h2 class="text-2xl font-bold text-gray-900 mb-6">Settings & Scopes</h2>

    <!-- Scopes Section -->
    <div class="bg-white shadow rounded-lg overflow-hidden">
      <div class="px-6 py-4 border-b">
        <h3 class="text-lg font-medium text-gray-900">OAuth2 Scopes</h3>
      </div>

      <div v-if="loading" class="p-6 text-center text-gray-500">Loading...</div>

      <table v-else class="min-w-full divide-y divide-gray-200">
        <thead class="bg-gray-50">
          <tr>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Name</th>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Description</th>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Mapped Role</th>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Default</th>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Admin Only</th>
          </tr>
        </thead>
        <tbody class="bg-white divide-y divide-gray-200">
          <tr v-for="scope in scopes" :key="scope.id" class="hover:bg-gray-50">
            <td class="px-6 py-4 text-sm font-medium text-gray-900 font-mono">{{ scope.name }}</td>
            <td class="px-6 py-4 text-sm text-gray-500">{{ scope.description || '—' }}</td>
            <td class="px-6 py-4 text-sm text-gray-500">{{ scope.mapped_role || '—' }}</td>
            <td class="px-6 py-4">
              <span v-if="scope.is_default" class="text-green-600">✓</span>
              <span v-else class="text-gray-300">—</span>
            </td>
            <td class="px-6 py-4">
              <span v-if="scope.requires_admin_role" class="text-red-600">✓</span>
              <span v-else class="text-gray-300">—</span>
            </td>
          </tr>
        </tbody>
      </table>
    </div>
  </div>
</template>
