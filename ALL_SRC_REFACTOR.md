# All Source Build Refactor Design Document

## Overview

FastLED currently uses a manual all-source build system where source files are split into:
- `*.cpp` files containing conditional wrapper code with `#if !FASTLED_ALL_SRC` guards
- `*.cpp.hpp` files containing the actual implementation

With automatic source file globbing now supported, we can eliminate this manual system by:
1. Moving implementation from `*.cpp.hpp` → `*.cpp` (overwriting wrappers)
2. Removing the conditional wrapper system
3. Disabling related test infrastructure

## Current System Analysis

### File Distribution
Current `.cpp.hpp` files (118 total):
- **`src/fl/`**: 52 files (largest module)
- **`src/`**: 14 files (root source)
- **`src/platforms/shared/ui/json/`**: 12 files
- **`src/platforms/wasm/`**: 8 files
- **`src/fx/`**: 8 files (across multiple subdirs)
- **`src/sensors/`**: 3 files
- **`src/third_party/`**: 4 files
- **`src/platforms/`**: 18 files (various platform subdirs)

### Current Wrapper Pattern
```cpp
// Current *.cpp file (wrapper):
#include "fl/compiler_control.h"

#if !FASTLED_ALL_SRC
#include "path/to/file.cpp.hpp"
#endif
```

```cpp
// Current *.cpp.hpp file (actual implementation):
#include "actual/includes.h"
// ... actual source code ...
```

### Tests to Disable
Key test file that validates the all-source build system:
- **`ci/ci/check_implementation_files.py`** - Scans for `*.cpp.hpp` files and validates inclusion in all-source build
  - This entire test should be disabled as it will no longer be relevant

## Refactor Plan

### Phase 1: Disable Test Infrastructure

#### 1.1 Disable check_implementation_files.py
**Search and Replace Operation:**
```bash
# Rename the test file to disable it
mv ci/ci/check_implementation_files.py ci/ci/check_implementation_files.py.disabled
```

**Test after this step:**
```bash
bash test --cpp --quick
```

### Phase 2: Library-by-Library Refactor

Process libraries in order from smallest to largest:

#### 2.1 Sensors Library (3 files)

**Files to process:**
1. `src/sensors/button.cpp.hpp` → `src/sensors/button.cpp`
2. `src/sensors/digital_pin.cpp.hpp` → `src/sensors/digital_pin.cpp`  
3. `src/sensors/touch.cpp.hpp` → `src/sensors/touch.cpp`

**Search and Replace Operations:**
```bash
# Copy implementations to overwrite wrappers
cp src/sensors/button.cpp.hpp src/sensors/button.cpp
cp src/sensors/digital_pin.cpp.hpp src/sensors/digital_pin.cpp
cp src/sensors/touch.cpp.hpp src/sensors/touch.cpp

# Remove .cpp.hpp files
rm src/sensors/button.cpp.hpp
rm src/sensors/digital_pin.cpp.hpp
rm src/sensors/touch.cpp.hpp
```

**Test after this step:**
```bash
bash test --cpp --quick
```

#### 2.2 FX Library (8 files across subdirs)

**Files to process:**
- `src/fx/frame.cpp.hpp` → `src/fx/frame.cpp`
- `src/fx/fx.cpp.hpp` → `src/fx/fx.cpp`
- `src/fx/storage.cpp.hpp` → `src/fx/storage.cpp`
- `src/fx/util.cpp.hpp` → `src/fx/util.cpp`
- `src/fx/2d/animartrix.cpp.hpp` → `src/fx/2d/animartrix.cpp`
- `src/fx/2d/blend.cpp.hpp` → `src/fx/2d/blend.cpp`
- `src/fx/2d/noisepalette.cpp.hpp` → `src/fx/2d/noisepalette.cpp`
- `src/fx/2d/pacifica.cpp.hpp` → `src/fx/2d/pacifica.cpp`
- `src/fx/detail/fx_compositor.cpp.hpp` → `src/fx/detail/fx_compositor.cpp`
- `src/fx/video/frame_interpolator.cpp.hpp` → `src/fx/video/frame_interpolator.cpp`
- `src/fx/video/pixel_stream.cpp.hpp` → `src/fx/video/pixel_stream.cpp`
- `src/fx/video/video.cpp.hpp` → `src/fx/video/video.cpp`
- `src/fx/video/video_impl.cpp.hpp` → `src/fx/video/video_impl.cpp`

