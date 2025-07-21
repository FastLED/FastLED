# FastLED WASM Asyncify Integration Task

## Overview

This document outlines the changes necessary to modify the JavaScript integration for FastLED to handle the async control loop using Emscripten's Asyncify feature. The compiler-side asyncify support has already been implemented in `compilation_flags.toml`.

## Current Implementation Status

✅ **Compiler Support**: Asyncify flags have been added to the WASM compilation:
- `-sASYNCIFY=1` - Enables asyncify support
- `-sASYNCIFY_STACK_SIZE=16384` - 16KB stack for async operations  
- `-sASYNCIFY_EXPORTS=['_extern_setup','_extern_loop']` - Main FastLED functions are async-enabled

⚠️ **Known Issue**: The default 16KB asyncify stack size may be insufficient for complex FastLED sketches, causing "RuntimeError: unreachable" errors.

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

## Troubleshooting Current Implementation

### "RuntimeError: unreachable" - Alternative Causes

**Error Message:**
```
fastled.js:73 Uncaught RuntimeError: Aborted(RuntimeError: unreachable). 
"unreachable" may be due to ASYNCIFY_STACK_SIZE not being large enough (try increasing it)
```

**Important Note:** If your ASYNCIFY_STACK_SIZE is already large (e.g., 250MB), the issue is **NOT** stack size. The error message can be misleading.

### 🚨 **Immediate Diagnosis (Run This Now)**

Since your stack size is 250MB, paste this diagnostic code into your browser console to identify the actual problem:

```javascript
// === FastLED WASM Async Diagnostic Tool ===
console.log("🔍 Starting FastLED WASM diagnostic...");

// Check 1: Module state
console.log("📦 Module status:", {
  moduleLoaded: typeof Module !== 'undefined',
  externSetup: typeof Module?._extern_setup,
  externLoop: typeof Module?._extern_loop,
  stackSize: "250MB (confirmed - not the issue)"
});

// Check 2: Controller state  
console.log("🎮 Controller status:", {
  controllerExists: !!window.fastLEDController,
  setupCompleted: window.fastLEDController?.setupCompleted,
  running: window.fastLEDController?.running,
  frameCount: window.fastLEDController?.frameCount
});

// Check 3: Test single function calls
async function testFunctions() {
  if (!Module?._extern_setup) {
    console.error("❌ _extern_setup not found - module loading issue");
    return;
  }
  
  try {
    console.log("🧪 Testing _extern_setup...");
    const setupResult = Module._extern_setup();
    if (setupResult instanceof Promise) {
      console.log("⏳ Setup returned Promise, awaiting...");
      await setupResult;
      console.log("✅ Setup completed successfully");
    } else {
      console.log("✅ Setup completed synchronously");
    }
  } catch (error) {
    console.error("❌ Setup failed:", error);
    console.error("This suggests the issue is in your setup() function");
    return;
  }
  
  try {
    console.log("🧪 Testing single _extern_loop call...");
    const loopResult = Module._extern_loop();
    if (loopResult instanceof Promise) {
      console.log("⏳ Loop returned Promise, awaiting...");
      await loopResult;
      console.log("✅ Single loop call succeeded");
    } else {
      console.log("✅ Single loop call succeeded (sync)");
    }
  } catch (error) {
    console.error("❌ Single loop call failed:", error);
    console.error("This suggests the issue is in your loop() function");
  }
}

// Check 4: Memory state
console.log("💾 Memory status:", {
  wasmMemory: Module?.HEAPU8?.length ? `${(Module.HEAPU8.length / 1024 / 1024).toFixed(1)}MB` : 'Unknown',
  jsMemory: performance.memory ? {
    used: `${(performance.memory.usedJSHeapSize / 1024 / 1024).toFixed(1)}MB`,
    total: `${(performance.memory.totalJSHeapSize / 1024 / 1024).toFixed(1)}MB`
  } : 'Not available'
});

// Run the test
testFunctions().then(() => {
  console.log("🔍 Diagnostic complete. Check results above.");
}).catch(error => {
  console.error("💥 Diagnostic failed:", error);
});
```

**Run this now and share the console output** - it will tell us exactly where the problem is occurring.

### When Stack Size Is NOT The Issue (Stack Size > 32MB)

If your stack size is already very large (250MB), the "unreachable" error is likely caused by:

#### **Root Cause 1: Infinite Loop in Async Code**
**Problem:** The FastLED sketch contains an infinite loop that never yields control back to the browser.

