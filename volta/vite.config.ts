import { defineConfig } from 'vite';

export default defineConfig({
  base: './',
  build: { target: 'es2022', chunkSizeWarningLimit: 2000, assetsInlineLimit: 0 },
  server: { host: true },
  test: { environment: 'node', testTimeout: 600000 },
} as never);
