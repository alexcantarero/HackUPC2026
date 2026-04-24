import { defineConfig } from 'vite'
import react, { reactCompilerPreset } from '@vitejs/plugin-react'
import babel from '@rolldown/plugin-babel'
import { resolve } from 'node:path'

// https://vite.dev/config/
export default defineConfig({
  assetsInclude: ['**/*.glb'],
  server: {
    fs: {
      allow: [resolve(__dirname, '..')],
    },
  },
  plugins: [
    react(),
    babel({ presets: [reactCompilerPreset()] })
  ],
})
