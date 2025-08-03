# FastLED Sketch Runner Feature

## Overview

This feature will create a sketch runner system that compiles FastLED example sketches (`examples/*.ino`) into shared libraries (`.so` files) and provides a simple test runner to execute them. The system will build upon the existing CI compilation infrastructure and extend it to export sketch functions and create a lightweight test execution framework.

## Architecture

### Current System Analysis

The existing CI system in `ci/ci-compile.py` already compiles `.ino` sketches using PlatformIO's `pio ci` command. The system:

1. Finds `.ino` files in example directories
2. Compiles them with board-specific configurations
3. Uses the stub platform for testing (`STUB_PLATFORM` define)
4. Has dynamic library building capabilities in `ci/compiler/build_dynamic_lib.py`
5. Includes a stub main system in `src/fl/stub_main.cpp` that can include `.ino` files

### Proposed Modifications

#### 1. Modify Example Builder (`ci/ci-compile.py`)

**Current Process:**
- Compiles `.ino` files to board-specific executables
- Uses PlatformIO `pio ci` command
- Builds for multiple target boards

**New Process for Sketch Runner:**
- Add a new compilation mode: `--sketch-runner`
- When in sketch runner mode:
  - Compile each `.ino` sketch to a shared library (`.so`)
  - Use native/stub platform instead of embedded targets
  - Export `setup()` and `loop()` functions from the shared library
  - Store compiled `.so` files in organized output directory

#### 2. Sketch Library Export Mechanism

Each compiled sketch will export two C-compatible functions:

```cpp
extern "C" {
    void sketch_setup();
    void sketch_loop();
}
```

**Implementation Strategy:**
1. Create a wrapper source file for each sketch that:
   - Includes the original `.ino` file content
   - Provides C-compatible export wrappers
   - Maps Arduino `setup()` and `loop()` to exported functions

**Example wrapper template:**
```cpp
// Generated wrapper for {sketch_name}.ino
#include "FastLED.h"

// Include the original sketch content
{SKETCH_CONTENT}

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

#### 3. Simple Test Runner

Create a minimal test runner executable that:

1. **Load Sketch Libraries:** Use `dlopen()` to dynamically load `.so` files
2. **Execute Pattern:** For each loaded sketch:
   - Call `sketch_setup()` once
   - Call `sketch_loop()` exactly 5 times
   - Unload the library
3. **Error Handling:** Catch and report any crashes or errors
4. **Output:** Simple pass/fail reporting per sketch

**Test Runner Pseudo-code:**
```cpp
int main(int argc, char* argv[]) {
    for (const auto& so_file : sketch_libraries) {
        void* lib = dlopen(so_file.c_str(), RTLD_LAZY);
        if (!lib) continue;
        
        auto setup_fn = (void(*)())dlsym(lib, "sketch_setup");
        auto loop_fn = (void(*)())dlsym(lib, "sketch_loop");
        
        if (setup_fn && loop_fn) {
            setup_fn();  // Call setup once
            for (int i = 0; i < 5; i++) {
                loop_fn();  // Call loop 5 times
            }
            printf("PASS: %s\n", so_file.c_str());
        } else {
            printf("FAIL: %s (missing functions)\n", so_file.c_str());
        }
        
        dlclose(lib);
    }
    return 0;
}
```

## Implementation Details

### Required Build Flag Changes in `ci/build_flags.toml`

#### New Section: `[sketch_runner]`
```toml
[sketch_runner]
# Sketch runner specific build flags
defines = [
    "STUB_PLATFORM",                    # Use stub platform
    "FASTLED_UNIT_TEST=1",             # Enable test mode
    "FASTLED_STUB_IMPL",               # Use stub implementations
    "ARDUINO=10808",                    # Arduino compatibility
    "FASTLED_USE_STUB_ARDUINO",        # Use stub Arduino
    "SKETCH_HAS_LOTS_OF_MEMORY=1",     # Assume memory available
    "FASTLED_FORCE_NAMESPACE=1",       # Force namespace usage
    "FASTLED_NO_AUTO_NAMESPACE",       # Disable auto namespace
    "FASTLED_TESTING",                 # Enable testing features
    "FASTLED_SKETCH_RUNNER",           # New: Identify sketch runner mode
]

