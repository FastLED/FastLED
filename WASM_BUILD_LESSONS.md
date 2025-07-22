# WASM Build Lessons - Cursor Rules Updates

## Critical WASM Build Issues and Solutions

### 1. Unified Build Conflicts (FASTLED_ALL_SRC=1)

**🚨 CRITICAL: WASM builds use unified compilation when `FASTLED_ALL_SRC=1` is enabled (automatic for Clang builds)**

**Problem**: Multiple .cpp files are compiled together in a single compilation unit, causing:
- Duplicate function definitions
- Type signature conflicts
- Symbol redefinition errors

**Key Lessons**:
- ✅ **Use `EMSCRIPTEN_KEEPALIVE` as canonical implementations** - These should be the primary/authoritative versions
- ✅ **Remove duplicate functions from other files** - Comment out or remove conflicting implementations  
- ✅ **Consistent external C function signatures** - Must match Emscripten headers exactly
- ❌ **Never have multiple definitions of the same function** across WASM platform files

**Example Fix Pattern**:
```cpp
// In timer.cpp (CANONICAL)
extern "C" {
EMSCRIPTEN_KEEPALIVE uint32_t millis() {
    // Implementation
}
}

// In js.cpp (FIXED)
extern "C" {
// NOTE: millis() and micros() functions are defined in timer.cpp with EMSCRIPTEN_KEEPALIVE  
// to avoid duplicate definitions in unified builds
}
```

### 2. Emscripten Function Signature Matching

**🚨 CRITICAL: External C function declarations must match Emscripten headers exactly**

**Problem**: Type mismatches cause compilation errors:
```
error: conflicting types for 'emscripten_sleep'
extern "C" void emscripten_sleep(int ms);           // ❌ WRONG
/emsdk/.../emscripten.h:194:6: note: previous declaration is here
void emscripten_sleep(unsigned int ms);             // ✅ CORRECT
```

**Solution**: Always match the official Emscripten signature:
```cpp
// ✅ CORRECT - matches official Emscripten header
#ifdef __EMSCRIPTEN__
extern "C" void emscripten_sleep(unsigned int ms);
#endif
```

### 3. WASM Async Platform Pump Pattern

**🚨 CRITICAL: Use FastLED's async infrastructure instead of blocking sleep**

**Problem**: Long blocking sleeps prevent async task processing:
```cpp
// ❌ BAD - blocks async processing
while (true) {
    emscripten_sleep(100); // 100ms blocking sleep
}
```

**Solution**: Use FastLED's async pump with short yields:
```cpp
// ✅ GOOD - processes async tasks with responsive yielding
#include "fl/async.h"

while (true) {
    fl::asyncrun();         // Process all async tasks
    emscripten_sleep(1);    // 1ms yield for responsiveness
}
```

**Why This Matters**:
- FastLED has comprehensive async infrastructure (`fl::AsyncManager`, `fl::asyncrun()`)
- HTTP fetch, timers, and other async operations depend on regular pumping
- Short sleep intervals (1ms) maintain responsiveness while allowing other threads to work
- Integrates with existing engine events and promise-based APIs

### 4. WASM Platform File Organization

**Best Practices for WASM platform files**:

- ✅ **Use `timer.cpp` for canonical timing functions** with `EMSCRIPTEN_KEEPALIVE`
- ✅ **Use `entry_point.cpp` for main() and setup/loop coordination** with async pumping
- ✅ **Use `js.cpp` for JavaScript utility functions** without duplicating timer functions
- ✅ **Include proper async infrastructure** (`fl/async.h`) in entry points
- ✅ **Comment when removing duplicate implementations** to explain unified build conflicts

### 5. WASM Compilation Testing

**Testing Strategy**:
```bash
# Test WASM compilation after changes
uv run ci/wasm_compile.py -b examples/wasm

# Quick test without full build
uv run ci/wasm_compile.py examples/wasm --quick
```

**Common Error Patterns to Watch For**:
- `error: conflicting types for 'function_name'`
- `error: redefinition of 'function_name'`  
- `warning: attribute declaration must precede definition`
- `RuntimeError: unreachable` (often async-related)

## Updated Cursor Rules Section

Add this to the **🚨 CRITICAL REQUIREMENTS FOR ALL AGENTS** section:

### WASM Platform Specific Rules

**🚨 WASM UNIFIED BUILD AWARENESS:**
- **ALWAYS check for unified builds** when modifying WASM platform files
- **NEVER create duplicate function definitions** across WASM .cpp files
- **USE `EMSCRIPTEN_KEEPALIVE` functions as canonical implementations**
- **MATCH Emscripten header signatures exactly** for external C functions
- **REMOVE conflicting implementations** and add explanatory comments

**🚨 WASM ASYNC PLATFORM PUMP PATTERN:**
- **ALWAYS use `fl::asyncrun()` for async processing** instead of long blocking sleeps
- **INCLUDE `fl/async.h`** in WASM entry points
- **USE 1ms sleep intervals** for responsive yielding in main loops
- **INTEGRATE with FastLED's existing async infrastructure** rather than custom solutions

**🚨 WASM TESTING REQUIREMENTS:**
- **ALWAYS test WASM compilation** after platform file changes
- **USE `uv run ci/wasm_compile.py` for validation**
- **WATCH for unified build conflicts** in compilation output
- **VERIFY async operations work properly** in browser environment 