**Search and Replace Operations:**
```bash
# Process all fx files
find src/fx -name "*.cpp.hpp" -exec bash -c 'cp "$1" "${1%.hpp}"' _ {} \;
find src/fx -name "*.cpp.hpp" -delete
```

**Test after this step:**
```bash
bash test --cpp --quick
```

#### 2.3 Third Party Libraries (4 files)

**Files to process:**
- `src/third_party/cq_kernel/channel.cpp.hpp` → `src/third_party/cq_kernel/channel.cpp`
- `src/third_party/cq_kernel/cq_kernel.cpp.hpp` → `src/third_party/cq_kernel/cq_kernel.cpp`
- `src/third_party/cq_kernel/queue.cpp.hpp` → `src/third_party/cq_kernel/queue.cpp`
- `src/third_party/object_fled/src/object_fled.cpp.hpp` → `src/third_party/object_fled/src/object_fled.cpp`

**Search and Replace Operations:**
```bash
# Process third party files
find src/third_party -name "*.cpp.hpp" -exec bash -c 'cp "$1" "${1%.hpp}"' _ {} \;
find src/third_party -name "*.cpp.hpp" -delete
```

**Test after this step:**
```bash
bash test --cpp --quick
```

#### 2.4 Platforms Libraries (18 files)

**Files to process by subdirectory:**

**platforms/shared/ui/json/ (12 files):**
- `button_field.cpp.hpp`, `checkbox_field.cpp.hpp`, `color_field.cpp.hpp`, `json_ui.cpp.hpp`, `number_field.cpp.hpp`, `range_field.cpp.hpp`, `select_field.cpp.hpp`, `slider_field.cpp.hpp`, `text_field.cpp.hpp`, `title_field.cpp.hpp`, `ui_internal.cpp.hpp`, `ui_manager.cpp.hpp`

**platforms/wasm/ (8 files):**
- `active_strip_data.cpp.hpp`, `compiler/Arduino.cpp.hpp`, `fs_wasm.cpp.hpp`, `js.cpp.hpp`, `js_bindings.cpp.hpp`, `led_strip.cpp.hpp`, `strip_id_map.cpp.hpp`, `ui.cpp.hpp`

**platforms/esp/32/ (5 files):**
- `esp32_display.cpp.hpp`, `i2s/i2s_parallel_driver.cpp.hpp`, `rmt_4/esp32_rmt_4.cpp.hpp`, `rmt_4/rmt_driver.cpp.hpp`, `rmt_5/esp32_rmt_5.cpp.hpp`, `rmt_5/rmt5_strip.cpp.hpp`, `spi_ws2812/spi_ws2812.cpp.hpp`

**Other platform files (3 files):**
- `platforms/arm/k20/octows2811_controller.cpp.hpp`
- `platforms/avr/avr_millis_timer_source.cpp.hpp`
- `platforms/stub/led_sysdefs_stub.cpp.hpp`
- `platforms/compile_test.cpp.hpp`

**Search and Replace Operations:**
```bash
# Process all platform files
find src/platforms -name "*.cpp.hpp" -exec bash -c 'cp "$1" "${1%.hpp}"' _ {} \;
find src/platforms -name "*.cpp.hpp" -delete
```

**Test after this step:**
```bash
bash test --cpp --quick
```

#### 2.5 FL Library (52 files - LARGEST, HIGH RISK)

**Critical Files to Process:**
All 52 files in `src/fl/` including core modules like:
- `allocator.cpp.hpp`, `audio.cpp.hpp`, `json.cpp.hpp`, `screenmap.cpp.hpp`, `ui.cpp.hpp`, etc.

**Search and Replace Operations:**
```bash
# BACKUP FIRST - this is the largest module
echo "Creating backup of fl/ directory..."
cp -r src/fl src/fl_backup_$(date +%Y%m%d_%H%M%S)

# Process all fl files
find src/fl -name "*.cpp.hpp" -exec bash -c 'cp "$1" "${1%.hpp}"' _ {} \;
find src/fl -name "*.cpp.hpp" -delete
```

