<script setup>
import { onMounted, ref } from 'vue'
import { useRoute, useRouter } from 'vue-router'
import {
  exchangeCodeForToken,
  storeTokens,
  introspectToken,
  parseOAuth2Error,
  getValidAccessToken
} from '@/utils/oauth2Helper'

const route = useRoute()
const router = useRouter()
const status = ref('Processing...')
const userInfo = ref(null)
const tokenInfo = ref(null)
const error = ref(null)

onMounted(async () => {
    const code = route.query.code
    const state = route.query.state

    if (!code) {
        error.value = "No authorization code found in URL"
        status.value = "Error"
        return
    }

    const provider = localStorage.getItem('auth_provider') || 'drogon'
    status.value = `Verifying ${provider} Login...`

    try {
        if (provider === 'wechat' || provider === 'google') {
            // Validate state for external providers
            const savedState = localStorage.getItem(`auth_state_${provider}`)
            if (state !== savedState) {
                throw new Error("Invalid state parameter (CSRF Protection)")
            }
            localStorage.removeItem(`auth_state_${provider}`)

            // Call our Backend to perform the server-side exchange for external providers
            const loginEndpoint = provider === 'wechat' ? '/api/wechat/login' : '/api/google/login'

            const response = await fetch(loginEndpoint, {
                method: 'POST',
                headers: { 'Content-Type': 'application/x-www-form-urlencoded' },
                body: new URLSearchParams({ code: code })
            })

            if (!response.ok) {
                const errText = await response.text()
                throw new Error(`${provider} Login Failed: ${errText}`)
            }

            userInfo.value = await response.json()

            // Normalize display fields
            if (provider === 'wechat') {
                userInfo.value.displayName = userInfo.value.nickname
                userInfo.value.avatarUrl = userInfo.value.headimgurl
            } else {
                userInfo.value.displayName = userInfo.value.name
                userInfo.value.avatarUrl = userInfo.value.picture
            }

            status.value = "Success!"
            return
        }

        // Standard OAuth / Drogon Flow
        // Validate state
        const savedState = localStorage.getItem('auth_state_drogon')
        if (state !== savedState) {
            throw new Error("Invalid state parameter (CSRF Protection)")
        }
        localStorage.removeItem('auth_state_drogon')

        status.value = "Exchanging authorization code..."

        // Exchange code for token (with PKCE support)
        const tokenData = await exchangeCodeForToken({
            code: code,
            redirectUri: window.location.origin + '/callback',
            clientId: 'vue-client'
        })

        // Store tokens securely
        storeTokens(tokenData)

        status.value = "Validating token..."

        // Introspect token to get detailed information
        const accessToken = getValidAccessToken()
        if (accessToken) {
            try {
                tokenInfo.value = await introspectToken(accessToken)

                // Add token metadata to user info
                if (tokenInfo.active) {
                    userInfo.value = {
                        ...tokenInfo,
                        displayName: tokenInfo.sub || tokenInfo.client_id,
                        tokenMetadata: {
                            issuedAt: new Date(tokenInfo.iat * 1000).toLocaleString(),
                            expiresAt: new Date(tokenInfo.exp * 1000).toLocaleString(),
                            scope: tokenInfo.scope,
                            issuer: tokenInfo.iss
                        }
                    }
                }
            } catch (introspectError) {
                console.warn('Token introspection failed, continuing with user info fetch:', introspectError)
            }
        }

        // Fetch detailed user info
        status.value = "Fetching user profile..."
        const userResponse = await fetch('/oauth2/userinfo', {
            headers: { 'Authorization': `Bearer ${accessToken}` }
        })

        if (!userResponse.ok) {
            throw new Error(`User Info failed: ${userResponse.status}`)
        }

        const detailedUserInfo = await userResponse.json()

        // Merge detailed user info with token info
        userInfo.value = {
            ...userInfo.value,
            ...detailedUserInfo,
            displayName: detailedUserInfo.name || detailedUserInfo.sub || userInfo.value.displayName
        }

        // Store user info
        localStorage.setItem('user_info', JSON.stringify(userInfo.value))

        status.value = "Success!"

    } catch (e) {
        error.value = e.message
        status.value = "Error"
        console.error('Authentication error:', e)
    }
})
</script>

