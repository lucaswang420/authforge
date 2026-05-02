<script setup>
import { ref, onMounted } from 'vue'
import { useRouter } from 'vue-router'

const router = useRouter()
const userInfo = ref(null)
const loading = ref(true)
const error = ref(null)

onMounted(async () => {
    // Get access token from localStorage
    const accessToken = localStorage.getItem('access_token')

    if (!accessToken) {
        // No token found - this should be handled by router guard
        // But set error state as fallback
        error.value = 'No access token found. Please login again.'
        loading.value = false
        return
    }

    try {
        // Fetch user info
        const response = await fetch('/oauth2/userinfo', {
            headers: {
                'Authorization': `Bearer ${accessToken}`
            }
        })

        if (!response.ok) {
            if (response.status === 401) {
                // Token expired or invalid, clear and redirect to login
                localStorage.removeItem('access_token')
                localStorage.removeItem('user_info')
                error.value = 'Session expired. Please login again.'
                // Don't redirect here, let router guard handle it
                loading.value = false
                return
            }
            throw new Error(`Failed to fetch user info: ${response.status}`)
        }

        userInfo.value = await response.json()

        // Store user info in localStorage
        localStorage.setItem('user_info', JSON.stringify(userInfo.value))

    } catch (err) {
        error.value = err.message
    } finally {
        loading.value = false
    }
})

const handleLogout = async () => {
    try {
        const accessToken = localStorage.getItem('access_token')

        // Optional: Call backend logout endpoint to revoke token
        if (accessToken) {
            try {
                await fetch('/oauth2/logout', {
                    method: 'POST',
                    headers: {
                        'Authorization': `Bearer ${accessToken}`
                    }
                })
            } catch (err) {
                console.warn('Backend logout failed (non-critical):', err)
                // Continue with local logout even if backend call fails
            }
        }

        // Clear local storage
        localStorage.removeItem('access_token')
        localStorage.removeItem('user_info')
        localStorage.removeItem('auth_provider')

        // Redirect to login
        router.push('/')

    } catch (err) {
        error.value = `Logout failed: ${err.message}`
    }
}

const getDisplayName = () => {
    if (!userInfo.value) return 'User'
    return userInfo.value.displayName ||
           userInfo.value.name ||
           userInfo.value.sub ||
           'User'
}

const getAvatarText = () => {
    const name = getDisplayName()
    return name.charAt(0).toUpperCase()
}

const getRoles = () => {
    if (!userInfo.value || !userInfo.value.roles) return []
    return userInfo.value.roles
}
</script>

<template>
  <div class="dashboard-container">
    <!-- Header with Logout Button -->
    <div class="dashboard-header">
      <div class="logo" aria-hidden="true">OAuth</div>
      <h1>Dashboard</h1>
      <button @click="handleLogout" class="btn-logout" title="Logout">
        <span aria-hidden="true">Logout</span>
      </button>
    </div>

    <!-- Loading State -->
    <div v-if="loading" class="loading-container">
      <div class="spinner"></div>
      <p>Loading user information...</p>
    </div>

    <!-- Error State -->
    <div v-if="error" class="error-container">
      <div class="error-box">
        <div class="icon" aria-hidden="true">Error</div>
        <h3>Error Loading Dashboard</h3>
        <p>{{ error }}</p>
        <button @click="router.push('/')" class="btn-primary">Return to Login</button>
      </div>
    </div>

    <!-- User Info Display -->
    <div v-if="userInfo && !loading" class="dashboard-content">
      <div class="glass-card">
        <!-- User Profile Section -->
        <div class="profile-section">
          <div class="avatar-large">
            {{ getAvatarText() }}
          </div>

          <h2 class="user-name">{{ getDisplayName() }}</h2>
          <p class="user-id">User ID: {{ userInfo.sub || userInfo.user_id || 'N/A' }}</p>
        </div>

        <!-- User Details -->
        <div class="details-section">
          <h3>Account Information</h3>

          <div class="detail-row">
            <span class="label">Email:</span>
            <span class="value">{{ userInfo.email || 'N/A' }}</span>
          </div>

          <div class="detail-row">
            <span class="label">Username:</span>
            <span class="value">{{ userInfo.preferred_username || userInfo.name || 'N/A' }}</span>
          </div>

          <div class="detail-row" v-if="getRoles().length > 0">
            <span class="label">Roles:</span>
            <div class="roles-list">
              <span v-for="role in getRoles()" :key="role" class="role-badge">
                {{ role }}
              </span>
            </div>
          </div>
        </div>

        <!-- Raw Data (for debugging) -->
        <details class="raw-data">
          <summary>View Raw User Data</summary>
          <pre>{{ JSON.stringify(userInfo, null, 2) }}</pre>
        </details>
      </div>

      <!-- Actions -->
      <div class="actions-section">
        <button @click="handleLogout" class="btn-danger btn-large">
          Logout
        </button>
      </div>
    </div>
  </div>
</template>

<style scoped>
:root {
  --primary-color: #667eea;
  --danger-color: #ef4444;
  --bg-primary: #0f0f23;
  --bg-secondary: #1a1a2e;
  --glass-bg: rgba(255, 255, 255, 0.05);
  --glass-border: rgba(255, 255, 255, 0.1);
  --text-primary: #e2e8f0;
  --text-secondary: #a0aec0;
}

.dashboard-container {
  min-height: 100vh;
  background: var(--bg-primary);
  background: radial-gradient(circle at 50% 0%, var(--bg-secondary) 0%, var(--bg-primary) 100%);
  color: var(--text-primary);
  font-family: 'Inter', sans-serif;
}

