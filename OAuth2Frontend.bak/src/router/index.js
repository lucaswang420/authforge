import { createRouter, createWebHistory } from 'vue-router'
import Login from '../views/Login.vue'
import Register from '../views/Register.vue'
import Callback from '../views/Callback.vue'
import Dashboard from '../views/Dashboard.vue'

const router = createRouter({
    history: createWebHistory(import.meta.env.BASE_URL),
    routes: [
        {
            path: '/',
            name: 'home',
            component: Login,
            meta: { requiresGuest: true }
        },
        {
            path: '/register',
            name: 'register',
            component: Register,
            meta: { requiresGuest: true }
        },
        {
            path: '/callback',
            name: 'callback',
            component: Callback
        },
        {
            path: '/dashboard',
            name: 'dashboard',
            component: Dashboard,
            meta: { requiresAuth: true }
        }
    ]
})

// Navigation guard for authentication
router.beforeEach((to, from, next) => {
    const accessToken = localStorage.getItem('access_token')
    const isAuthenticated = !!accessToken

    // Check if route requires authentication
    if (to.meta.requiresAuth && !isAuthenticated) {
        // Route requires auth but user is not logged in
        next('/')
        return
    }

    // Check if route requires guest (not logged in)
    if (to.meta.requiresGuest && isAuthenticated) {
        // Route is for guests only but user is logged in
        next('/dashboard')
        return
    }

    // Proceed to route
    next()
})

export default router
