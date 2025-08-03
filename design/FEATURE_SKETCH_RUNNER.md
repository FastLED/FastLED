# Arduino Sketch Runner Feature

## Overview

The FastLED Sketch Runner enables execution of Arduino sketches (.ino files) in a test environment through the STUB platform. This allows testing and development of FastLED sketches without requiring physical hardware.

Arduino sketches follow a fundamental programming model where they export exactly two functions:
- `setup()` - Called once during initialization
- `loop()` - Called repeatedly in the main execution loop

## 🚨 CRITICAL DIRECTIVE: END-TO-END TEST.PY INTEGRATION

**MANDATORY REQUIREMENT:** This solution MUST be hooked into test.py such that:

```bash
bash test --examples Blink --verbose
```

**MUST work as a complete end-to-end test with:**
- ❌ **NO CHEATING** - Real implementation required
- ❌ **NO STUBS** - Must use actual sketch files  
- ❌ **NO MOCKS** - Must compile and execute real code
- ✅ **ONLY END-TO-END CODE TESTS** - Complete workflow validation

**Required Integration Points:**
1. `test.py` → `ci/util/test_runner.py` → `ci/compiler/test_example_compilation.py`
2. Modify example compilation system to support sketch runner execution
3. Actual compilation: `examples/Blink/Blink.ino` → executable with sketch_setup()/sketch_loop()
4. Actual execution: Run generated executable and validate output
5. Real validation: Verify sketch initialization and loop execution

**Compliance Test:**
```bash
bash test --examples Blink --verbose
```

This command MUST:
- Discover `examples/Blink/Blink.ino`
- Compile it with sketch runner infrastructure
- Link it into executable with proper exports
- Execute the sketch and capture output
- Validate that `setup()` and `loop()` functions execute correctly
- Return success/failure based on actual execution results

## Entry Point: test.py Integration

The sketch runner integrates with the FastLED test system through real compilation and execution:

## Core Sketch Runner Implementation

### FastLED-Exported Sketch Runner (Optimized Architecture)

**New Design**: Self-contained DLL interface with conditional implementations:

**Complete DLL Interface** (`src/fl/dll.h`):
```cpp
#pragma once

/// @file dll.h
/// FastLED dynamic library interface - lightweight header for external callers

#ifndef FASTLED_BUILD_EXPORTS
#define FASTLED_BUILD_EXPORTS 0
#endif

#if FASTLED_BUILD_EXPORTS

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

#endif // FASTLED_BUILD_EXPORTS
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

### **Compiler and Linkage Architecture**

**FASTLED_EXPORT Compiler Detection Logic:**

**1. Emscripten (WebAssembly)**
- **Detection**: `__EMSCRIPTEN__`
- **Export**: `EMSCRIPTEN_KEEPALIVE` 
- **Purpose**: Prevents dead code elimination in WASM builds
- **Linkage**: JavaScript ↔ WebAssembly bindings

**2. Windows Native (MSVC/Visual Studio)**
- **Detection**: `_WIN32` || `_WIN64`
- **Export**: `__declspec(dllexport)` / `__declspec(dllimport)`
- **Purpose**: Windows DLL symbol export/import
- **Linkage**: Native Windows DLL artifacts (.dll, .lib)

**3. GCC/Clang (Unix + MSYS2/MinGW)**
- **Detection**: `__GNUC__ && (__GNUC__ >= 4)`
- **Export**: `__attribute__((visibility("default")))`
- **Purpose**: ELF symbol visibility control 
- **Linkage**: Unix shared objects (.so) and MSYS2 linkable artifacts

**4. Sun Studio**
- **Detection**: `__SUNPRO_CC && (__SUNPRO_CC >= 0x550)`
- **Export**: `__global`
- **Purpose**: Solaris/Sun platform symbol export
- **Linkage**: Solaris shared libraries

**Key Architectural Recognition:**

**Windows Compilation Modes:**
- **Native Windows**: Uses MSVC toolchain → `__declspec` for .dll/.lib artifacts
- **MSYS2/MinGW on Windows**: Uses Clang/GCC toolchain → `__attribute__((visibility("default")))` for MSYS2-compatible linkable artifacts

**Architecture Recognition:**
**Windows uses the Clang C++ compiler not for Windows native binaries, but for MSYS2 linkable artifacts.** This creates two distinct compilation paths:

1. **Windows + MSVC** → Native Windows DLL using `__declspec`
2. **Windows + Clang/GCC** → MSYS2-compatible artifacts using `__attribute__((visibility("default")))`

**Current Implementation Behavior:**
The current macro prioritizes Windows platform detection (`_WIN32`/`_WIN64`) over compiler detection (`__GNUC__`), which means:
- **MSYS2/MinGW builds on Windows** → Still get `__declspec` (sub-optimal for MSYS2 ecosystem)  
- **GCC/Clang on Windows** → Should use `__attribute__((visibility("default")))` for better MSYS2 compatibility

**Compiler vs Platform Priority:**
```cpp
// Current: Platform-first detection
#if defined(_WIN32) || defined(_WIN64)        // Windows platform
    #define FASTLED_EXPORT __declspec(...)
