# JavaScript Linting for FastLED

FastLED uses **Deno** for JavaScript linting and formatting in the WASM platform code. This provides a simple, single-binary solution without requiring Node.js or complex npm setup.

## Quick Start

### 1. Setup (One-time)

```bash
python3 ci/setup-js-linting.py
```

This downloads Deno and sets up the configuration. No Node.js, npm, or complex dependencies required!

### 2. Lint JavaScript Files

```bash
./lint-js              # Lint JavaScript files
./format-js            # Format JavaScript files  
./check-js             # Enhanced linting & optional type checking
bash lint               # Run all linting (includes JS)
```

## What Gets Linted

The system lints all JavaScript files in `src/platforms/wasm/`:

- `src/platforms/wasm/compiler/index.js`
- `src/platforms/wasm/compiler/modules/*.js`

## Linting Rules

Deno uses recommended rules that catch common issues:

### Error Prevention
- **require-await**: Async functions must use `await`
- **no-global-assign**: Don't reassign global variables
- **no-debugger**: Remove debugger statements
- **no-func-assign**: Don't reassign function declarations

### Code Quality  
- **prefer-const**: Use `const` for variables that aren't reassigned
- **no-window-prefix**: Use `self` instead of `window` for Web Worker compatibility

### Performance
- Optimized for real-time graphics and audio processing
- Catches patterns that could impact frame rates

## JSDoc Type Checking (Optional)

The system includes built-in TypeScript-powered type checking for JavaScript files using JSDoc annotations.

### Current Status
- **Enhanced Linting**: Always enabled (syntax, style, best practices)
- **JSDoc Type Checking**: Currently disabled (can be enabled)

### Enable Type Checking

Edit `deno.json` and change:
```json
{
  "compilerOptions": {
    "checkJs": true  // Change from false to true
  }
}
```

### Adding JSDoc Types

```javascript
/**
 * Process audio data for visualization
 * @param {Float32Array} audioData - Raw audio samples
 * @param {number} sampleRate - Audio sample rate in Hz
 * @param {Object} options - Processing options
 * @param {boolean} options.normalize - Whether to normalize output
 * @returns {Promise<Int16Array>} Processed audio samples
 */
async function processAudio(audioData, sampleRate, options) {
  // Implementation...
}
```

### Benefits of JSDoc Type Checking
- **Catch type errors** before runtime
- **Better IDE support** with autocomplete and error highlighting  
- **Documentation** that stays synchronized with code
- **Gradual adoption** - add types incrementally
- **Zero runtime overhead** - types are compile-time only

### Type Definitions

Global types for FastLED WASM are defined in `src/platforms/wasm/types.d.ts`:
- Window extensions (audio functions, UI manager)
- Audio Worklet processor types
- DOM element extensions
- Custom FastLED interfaces

## Configuration

Configuration is in `deno.json`:

```json
{
  "lint": {
    "rules": {
      "tags": ["recommended"],
      "exclude": [
        "no-unused-vars",  // Too noisy for existing code
        "no-console"       // Console is needed for debugging
      ]
    },
    "include": ["src/platforms/wasm/"],
    "exclude": []
  }
}
```

## Integration with Main Lint Script

JavaScript linting and type checking are integrated into the main `bash lint` command:

1. **Python linting** (ruff, black, isort, pyright)
2. **C++ formatting** (clang-format)  
3. **JavaScript linting** (Deno) ← **New!**
4. **JavaScript enhanced linting & type checking** (Deno) ← **New!**

## Why Deno Instead of ESLint?

| Feature | Deno | ESLint/Node.js |
|---------|------|----------------|
| **Installation** | Single binary download | Complex npm setup |
| **Dependencies** | Zero | Node.js + npm packages |
| **Speed** | Fast startup | Slower due to npm overhead |
| **Configuration** | Simple JSON | Complex config files |
| **Cross-platform** | Works everywhere | Platform-dependent |

## Files Created

- `.js-tools/deno/` - Deno binary (single file!)
- `deno.json` - Configuration file with TypeScript compiler options
- `lint-js` - Linting script
- `format-js` - Formatting script
- `check-js` - Enhanced linting & type checking script
- `src/platforms/wasm/types.d.ts` - TypeScript definitions for FastLED WASM globals

## Example Output

```bash
🔍 FastLED JavaScript Linting (Deno)
Found JavaScript files:
  src/platforms/wasm/compiler/index.js
  src/platforms/wasm/compiler/modules/ui_manager.js
  ...

(require-await) Async function '_loadFastLED' has no 'await' expression.
async function _loadFastLED(options) {
^^^^^
    hint: Remove 'async' keyword from the function or use 'await' expression inside.

(prefer-const) `DEFAULT_PROCESSOR_TYPE` is never reassigned
let DEFAULT_PROCESSOR_TYPE = AUDIO_PROCESSOR_TYPES.SCRIPT_PROCESSOR;
    hint: Use `const` instead

Found 2 problems
```

## Troubleshooting

### "Deno not found"
```bash
python3 ci/setup-js-linting.py
```

### "JavaScript linting tools not found"
Make sure both files exist:
- `./lint-js` 
- `.js-tools/deno/deno`

### Cache warnings
Deno cache warnings are harmless and don't affect linting functionality.

## No Node.js Required! 🎊

This solution specifically avoids the complexity of Node.js/npm while providing excellent JavaScript linting for the FastLED project. 