<template>
  <div class="callback-container">
    <div class="glass-card">
        <div class="logo" aria-hidden="true">OAuth</div>

        <!-- Loading State -->
        <div v-if="!userInfo && !error" class="status-box">
            <div class="spinner"></div>
            <p>{{ status }}</p>
        </div>

        <!-- Error State -->
        <div v-if="error" class="error-box">
            <div class="icon" aria-hidden="true">Error</div>
            <h3>Authentication Failed</h3>
            <p>{{ error }}</p>
            <router-link to="/" class="btn-primary">Return Home</router-link>
        </div>

        <!-- Success State -->
        <div v-if="userInfo" class="success-box">
            <div class="icon" aria-hidden="true">OK</div>
            <h3>Login Successful!</h3>
            <p class="welcome-text">Welcome back, <strong>{{ userInfo.displayName }}</strong></p>

            <div class="user-profile">
                <img v-if="userInfo.avatarUrl" :src="userInfo.avatarUrl" alt="Avatar" class="avatar">
                <div v-else class="avatar-placeholder">{{ userInfo.displayName ? userInfo.displayName[0].toUpperCase() : 'U' }}</div>

                <div class="user-details">
                    <h4>Profile Information</h4>
                    <pre>{{ JSON.stringify(userInfo, null, 2) }}</pre>

                    <!-- Token Metadata (if available) -->
                    <div v-if="userInfo.tokenMetadata" class="token-metadata">
                        <h4>Token Information</h4>
                        <div class="metadata-grid">
                            <div class="metadata-item">
                                <span class="label">Issued At:</span>
                                <span class="value">{{ userInfo.tokenMetadata.issuedAt }}</span>
                            </div>
                            <div class="metadata-item">
                                <span class="label">Expires At:</span>
                                <span class="value">{{ userInfo.tokenMetadata.expiresAt }}</span>
                            </div>
                            <div class="metadata-item">
                                <span class="label">Scope:</span>
                                <span class="value">{{ userInfo.tokenMetadata.scope }}</span>
                            </div>
                            <div class="metadata-item">
                                <span class="label">Issuer:</span>
                                <span class="value">{{ userInfo.tokenMetadata.issuer }}</span>
                            </div>
                        </div>
                    </div>

                    <!-- Roles and Scopes (if available) -->
                    <div v-if="userInfo.roles || userInfo.scope" class="permissions">
                        <h4>Permissions</h4>
                        <div v-if="userInfo.roles" class="permission-group">
                            <span class="permission-label">Roles:</span>
                            <div class="permission-badges">
                                <span v-for="role in userInfo.roles" :key="role" class="badge badge-role">{{ role }}</span>
                            </div>
                        </div>
                        <div v-if="userInfo.scope" class="permission-group">
                            <span class="permission-label">Scopes:</span>
                            <div class="permission-badges">
                                <span v-for="scope in userInfo.scope.split(' ')" :key="scope" class="badge badge-scope">{{ scope }}</span>
                            </div>
                        </div>
                    </div>
                </div>
            </div>

            <router-link to="/dashboard" class="btn-primary">Continue to Dashboard</router-link>
        </div>
    </div>
  </div>
</template>

<style scoped>
:root {
  --primary-color: #667eea;
  --bg-primary: #0f0f23;
  --glass-bg: rgba(255, 255, 255, 0.05);
  --glass-border: rgba(255, 255, 255, 0.1);
  --text-primary: #e2e8f0;
  --text-secondary: #a0aec0;
}