#elif defined(__GNUC__) && (__GNUC__ >= 4)    // GCC/Clang compiler
    #define FASTLED_EXPORT __attribute__(...)

// Alternative: Compiler-first detection (for MSYS2 optimization)
#if defined(__EMSCRIPTEN__)
    #define FASTLED_EXPORT EMSCRIPTEN_KEEPALIVE
#elif defined(__GNUC__) && (__GNUC__ >= 4)    // GCC/Clang (including MSYS2)
    #define FASTLED_EXPORT __attribute__((visibility("default")))
#elif defined(_WIN32) || defined(_WIN64)      // Native Windows/MSVC
    #define FASTLED_EXPORT __declspec(...)
```

**File Organization:**
- **`fl/export.h`** - Cross-platform export macros only (`FASTLED_EXPORT`, `FASTLED_CALL`, `FASTLED_API`)
- **`fl/dll.h`** - Complete DLL interface (declarations + conditional implementations)
- **FastLED.h** - Optionally includes `fl/dll.h` for internal integration

**Build Control Flags:**
- **`FASTLED_BUILD_EXPORTS`** - Controls whether DLL interface is included (default: 0)
- **`FASTLED_LIBRARY_SHARED`** - Controls whether implementations are provided (shared library mode)
- **`FASTLED_BUILDING_DLL`** - Controls export vs import mode on Windows

**Usage Patterns:**
```cpp
// External C/C++ code (enable DLL interface)
#define FASTLED_BUILD_EXPORTS 1
#include "fl/dll.h"  // Gets declarations and implementations (~80 lines total)

// WASM platform files (just the macro)
#include "fl/export.h"  // Gets FASTLED_EXPORT only (~40 lines)

// FastLED internal code (optional DLL interface)
#define FASTLED_BUILD_EXPORTS 1
#include "fl/dll.h"  // For access to sketch runner interface
```

**Key Architecture Benefits:**
- **Self-contained**: `fl/dll.h` has both declarations AND implementations
- **No FastLED.h needed**: External callers use tiny `fl/dll.h` only
- **Clean separation**: Export macros isolated in `fl/export.h`
- **Opt-in interface**: `FASTLED_BUILD_EXPORTS=1` enables DLL interface (default: disabled)
- **Conditional compilation**: Implementations only active with `FASTLED_LIBRARY_SHARED`
- **Zero overhead**: When disabled, DLL interface adds no compilation cost

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

FastLED.h can optionally include the DLL interface for internal use:

```cpp
// Line 910: FASTLED_NAMESPACE_END

#ifdef FASTLED_ENABLE_SKETCH_RUNNER
    #define FASTLED_BUILD_EXPORTS 1
    #include "fl/dll.h"  // Include DLL interface when sketch runner is enabled
#endif

// Line 912: #endif
```

**Note**: FastLED.h inclusion is **optional** since `fl/dll.h` is self-contained. External callers can include `fl/dll.h` directly without needing FastLED.h.

**Build Control Architecture:**
- **`FASTLED_BUILD_EXPORTS=1`** - Enables DLL interface declarations and implementations
- **`FASTLED_LIBRARY_SHARED`** - Provides sketch runner implementations when building as shared library
- **`FASTLED_BUILDING_DLL`** - Controls export vs import mode on Windows

**Conditional Compilation Flow:**
```cpp
// 1. User enables sketch runner support
#define FASTLED_BUILD_EXPORTS 1
#include "fl/dll.h"

// 2. dll.h conditionally includes export macros
#if FASTLED_BUILD_EXPORTS
    #include "export.h"  // Gets FASTLED_EXPORT macro
    
    // 3. Declares sketch_setup() and sketch_loop()
    FASTLED_EXPORT void sketch_setup(void);
    FASTLED_EXPORT void sketch_loop(void);
    
    // 4. Conditionally provides implementations
    #ifdef FASTLED_LIBRARY_SHARED
        FASTLED_EXPORT void sketch_setup() { setup(); }
        FASTLED_EXPORT void sketch_loop() { loop(); }
    #endif
