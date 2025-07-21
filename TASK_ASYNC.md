# FastLED WASM Asyncify Integration Task

## Overview

This document outlines the changes necessary to modify the JavaScript integration for FastLED to handle the async control loop using Emscripten's Asyncify feature. The compiler-side asyncify support has already been implemented in `compilation_flags.toml`.

## Current Implementation Status

✅ **Compiler Support**: Asyncify flags have been added to the WASM compilation:
- `-sASYNCIFY=1` - Enables asyncify support
- `-sASYNCIFY_STACK_SIZE=16384` - 16KB stack for async operations  
- `-sASYNCIFY_EXPORTS=['_extern_setup','_extern_loop']` - Main FastLED functions are async-enabled

## Required JavaScript Changes

### 1. Update Module Loading and Initialization

**Current Pattern (Synchronous)**:
```javascript
// Traditional synchronous module loading
const fastled = await import('./fastled.js');
await fastled.default();

// Direct function calls
fastled._extern_setup();
setInterval(() => {
    fastled._extern_loop();
}, 16); // ~60 FPS
```

**New Pattern (Asyncify-Aware)**:
```javascript
// Asyncify-aware module loading
const fastled = await import('./fastled.js');
const Module = await fastled.default();

// Async function wrapper
async function callAsyncFunction(funcName, ...args) {
    const result = Module.ccall(funcName, 'number', [], [], {async: true});
    return await result;
}

// Or using direct async calls if functions return promises
async function setup() {
    const setupResult = Module._extern_setup();
    if (setupResult instanceof Promise) {
        await setupResult;
    }
}

async function loop() {
    const loopResult = Module._extern_loop();
    if (loopResult instanceof Promise) {
        await loopResult;
    }
}
```

### 2. Implement Async Control Loop

**Replace the traditional setInterval loop with an async-aware version**:

```javascript
class FastLEDController {
    constructor(module) {
        this.module = module;
        this.running = false;
        this.frameCount = 0;
    }

    async setup() {
        console.log("Setting up FastLED...");
        try {
            const setupResult = this.module._extern_setup();
            // Handle both sync and async returns from asyncify
            if (setupResult instanceof Promise) {
                await setupResult;
            }
            console.log("FastLED setup completed");
        } catch (error) {
            console.error("FastLED setup failed:", error);
            throw error;
        }
    }

    async loop() {
        if (!this.running) return;
        
        try {
            const loopResult = this.module._extern_loop();
            
            // Handle asyncify promise returns
            if (loopResult instanceof Promise) {
                await loopResult;
            }
            
            this.frameCount++;
            
            // Schedule next frame using requestAnimationFrame for smooth animation
            requestAnimationFrame(() => this.loop());
            
        } catch (error) {
            console.error("FastLED loop error:", error);
            this.stop();
        }
    }

    start() {
        if (this.running) return;
        this.running = true;
        this.loop();
    }

    stop() {
        this.running = false;
    }
}
```

### 3. Handle delay() Function Behavior

**Key Understanding**: With asyncify enabled, `delay()` calls in C++ code become non-blocking:

```javascript
// Before asyncify: delay(500) blocks the entire browser
// After asyncify: delay(500) yields to browser event loop

// Example FastLED sketch that now works smoothly:
/*
void loop() {
    leds[0] = CRGB::Red;
    FastLED.show();
    delay(500);        // Now yields to browser - no blocking!
    leds[0] = CRGB::Blue;
    FastLED.show();  
    delay(500);        // Allows other JS to run during delays
}
*/
```

### 4. Error Handling and Debugging

```javascript
// Enhanced error handling for async operations
class AsyncFastLEDController extends FastLEDController {
    async safeCall(funcName, ...args) {
        try {
            const result = this.module.ccall(
                funcName, 
                'number', 
                ['number'], 
                args, 
                {async: true}
            );
            
            if (result instanceof Promise) {
                return await result;
            }
            return result;
            
        } catch (error) {
            if (error.name === 'ExitStatus') {
                console.error(`FastLED function ${funcName} exited with code:`, error.status);
            } else if (error.message && error.message.includes('unreachable')) {
                console.error(`FastLED function ${funcName} hit unreachable code - possible memory corruption`);
            } else {
                console.error(`FastLED function ${funcName} error:`, error);
            }
            throw error;
        }
    }

    async loop() {
        if (!this.running) return;
        
        try {
            await this.safeCall('_extern_loop');
            this.frameCount++;
            requestAnimationFrame(() => this.loop());
        } catch (error) {
            console.error("Fatal error in FastLED loop:", error);
            this.stop();
            // Optionally attempt recovery or notify user
        }
    }
}
```

### 5. Performance Monitoring and Optimization

