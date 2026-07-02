import { createApp } from 'vue'
import { createPinia } from 'pinia'
import router from './router'
import App from './App.vue'
import { useAuthStore } from './stores/auth'
import './style.css'

const app = createApp(App)
const pinia = createPinia()
app.use(pinia)
app.use(router)

// Attempt to restore an existing session (refresh token persisted in
// sessionStorage) before mounting + initial navigation. The auth guard reads
// isAuthenticated, which requires the access token to be present. See A-LOGIN-014.
await useAuthStore().restoreSession()

app.mount('#app')