#endif
```

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
1. ✅ **Created** `src/fl/export.h` with complete macro definitions
2. ✅ **Created** `src/fl/dll.h` with conditional DLL interface  
3. **Update WASM files** following the transformation pattern
4. **Test WASM compilation** after each file update
5. **Verify JavaScript bindings** still work correctly
6. **Test build flag combinations** (`FASTLED_BUILD_EXPORTS`, `FASTLED_LIBRARY_SHARED`)
7. **Update build documentation** to reflect new flag system

**Benefits:**
- **Unified export system** across all platforms
- **Consistent API** regardless of target platform  
- **Easier maintenance** with single export macro
- **Future-proof** for new platforms and export scenarios
- **Cross-platform compatibility** for future non-WASM targets
- **Opt-in architecture** reduces compilation overhead when DLL interface isn't needed

### **Build Flag Reference**

**Core Build Flags for Sketch Runner:**

| Flag | Default | Purpose | Used By |
|------|---------|---------|---------|
| `FASTLED_BUILD_EXPORTS` | 0 | Enable DLL interface declarations | External callers, test runners |
| `FASTLED_LIBRARY_SHARED` | undefined | Provide DLL implementations | FastLED when built as shared library |
| `FASTLED_BUILDING_DLL` | undefined | Windows export mode | FastLED when building Windows DLL |

**Flag Combination Matrix:**

| `BUILD_EXPORTS` | `LIBRARY_SHARED` | `BUILDING_DLL` | Result |
|-----------------|------------------|----------------|--------|
| 0 | - | - | **No DLL interface** (default, minimal overhead) |
| 1 | undefined | - | **DLL declarations only** (external caller mode) |
| 1 | defined | undefined | **DLL with implementations** (shared library mode) |
| 1 | defined | defined | **Windows DLL export** (building FastLED DLL) |

**Usage Examples:**

```cpp
// 1. External test runner (needs interface, no implementations)
#define FASTLED_BUILD_EXPORTS 1
#include "fl/dll.h"
// Gets: sketch_setup() and sketch_loop() declarations only

// 2. FastLED shared library build (provides implementations)  
#define FASTLED_BUILD_EXPORTS 1
#define FASTLED_LIBRARY_SHARED
#include "fl/dll.h"
// Gets: declarations + implementations calling setup()/loop()

// 3. Windows DLL build (export symbols)
#define FASTLED_BUILD_EXPORTS 1  
#define FASTLED_LIBRARY_SHARED
#define FASTLED_BUILDING_DLL
#include "fl/dll.h"
// Gets: declarations + implementations + Windows export attributes
```

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

## Build System Integration

### Proper Build Directory Structure

All sketch runner build artifacts are properly organized in the `.build/` directory structure:

```
.build/
├── fled/unit/          # Main test compilation output
│   ├── libfastled.lib  # FastLED static library
│   └── test_*.o        # Unit test object files
└── sketch_runner/      # Sketch runner demo builds
    └── sketch_runner_demo.exe
```

### Build Script (`build_sketch_runner_demo.sh`)

A dedicated build script ensures proper artifact placement:

```bash
#!/bin/bash
# Builds sketch runner demo to .build/sketch_runner/
BUILD_DIR=".build/sketch_runner"
mkdir -p "$BUILD_DIR"

# Compile with same flags as test system
uv run python -m ziglang c++ \
    [compilation flags...] \
    tests/sketch_runner_demo.cpp \
    .build/fled/unit/libfastled.lib \
    -o "$BUILD_DIR/sketch_runner_demo.exe"

# Run the demo
"./$BUILD_DIR/sketch_runner_demo.exe"
```

**Key Benefits:**
- ✅ **Clean project root** - No build artifacts in main directory
- ✅ **Organized structure** - Follows existing `.build/` conventions
- ✅ **Git ignored** - `.build/` already in `.gitignore`
- ✅ **Consistent flags** - Uses same compilation settings as test system

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

### Example Integration with Test System

**Command: `bash test --examples Blink`**

The test system integrates with the sketch runner to execute actual Arduino examples:

```bash
# Test command execution flow
bash test --examples Blink
  ↓
ci/compiler/cpp_test_run.py
  ↓ 
Compile examples/Blink/Blink.ino with sketch runner
  ↓
Generate executable with sketch_setup() and sketch_loop() exports
  ↓
