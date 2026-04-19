import { defineConfig } from 'vite';
import react from '@vitejs/plugin-react';

export default defineConfig({
  plugins: [react()],
  server: {
    port: 3000,
    proxy: {
      '/api': {
        target: 'http://localhost:4000',
        changeOrigin: true,
        // Camera discovery fans out across ~17 external channels; cold runs
        // can take 30–60s before the SQLite cache warms. Give the proxy a
        // generous window so the browser doesn't ECONNRESET mid-request.
        timeout: 120000,
        proxyTimeout: 120000,
      },
    },
  },
  build: {
    outDir: 'dist',
    sourcemap: true,
  },
});