.callback-container {
    min-height: 100vh;
    display: flex;
    align-items: center;
    justify-content: center;
    background: #0f0f23;
    background: radial-gradient(circle at 50% 0%, #1a1a2e 0%, #0f0f23 100%);
    color: #e2e8f0;
    font-family: 'Inter', sans-serif;
}

.glass-card {
    background: rgba(255, 255, 255, 0.05);
    backdrop-filter: blur(20px);
    border: 1px solid rgba(255, 255, 255, 0.1);
    border-radius: 20px;
    padding: 3rem;
    width: 100%;
    max-width: 600px;
    box-shadow: 0 20px 50px rgba(0, 0, 0, 0.5);
    text-align: center;
    animation: fadeIn 0.5s ease-out;
}

.logo {
    font-size: 3rem;
    margin-bottom: 1.5rem;
    animation: bounce 2s infinite;
}

.status-box, .error-box, .success-box {
    display: flex;
    flex-direction: column;
    align-items: center;
    gap: 1rem;
}

.spinner {
    width: 40px;
    height: 40px;
    border: 3px solid rgba(255, 255, 255, 0.1);
    border-top-color: #667eea;
    border-radius: 50%;
    animation: spin 1s linear infinite;
}

.btn-primary {
    display: inline-block;
    margin-top: 1.5rem;
    padding: 0.8rem 2rem;
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    color: white;
    text-decoration: none;
    border-radius: 50px;
    font-weight: 600;
    transition: transform 0.2s, box-shadow 0.2s;
}

.btn-primary:hover {
    transform: translateY(-2px);
    box-shadow: 0 10px 20px rgba(118, 75, 162, 0.3);
}

.user-profile {
    margin-top: 1.5rem;
    width: 100%;
}

.avatar {
    width: 80px;
    height: 80px;
    border-radius: 50%;
    border: 3px solid #667eea;
    margin-bottom: 1rem;
}

.avatar-placeholder {
    width: 80px;
    height: 80px;
    border-radius: 50%;
    background: linear-gradient(135deg, #667eea 0%, #764ba2 100%);
    display: flex;
    align-items: center;
    justify-content: center;
    font-size: 2rem;
    font-weight: bold;
    margin: 0 auto 1rem;
}

.user-details {
    text-align: left;
    margin-top: 1rem;
}

.user-details h4 {
    color: #667eea;
    margin-bottom: 0.5rem;
    font-size: 1rem;
}

pre {
    background: rgba(0, 0, 0, 0.3);
    padding: 1rem;
    border-radius: 8px;
    text-align: left;
    overflow-x: auto;
    font-size: 0.85rem;
    color: #a0aec0;
    max-height: 200px;
    width: 100%;
}

.token-metadata, .permissions {
    margin-top: 1rem;
    padding: 1rem;
    background: rgba(102, 126, 234, 0.1);
    border-radius: 8px;
}

.metadata-grid {
    display: grid;
    grid-template-columns: auto 1fr;
    gap: 0.5rem 1rem;
    margin-top: 0.5rem;
}

.metadata-item {
    display: contents;
}

.metadata-item .label {
    color: #a0aec0;
    font-size: 0.875rem;
}

.metadata-item .value {
    color: #e2e8f0;
    font-size: 0.875rem;
}

.permission-group {
    margin-bottom: 0.5rem;
}

.permission-label {
    display: block;
    color: #a0aec0;
    font-size: 0.875rem;
    margin-bottom: 0.25rem;
}

.permission-badges {
    display: flex;
    flex-wrap: wrap;
    gap: 0.5rem;
}

.badge {
    padding: 0.25rem 0.75rem;
    border-radius: 12px;
    font-size: 0.75rem;
    font-weight: 600;
}

.badge-role {
    background: rgba(102, 126, 234, 0.3);
    color: #667eea;
}

.badge-scope {
    background: rgba(118, 75, 162, 0.3);
    color: #764ba2;
}

@keyframes spin { to { transform: rotate(360deg); } }
@keyframes fadeIn { from { opacity: 0; transform: translateY(20px); } to { opacity: 1; transform: translateY(0); } }
@keyframes bounce { 0%, 100% { transform: translateY(0); } 50% { transform: translateY(-10px); } }
</style>
