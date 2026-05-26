/**
 * Authentication Configuration
 *
 * This configuration supports multiple environments through environment variables.
 * Priority: Environment Variables > Runtime Config > Defaults
 */

// Default configuration for development
const defaultConfig = {
  // OAuth2 Provider Configuration
  oauth2: {
    clientId: import.meta.env.VITE_OAUTH2_CLIENT_ID || 'vue-client',
    authorizeEndpoint: '/oauth2/authorize',
    tokenEndpoint: '/oauth2/token',
    scope: 'openid profile',
  },

  // External Providers Configuration
  providers: {
    wechat: {
      enabled: import.meta.env.VITE_WECHAT_ENABLED !== 'false',
      appId: import.meta.env.VITE_WECHAT_APPID || '',
      redirectUri: import.meta.env.VITE_WECHAT_REDIRECT_URI || '',
      authUrl: 'https://open.weixin.qq.com/connect/qrconnect',
      scope: 'snsapi_login',
    },

    google: {
      enabled: import.meta.env.VITE_GOOGLE_ENABLED !== 'false',
      clientId: import.meta.env.VITE_GOOGLE_CLIENT_ID || '',
      redirectUri: import.meta.env.VITE_GOOGLE_REDIRECT_URI || '',
      authUrl: 'https://accounts.google.com/o/oauth2/v2/auth',
      scope: 'openid email profile',
    },
  },

  // Application Configuration
  app: {
    apiBaseUrl: import.meta.env.VITE_API_BASE_URL || '',
    callbackPath: '/callback',
  },
}

/**
 * Load runtime configuration from public/config.json if available
 * This allows configuration without rebuild
 */
let runtimeConfig = {}

async function loadRuntimeConfig() {
  try {
    const response = await fetch('/config.json')
    if (response.status === 404) {
      console.info('No runtime configuration found, using defaults.')
      return
    }
    if (!response.ok) throw new Error(`HTTP error! status: ${response.status}`)
    runtimeConfig = await response.json()
    console.info('Runtime configuration loaded successfully')
  } catch (error) {
    console.warn('Failed to load runtime configuration, using defaults:', error)
  }
}

/**
 * Get merged configuration
 * Priority: Runtime Config > Environment Variables > Defaults
 */
function getConfig() {
  // Get current origin for default redirect URIs
  const origin = typeof window !== 'undefined' ? window.location.origin : 'http://localhost:5173'

  const mergedConfig = {
    ...defaultConfig,
    ...runtimeConfig,
    oauth2: {
      ...defaultConfig.oauth2,
      ...(runtimeConfig.oauth2 || {}),
    },
    providers: {
      wechat: {
        ...defaultConfig.providers.wechat,
        ...(runtimeConfig.providers?.wechat || {}),
        redirectUri: runtimeConfig.providers?.wechat?.redirectUri ||
                     import.meta.env.VITE_WECHAT_REDIRECT_URI ||
                     `${origin}/callback`,
      },
      google: {
        ...defaultConfig.providers.google,
        ...(runtimeConfig.providers?.google || {}),
        redirectUri: runtimeConfig.providers?.google?.redirectUri ||
                     import.meta.env.VITE_GOOGLE_REDIRECT_URI ||
                     `${origin}/callback`,
      },
    },
    app: {
      ...defaultConfig.app,
      ...(runtimeConfig.app || {}),
    },
  }

  return mergedConfig
}

// Initialize configuration on module load
loadRuntimeConfig()

export default getConfig
export { loadRuntimeConfig }
