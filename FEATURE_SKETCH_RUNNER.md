# Arduino Sketch Runner Feature

## Overview

The FastLED Sketch Runner enables execution of Arduino sketches (.ino files) in a test environment through the STUB platform. This allows testing and development of FastLED sketches without requiring physical hardware.

Arduino sketches follow a fundamental programming model where they export exactly two functions:
- `setup()` - Called once during initialization
- `loop()` - Called repeatedly in the main execution loop

## Entry Point: test.py

The sketch runner is accessible through the FastLED test system:

**Execution Flow:**
1. `test.py` → `ci/util/test_runner.py` → `ci/compiler/cpp_test_run.py`
2. Test compilation system compiles and links sketch with `src/fl/stub_main.cpp`
3. Generated executable runs the sketch with proper setup()/loop() pattern

## Core Sketch Runner Implementation

### FastLED-Exported Sketch Runner (Optimized Architecture)

**New Design**: Self-contained DLL interface with conditional implementations:

**Complete DLL Interface** (`src/fl/dll.h`):
```cpp
#pragma once

/// @file dll.h
/// FastLED dynamic library interface - lightweight header for external callers

#include "export.h"

#ifdef __cplusplus
extern "C" {
#endif

/// Call the sketch's setup() function
/// @note This is the C ABI export for external sketch runners
FASTLED_EXPORT void sketch_setup(void);

/// Call the sketch's loop() function  
/// @note This is the C ABI export for external sketch runners
FASTLED_EXPORT void sketch_loop(void);

#ifdef __cplusplus
}
#endif

// ================================================================================================
// IMPLEMENTATIONS (when building FastLED as shared library)
// ================================================================================================

#ifdef FASTLED_LIBRARY_SHARED

#ifdef __cplusplus
// Forward declarations - provided by sketch
extern void setup();
extern void loop();

// Provide implementations for the exported functions
FASTLED_EXPORT void sketch_setup() { 
    setup(); 
}

FASTLED_EXPORT void sketch_loop() { 
    loop(); 
}
#endif // __cplusplus

#endif // FASTLED_LIBRARY_SHARED
```

**Complete Export Macro Definition** (`src/fl/export.h`):
```cpp
#pragma once

/// @file export.h
/// Cross-platform export macros for FastLED dynamic library support

#ifndef FASTLED_EXPORT
    #if defined(__EMSCRIPTEN__)
        #include <emscripten.h>
        #define FASTLED_EXPORT EMSCRIPTEN_KEEPALIVE
    #elif defined(_WIN32) || defined(_WIN64)
        // Windows DLL export/import
        #ifdef FASTLED_BUILDING_DLL
            #define FASTLED_EXPORT __declspec(dllexport)
        #else
            #define FASTLED_EXPORT __declspec(dllimport)
        #endif
    #elif defined(__GNUC__) && (__GNUC__ >= 4)
        // GCC/Clang visibility attributes
        #define FASTLED_EXPORT __attribute__((visibility("default")))
    #elif defined(__SUNPRO_CC) && (__SUNPRO_CC >= 0x550)
        // Sun Studio visibility attributes
        #define FASTLED_EXPORT __global
    #else
        // Fallback for other platforms
        #define FASTLED_EXPORT
    #endif
#endif

#ifndef FASTLED_CALL
    #if defined(_WIN32) || defined(_WIN64)
        // Windows calling convention
        #define FASTLED_CALL __stdcall
    #else
        // Unix-like platforms - no special calling convention
        #define FASTLED_CALL
    #endif
#endif

/// Combined export and calling convention macro
#define FASTLED_API FASTLED_EXPORT FASTLED_CALL
```

**File Organization:**
- **`fl/export.h`** - Cross-platform export macros only (`FASTLED_EXPORT`, `FASTLED_CALL`, `FASTLED_API`)
- **`fl/dll.h`** - Complete DLL interface (declarations + conditional implementations)
- **FastLED.h** - Optionally includes `fl/dll.h` for internal integration

**Usage Patterns:**
```cpp
// External C/C++ code (minimal dependencies)
#include "fl/dll.h"  // Gets everything needed (~90 lines total)

// WASM platform files (just the macro)
#include "fl/export.h"  // Gets FASTLED_EXPORT only (~40 lines)

// FastLED internal code (optional)
#include "fl/dll.h"  // For access to sketch runner interface
```

