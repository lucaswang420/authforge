import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'
import tailwindcss from '@tailwindcss/vite'

export default defineConfig({
  plugins: [vue(), tailwindcss()],
  base: '/admin/',
  server: {
    port: 5174,
    proxy: {
      '/api': {
        target: 'http://127.0.0.1:5555',
        changeOrigin: true,
      },
      '/oauth2': {
        target: 'http://127.0.0.1:5555',
        changeOrigin: true,
      },
      '/health': {
        target: 'http://127.0.0.1:5555',
        changeOrigin: true,
      },
      '/.well-known': {
        target: 'http://127.0.0.1:5555',
        changeOrigin: true,
      },
    },
  },
})
