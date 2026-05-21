import { createRouter, createWebHistory } from 'vue-router'
import { useAuthStore } from '../stores/auth'

const router = createRouter({
  history: createWebHistory('/admin/'),
  routes: [
    {
      path: '/login',
      name: 'login',
      component: () => import('../pages/LoginPage.vue'),
      meta: { requiresAuth: false },
    },
    {
      path: '/callback',
      name: 'callback',
      component: () => import('../pages/CallbackPage.vue'),
      meta: { requiresAuth: false },
    },
    {
      path: '/',
      component: () => import('../components/layout/AdminLayout.vue'),
      meta: { requiresAuth: true },
      children: [
        {
          path: '',
          name: 'dashboard',
          component: () => import('../pages/dashboard/DashboardPage.vue'),
        },
        {
          path: 'applications',
          name: 'applications',
          component: () => import('../pages/applications/ApplicationsPage.vue'),
        },
        {
          path: 'users',
          name: 'users',
          component: () => import('../pages/users/UsersPage.vue'),
        },
        {
          path: 'logs',
          name: 'logs',
          component: () => import('../pages/logs/LogsPage.vue'),
        },
        {
          path: 'settings',
          name: 'settings',
          component: () => import('../pages/settings/SettingsPage.vue'),
        },
      ],
    },
  ],
})

router.beforeEach((to, _from, next) => {
  const auth = useAuthStore()
  if (to.meta.requiresAuth !== false && !auth.isAuthenticated) {
    next({ name: 'login' })
  } else {
    next()
  }
})

export default router