**Critical Test after this step:**
```bash
bash test --cpp --quick
```

#### 2.6 Root Source Files (14 files - FINAL PHASE)

**Critical Root Files:**
- `FastLED.cpp.hpp` → `FastLED.cpp`
- `crgb.cpp.hpp` → `crgb.cpp`
- `bitswap.cpp.hpp` → `bitswap.cpp`
- `cled_controller.cpp.hpp` → `cled_controller.cpp`
- `colorpalettes.cpp.hpp` → `colorpalettes.cpp`
- `hsv2rgb.cpp.hpp` → `hsv2rgb.cpp`
- `lib8tion.cpp.hpp` → `lib8tion.cpp`
- `noise.cpp.hpp` → `noise.cpp`
- `platforms.cpp.hpp` → `platforms.cpp`
- `power_mgt.cpp.hpp` → `power_mgt.cpp`
- `rgbw.cpp.hpp` → `rgbw.cpp`
- `simplex.cpp.hpp` → `simplex.cpp`
- `transpose8x1_noinline.cpp.hpp` → `transpose8x1_noinline.cpp`
- `wiring.cpp.hpp` → `wiring.cpp`

**Search and Replace Operations:**
```bash
# BACKUP FIRST - these are core FastLED files
echo "Creating backup of root src files..."
find src -maxdepth 1 -name "*.cpp.hpp" -exec cp {} {}.backup_$(date +%Y%m%d_%H%M%S) \;

# Process root source files
find src -maxdepth 1 -name "*.cpp.hpp" -exec bash -c 'cp "$1" "${1%.hpp}"' _ {} \;
find src -maxdepth 1 -name "*.cpp.hpp" -delete
```

**Critical Test after this step:**
```bash
bash test --cpp  # Full C++ test suite
```

### Phase 3: Cleanup All-Source Build Infrastructure

#### 3.1 Remove Hierarchical Include Files

**Files to remove:**
```bash
rm src/fastled_compile.hpp.cpp
rm src/src_compile.hpp
rm src/fl/fl_compile.hpp
rm src/fx/fx_compile.hpp
rm src/sensors/sensors_compile.hpp
rm src/platforms/platforms_compile.hpp
rm src/third_party/third_party_compile.hpp
```

#### 3.2 Update Build System Files

**Update `src/fl/compiler_control.h`:**
Search for:
```cpp
// All Source Build Control
// When FASTLED_ALL_SRC is enabled, all source is compiled into a single translation unit
// Individual compilation (FASTLED_ALL_SRC=0) is only used for release builds
#ifndef FASTLED_ALL_SRC
  #if defined(__EMSCRIPTEN__) || defined(ESP32)
    #define FASTLED_ALL_SRC 1  // Individual compilation for Emscripten builds only
  #elif defined(RELEASE) || defined(NDEBUG)
    #define FASTLED_ALL_SRC 0  // Individual compilation for release builds only
  #else
    #define FASTLED_ALL_SRC 1  // Unified compilation for all other builds (debug, testing, non-release)
  #endif
#endif
```

Replace with:
```cpp
// All Source Build Control - DISABLED
// All source build system has been removed in favor of automatic source file globbing
// All source files are now compiled individually using standard CMake globbing
#ifndef FASTLED_ALL_SRC
  #define FASTLED_ALL_SRC 0  // All source build disabled - using individual file compilation
#endif
```

**Update `src/CMakeLists.txt`:**
Search for:
```cmake
if(DEFINED FASTLED_ALL_SRC AND FASTLED_ALL_SRC)
    message(STATUS "FASTLED_ALL_SRC=1 detected: Using unified compilation mode")
    
    # In unified mode, only compile the main fastled_compile.hpp.cpp file
    # This file includes all the .cpp.hpp files, so we don't need to compile them individually
    set(FASTLED_SOURCES "${FASTLED_SOURCE_DIR}/fastled_compile.hpp.cpp")
    
    # Also include any regular .cpp files that aren't .cpp.hpp files
    file(GLOB_RECURSE REGULAR_CPP_SOURCES "${FASTLED_SOURCE_DIR}/*.cpp")
    list(FILTER REGULAR_CPP_SOURCES EXCLUDE REGEX ".*\\.cpp\\.hpp$")
    
    # Combine the main file with regular .cpp files
    list(APPEND FASTLED_SOURCES ${REGULAR_CPP_SOURCES})
    
    message(STATUS "Unified compilation mode: Using ${FASTLED_SOURCES}")
else()
    message(STATUS "FASTLED_ALL_SRC not set or disabled: Using individual file compilation mode")
    
    # === Get all the source files ===
    file(GLOB_RECURSE FASTLED_SOURCES "${FASTLED_SOURCE_DIR}/*.c*")
    message(STATUS "Found source files: ${FASTLED_SOURCES}")
    
    if(FASTLED_SOURCES STREQUAL "")
        message(FATAL_ERROR "Error: No source files found in ${FASTLED_SOURCE_DIR}!")
    endif()
    
    # Exclude platform-specific files (e.g. esp, arm, avr)
    list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*esp.*")
    list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*arm.*")
    list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*avr.*")
endif()
```