.dashboard-header {
  display: flex;
  align-items: center;
  justify-content: space-between;
  padding: 1.5rem 3rem;
  background: rgba(15, 15, 35, 0.8);
  backdrop-filter: blur(20px);
  border-bottom: 1px solid var(--glass-border);
  position: sticky;
  top: 0;
  z-index: 100;
}

.dashboard-header .logo {
  font-size: 1.5rem;
  font-weight: bold;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
}

.dashboard-header h1 {
  font-size: 1.5rem;
  margin: 0 0 0 1rem;
  flex: 1;
}

.btn-logout {
  padding: 0.6rem 1.5rem;
  background: var(--glass-bg);
  border: 1px solid var(--glass-border);
  color: var(--text-primary);
  border-radius: 8px;
  cursor: pointer;
  transition: all 0.2s;
  font-weight: 500;
}

.btn-logout:hover {
  background: rgba(239, 68, 68, 0.2);
  border-color: rgba(239, 68, 68, 0.5);
  transform: translateY(-1px);
}

.loading-container {
  display: flex;
  flex-direction: column;
  align-items: center;
  justify-content: center;
  min-height: 50vh;
  gap: 1rem;
}

.spinner {
  width: 50px;
  height: 50px;
  border: 4px solid rgba(255, 255, 255, 0.1);
  border-top-color: var(--primary-color);
  border-radius: 50%;
  animation: spin 1s linear infinite;
}

.error-container {
  display: flex;
  align-items: center;
  justify-content: center;
  min-height: 50vh;
  padding: 2rem;
}

.error-box {
  background: rgba(239, 68, 68, 0.1);
  border: 1px solid rgba(239, 68, 68, 0.3);
  border-radius: 16px;
  padding: 2rem;
  text-align: center;
  max-width: 400px;
}

.error-box .icon {
  font-size: 3rem;
  margin-bottom: 1rem;
}

.dashboard-content {
  max-width: 800px;
  margin: 0 auto;
  padding: 3rem 2rem;
}

.glass-card {
  background: var(--glass-bg);
  backdrop-filter: blur(20px);
  border: 1px solid var(--glass-border);
  border-radius: 20px;
  padding: 3rem;
  margin-bottom: 2rem;
  box-shadow: 0 20px 50px rgba(0, 0, 0, 0.3);
}

.profile-section {
  text-align: center;
  margin-bottom: 3rem;
  padding-bottom: 2rem;
  border-bottom: 1px solid var(--glass-border);
}

.avatar-large {
  width: 120px;
  height: 120px;
  border-radius: 50%;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  display: flex;
  align-items: center;
  justify-content: center;
  font-size: 3rem;
  font-weight: bold;
  margin: 0 auto 1.5rem;
  border: 4px solid var(--primary-color);
}

.user-name {
  font-size: 2rem;
  margin-bottom: 0.5rem;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  -webkit-background-clip: text;
  -webkit-text-fill-color: transparent;
  background-clip: text;
}

.user-id {
  color: var(--text-secondary);
  font-size: 0.9rem;
}

.details-section {
  margin-bottom: 2rem;
}

.details-section h3 {
  font-size: 1.2rem;
  margin-bottom: 1.5rem;
  color: var(--text-secondary);
}

.detail-row {
  display: flex;
  align-items: center;
  padding: 1rem 0;
  border-bottom: 1px solid var(--glass-border);
}

.detail-row:last-child {
  border-bottom: none;
}

.detail-row .label {
  font-weight: 600;
  min-width: 120px;
  color: var(--text-secondary);
}

.detail-row .value {
  flex: 1;
  color: var(--text-primary);
}

.roles-list {
  display: flex;
  gap: 0.5rem;
  flex-wrap: wrap;
}

.role-badge {
  padding: 0.4rem 1rem;
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  border-radius: 20px;
  font-size: 0.85rem;
  font-weight: 500;
}

.raw-data {
  margin-top: 2rem;
}

.raw-data summary {
  cursor: pointer;
  color: var(--text-secondary);
  padding: 1rem;
  background: rgba(0, 0, 0, 0.2);
  border-radius: 8px;
  transition: background 0.2s;
}

.raw-data summary:hover {
  background: rgba(0, 0, 0, 0.3);
}

.raw-data pre {
  margin-top: 1rem;
  background: rgba(0, 0, 0, 0.3);
  padding: 1rem;
  border-radius: 8px;
  overflow-x: auto;
  font-size: 0.85rem;
  color: var(--text-secondary);
  max-height: 300px;
}

.actions-section {
  display: flex;
  justify-content: center;
  gap: 1rem;
}

.btn-primary, .btn-danger {
  padding: 1rem 3rem;
  border: none;
  border-radius: 50px;
  font-weight: 600;
  font-size: 1rem;
  cursor: pointer;
  transition: all 0.2s;
  text-decoration: none;
  display: inline-block;
}

.btn-primary {
  background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
  color: white;
}

.btn-danger {
  background: linear-gradient(135deg, #ef4444 0%, #dc2626 100%);
  color: white;
}

.btn-primary:hover, .btn-danger:hover {
  transform: translateY(-2px);
  box-shadow: 0 10px 20px rgba(0, 0, 0, 0.3);
}

@keyframes spin {
  to { transform: rotate(360deg); }
}

@media (max-width: 768px) {
  .dashboard-header {
    padding: 1rem;
  }

  .dashboard-header h1 {
    font-size: 1.2rem;
  }

  .dashboard-content {
    padding: 2rem 1rem;
  }

  .glass-card {
    padding: 2rem;
  }

  .detail-row {
    flex-direction: column;
    align-items: flex-start;
    gap: 0.5rem;
  }

  .detail-row .label {
    min-width: auto;
  }
}
</style>