**Key Architecture Benefits:**
- **Self-contained**: `fl/dll.h` has both declarations AND implementations
- **No FastLED.h needed**: External callers use tiny `fl/dll.h` only
- **Clean separation**: Export macros isolated in `fl/export.h`
- **Conditional compilation**: Implementations only active with `FASTLED_LIBRARY_SHARED`

**Sketch Simplification** - No boilerplate needed:
```cpp
// Sketch only needs traditional Arduino functions
void setup() {
    FastLED.addLeds<WS2812, 2, GRB>(leds, NUM_LEDS);
}

void loop() {
    leds[0] = CRGB::Red;
    FastLED.show();
    delay(1000);
}
// No extern "C" wrapper needed!
```

**Key Benefits:**
- **Zero linking step**: Sketch loads pre-built FastLED .so/.dll directly
- **No sketch boilerplate**: Just standard Arduino `setup()`/`loop()`
- **Sub-millisecond loading**: No compilation or linking required
- **Cross-platform exports**: `FASTLED_EXPORT` handles all platforms
- **Single source of truth**: FastLED controls the ABI boundary

### **Workflow Comparison**

**Old Approach (Linking Required):**
```bash
# 1. Compile sketch with wrappers
clang++ -c sketch.cpp -o sketch.o           # ~0.05s

# 2. Link against static library  
clang++ sketch.o libfastled.a -o sketch.so  # ~0.2-0.5s
                                             # Total: ~0.25-0.55s
```

**New Approach (Direct Loading):**
```bash
# 1. FastLED .so pre-built once (done by FastLED distribution)
# libfastled.so already contains sketch_setup()/sketch_loop() exports

# 2. Compile sketch only (no linking)
clang++ -shared -fPIC sketch.cpp -o sketch.so  # ~0.02-0.05s

# 3. Runtime: load both libraries
dlopen("libfastled.so")                     # ~0.001s
dlopen("sketch.so")                         # ~0.001s
sketch_setup()  # Calls sketch's setup()   # immediate
                                            # Total: ~0.025s
```

### **Runtime Symbol Resolution**

```cpp
// At runtime, FastLED's exported functions automatically bind:
FASTLED_EXPORT void sketch_setup() {
    setup();  // Resolves to sketch.so's setup() symbol
}

FASTLED_EXPORT void sketch_loop() {
    loop();   // Resolves to sketch.so's loop() symbol
}

// External runners call:
sketch_setup();  // → FastLED function → sketch's setup()
sketch_loop();   // → FastLED function → sketch's loop()
```

### Legacy STUB Implementation (`src/fl/stub_main.cpp`)

The legacy implementation provides direct `.ino` file inclusion:

```cpp
// src/fl/stub_main.cpp
#if defined(FASTLED_STUB_MAIN) || defined(FASTLED_STUB_MAIN_INCLUDE_INO)

#ifdef FASTLED_STUB_MAIN_INCLUDE_INO
// Correctly include the file by expanding and stringifying the macro value
#include _FASTLED_STRINGIFY(FASTLED_STUB_MAIN_INCLUDE_INO)
#else
void setup() {}
void loop() {}
#endif // FASTLED_STUB_MAIN_INCLUDE_INO

int main() {
    // Super simple main function that just calls the setup and loop functions.
    setup();
    while (1) {
        loop();
    }
}
#endif
```

**Key Features:**
- **Macro-based .ino inclusion**: Uses `FASTLED_STUB_MAIN_INCLUDE_INO` to include sketch files
- **Direct Arduino pattern**: Calls `setup()` and `loop()` directly
- **Build system integration**: Activated via `FASTLED_STUB_MAIN` compilation flag

### Alternative STUB Header (`src/platforms/stub_main.hpp`)

Simpler version for direct inclusion:

```cpp
// src/platforms/stub_main.hpp
#pragma once

extern void setup();
extern void loop();

int main() {
    // Super simple main function that just calls the setup and loop functions.
    setup();
    while (1) {
        loop();
    }
}
```

## Core Export Pattern

### Traditional Arduino Pattern

Arduino sketches provide these standard functions:

```cpp
void setup() {
    // One-time initialization code
    // Configure FastLED, pins, etc.
}

void loop() {
    // Main animation/logic code
    // Called repeatedly at runtime
}
```

### Sketch Runner Export Pattern

For sketch runner integration, sketches export C-style wrapper functions:

```cpp
// {SKETCH_CONTENT}

// Export functions for sketch runner
extern "C" {
    void sketch_setup() {
        setup();
    }
    
    void sketch_loop() {
        loop();
    }
}
```