**Debugging:**
```javascript
// Add this to debug infinite loops
let loopCount = 0;
const originalLoop = window.fastLEDController?.moduleInstance?._extern_loop;
if (originalLoop) {
  window.fastLEDController.moduleInstance._extern_loop = function() {
    console.log(`Loop call #${++loopCount}`);
    if (loopCount > 1000) {
      console.error('Possible infinite loop detected!');
      return;
    }
    return originalLoop.call(this);
  };
}
```

**Solutions:**
- Add `yield()` or `delay(1)` calls in tight loops
- Check for `while(true)` loops without delays
- Ensure `loop()` function exits normally

#### **Root Cause 2: Memory Corruption or Invalid Access**
**Problem:** The sketch is accessing invalid memory or causing heap corruption.

**Debugging:**
```cpp
// Add bounds checking in your FastLED sketch
void loop() {
  // Check array bounds before accessing
  if (ledIndex < NUM_LEDS) {
    leds[ledIndex] = CRGB::Red;
  }
  
  // Add debug output
  Serial.print("Loop iteration: ");
  Serial.println(millis());
  
  FastLED.show();
  delay(20);  // Important: yield control
}
```

**Solutions:**
- Check all array access for bounds
- Verify `NUM_LEDS` matches actual LED count
- Use smaller test arrays initially
- Add memory debugging to your sketch

#### **Root Cause 3: Incorrect Asyncify Function Exports**
**Problem:** Functions being called aren't properly marked as async-capable.

**Check Current Exports:**
```javascript
// In browser console, check what functions are asyncified
console.log('Module exports:', Object.keys(Module));
console.log('_extern_setup type:', typeof Module._extern_setup);
console.log('_extern_loop type:', typeof Module._extern_loop);
```

**Expected:** Both should be functions that can return Promises.

#### **Root Cause 4: WebAssembly Module Compilation Issues**
**Problem:** The WASM module itself has compilation errors or corruption.

**Solutions:**
1. **Recompile with Debug Info:**
   ```bash
   # If using fastled compiler
   fastled --debug examples/wasm
   
   # Or add debug flags to compilation
   -g -O1  # Lower optimization, more debug info
   ```

2. **Test with Minimal Sketch:**
   ```cpp
   #include <FastLED.h>
   
   #define NUM_LEDS 10
   #define DATA_PIN 3
   
   CRGB leds[NUM_LEDS];
   
   void setup() {
     FastLED.addLeds<NEOPIXEL, DATA_PIN>(leds, NUM_LEDS);
   }
   
   void loop() {
     static uint8_t hue = 0;
     leds[0] = CHSV(hue++, 255, 255);
     FastLED.show();
     delay(100);  // Critical: must yield
   }
   ```

### Debugging Steps for Large Stack Size Scenarios

#### **Step 1: Isolate the Problem**
```javascript
// Test if setup() works independently
try {
  await window.fastLEDController.moduleInstance._extern_setup();
  console.log('✅ Setup completed successfully');
} catch (error) {
  console.error('❌ Setup failed:', error);
}

// Test if a single loop() call works
try {
  const result = window.fastLEDController.moduleInstance._extern_loop();
  if (result instanceof Promise) {
    const awaited = await result;
    console.log('✅ Single loop call succeeded:', awaited);
  } else {
    console.log('✅ Single loop call succeeded (sync):', result);
  }
} catch (error) {
  console.error('❌ Single loop call failed:', error);
}
```

#### **Step 2: Enable Verbose Logging**
```javascript
// Add comprehensive logging to AsyncFastLEDController
window.fastLEDController.safeCall = async function(funcName, func, ...args) {
  console.log(`🔄 Calling ${funcName}...`);
  try {
    const startTime = performance.now();
    const result = func(...args);
    
    if (result instanceof Promise) {
      console.log(`⏳ ${funcName} returned Promise, awaiting...`);
      const awaited = await result;
      const duration = performance.now() - startTime;
      console.log(`✅ ${funcName} completed async in ${duration.toFixed(2)}ms:`, awaited);
      return awaited;
    } else {
      const duration = performance.now() - startTime;
      console.log(`✅ ${funcName} completed sync in ${duration.toFixed(2)}ms:`, result);
      return result;
    }
  } catch (error) {
    console.error(`❌ ${funcName} failed:`, error);
    console.error('Error stack:', error.stack);
    throw error;
  }
};
```

#### **Step 3: Check for Memory Leaks**
```javascript
// Monitor memory usage
function monitorMemory() {
  if (performance.memory) {
    const mem = performance.memory;
    console.log('Memory usage:', {
      used: (mem.usedJSHeapSize / 1024 / 1024).toFixed(2) + ' MB',
      total: (mem.totalJSHeapSize / 1024 / 1024).toFixed(2) + ' MB',
      limit: (mem.jsHeapSizeLimit / 1024 / 1024).toFixed(2) + ' MB'
    });
  }
}

// Run every 5 seconds
setInterval(monitorMemory, 5000);
```

### Advanced Debugging: WebAssembly Inspection

If the above steps don't help, the issue might be deeper in the WebAssembly module:

```javascript
// Check if the module is properly loaded
console.log('Module ready:', !!Module);
console.log('Module instance exports:', Module.asm ? Object.keys(Module.asm) : 'No asm exports');

// Check memory state
console.log('WASM Memory size:', Module.HEAPU8?.length || 'Unknown');
console.log('WASM stack pointer:', Module.stackSave?.() || 'Unknown');
```

### When to File a Bug Report

File a bug report with the FastLED WASM compiler if:
1. ✅ Stack size is large (>32MB) 
2. ✅ Minimal sketch still fails
3. ✅ No infinite loops in your code
4. ✅ Memory usage appears normal
5. ✅ Setup() works but loop() fails immediately

**Include in your bug report:**
- Exact FastLED sketch code (minimal reproduction case)
- Browser console output with verbose logging enabled
- WebAssembly module inspection results
- Platform details (OS, browser, FastLED compiler version)
- Confirmation that stack size is large (250MB in your case)

## Next Steps

1. ✅ **JavaScript Integration Complete**: The `AsyncFastLEDController` class has been implemented in `src/platforms/wasm/compiler/index.js`
2. ✅ **Async Loop Pattern**: Updated `FastLED_SetupAndLoop` to use async/await pattern  
3. ✅ **Error Handling**: Added comprehensive error handling for async operations
4. ✅ **Performance Monitoring**: Added FPS tracking and adaptive frame rates
5. ✅ **UI Controls**: Added start/stop/toggle controls in the web interface

**Remaining Tasks:**
1. **Increase ASYNCIFY_STACK_SIZE**: Work with FastLED WASM compiler maintainers to increase default stack size
2. **Test Complex Sketches**: Validate the async implementation with complex FastLED animations
3. **Documentation**: Update FastLED documentation to include async best practices
4. **Performance Optimization**: Fine-tune adaptive frame rate algorithms for better performance

The asyncify integration enables FastLED sketches to work seamlessly in web browsers without blocking the main thread, providing a much better user experience for LED control applications. 
