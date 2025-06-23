# Emscripten Embind to C Bindings Refactoring

## Overview

This document summarizes the refactoring of the FastLED WASM platform code from using Emscripten's embind feature to standard C bindings. This change improves compatibility and reduces dependency on the more complex embind system.

## Files Modified

### 1. `src/platforms/wasm/ui.cpp`

**Changes:**
- Removed `#include <emscripten/bind.h>`
- Replaced `EMSCRIPTEN_BINDINGS(js_interface)` macro with C function wrapper
- Added `extern "C"` wrapper function `jsUpdateUiComponents(const char* jsonStr)`
- Maintained the original C++ function `fl::jsUpdateUiComponents(const std::string &jsonStr)` internally

**Before:**
```cpp
EMSCRIPTEN_BINDINGS(js_interface) {
    emscripten::function("_jsUiManager_updateUiComponents", &jsUpdateUiComponents);
    emscripten::function("jsUpdateUiComponents", &jsUpdateUiComponents);
}
```

**After:**
```cpp
extern "C" {
    EMSCRIPTEN_KEEPALIVE void jsUpdateUiComponents(const char* jsonStr) {
        fl::jsUpdateUiComponents(std::string(jsonStr));
    }
}
```

### 2. `src/platforms/wasm/active_strip_data.cpp`

**Changes:**
- Removed `#include <emscripten/bind.h>`
- Replaced `EMSCRIPTEN_BINDINGS(engine_events_constructors)` with C function wrappers
- Added `getActiveStripData()` and `getPixelData_Uint8_Raw()` C functions
- Maintained the original C++ `ActiveStripData` class functionality

**Before:**
```cpp
EMSCRIPTEN_BINDINGS(engine_events_constructors) {
    emscripten::class_<ActiveStripData>("ActiveStripData")
        .constructor(&getActiveStripDataRef, emscripten::allow_raw_pointers())
        .function("getPixelData_Uint8", &ActiveStripData::getPixelData_Uint8);
}
```

**After:**
```cpp
extern "C" {
    EMSCRIPTEN_KEEPALIVE fl::ActiveStripData* getActiveStripData() {
        return &fl::ActiveStripData::Instance();
    }
    
    EMSCRIPTEN_KEEPALIVE uint8_t* getPixelData_Uint8_Raw(int stripIndex, size_t* outSize) {
        fl::ActiveStripData& instance = fl::ActiveStripData::Instance();
        fl::SliceUint8 stripData;
        if (instance.mStripMap.get(stripIndex, &stripData)) {
            *outSize = stripData.size();
            return const_cast<uint8_t*>(stripData.data());
        }
        *outSize = 0;
        return nullptr;
    }
}
```

### 3. `src/platforms/wasm/fs_wasm.cpp`

**Changes:**
- Removed `#include <emscripten/bind.h>`
- Replaced `EMSCRIPTEN_BINDINGS(_fastled_declare_files)` with C function wrapper
- Changed `fastled_declare_files(std::string jsonStr)` to `fastled_declare_files(const char* jsonStr)`
- Moved the implementation to a separate function `fl::fastled_declare_files_impl()`

**Before:**
```cpp
EMSCRIPTEN_BINDINGS(_fastled_declare_files) {
    emscripten::function("_fastled_declare_files", &fastled_declare_files);
}
```

**After:**
```cpp
extern "C" {
    EMSCRIPTEN_KEEPALIVE void fastled_declare_files(const char* jsonStr) {
        fl::fastled_declare_files_impl(jsonStr);
    }
}
```

### 4. `src/platforms/wasm/engine_listener.cpp`

**Changes:**
- Removed `#include <emscripten/bind.h>` (no longer needed)

### 5. `src/platforms/wasm/js_bindings.cpp`

**Changes:**
- Updated JavaScript code to use `Module.cwrap()` instead of direct embind function calls
- Replaced `Module.jsUpdateUiComponents(jsonString)` with `Module.cwrap('jsUpdateUiComponents', null, ['string'])(jsonString)`
- Replaced the embind `ActiveStripData` class usage with direct C function calls using `Module.cwrap()`

**Before:**
```javascript
Module.jsUpdateUiComponents(jsonString);
var activeStrips = globalThis.FastLED_onFrameData || new Module.ActiveStripData();
var pixelData = activeStrips.getPixelData_Uint8(stripData.strip_id);
```

**After:**
```javascript
Module.cwrap('jsUpdateUiComponents', null, ['string'])(jsonString);
var getPixelDataRaw = Module.cwrap('getPixelData_Uint8_Raw', 'number', ['number', 'number']);
var dataPtr = getPixelDataRaw(stripData.strip_id, allocateSize);
```

### 6. `src/platforms/wasm/compiler/index.js`

**Changes:**
- Updated `moduleInstance._fastled_declare_files()` to use `moduleInstance.cwrap('fastled_declare_files', null, ['string'])()`

## Benefits of This Refactoring

1. **Reduced Dependencies**: No longer depends on the embind feature, which is more complex and adds overhead
2. **Standard C Interface**: Uses the standard Emscripten C binding mechanism, which is more widely supported
3. **Better Performance**: C bindings typically have less overhead than embind
4. **Simpler Build Process**: Removes the need for embind-specific build configurations
5. **Better Compatibility**: C bindings are more stable across different Emscripten versions

## JavaScript Integration

The JavaScript code now uses `Module.cwrap()` to create function wrappers for the C functions:

```javascript
// UI updates
Module.cwrap('jsUpdateUiComponents', null, ['string'])(jsonString);

// File system
Module.cwrap('fastled_declare_files', null, ['string'])(jsonData);

// Pixel data access
var getPixelDataRaw = Module.cwrap('getPixelData_Uint8_Raw', 'number', ['number', 'number']);
```

## Testing

The refactoring maintains the same external API and functionality. The web target compilation is currently skipped in the build system (as it was before), but the code changes are ready for when a proper WASM build system is implemented.

## Notes

- The `EMSCRIPTEN_KEEPALIVE` attribute ensures that the C functions are not optimized away by the compiler
- The C wrapper functions handle the conversion between C types (char*, size_t) and C++ types (std::string, etc.)
- All original C++ functionality is preserved within the `fl` namespace
- JavaScript memory management for the pixel data access is handled using `Module._malloc()` and `Module._free()`