compiler_flags = [
    "-std=c++17",
    "-fpermissive",
    "-Wall",
    "-Wextra",
    "-fno-exceptions",
    "-fno-rtti",
    "-fPIC",                           # Position Independent Code for .so
    "-shared",                         # Build as shared library
]

include_flags = [
    "-I.",
    "-Isrc",
    "-Isrc/platforms/stub",
    "-Itests",
]
```

#### Modified Flags for Shared Libraries

**In `[linking.unix]` section, add:**
```toml
sketch_runner_flags = [
    "-shared",                         # Create shared library
    "-fPIC",                          # Position independent code
    "-Wl,--export-dynamic",           # Export all symbols
    "-pthread",                       # Thread support
    "-lm",                           # Math library
    "-ldl",                          # Dynamic loading
]
```

### Directory Structure

```
build/
├── sketch_runner/
│   ├── libraries/                    # Compiled .so files
│   │   ├── Blink.so
│   │   ├── DemoReel100.so
│   │   └── ...
│   ├── runner                        # Test runner executable
│   └── logs/                        # Execution logs
└── ...
```

### Integration with Existing CI

#### New Command Line Options

Add to `ci/ci-compile.py`:
```python
parser.add_argument(
    "--sketch-runner", 
    action="store_true",
    help="Build sketches as shared libraries for test runner"
)

parser.add_argument(
    "--runner-only",
    action="store_true", 
    help="Build only the test runner executable"
)
```

#### Modified Compilation Flow

1. **When `--sketch-runner` is specified:**
   - Skip board-specific compilation
   - Use native/stub platform only
   - Generate wrapper files for each sketch
   - Compile to `.so` instead of executable
   - Use `[sketch_runner]` flags from `build_flags.toml`

2. **Library Building Process:**
   - For each `examples/*.ino`:
     - Generate wrapper `.cpp` file
     - Compile with sketch runner flags
     - Link as shared library with exported symbols
     - Verify exported functions exist

3. **Test Runner Building:**
   - Simple C++ executable
   - Links against `libdl` for dynamic loading
   - Uses standard STUB platform configuration

## Benefits

1. **Fast Testing:** Quick validation that sketches compile and run
2. **Isolation:** Each sketch runs in its own library context
3. **Automation:** Easy integration with CI/CD pipelines
4. **Minimal Dependencies:** Builds on existing infrastructure
5. **Cross-Platform:** Works on Linux, macOS, and Windows (with appropriate dynamic library formats)

## Future Extensions

1. **Parameterized Testing:** Pass configuration to sketches
2. **Performance Metrics:** Measure execution time
3. **Memory Analysis:** Track memory usage per sketch
4. **Failure Analysis:** Detailed crash reporting
5. **Parallel Execution:** Run multiple sketches concurrently
6. **Web Interface:** Browser-based sketch runner dashboard

## Files to Modify

### Primary Changes
- `ci/ci-compile.py` - Add sketch runner compilation mode
- `ci/build_flags.toml` - Add sketch runner build configuration

### New Files to Create
- `ci/sketch_runner/` - New directory for sketch runner components
- `ci/sketch_runner/wrapper_generator.py` - Generate wrapper files
- `ci/sketch_runner/runner.cpp` - Simple test runner executable
- `ci/sketch_runner/build_runner.py` - Build the test runner

### Minimal Changes
- `src/fl/stub_main.cpp` - Potentially add sketch runner detection
- `ci/compiler/build_dynamic_lib.py` - Reference for shared library building

## Dependencies

The sketch runner will leverage existing dependencies:
- **ziglang/clang++**: From `[tools]` section in `build_flags.toml`
- **STUB platform**: Already configured for testing
- **Dynamic loading**: Standard system libraries (`libdl`)
- **FastLED source**: Existing `src/` directory structure

This design provides a clean, efficient way to validate that FastLED sketches compile and execute correctly without requiring actual hardware, building naturally on the existing CI infrastructure while adding powerful automated testing capabilities.