Replace with:
```cmake
# All source build system removed - using standard individual file compilation
message(STATUS "Using individual file compilation mode with automatic source globbing")

# === Get all the source files ===
file(GLOB_RECURSE FASTLED_SOURCES "${FASTLED_SOURCE_DIR}/*.c*")
message(STATUS "Found source files: ${FASTLED_SOURCES}")

if(FASTLED_SOURCES STREQUAL "")
    message(FATAL_ERROR "Error: No source files found in ${FASTLED_SOURCE_DIR}!")
endif()

# Exclude platform-specific files (e.g. esp, arm, avr)
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*esp.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*arm.*")
list(FILTER FASTLED_SOURCES EXCLUDE REGEX ".*avr.*")
```

### Phase 4: Final Verification

#### 4.1 Complete Cleanup Verification
```bash
# Verify no .cpp.hpp files remain
echo "Checking for remaining .cpp.hpp files..."
find src -name "*.cpp.hpp"
# Should return no results

# Verify all include files are removed
echo "Checking for removed include files..."
ls -la src/fastled_compile.hpp.cpp src/src_compile.hpp src/*/compile.hpp 2>/dev/null || echo "All include files successfully removed"
```

#### 4.2 Full Test Suite
```bash
# Run complete test suite
bash test

# Test multiple platform compilations
bash compile uno --examples Blink
bash compile esp32dev --examples Blink
bash compile teensy31 --examples Blink
```

#### 4.3 Final File Count Verification
```bash
echo "=== ALL SOURCE REFACTOR COMPLETION VERIFICATION ==="
echo "Total .cpp files now: $(find src -name "*.cpp" | wc -l)"
echo "Remaining .cpp.hpp files: $(find src -name "*.cpp.hpp" | wc -l)"
echo "Should be 118 .cpp files and 0 .cpp.hpp files"
```

## Implementation Scripts