```javascript
class PerformanceAwareFastLEDController extends AsyncFastLEDController {
    constructor(module) {
        super(module);
        this.frameTimes = [];
        this.maxFrameTimeHistory = 60; // Track last 60 frames
    }

    async loop() {
        if (!this.running) return;
        
        const startTime = performance.now();
        
        try {
            await this.safeCall('_extern_loop');
            
            const endTime = performance.now();
            const frameTime = endTime - startTime;
            
            // Track performance
            this.frameTimes.push(frameTime);
            if (this.frameTimes.length > this.maxFrameTimeHistory) {
                this.frameTimes.shift();
            }
            
            // Log performance warnings for slow frames
            if (frameTime > 32) { // Slower than ~30 FPS
                console.warn(`Slow FastLED frame: ${frameTime.toFixed(2)}ms`);
            }
            
            this.frameCount++;
            
            // Adaptive frame rate based on performance
            const avgFrameTime = this.frameTimes.reduce((a, b) => a + b, 0) / this.frameTimes.length;
            if (avgFrameTime > 16) {
                // If we can't maintain 60fps, throttle to 30fps
                setTimeout(() => requestAnimationFrame(() => this.loop()), 16);
            } else {
                requestAnimationFrame(() => this.loop());
            }
            
        } catch (error) {
            console.error("Fatal error in FastLED loop:", error);
            this.stop();
        }
    }

    getAverageFrameTime() {
        if (this.frameTimes.length === 0) return 0;
        return this.frameTimes.reduce((a, b) => a + b, 0) / this.frameTimes.length;
    }

    getFPS() {
        const avgFrameTime = this.getAverageFrameTime();
        return avgFrameTime > 0 ? 1000 / avgFrameTime : 0;
    }
}
```

### 6. Complete Integration Example

```javascript
// Complete example showing how to integrate asyncify-enabled FastLED
async function initializeFastLED() {
    try {
        // Load the FastLED WASM module
        const fastledModule = await import('./fastled.js');
        const Module = await fastledModule.default();
        
        // Create controller
        const controller = new PerformanceAwareFastLEDController(Module);
        
        // Setup FastLED
        await controller.setup();
        
        // Start the async loop
        controller.start();
        
        // Add UI controls
        document.getElementById('start-btn').onclick = () => controller.start();
        document.getElementById('stop-btn').onclick = () => controller.stop();
        
        // Performance monitoring
        setInterval(() => {
            const fps = controller.getFPS();
            document.getElementById('fps-display').textContent = `FPS: ${fps.toFixed(1)}`;
        }, 1000);
        
        console.log("FastLED initialized successfully with asyncify support");
        
    } catch (error) {
        console.error("Failed to initialize FastLED:", error);
        // Show user-friendly error message
        document.getElementById('error-display').textContent = 
            "Failed to load FastLED. Please refresh the page.";
    }
}

// Initialize when page loads
document.addEventListener('DOMContentLoaded', initializeFastLED);
```

## Migration Guide

### For Existing FastLED Web Projects

1. **Update compilation flags** (✅ Already Done): The asyncify flags are now included in the centralized configuration.

2. **Replace synchronous loop with async loop**:
   ```diff
   - setInterval(() => Module._extern_loop(), 16);
   + const controller = new AsyncFastLEDController(Module);
   + await controller.setup();
   + controller.start();
   ```

3. **Handle promise returns**:
   ```diff
   - Module._extern_setup();
   + const result = Module._extern_setup();
   + if (result instanceof Promise) await result;
   ```

4. **Update error handling** to catch async errors and provide better user feedback.

## Testing Strategy

### Unit Tests for Async Integration
```javascript
// Test that async functions return promises when needed
describe('FastLED Async Integration', () => {
    test('setup function handles both sync and async returns', async () => {
        const mockModule = {
            _extern_setup: jest.fn().mockReturnValue(Promise.resolve())
        };
        
        const controller = new AsyncFastLEDController(mockModule);
        await expect(controller.setup()).resolves.toBeUndefined();
    });

    test('loop continues after async delays', async () => {
        // Test that delay() calls don't break the loop
        const mockModule = {
            _extern_loop: jest.fn()
                .mockReturnValueOnce(Promise.resolve())
                .mockReturnValueOnce(42) // sync return
        };
        
        const controller = new AsyncFastLEDController(mockModule);
        controller.start();
        
        // Verify loop handles both async and sync returns
        await new Promise(resolve => setTimeout(resolve, 100));
        expect(mockModule._extern_loop).toHaveBeenCalled();
    });
});
```

## Benefits of Asyncify Integration

1. **Non-blocking delays**: `delay()` calls in FastLED sketches no longer freeze the browser
2. **Smooth animations**: Browser can handle other tasks during FastLED delays
3. **Better user experience**: UI remains responsive during LED operations
4. **Event handling**: Mouse/keyboard events work normally even during LED delays
5. **Performance monitoring**: Can track frame times and optimize performance

## Potential Issues and Solutions

### Issue: Stack Overflow with Complex Sketches
**Solution**: Increase asyncify stack size in compilation flags:
```toml
"-sASYNCIFY_STACK_SIZE=32768"  # Increase from 16KB to 32KB
```

### Issue: Functions Not Properly Asyncified
**Solution**: Add more functions to `ASYNCIFY_EXPORTS`:
```toml
"-sASYNCIFY_EXPORTS=['_extern_setup','_extern_loop','_custom_function']"
```

### Issue: Performance Degradation
**Solution**: Use performance monitoring and adaptive frame rates as shown in the examples above.

## Next Steps

1. Implement the `AsyncFastLEDController` class in your FastLED web project
2. Update existing `setInterval` loops to use the new async pattern  
3. Add performance monitoring and error handling
4. Test with complex FastLED sketches that use `delay()`
5. Consider adding UI controls for start/stop/performance monitoring

The asyncify integration enables FastLED sketches to work seamlessly in web browsers without blocking the main thread, providing a much better user experience for LED control applications. 
