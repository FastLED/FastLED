# FastLED WASM Graphics Bug - Nothing Showing

**Status**: BROKEN - Graphics not rendering  
**Date**: 2025-09-29  
**Severity**: P0 Critical

## Problem
Graphics modes completely broken after reverting worker code. Browser shows nothing.

## Root Cause  
`index.js` imports deleted file `fastled_async_controller.js` (line 37).

Files deleted but still referenced:
- `modules/fastled_async_controller.js`
- `modules/fastled_background_worker.js`
- `modules/fastled_worker_manager.js`
- `modules/graphics_manager_base.js`
- `modules/offscreen_graphics_adapter.js`

## The Fix

### Step 1: View Original Working Code
```bash
git show 4caa3a0ec3~1:src/platforms/wasm/compiler/index.js > /tmp/index_before_worker.js
cat /tmp/index_before_worker.js | less
```

### Step 2: Remove Worker Imports from index.js

**File**: `src/platforms/wasm/compiler/index.js`

DELETE these lines:
- Line 37: `import { FastLEDAsyncController } from './modules/fastled_async_controller.js';`
- Line 120: `let fastLEDController = null;`
- Lines 458-460: FastLEDAsyncController instantiation

### Step 3: Restore Frame Loop

Replace async controller with original pattern (see commit 4caa3a0ec3~1):

```javascript
function startFrameLoop() {
    function updateFrame() {
        if (!moduleInstance) return;
        
        const hasNewFrame = moduleInstance._hasNewFrameData();
        if (hasNewFrame) {
            const frameData = getFrameData(moduleInstance);
            if (frameData && graphicsManager) {
                graphicsManager.updateCanvas(frameData);
            }
        }
        
        moduleInstance._extern_loop();
        requestAnimationFrame(updateFrame);
    }
    requestAnimationFrame(updateFrame);
}
```

### Step 4: Search for Remaining References
```bash
cd /c/Users/niteris/dev/fastled3
grep -rn "FastLEDAsyncController" src/platforms/wasm/compiler/
grep -rn "OffscreenCanvas" src/platforms/wasm/compiler/
```

Remove any found.

## Testing

### Quick Manual Test
```bash
cd /c/Users/niteris/dev/fastled3
uv run ci/wasm_compile.py examples/Blink --just-compile
# Open browser to URL printed, check for:
# 1. No console errors
# 2. Blinking LEDs visible
```

### Playwright Test
```bash
# Install if needed
uv pip install playwright
playwright install chromium

# Test (manual inspection)
python -m playwright codegen http://localhost:8141
```

## Success Criteria
- [x] Compile succeeds
- [ ] Browser shows graphics  
- [ ] No module loading errors in console
- [ ] Both `?gfx=0` and `?gfx=1` work
- [ ] `bash lint --js` passes

## Architecture Notes

### Graphics Modes
- **gfx=0**: Fast WebGL 2D (`graphics_manager.js`)
- **gfx=1**: Beautiful Three.js 3D (`graphics_manager_threejs.js`)

Both already restored to working pre-worker versions ✅

### Why Workers Failed
Workers can't access:
- `document.getElementById()` 
- `canvas.style.*`
- `clientWidth/clientHeight`

Three.js needs DOM. Keep everything on main thread.

## Key Commands
```bash
# View diff between working and current
git diff 4caa3a0ec3~1 HEAD -- src/platforms/wasm/compiler/index.js

# Check deleted files
git status --short

# Lint when done
bash lint --js

# Full test
uv run ci/wasm_compile.py examples/Blink --just-compile
```

## Files to Edit
- `src/platforms/wasm/compiler/index.js` (remove worker code)

## Files Already Fixed
- `src/platforms/wasm/compiler/modules/graphics_manager.js` ✅
- `src/platforms/wasm/compiler/modules/graphics_manager_threejs.js` ✅

---

**TL;DR**: Remove FastLEDAsyncController import and instantiation from `index.js`. Restore original frame loop from commit `4caa3a0ec3~1`. Graphics managers are already fixed.