**Key Features:**
- **C Linkage**: `extern "C"` provides stable ABI for cross-language interfacing
- **Wrapper Functions**: `sketch_setup()` and `sketch_loop()` wrap traditional Arduino functions
- **API Boundary**: Clear interface for sketch runner systems to call into sketches
- **Compatibility**: Maintains traditional Arduino patterns while adding runner support

## Build System Integration

### Compilation Process
1. **Test Discovery**: `cpp_test_run.py` discovers test files and sketches
2. **CMake Integration**: CMake build system compiles with STUB platform
3. **Linking**: Links user sketch with `stub_main.cpp` to create executable
4. **Execution**: Generated executable runs the sketch in test environment

### Build Flags
- `FASTLED_STUB_MAIN`: Enables stub main compilation
- `FASTLED_STUB_MAIN_INCLUDE_INO`: Specifies .ino file to include
- Platform automatically set to STUB for test environment

## Performance Comparison

### Loading Speed Analysis

| Architecture | Setup Time | Notes |
|-------------|------------|--------|
| **FastLED-Exported** | ~0.001-0.01s | Pre-built .so/.dll, just symbol resolution |
| Legacy Linking | ~0.1-0.5s | Must link sketch + libfastled.a |
| Full Compilation | ~2-10s | Compile + link from source |

### **FastLED Integration**

FastLED.h simply includes the DLL interface (optional - for internal use):

```cpp
// Line 910: FASTLED_NAMESPACE_END

#include "fl/dll.h"  // Optional: include DLL interface

// Line 912: #endif
```

**Note**: FastLED.h inclusion is **optional** since `fl/dll.h` is self-contained. The `FASTLED_LIBRARY_SHARED` flag in `dll.h` automatically provides implementations when building FastLED as a shared library.

**Build Flags Required:**
- `FASTLED_LIBRARY_SHARED` - Enables sketch runner exports
- `FASTLED_BUILDING_DLL` - Sets export vs import mode (Windows)

### **WASM Platform Unification**

The WASM platform files currently use `EMSCRIPTEN_KEEPALIVE` directly and need to be updated to use `FASTLED_EXPORT`:

### **Analysis of Current WASM Export Usage**

**Files requiring updates with specific function analysis:**

**1. `src/platforms/wasm/ui.cpp` (1 export)**
- Current includes: `#include <emscripten.h>`, `#include <emscripten/emscripten.h>`, `#include <emscripten/html5.h>`
- Current export: `EMSCRIPTEN_KEEPALIVE void jsUpdateUiComponents(const char* jsonStr)`
- Pattern: Simple C function wrapper for UI updates

**2. `src/platforms/wasm/timer.cpp` (3 exports)**
- Current includes: `#include <emscripten.h>`, `#include <emscripten/html5.h>` 
- Current exports:
  - `EMSCRIPTEN_KEEPALIVE uint32_t millis()`
  - `EMSCRIPTEN_KEEPALIVE uint32_t micros()`
  - `EMSCRIPTEN_KEEPALIVE void yield()`
- Pattern: Arduino-compatible timing functions with `extern "C"` linkage

**3. `src/platforms/wasm/js_fetch.cpp` (2 exports)**
- Current includes: `#include <emscripten.h>`, `#include <emscripten/val.h>`
- Current exports:
  - `extern "C" EMSCRIPTEN_KEEPALIVE void js_fetch_success_callback(uint32_t request_id, const char* content)`
  - `extern "C" EMSCRIPTEN_KEEPALIVE void js_fetch_error_callback(uint32_t request_id, const char* error_message)`
- Pattern: Callback functions for JavaScript fetch API

**4. `src/platforms/wasm/js_bindings.cpp` (12+ exports)**
- Current includes: `#include <emscripten.h>`, `#include <emscripten/emscripten.h>`, `#include <emscripten/html5.h>`
- Current exports include:
  - `EMSCRIPTEN_KEEPALIVE void* getFrameData(int* dataSize)`
  - `EMSCRIPTEN_KEEPALIVE void* getScreenMapData(int* dataSize)`
  - `EMSCRIPTEN_KEEPALIVE void freeFrameData(void* data)`
  - `EMSCRIPTEN_KEEPALIVE uint32_t getFrameVersion()`
  - `EMSCRIPTEN_KEEPALIVE bool hasNewFrameData(uint32_t lastKnownVersion)`
  - `EMSCRIPTEN_KEEPALIVE void processUiInput(const char* jsonInput)`
  - `EMSCRIPTEN_KEEPALIVE void* getStripUpdateData(int stripId, int* dataSize)`
  - `EMSCRIPTEN_KEEPALIVE void notifyStripAdded(int stripId, int numLeds)`
  - `EMSCRIPTEN_KEEPALIVE void* getUiUpdateData(int* dataSize)`
  - And more...