### Master Refactor Script
```bash
#!/bin/bash
# all_src_refactor.sh - Master script to execute the complete refactor

set -e  # Exit on any error

echo "=== FASTLED ALL SOURCE BUILD REFACTOR ==="
echo "This will convert 118 .cpp.hpp files to .cpp files"
echo "Press Enter to continue or Ctrl+C to abort..."
read

# Phase 1: Disable tests
echo "Phase 1: Disabling test infrastructure..."
mv ci/ci/check_implementation_files.py ci/ci/check_implementation_files.py.disabled
bash test --cpp --quick

# Phase 2.1: Sensors (3 files)
echo "Phase 2.1: Processing sensors library (3 files)..."
find src/sensors -name "*.cpp.hpp" -exec bash -c 'cp "$1" "${1%.hpp}"' _ {} \;
find src/sensors -name "*.cpp.hpp" -delete
bash test --cpp --quick

# Phase 2.2: FX (8 files)
echo "Phase 2.2: Processing fx library (8 files)..."
find src/fx -name "*.cpp.hpp" -exec bash -c 'cp "$1" "${1%.hpp}"' _ {} \;
find src/fx -name "*.cpp.hpp" -delete
bash test --cpp --quick

# Phase 2.3: Third Party (4 files)
echo "Phase 2.3: Processing third party libraries (4 files)..."
find src/third_party -name "*.cpp.hpp" -exec bash -c 'cp "$1" "${1%.hpp}"' _ {} \;
find src/third_party -name "*.cpp.hpp" -delete
bash test --cpp --quick

# Phase 2.4: Platforms (18 files)
echo "Phase 2.4: Processing platforms libraries (18 files)..."
find src/platforms -name "*.cpp.hpp" -exec bash -c 'cp "$1" "${1%.hpp}"' _ {} \;
find src/platforms -name "*.cpp.hpp" -delete
bash test --cpp --quick

# Phase 2.5: FL (52 files - HIGH RISK)
echo "Phase 2.5: Processing fl library (52 files - LARGEST MODULE)..."
echo "Creating backup..."
cp -r src/fl src/fl_backup_$(date +%Y%m%d_%H%M%S)
find src/fl -name "*.cpp.hpp" -exec bash -c 'cp "$1" "${1%.hpp}"' _ {} \;
find src/fl -name "*.cpp.hpp" -delete
bash test --cpp --quick

# Phase 2.6: Root (14 files - FINAL)
echo "Phase 2.6: Processing root source files (14 files - CORE FASTLED)..."
echo "Creating backup..."
find src -maxdepth 1 -name "*.cpp.hpp" -exec cp {} {}.backup_$(date +%Y%m%d_%H%M%S) \;
find src -maxdepth 1 -name "*.cpp.hpp" -exec bash -c 'cp "$1" "${1%.hpp}"' _ {} \;
find src -maxdepth 1 -name "*.cpp.hpp" -delete
bash test --cpp

# Phase 3: Cleanup infrastructure
echo "Phase 3: Cleaning up all-source build infrastructure..."
rm -f src/fastled_compile.hpp.cpp
rm -f src/src_compile.hpp
rm -f src/fl/fl_compile.hpp
rm -f src/fx/fx_compile.hpp
rm -f src/sensors/sensors_compile.hpp
rm -f src/platforms/platforms_compile.hpp
rm -f src/third_party/third_party_compile.hpp

echo "=== REFACTOR COMPLETE ==="
echo "Total .cpp files: $(find src -name "*.cpp" | wc -l)"
echo "Remaining .cpp.hpp files: $(find src -name "*.cpp.hpp" | wc -l)"
echo "Running final test suite..."
bash test
echo "SUCCESS: All source build refactor completed!"
```

### Pre-Flight Verification Script
```bash
#!/bin/bash
# verify_identical_files.sh
# Verify that .cpp and .cpp.hpp files are identical (except for wrapper code)

echo "=== VERIFYING FILE CONTENT CONSISTENCY ==="

# Check a sample from each library
dirs=("src/sensors" "src/fx" "src/fl" "src")

for dir in "${dirs[@]}"; do
    echo "Checking $dir..."
    find "$dir" -maxdepth 1 -name "*.cpp.hpp" | while read hpp_file; do
        cpp_file="${hpp_file%.hpp}"
        if [[ -f "$cpp_file" ]]; then
            echo "  Comparing: $cpp_file vs $hpp_file"
            if grep -q "FASTLED_ALL_SRC" "$cpp_file"; then
                echo "    ✓ $cpp_file contains wrapper (as expected)"
            else
                echo "    ⚠ $cpp_file does not contain wrapper - manual check needed"
            fi
        fi
    done
done
```

## Validation Steps

### Before Each Library
1. Run verification script to check file consistency
2. Ensure `bash test --cpp --quick` passes
3. Count files to be processed
4. Create backup if processing large modules (fl/, root)

### After Each Library  
1. **MANDATORY**: Run `bash test --cpp --quick`
2. Verify no `.cpp.hpp` files remain in processed directory
3. Spot-check a few files to ensure content was moved correctly
4. Check compilation still works: `bash compile uno --examples Blink`

### Final Validation
1. **MANDATORY**: Run full test suite: `bash test`
2. Verify all `.cpp.hpp` files are gone: `find src -name "*.cpp.hpp"`
3. Test multiple platform compilations
4. Remove disabled test infrastructure
5. Update documentation

## Risk Mitigation

### High-Risk Areas
1. **FL Library (52 files)**: Largest module, most complex dependencies
2. **Root Source (14 files)**: Core FastLED files like `FastLED.cpp`, `crgb.cpp`
3. **Platform Files**: Platform-specific code with conditional compilation