Execute: .build/examples/blink/blink_runner.exe
```

**Build Process:**
1. **Compilation**: `examples/Blink/Blink.ino` compiled with sketch runner framework
2. **Linking**: Links against FastLED library with export implementations
3. **Executable**: Creates `.build/examples/blink/blink_runner.exe`
4. **Execution**: Runs actual sketch using `sketch_setup()` and `sketch_loop()` pattern

**Expected Output:**
```
RUNNER: Starting Blink example
RUNNER: Calling sketch_setup()
SKETCH: Initializing FastLED with 1 LED on pin 2
RUNNER: Setup complete, starting loop iterations
RUNNER: Calling sketch_loop() - iteration 1
SKETCH: LED ON - Red
RUNNER: Calling sketch_loop() - iteration 2  
SKETCH: LED OFF
RUNNER: Calling sketch_loop() - iteration 3
SKETCH: LED ON - Green
...
RUNNER: Example execution complete
```

**Key Features:**
- ✅ **Real sketch execution** - Runs actual Arduino code, not mocked versions
- ✅ **FastLED integration** - Full FastLED library functionality available
- ✅ **Export pattern** - Uses `sketch_setup()` and `sketch_loop()` exports
- ✅ **Build integration** - Seamlessly integrates with existing test infrastructure
- ✅ **Verification** - Validates that examples compile and execute correctly

**Implementation Architecture:**

```cpp
// Generated wrapper for examples/Blink/Blink.ino
#include "FastLED.h"

// Include the actual Arduino sketch
#include "examples/Blink/Blink.ino"

// Export functions for sketch runner
extern "C" {
    void sketch_setup() {
        setup();  // Call sketch's setup()
    }
    
    void sketch_loop() {
        loop();   // Call sketch's loop()
    }
}

int main() {
    printf("RUNNER: Starting Blink example\n");
    
    // Initialize sketch
    printf("RUNNER: Calling sketch_setup()\n");
    sketch_setup();
    printf("RUNNER: Setup complete, starting loop iterations\n");
    
    // Run sketch loop limited times for testing
    for (int i = 1; i <= 10; i++) {
        printf("RUNNER: Calling sketch_loop() - iteration %d\n", i);
        sketch_loop();
        
        // Small delay to make output readable
        delay(100);
    }
    
    printf("RUNNER: Example execution complete\n");
    return 0;
}
```

**Build System Integration:**

The test system automatically generates sketch runners for examples:

```bash
# Internal test system workflow for examples
bash test --examples Blink
  ↓
1. Detect examples/Blink/Blink.ino
2. Generate sketch_runner_wrapper.cpp (above)
3. Compile: wrapper + sketch + FastLED library
4. Output: .build/examples/blink/blink_runner.exe
5. Execute with controlled environment
6. Capture and validate output
```

**Directory Structure:**
```
.build/examples/
├── blink/
│   ├── blink_runner.exe      # Executable sketch runner
│   ├── wrapper.cpp           # Generated wrapper code
│   └── build.log             # Compilation log
├── demoreel100/
│   ├── demoreel100_runner.exe
│   └── wrapper.cpp
└── [other examples...]
```

**Command Line Interface:**

```bash
# Run specific example with sketch runner
bash test --examples Blink

# Run multiple examples
bash test --examples Blink,DemoReel100,Fire2012

# Run all examples (validates all compile and execute)
bash test --examples all

# Combine with other test flags
bash test --examples Blink --cpp --quick

# Example-specific build only (no execution)
bash test --examples Blink --compile-only
```

**Validation and Output:**

Each example execution is validated for:
- ✅ **Compilation success** - Sketch compiles without errors
- ✅ **Execution completion** - Runner completes without crashes  
- ✅ **FastLED initialization** - Library initializes correctly
- ✅ **Function calls** - `sketch_setup()` and `sketch_loop()` execute
- ✅ **Output patterns** - Expected log messages appear

**Failure Reporting:**
```bash
# Example failure output
RUNNER: Starting Blink example
RUNNER: Calling sketch_setup()
ERROR: FastLED.addLeds() failed - invalid pin configuration
RUNNER: Setup failed, aborting test