- Pattern: Data export functions for JavaScript ↔ C++ communication

**5. `src/platforms/wasm/fs_wasm.cpp` (4 exports)**
- Current includes: None for EMSCRIPTEN_KEEPALIVE (uses EMSCRIPTEN_BINDINGS)
- Current exports:
  - `EMSCRIPTEN_KEEPALIVE bool jsInjectFile(const char *path, const uint8_t *data, size_t len)`
  - `EMSCRIPTEN_KEEPALIVE bool jsAppendFile(const char *path, const uint8_t *data, size_t len)`
  - `EMSCRIPTEN_KEEPALIVE bool jsDeclareFile(const char *path, size_t len)`
  - `EMSCRIPTEN_KEEPALIVE void fastled_declare_files(const char* jsonStr)`
- Pattern: File system operations with `extern "C"` linkage

**6. `src/platforms/wasm/entry_point.cpp` (2 exports)**
- Current includes: `#include <emscripten.h>`, `#include <emscripten/emscripten.h>`, `#include <emscripten/html5.h>`
- Current exports:
  - `EMSCRIPTEN_KEEPALIVE int extern_setup()`
  - `EMSCRIPTEN_KEEPALIVE int extern_loop()`
- Pattern: Arduino sketch bridge functions with `extern "C"` linkage

### **Required Changes Pattern**

**Complete transformation needed for each file:**

```cpp
// Before (current pattern)
#include <emscripten.h>
#include <emscripten/emscripten.h>  // Remove
#include <emscripten/html5.h>       // Keep if needed for other Emscripten APIs

extern "C" {
EMSCRIPTEN_KEEPALIVE void functionName() { ... }
}

// After (unified pattern)
#include "fl/export.h"  // Gets FASTLED_EXPORT macro (replaces EMSCRIPTEN_KEEPALIVE)
#include <emscripten/html5.h>  // Keep if needed for emscripten_get_now(), etc.

extern "C" {
FASTLED_EXPORT void functionName() { ... }
}
```

**Key considerations:**
- **Remove**: `#include <emscripten.h>` and `#include <emscripten/emscripten.h>` 
- **Keep**: `#include <emscripten/html5.h>` if file uses `emscripten_get_now()` or other HTML5 APIs
- **Add**: `#include "fl/export.h"` for unified `FASTLED_EXPORT` macro
- **Replace**: All `EMSCRIPTEN_KEEPALIVE` → `FASTLED_EXPORT`
- **Preserve**: All `extern "C"` linkage specifications
- **Test**: WASM compilation after changes to ensure no breakage

**Note**: WASM platform files only need `fl/export.h` for the macro, not `fl/dll.h`.

### **Summary of WASM Platform Changes Required**

**Total Functions to Update:** 22+ EMSCRIPTEN_KEEPALIVE functions across 6 files
**Estimated Impact:** Low-risk changes (macro replacement only)
**Testing Required:** WASM compilation validation for each file

**File-by-file effort estimation:**
- `ui.cpp`: Simple (1 function)
- `timer.cpp`: Simple (3 functions)  
- `js_fetch.cpp`: Simple (2 functions)
- `entry_point.cpp`: Simple (2 functions)
- `fs_wasm.cpp`: Simple (4 functions)
- `js_bindings.cpp`: Complex (12+ functions, largest file)

**Next Steps:**
1. **Create and test** `src/fl/export.h` with complete macro definitions
2. **Update each file** following the transformation pattern
3. **Test WASM compilation** after each file update
4. **Verify JavaScript bindings** still work correctly
5. **Update documentation** to reflect unified export system

**Benefits:**
- **Unified export system** across all platforms
- **Consistent API** regardless of target platform  
- **Easier maintenance** with single export macro
- **Future-proof** for new platforms and export scenarios
- **Cross-platform compatibility** for future non-WASM targets

## Platform Comparison

| Platform | Use Case | Execution Model | Loading Method |
|----------|----------|----------------|----------------|
| **FastLED-Exported** | Production/Development | Dynamic library loading | Pre-built .so/.dll |
| STUB (Test) | Internal testing | Synchronous blocking loop | Static linking |
| WASM (Browser) | Web deployment | Async JavaScript-controlled | WASM module loading |
| Arduino (Hardware) | Embedded production | Hardware loop | Flash memory |

