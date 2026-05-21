<script setup lang="ts">
import { ref, onMounted } from 'vue'
import axios from 'axios'

const users = ref<any[]>([])
const loading = ref(true)
const showRoleModal = ref(false)
const selectedUser = ref<any>(null)
const roleInput = ref('')
const saving = ref(false)

async function fetchUsers() {
  loading.value = true
  try {
    const resp = await axios.get('/api/admin/users')
    users.value = resp.data.users || []
  } catch (e) {
    console.error('Failed to fetch users:', e)
  } finally {
    loading.value = false
  }
}

function openRoleModal(user: any) {
  selectedUser.value = user
  roleInput.value = ''  // Will be populated when we have role info
  showRoleModal.value = true
}

async function assignRoles() {
  if (!selectedUser.value || !roleInput.value.trim()) return
  saving.value = true
  try {
    const roles = roleInput.value.split(',').map((r: string) => r.trim()).filter(Boolean)
    await axios.put(`/api/admin/users/${selectedUser.value.id}/roles`, { roles }, {
      headers: { 'Content-Type': 'application/json' },
    })
    showRoleModal.value = false
    await fetchUsers()
  } catch (e: any) {
    alert(e.response?.data?.message || 'Failed to assign roles')
  } finally {
    saving.value = false
  }
}

onMounted(fetchUsers)
</script>

<template>
  <div>
    <h2 class="text-2xl font-bold text-gray-900 mb-6">Users</h2>

    <div v-if="loading" class="text-center py-12 text-gray-500">Loading...</div>

    <div v-else class="bg-white shadow rounded-lg overflow-hidden">
      <table class="min-w-full divide-y divide-gray-200">
        <thead class="bg-gray-50">
          <tr>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Username</th>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Email</th>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Verified</th>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">MFA</th>
            <th class="px-6 py-3 text-left text-xs font-medium text-gray-500 uppercase">Actions</th>
          </tr>
        </thead>
        <tbody class="bg-white divide-y divide-gray-200">
          <tr v-for="user in users" :key="user.id" class="hover:bg-gray-50">
            <td class="px-6 py-4 text-sm font-medium text-gray-900">{{ user.username }}</td>
            <td class="px-6 py-4 text-sm text-gray-500">{{ user.email || '—' }}</td>
            <td class="px-6 py-4">
              <span class="px-2 py-1 text-xs rounded-full" :class="user.email_verified ? 'bg-green-100 text-green-800' : 'bg-yellow-100 text-yellow-800'">
                {{ user.email_verified ? 'Verified' : 'Pending' }}
              </span>
            </td>
            <td class="px-6 py-4">
              <span class="px-2 py-1 text-xs rounded-full" :class="user.mfa_enabled ? 'bg-green-100 text-green-800' : 'bg-gray-100 text-gray-600'">
                {{ user.mfa_enabled ? 'Enabled' : 'Off' }}
              </span>
            </td>
            <td class="px-6 py-4 text-sm">
              <button @click="openRoleModal(user)" class="text-indigo-600 hover:text-indigo-900">Assign Roles</button>
            </td>
          </tr>
        </tbody>
      </table>
    </div>

    <!-- Role Assignment Modal -->
    <div v-if="showRoleModal" class="fixed inset-0 bg-black/50 flex items-center justify-center z-50">
      <div class="bg-white rounded-lg shadow-xl p-6 w-full max-w-md">
        <h3 class="text-lg font-semibold mb-2">Assign Roles</h3>
        <p class="text-sm text-gray-600 mb-4">User: <strong>{{ selectedUser?.username }}</strong></p>
        <div>
          <label class="block text-sm font-medium text-gray-700">Roles (comma-separated)</label>
          <input v-model="roleInput" class="mt-1 block w-full px-3 py-2 border border-gray-300 rounded-md text-sm" placeholder="admin, user" />
          <p class="mt-1 text-xs text-gray-500">Available: admin, user</p>
        </div>
        <div class="flex justify-end space-x-3 mt-4">
          <button @click="showRoleModal = false" class="px-4 py-2 border border-gray-300 rounded-md text-sm">Cancel</button>
          <button @click="assignRoles" :disabled="saving" class="px-4 py-2 bg-indigo-600 text-white rounded-md text-sm hover:bg-indigo-700 disabled:opacity-50">
            {{ saving ? 'Saving...' : 'Save Roles' }}
          </button>
        </div>
      </div>
    </div>
  </div>
</template>