### Safety Measures
1. **Incremental Processing**: One library at a time with testing
2. **Automated Verification**: Scripts to verify file content consistency
3. **Mandatory Testing**: `bash test --cpp --quick` after each library
4. **Backup Strategy**: Create backups for large modules
5. **Stop on Failure**: Halt process if any tests fail

### Rollback Plan
If issues occur:
1. **Git Reset**: Use git to revert to pre-refactor state
2. **Backup Restoration**: Restore from backups if needed
3. **Incremental Recovery**: Fix issues in smallest possible units
4. **Alternative Approach**: Consider keeping problematic modules in old system temporarily

## Expected Benefits

### Build System Simplification
- Remove 118 `.cpp.hpp` files and their management overhead
- Eliminate conditional compilation wrapper system
- Simplify build configuration and tooling

### Developer Experience  
- Standard `.cpp` files for all implementations
- No confusion between `.cpp` and `.cpp.hpp` files
- Simpler debugging and development workflow

### Maintenance Reduction
- Remove complex all-source build test infrastructure
- Eliminate need to maintain dual file system
- Reduce build system complexity

## Conclusion

This refactor will modernize FastLED's build system by eliminating the manual all-source build infrastructure in favor of automatic source file globbing. The incremental, library-by-library approach with mandatory testing at each step ensures safety while achieving the goal of a simplified, standard C++ source file organization.

**Total Scope**: 118 files across 7 major libraries
**Estimated Time**: 2-4 hours with careful testing
**Risk Level**: Medium (mitigated by incremental approach and testing)

## AI Implementation Instructions

To implement this refactor, follow these specific steps:

1. **Start with Phase 1**: Disable the test infrastructure by renaming `ci/ci/check_implementation_files.py`

2. **Execute Phase 2 in exact order**: Process each library from smallest (sensors) to largest (fl, root), using the provided search and replace operations

3. **For each `.cpp.hpp → .cpp` operation**: Use `cp source.cpp.hpp destination.cpp` followed by `rm source.cpp.hpp`

4. **Test religiously**: Run `bash test --cpp --quick` after each library phase

5. **Handle the search and replace operations**: Use the exact commands provided in each phase section

6. **Complete with Phase 3**: Clean up all infrastructure files and update build system files using the exact search/replace patterns provided

7. **Final verification**: Ensure all 118 `.cpp.hpp` files are converted and all tests pass

### 🚨 Error Recovery Protocol

**If any test failures occur during the refactor:**

1. **STOP IMMEDIATELY** - Do not proceed to the next phase
2. **Attempt to fix the issue** - You have **5 ATTEMPTS MAXIMUM** to resolve test failures
3. **For each attempt:**
   - Analyze the test failure output carefully
   - Identify the specific issue (missing files, compilation errors, etc.)
   - Apply targeted fixes (restore files, fix syntax, adjust paths)
   - Re-run the failed test: `bash test --cpp --quick <TEST NAME>`
   - Document what was attempted in your response
4. **After 5 failed attempts:** 
   - **HALT THE REFACTOR PROCESS**
   - Report the final error state
   - Recommend manual intervention or rollback to previous working state
   - Do **NOT** continue to subsequent phases

### 🔄 Recovery Actions by Failure Type

**Common failure scenarios and recovery steps:**

- **Missing files**: Verify all `.cpp.hpp` files were properly copied before deletion
- **Compilation errors**: Check for syntax issues in moved files, verify includes
- **Linker errors**: Ensure no duplicate symbols or missing implementations
- **Path issues**: Verify relative paths in moved files are still correct
- **Platform-specific failures**: Check platform conditional compilation blocks

### 📋 Attempt Tracking Template

For each recovery attempt, document:
```
ATTEMPT [1-5]: [Brief description of fix attempted]
ERROR: [Specific error message or test failure]
ACTION: [What you tried to fix it]
RESULT: [Success/Failure]
```

**Example:**
```
ATTEMPT 1: Fix missing include in sensors/button.cpp
ERROR: fatal error: 'sensors/button.h' file not found
ACTION: Added missing #include "sensors/button.h" to src/sensors/button.cpp
RESULT: Success - tests now pass
```

The design document provides the complete roadmap - follow it step by step with mandatory testing at each checkpoint. **Remember: 5 attempts maximum per failure, then halt.** 
