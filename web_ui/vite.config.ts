import { defineConfig } from 'vite'
import vue from '@vitejs/plugin-vue'

export default defineConfig({
  plugins: [vue()],
  server: {
    proxy: {
      '/api': 'http://localhost:8080',
      '/ws': { target: 'ws://localhost:8080', ws: true },
      '/health': 'http://localhost:8080'
    }
  },
  build: {
    outDir: '../web',
    emptyOutDir: true
  }
})