## WASM Platform (Alternative Runner)

For browser execution, the WASM platform provides sophisticated async support:

```cpp
// src/platforms/wasm/entry_point.cpp
void fastled_setup_once() {
    // Platform initialization + engine setup
    setup(); // Call user's setup function
}

void fastled_loop_once() {
    // Pre-loop engine events
    fl::EngineEvents::onPlatformPreLoop();
    loop(); // Call user's loop function
    // Handle frame completion events
}
```

## Testing Sketch Runner

### Unit Test Framework vs Sketch Runner

**Unit Tests** (`tests/test_*.cpp`):
- Use doctest framework with `TEST_CASE` macros
- Have `main()` provided by `doctest_main.cpp`  
- Test specific FastLED functionality

**Sketch Runner** (`src/fl/stub_main.cpp`):
- Executes actual Arduino sketches (.ino files)
- Provides Arduino-compatible `setup()`/`loop()` environment
- Tests complete sketch behavior

### Example Usage

Testing a simple sketch:
```cpp
// example.ino
#include "FastLED.h"

#define NUM_LEDS 100
CRGB leds[NUM_LEDS];

void setup() {
    FastLED.addLeds<WS2812, 2, GRB>(leds, NUM_LEDS);
}

void loop() {
    leds[0] = CRGB::Red;
    FastLED.show();
    delay(1000);
}
```

Compilation with sketch runner:
```bash
# Compiled via test.py → cpp_test_run.py with STUB platform
# Links with src/fl/stub_main.cpp 
# Includes example.ino via FASTLED_STUB_MAIN_INCLUDE_INO
```

## Practical Implementation Example

### **Python Sketch Runner Using FastLED Exports**

**C Integration** (if needed):
```c
// C code includes only lightweight header
#include "fl/dll.h"  // ~50 lines, no FastLED dependencies

int main() {
    sketch_setup();  // Calls sketch's setup()
    while (1) {
        sketch_loop();  // Calls sketch's loop()  
    }
    return 0;
}
```

**Python Integration** (no headers needed):
```python
import ctypes
import time

# Load pre-built FastLED library with exports
fastled = ctypes.CDLL("./libfastled.so")  # or "fastled.dll" on Windows
sketch = ctypes.CDLL("./sketch.so")

# FastLED exported functions (C ABI) - no headers needed!
sketch_setup = fastled.sketch_setup
sketch_loop = fastled.sketch_loop

# Set return types for safety
sketch_setup.restype = None
sketch_loop.restype = None

# Run sketch
sketch_setup()  # Calls sketch's setup() via FastLED export
while True:
    sketch_loop()  # Calls sketch's loop() via FastLED export
    time.sleep(0.016)  # ~60 FPS
```

### **Build Commands**

```bash
# 1. Build FastLED library once (ships with distribution)
clang++ -shared -fPIC -DFASTLED_LIBRARY_SHARED -DFASTLED_BUILDING_DLL \
    src/FastLED.cpp [other FastLED sources] -o libfastled.so
    # ↑ sketch_setup()/sketch_loop() automatically included via FastLED.h

# 2. Build sketch (no FastLED linking needed)
clang++ -shared -fPIC my_sketch.cpp -o sketch.so

# 3. Run via any language that supports FFI
python sketch_runner.py  # or Go, Rust, JavaScript, etc.
```

## Cross-Platform Considerations

### **Windows**
- Use `LoadLibrary()` and `GetProcAddress()`
- Export with `__declspec(dllexport)` 
- Import with `__declspec(dllimport)`

### **Linux/macOS**
- Use `dlopen()` and `dlsym()`
- Export with `__attribute__((visibility("default")))`
- No import decorations needed

### **WASM/Emscripten**
- Use `EMSCRIPTEN_KEEPALIVE` for exports
- Call via `Module.cwrap()` from JavaScript
- Compatible with existing WASM workflow

## Implementation Notes

- **Zero linking dependency**: Sketches compile independently of FastLED
- **Faster development cycle**: Change sketch, recompile in ~50ms, reload
- **Language agnostic**: Any language with FFI can run FastLED sketches
- **Distribution friendly**: FastLED .so/.dll ships once, works with all sketches
- **Platform abstraction**: Sketches remain standard Arduino code