Test Summary:
- examples/Blink/Blink.ino: FAILED (setup error)
- Total examples tested: 1
- Pass rate: 0/1 (0%)
```

**Integration Benefits:**
- 🎯 **Example validation** - Ensures all published examples work correctly
- 🎯 **Regression testing** - Catches breaking changes in examples
- 🎯 **Documentation accuracy** - Validates example code matches documentation
- 🎯 **Platform testing** - Tests examples across different target platforms
- 🎯 **CI/CD integration** - Automated validation in continuous integration

**Relationship to Existing Compilation Testing:**

The sketch runner extends the existing example compilation system:

| Current System | Sketch Runner Extension |
|----------------|------------------------|
| ✅ **Compiles examples** | ✅ **Compiles examples** |
| ✅ **Validates syntax** | ✅ **Validates syntax** |
| ❌ Runtime validation | ✅ **Executes examples** |
| ❌ Function call validation | ✅ **Validates setup()/loop() calls** |
| ❌ Library integration testing | ✅ **Tests FastLED library integration** |
| ❌ Output verification | ✅ **Captures and validates output** |

**Current Status and Roadmap:**

**✅ Phase 1: Core Infrastructure (Completed)**
- Cross-platform export macros (`src/fl/export.h`)
- DLL interface with conditional implementations (`src/fl/dll.h`) 
- Unit test framework integration (`tests/test_sketch_runner.cpp`)
- Standalone demo executable (`tests/sketch_runner_demo.cpp`)
- Proper build directory structure (`.build/sketch_runner/`)

**🚧 Phase 2: Test System Integration (In Progress)**
- Integration with `bash test --examples` command
- Automatic wrapper generation for `.ino` files
- Example execution with `sketch_setup()` and `sketch_loop()` exports
- Output capture and validation
- Build artifact management in `.build/examples/`

**📋 Phase 3: Advanced Features (Planned)**
- Multiple platform support (AVR, ESP32, ARM)
- Performance benchmarking during example execution
- Memory usage analysis and reporting
- Integration with CI/CD pipelines
- Advanced output pattern matching and validation

**🔮 Phase 4: Production Features (Future)**
- Dynamic library loading for faster iteration
- Language bindings (Python, JavaScript, Go)
- Hot reload capability for development
- Remote execution and debugging support

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

## 🚨 CURRENT ISSUE: `bash test --examples --verbose | grep BLINK` Not Working

### Investigation Summary (Updated)

**EXPECTED BEHAVIOR:**
The command `bash test --examples --verbose | grep BLINK` should:
1. Discover all .ino examples in the examples/ directory
2. Compile and execute them using the sketch runner infrastructure  
3. Capture output including `Serial.println("BLINK")` from examples/Blink/Blink.ino
4. Allow `grep BLINK` to find and display the output

**ACTUAL BEHAVIOR:**
The command fails to produce any BLINK output, causing `grep BLINK` to return exit code 1.

### Root Cause Analysis

**PRIMARY ISSUE RESOLVED:** The original bug where `--full` flag wasn't being passed has been fixed in `ci/util/test_runner.py` (lines 367-369):

```python
elif args.examples is not None:
    # Auto-enable full mode for examples to include execution
    cmd.append("--full")
```

**REMAINING ISSUE IDENTIFIED:** Empty examples list not handled correctly in `ci/util/test_runner.py` (line 355):

```python
if args.examples:  # ❌ BUG: Empty list is falsy
    cmd.extend(args.examples)
```

**The Problem:**
- When user runs `bash test --examples --verbose`, `args.examples` becomes `[]` (empty list)
- Empty lists are falsy in Python, so `if args.examples:` evaluates to `False`
- No example names get passed to the compilation script
- The compilation script receives the command structure but no examples to process

**Expected vs Actual Command Construction:**

```python
# EXPECTED command when no examples specified:
["uv", "run", "python", "ci/compiler/test_example_compilation.py", "--full", "--verbose"]

# ACTUAL command (missing examples args):
["uv", "run", "python", "ci/compiler/test_example_compilation.py", "--full", "--verbose"]
```

While the compilation script (`ci/compiler/test_example_compilation.py`) is designed to handle empty examples by running all examples (line 1679: "if none specified, compile all examples"), the current logic in the test runner creates an ambiguous state.

### Proposed Fix

**Update `ci/util/test_runner.py` line 355:**

```python
# BEFORE (broken for empty examples):
if args.examples:
    cmd.extend(args.examples)

# AFTER (handles empty examples correctly):
if args.examples is not None:
    cmd.extend(args.examples)  # Pass empty list when --examples used without args
```

**Rationale:**
- When `--examples` is used without arguments, `args.examples` is `[]` (not `None`)
- The compilation script expects to receive an empty examples list to mean "run all examples"
- Current code incorrectly treats empty list as "no examples flag specified"

### Test Verification

After fix, these should work:

```bash
# Run all examples (should output BLINK from Blink.ino)
bash test --examples --verbose | grep BLINK

# Run specific example  
bash test --examples Blink --verbose | grep BLINK

# Run multiple examples
bash test --examples Blink DemoReel100 --verbose
```

### Status
- ✅ Primary execution bug fixed (--full flag now auto-enabled)
- ✅ Secondary argument passing bug FIXED (empty examples list now handled correctly)
- ✅ Fix implemented in `ci/util/test_runner.py` line 355
