import { defineConfig, Plugin } from 'vite';
import { readFileSync, renameSync, existsSync } from 'fs';
import { resolve, join } from 'path';

/**
 * Custom plugin to compile audio_worklet_processor as a standalone IIFE asset.
 * AudioWorklets cannot use ES module imports, so we compile it separately
 * and emit it as a static asset that gets loaded via audioWorklet.addModule().
 */
function audioWorkletPlugin(): Plugin {
  const WORKLET_ID = 'virtual:audio-worklet';
  return {
    name: 'audio-worklet-plugin',
    generateBundle() {
      // Read the worklet source and emit as a standalone asset
      const workletPath = resolve(__dirname, 'modules/audio/audio_worklet_processor.js');
      try {
        const source = readFileSync(workletPath, 'utf-8');
        this.emitFile({
          type: 'asset',
          fileName: 'audio_worklet_processor.js',
          source,
        });
      } catch {
        // Worklet file may not exist yet during TS migration
        console.warn('[audio-worklet-plugin] Could not read audio_worklet_processor.js');
      }
    },
  };
}

export default defineConfig({
  // Critical: output is served from arbitrary paths like examples/Blink/fastled_js/
  base: './',

  build: {
    outDir: 'dist',
    emptyOutDir: true,

    rollupOptions: {
      output: {
        // No content hashing — wasm_build.py references files by name
        entryFileNames: '[name].js',
        chunkFileNames: '[name].js',
        assetFileNames: (assetInfo) => {
          // Place font files in assets/fonts/ subdirectory
          if (assetInfo.names?.some(n => /\.(ttf|woff2?|eot|otf)$/i.test(n))) {
            return 'assets/fonts/[name][extname]';
          }
          return '[name][extname]';
        },
      },
    },

    // Don't minify during development/debugging
    minify: false,

    // Generate source maps for debugging
    sourcemap: true,

    // Target ES2021 to match tsconfig
    target: 'es2021',
  },

  worker: {
    format: 'es',
    rollupOptions: {
      output: {
        entryFileNames: 'fastled_background_worker.js',
      },
    },
  },

  plugins: [
    audioWorkletPlugin(),
    // Rename .ts worker output to .js after build
    {
      name: 'rename-worker-output',
      closeBundle() {
        const distDir = resolve(__dirname, 'dist');
        const tsWorker = join(distDir, 'fastled_background_worker.ts');
        const jsWorker = join(distDir, 'fastled_background_worker.js');
        if (existsSync(tsWorker)) {
          renameSync(tsWorker, jsWorker);
        }
      },
    },
  ],

  // Ensure Vite serves from the compiler directory during dev
  root: '.',
  publicDir: false,
});
