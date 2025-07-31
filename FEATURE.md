# Enhanced Build Configuration System

## Overview
A comprehensive build system that separates configuration from target selection, supporting modern build optimizations, multiple target types, and flexible compiler invocation patterns.

## Design Philosophy

1. **Configuration-Target Separation** - Build flags live in TOML, targets specified via CLI
2. **Simple CLI Interface** - Clean command line with minimal arguments
3. **Composable Platforms** - Mix and match platforms easily
4. **Mode-Based Building** - Predefined modes for common scenarios
5. **Zero Configuration Defaults** - Sensible defaults that just work
6. **Build Target Distinction** - Library, sketch, and test builds with appropriate strategies
7. **Compilation Strategy Selection** - Unity vs individual builds for different needs
8. **Build Speed Optimization** - PCH, ccache, thin archives, and parallel builds

## Command Line Interface

```bash
# Simple platform builds with target types
compiler --flags build_flags.toml --build=esp32dev --mode=quick --target=sketch
compiler --flags build_flags.toml --build=uno,esp32dev --mode=release --target=library

# Multi-platform builds with compilation strategies
compiler --flags build_flags.toml --build=esp32dev,wasm,uno --mode=debug --target=test --compile-strategy=unity

# Advanced usage with build optimizations
compiler --flags build_flags.toml --build=teensy31 --mode=performance --features=simd,parallel --pch=enable --archive=thin

# Compiler invocation with ccache
compiler --flags build_flags.toml --build=x86_64 --mode=quick-debug --cc="ccache clang++" --target=library

# Library vs sketch builds
compiler --flags build_flags.toml --build=esp32dev --mode=release --target=library --compile-strategy=unity
compiler --flags build_flags.toml --build=esp32dev --mode=debug --target=sketch --compile-strategy=individual
```

## Build Flags Configuration (build_flags.toml)

```toml
# Global build settings
[global]
cxx_standard = "17"
defines = ["FASTLED_VERSION=4000"]
parallel_jobs = "auto"  # Number of parallel compilation jobs
default_target = "sketch"  # library, sketch, test
default_compile_strategy = "auto"  # unity, individual, auto
default_archive_type = "regular"  # thin, regular
enable_ccache = "auto"  # true, false, auto (detect if available)

# Compiler toolchain configurations
[toolchain.gcc]
c_flags = ["-Wall", "-Wextra", "-fdiagnostics-color=always", "-fno-strict-aliasing"]
cpp_flags = ["-Wall", "-Wextra", "-fdiagnostics-color=always", "-fno-strict-aliasing", "-Wno-psabi"]
link_flags = ["-fdiagnostics-color=always"]

[toolchain.clang]
c_flags = ["-Wall", "-Wextra", "-fcolor-diagnostics"]
cpp_flags = ["-Wall", "-Wextra", "-fcolor-diagnostics", "-Weverything", "-Wno-c++98-compat", "-Wno-unknown-warning-option"]
link_flags = ["-fcolor-diagnostics"]

[toolchain.msvc]
c_flags = ["/W4"]
cpp_flags = ["/W4", "/permissive-"]
link_flags = ["/NOLOGO"]
defines = ["_CRT_SECURE_NO_WARNINGS"]

# Build mode configurations
[modes.debug]
c_flags = ["-g", "-O0"]
cpp_flags = ["-g", "-O0"]
defines = ["DEBUG=1", "_DEBUG"]

[modes.release]
c_flags = ["-O3", "-DNDEBUG"]
cpp_flags = ["-O3", "-DNDEBUG"]
link_flags = ["-O3"]
defines = ["NDEBUG"]

[modes.quick]
c_flags = ["-O1", "-g"]
cpp_flags = ["-O1", "-g"]
defines = ["DEBUG=1"]

[modes.quick-debug]
c_flags = ["-O1", "-g", "-fno-omit-frame-pointer"]
cpp_flags = ["-O1", "-g", "-fno-omit-frame-pointer"] 
defines = ["DEBUG=1", "_DEBUG"]

[modes.performance]
c_flags = ["-O3", "-ffast-math", "-DNDEBUG"]
cpp_flags = ["-O3", "-ffast-math", "-funroll-loops", "-DNDEBUG"]
link_flags = ["-O3", "-ffast-math"]
defines = ["NDEBUG"]

[modes.size]
c_flags = ["-Os", "-ffunction-sections", "-fdata-sections"]
cpp_flags = ["-Os", "-ffunction-sections", "-fdata-sections"]
link_flags = ["-Wl,--gc-sections", "-Os"]

# Platform-specific configurations
[platforms.uno]
c_flags = ["-mmcu=atmega328p"]
cpp_flags = ["-mmcu=atmega328p"]
link_flags = ["-mmcu=atmega328p"]
defines = ["F_CPU=16000000L", "ARDUINO_ARCH_AVR", "__AVR_ATmega328P__", "ARDUINO=10819"]

[platforms.esp32dev]
c_flags = ["-mfix-esp32-psram-cache-issue"]
cpp_flags = ["-mfix-esp32-psram-cache-issue"]
link_flags = ["-mfix-esp32-psram-cache-issue"]
defines = ["ESP32=1", "ARDUINO_ARCH_ESP32", "F_CPU=240000000L", "ARDUINO=10819"]

[platforms.teensy31]
c_flags = ["-mcpu=cortex-m4", "-mthumb"]
cpp_flags = ["-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=hard", "-mfpu=fpv4-sp-d16"]
link_flags = ["-mcpu=cortex-m4", "-mthumb", "-mfloat-abi=hard", "-mfpu=fpv4-sp-d16"]
defines = ["ARM_MATH_CM4=1", "ARDUINO=10819"]

[platforms.wasm]
c_flags = ["-sUSE_PTHREADS=1"]
cpp_flags = ["-sUSE_PTHREADS=1", "-sASYNCIFY=1"]
link_flags = ["-sUSE_PTHREADS=1", "-sASYNCIFY=1", "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"]
defines = ["FASTLED_WASM=1"]

[platforms.x86_64]
c_flags = ["-m64"]
cpp_flags = ["-m64", "-msse4.2"]
link_flags = ["-m64"]
defines = ["FASTLED_X86_64=1"]

# Optional feature configurations
[features.simd]
c_flags = ["-msse4.2"]
cpp_flags = ["-msse4.2", "-mavx2"]
defines = ["FASTLED_SIMD=1"]

[features.parallel]
c_flags = ["-fopenmp"]
cpp_flags = ["-fopenmp"]
link_flags = ["-fopenmp"]
defines = ["FASTLED_PARALLEL=1"]

[features.network]
defines = ["FASTLED_NETWORK=1"]

[features.testing]
c_flags = ["-coverage", "--coverage"]
cpp_flags = ["-coverage", "--coverage"]
link_flags = ["--coverage"]
defines = ["TESTING=1", "FASTLED_TESTING=1"]

[features.sanitizers]
c_flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
cpp_flags = ["-fsanitize=address,undefined", "-fno-omit-frame-pointer"]
link_flags = ["-fsanitize=address,undefined"]

[features.profiling]
c_flags = ["-pg", "-fno-omit-frame-pointer"]
cpp_flags = ["-pg", "-fno-omit-frame-pointer"]
link_flags = ["-pg"]

# Build target configurations
[targets.library]
compile_strategy = "unity"  # Default to unity builds for faster library compilation
archive_type = "thin"  # Use thin archives for faster linking
pch = true  # Enable PCH for large library builds
pch_includes = ["FastLED.h", "src/fl/fl.h"]  # Core headers to precompile
unity_batch_size = "all"  # Files per unity batch
defines = ["FASTLED_LIBRARY_BUILD=1"]

[targets.sketch]
compile_strategy = "individual"  # Individual files for better debugging
archive_type = "regular"  # Regular archives for sketch builds
pch = false  # Disable PCH for simple sketches
defines = ["FASTLED_SKETCH_BUILD=1"]

[targets.test]
compile_strategy = "individual"  # Individual files for test isolation
archive_type = "regular"  # Regular archives for test builds
pch = false  # Disable PCH for test builds
defines = ["FASTLED_TEST_BUILD=1", "TESTING=1"]

# Compilation strategy configurations
[compile_strategies.unity]
batch_size = "all"  # Default files per unity compilation unit
exclude_patterns = ["test_*.cpp", "*_test.cpp"]  # Files to compile individually
c_flags = ["-DUNITY_BUILD=1"]
cpp_flags = ["-DUNITY_BUILD=1"]

[compile_strategies.individual]
c_flags = ["-DINDIVIDUAL_BUILD=1"]
cpp_flags = ["-DINDIVIDUAL_BUILD=1"]

# Build optimization configurations
[optimizations.pch]
# Precompiled header configuration
common_headers = [
    "FastLED.h",
    "src/fl/fl.h",
    "src/fl/vector.h",
    "src/fl/map.h"
]
platform_headers = {
    "esp32dev" = ["Arduino.h", "WiFi.h"],
    "uno" = ["Arduino.h"],
    "wasm" = ["emscripten.h"],
    "x86_64" = ["chrono", "thread"]
}
target_headers = {
    "library" = ["src/platforms/*/platform.h"],
    "sketch" = ["Arduino.h"],
    "test" = ["test.h", "gtest/gtest.h"]
}

[optimizations.ccache]
# Compiler cache configuration  
enable_for_targets = ["library", "test"]  # Enable ccache for these targets
disable_for_modes = ["release"]  # Disable ccache for release builds
cache_dir = ".ccache"  # Cache directory
max_size = "1G"  # Maximum cache size

[optimizations.archives]
# Archive optimization configuration
thin_archive_platforms = ["x86_64", "wasm"]  # Platforms supporting thin archives
regular_archive_platforms = ["uno", "esp32dev", "teensy31"]  # Platforms requiring regular archives
```

## Command Line Arguments

### Platform Selection (`--build`)
Specify one or more platforms to build for:
```bash
# Single platform
--build=esp32dev

# Multiple platforms 
--build=uno,esp32dev,teensy31

# WASM target
--build=wasm

# Mix platforms
--build=esp32dev,wasm,x86_64
```

### Build Modes (`--mode`)
Predefined optimization and debug configurations:
```bash
--mode=debug       # Debug symbols, no optimization
--mode=release     # Full optimization, no debug symbols  
--mode=quick       # Light optimization with debug symbols
--mode=quick-debug # Light optimization with enhanced debugging
--mode=performance # Maximum performance optimization
--mode=size        # Size-optimized build
```

### Features (`--features`)
Optional features to enable:
```bash
# Single feature
--features=simd

# Multiple features
--features=simd,parallel,network

# Advanced features
--features=testing,sanitizers,profiling
```

### Toolchain Selection (`--toolchain`)
Override default toolchain per platform:
```bash
# Use specific toolchain
--toolchain=clang

# Let system auto-detect (default)
--toolchain=auto
```

### Build Targets (`--target`)
Specify what type of build to perform:
```bash
--target=library   # Build FastLED library (default: unity builds, PCH enabled)
--target=sketch    # Build Arduino sketch (default: individual files, PCH disabled)
--target=test      # Build test suite (default: individual files, testing features)
```

### Compilation Strategy (`--compile-strategy`)
Override default compilation approach:
```bash
--compile-strategy=unity      # Combine source files for faster compilation
--compile-strategy=individual # Compile each file separately for debugging
--compile-strategy=auto       # Let target type choose strategy (default)
```

### Build Optimizations (`--pch`, `--archive`)
Control build speed optimizations:
```bash
# Precompiled headers
--pch=enable       # Force enable PCH
--pch=disable      # Force disable PCH
--pch=auto         # Let target/platform decide (default)

# Archive types
--archive=thin     # Use thin archives (faster linking, less disk space)
--archive=regular  # Use regular archives (better compatibility)
--archive=auto     # Let platform decide (default)
```

### Compiler Invocation (`--cc`, `--cxx`)
Specify compiler commands with optional wrappers:
```bash
# Use ccache for faster rebuilds
--cc="ccache gcc" --cxx="ccache g++"

# Use distcc for distributed compilation
--cc="distcc gcc" --cxx="distcc g++"

# Combine ccache and distcc
--cc="ccache distcc gcc" --cxx="ccache distcc g++"

# Custom compiler paths
--cc="/opt/gcc-12/bin/gcc" --cxx="/opt/gcc-12/bin/g++"
```

## Configuration Resolution

### Resolution Order
Configuration is resolved in multiple phases to determine the final build configuration:

### 1. Build Strategy Resolution
Determine compilation approach and optimizations (highest priority first):
1. **Command line overrides** - Explicit CLI options (`--target`, `--compile-strategy`, `--pch`, `--archive`)
2. **Target defaults** - Target-specific strategy from `[targets.{library|sketch|test}]`
3. **Platform constraints** - Platform-specific limitations (e.g., no thin archives on AVR)
4. **Global defaults** - Fallback values from `[global]` section

### 2. Flag Composition Order
Flags are composed separately for each compilation phase (later overrides earlier):

**C Compilation (`c_flags`):**
1. **Toolchain c_flags** - Core C compiler settings
2. **Platform c_flags** - Architecture-specific C flags
3. **Compilation strategy c_flags** - Unity vs individual build flags
4. **Mode c_flags** - C optimization and debug settings
5. **Feature c_flags** - Optional C feature additions
6. **Target c_flags** - Target-specific C additions
7. **Command line overrides** - Explicit CLI C flags

**C++ Compilation (`cpp_flags`):**
1. **Toolchain cpp_flags** - Core C++ compiler settings
2. **Platform cpp_flags** - Architecture-specific C++ flags
3. **Compilation strategy cpp_flags** - Unity vs individual build flags
4. **Mode cpp_flags** - C++ optimization and debug settings
5. **Feature cpp_flags** - Optional C++ feature additions
6. **Target cpp_flags** - Target-specific C++ additions
7. **Command line overrides** - Explicit CLI C++ flags

**Linking (`link_flags`):**
1. **Toolchain link_flags** - Core linker settings
2. **Platform link_flags** - Architecture-specific linker flags
3. **Mode link_flags** - Optimization and debug linker settings
4. **Feature link_flags** - Optional feature linker flags
5. **Target link_flags** - Target-specific linker flags
6. **Command line overrides** - Explicit CLI linker flags

### 3. PCH Resolution
Precompiled header configuration (when enabled):
1. **Common headers** - Base headers from `[optimizations.pch.common_headers]`
2. **Platform headers** - Platform-specific headers from `[optimizations.pch.platform_headers.{platform}]`
3. **Target headers** - Target-specific headers from `[optimizations.pch.target_headers.{target}]`
4. **Feature headers** - Headers required by enabled features

### 4. Compiler Invocation Resolution
Final compiler command construction:
1. **Base compiler** - From toolchain or `--cc`/`--cxx` options
2. **Ccache wrapper** - Applied if enabled for current target/mode
3. **Distcc wrapper** - Applied if configured in compiler command
4. **Final command** - `{wrappers} {base_compiler} {all_flags}`

### Example Resolution
```bash
compiler --flags build_flags.toml --build=esp32dev --mode=debug --features=network --target=sketch
```

**Step 1: Build Strategy Resolution**
- **Target:** `sketch` (from CLI)
- **Compile Strategy:** `individual` (from `[targets.sketch]`)
- **Archive Type:** `regular` (from `[targets.sketch]`)
- **PCH:** `disabled` (from `[targets.sketch]`)

**Step 2: Flag Composition**
- **Global:** `cxx_standard=17`
- **Defines:** `FASTLED_VERSION=4000`, `ESP32=1`, `ARDUINO_ARCH_ESP32`, `DEBUG=1`, `_DEBUG`, `FASTLED_NETWORK=1`, `FASTLED_SKETCH_BUILD=1`
- **C Flags:** `-Wall -Wextra -fdiagnostics-color=always -fno-strict-aliasing -mfix-esp32-psram-cache-issue -DINDIVIDUAL_BUILD=1 -g -O0`
- **C++ Flags:** `-Wall -Wextra -fdiagnostics-color=always -fno-strict-aliasing -Wno-psabi -mfix-esp32-psram-cache-issue -DINDIVIDUAL_BUILD=1 -g -O0`
- **Link Flags:** `-fdiagnostics-color=always -mfix-esp32-psram-cache-issue`

**Step 3: Compiler Invocation**
- **Compiler:** `gcc` (auto-detected for ESP32)
- **Final Commands:** `gcc {c_flags}` and `g++ {cpp_flags}`

## Composition Examples

### Library Build with Unity Compilation and PCH
```bash
compiler --flags build_flags.toml --build=x86_64 --mode=release --target=library --cc="ccache clang++"
```

**Step-by-Step Composition:**
1. **Build Strategy:** `unity` compilation, `thin` archives, `PCH enabled`
2. **Global:** `cxx_standard=17`, `FASTLED_VERSION=4000`
3. **Toolchain (Clang):** `-Wall -Wextra -fcolor-diagnostics` base flags
4. **Platform (x86_64):** `-m64 -msse4.2` architecture flags
5. **Compilation Strategy (Unity):** `-DUNITY_BUILD=1`
6. **Mode (Release):** `-O3 -DNDEBUG` optimization flags
7. **Target (Library):** `FASTLED_LIBRARY_BUILD=1`

**PCH Headers:**
- **Common:** `FastLED.h`, `src/fl/fl.h`, `src/fl/vector.h`, `src/fl/map.h`
- **Platform:** `chrono`, `thread`
- **Target:** `src/platforms/*/platform.h`

**Final Configuration:**
```toml
[aggregate.x86_64_library_release]
cxx_standard = "17"
compile_strategy = "unity"
archive_type = "thin"
pch = true
ccache_enabled = true
defines = ["FASTLED_VERSION=4000", "FASTLED_X86_64=1", "UNITY_BUILD=1", "NDEBUG", "FASTLED_LIBRARY_BUILD=1"]
c_flags = ["-Wall", "-Wextra", "-fcolor-diagnostics", "-m64", "-DUNITY_BUILD=1", "-O3", "-DNDEBUG"]
cpp_flags = ["-Wall", "-Wextra", "-fcolor-diagnostics", "-Weverything", "-Wno-c++98-compat", "-m64", "-msse4.2", "-DUNITY_BUILD=1", "-O3", "-DNDEBUG"]
link_flags = ["-fcolor-diagnostics", "-m64", "-O3"]
compiler_command = "ccache clang++"
```

### WASM Quick Build for Testing
```bash
compiler --flags build_flags.toml --build=wasm --mode=quick-debug --target=test
```

**Step-by-Step Composition:**
1. **Build Strategy:** `individual` compilation, `regular` archives, `PCH disabled`
2. **Global:** `cxx_standard=17`, `FASTLED_VERSION=4000`
3. **Toolchain (Clang):** `-Wall -Wextra -fcolor-diagnostics` base flags
4. **Platform (WASM):** `-sUSE_PTHREADS=1`, `-sASYNCIFY=1`, `FASTLED_WASM=1`
5. **Compilation Strategy (Individual):** `-DINDIVIDUAL_BUILD=1`
6. **Mode (Quick-Debug):** `-O1 -g -fno-omit-frame-pointer`, `DEBUG=1`, `_DEBUG`
7. **Target (Test):** `FASTLED_TEST_BUILD=1`, `TESTING=1`

**Final Configuration:**
```toml
[aggregate.wasm_test_quick_debug]
cxx_standard = "17"
compile_strategy = "individual"
archive_type = "regular"
pch = false
defines = ["FASTLED_VERSION=4000", "FASTLED_WASM=1", "INDIVIDUAL_BUILD=1", "DEBUG=1", "_DEBUG", "FASTLED_TEST_BUILD=1", "TESTING=1"]
c_flags = ["-Wall", "-Wextra", "-fcolor-diagnostics", "-sUSE_PTHREADS=1", "-DINDIVIDUAL_BUILD=1", "-O1", "-g", "-fno-omit-frame-pointer"]
cpp_flags = ["-Wall", "-Wextra", "-fcolor-diagnostics", "-Weverything", "-Wno-c++98-compat", "-sUSE_PTHREADS=1", "-sASYNCIFY=1", "-DINDIVIDUAL_BUILD=1", "-O1", "-g", "-fno-omit-frame-pointer"]
link_flags = ["-fcolor-diagnostics", "-sUSE_PTHREADS=1", "-sASYNCIFY=1", "-sEXPORTED_RUNTIME_METHODS=['ccall','cwrap']"]
```

### ESP32 Sketch Build with Performance Features
```bash
compiler --flags build_flags.toml --build=esp32dev --mode=performance --features=simd --target=sketch
```

**Step-by-Step Composition:**
1. **Build Strategy:** `individual` compilation (sketch target), `regular` archives, `PCH disabled`
2. **Global:** `cxx_standard=17`, `FASTLED_VERSION=4000`
3. **Toolchain (GCC):** `-Wall -Wextra -fdiagnostics-color=always -fno-strict-aliasing`
4. **Platform (ESP32):** `-mfix-esp32-psram-cache-issue`, ESP32 defines
5. **Compilation Strategy (Individual):** `-DINDIVIDUAL_BUILD=1`
6. **Mode (Performance):** `-O3 -ffast-math -funroll-loops`, `NDEBUG`
7. **Features (SIMD):** `-msse4.2 -mavx2`, `FASTLED_SIMD=1`
8. **Target (Sketch):** `FASTLED_SKETCH_BUILD=1`

**Final Configuration:**
```toml
[aggregate.esp32_sketch_performance_simd]
cxx_standard = "17"
compile_strategy = "individual"
archive_type = "regular"
pch = false
defines = ["FASTLED_VERSION=4000", "ESP32=1", "ARDUINO_ARCH_ESP32", "F_CPU=240000000L", "ARDUINO=10819", "INDIVIDUAL_BUILD=1", "NDEBUG", "FASTLED_SIMD=1", "FASTLED_SKETCH_BUILD=1"]
c_flags = ["-Wall", "-Wextra", "-fdiagnostics-color=always", "-fno-strict-aliasing", "-mfix-esp32-psram-cache-issue", "-DINDIVIDUAL_BUILD=1", "-O3", "-ffast-math", "-msse4.2"]
cpp_flags = ["-Wall", "-Wextra", "-fdiagnostics-color=always", "-fno-strict-aliasing", "-Wno-psabi", "-mfix-esp32-psram-cache-issue", "-DINDIVIDUAL_BUILD=1", "-O3", "-ffast-math", "-funroll-loops", "-msse4.2", "-mavx2"]
link_flags = ["-fdiagnostics-color=always", "-mfix-esp32-psram-cache-issue", "-O3", "-ffast-math"]
```

### UNO Size-Optimized Sketch with ccache
```bash
compiler --flags build_flags.toml --build=uno --mode=size --target=sketch --cc="ccache avr-gcc"
```

**Step-by-Step Composition:**
1. **Build Strategy:** `individual` compilation, `regular` archives (AVR constraint), `PCH disabled`
2. **Global:** `cxx_standard=17`, `FASTLED_VERSION=4000`
3. **Toolchain (GCC):** `-Wall -Wextra -fdiagnostics-color=always -fno-strict-aliasing`
4. **Platform (UNO):** `-mmcu=atmega328p`, AVR defines
5. **Compilation Strategy (Individual):** `-DINDIVIDUAL_BUILD=1`
6. **Mode (Size):** `-Os -ffunction-sections -fdata-sections`, `-Wl,--gc-sections`
7. **Target (Sketch):** `FASTLED_SKETCH_BUILD=1`

**Final Configuration:**
```toml
[aggregate.uno_sketch_size]
cxx_standard = "17"
compile_strategy = "individual"
archive_type = "regular"  # AVR doesn't support thin archives
pch = false
ccache_enabled = true
defines = ["FASTLED_VERSION=4000", "F_CPU=16000000L", "ARDUINO_ARCH_AVR", "__AVR_ATmega328P__", "ARDUINO=10819", "INDIVIDUAL_BUILD=1", "FASTLED_SKETCH_BUILD=1"]
c_flags = ["-Wall", "-Wextra", "-fdiagnostics-color=always", "-fno-strict-aliasing", "-mmcu=atmega328p", "-DINDIVIDUAL_BUILD=1", "-Os", "-ffunction-sections", "-fdata-sections"]
cpp_flags = ["-Wall", "-Wextra", "-fdiagnostics-color=always", "-fno-strict-aliasing", "-Wno-psabi", "-mmcu=atmega328p", "-DINDIVIDUAL_BUILD=1", "-Os", "-ffunction-sections", "-fdata-sections"]
link_flags = ["-fdiagnostics-color=always", "-mmcu=atmega328p", "-Wl,--gc-sections", "-Os"]
compiler_command = "ccache avr-gcc"
```

## Advanced Usage

### Multi-Target Builds
Build different target types for the same platform:
```bash
# Build library and sketch for ESP32
compiler --flags build_flags.toml --build=esp32dev --mode=release --target=library
compiler --flags build_flags.toml --build=esp32dev --mode=debug --target=sketch

# Build all target types for x86_64 with optimizations
compiler --flags build_flags.toml --build=x86_64 --mode=quick --target=library --cc="ccache clang++"
compiler --flags build_flags.toml --build=x86_64 --mode=debug --target=test --compile-strategy=individual
```

### Build Strategy Optimization
```bash
# Fast library builds with unity compilation and PCH
compiler --flags build_flags.toml --build=x86_64 --mode=release --target=library --compile-strategy=unity --pch=enable

# Debug builds with individual files for better debugging
compiler --flags build_flags.toml --build=esp32dev --mode=debug --target=sketch --compile-strategy=individual

# Test builds with ccache for faster rebuilds
compiler --flags build_flags.toml --build=x86_64 --mode=quick-debug --target=test --cc="ccache clang++"
```

### Development Workflows
```bash
# Fast iteration with library builds (unity + PCH + ccache)
compiler --flags build_flags.toml --build=x86_64 --mode=quick --target=library --cc="ccache clang++"

# Embedded sketch debugging (individual files, no optimizations)
compiler --flags build_flags.toml --build=esp32dev --mode=debug --target=sketch --compile-strategy=individual

# Performance testing with SIMD and unity builds
compiler --flags build_flags.toml --build=x86_64 --mode=performance --target=library --features=simd --compile-strategy=unity

# Size optimization for constrained platforms
compiler --flags build_flags.toml --build=uno --mode=size --target=sketch --archive=regular

# WASM development with testing features
compiler --flags build_flags.toml --build=wasm --mode=quick-debug --target=test --features=network
```

### CI/CD Integration
```bash
# Fast library builds for CI (unity + ccache)
compiler --flags build_flags.toml --build=x86_64 --mode=quick --target=library --cc="ccache clang++"

# Comprehensive test builds - all platforms
compiler --flags build_flags.toml --build=uno,esp32dev,teensy31,wasm,x86_64 --mode=debug --target=test --features=testing

# Production library builds - optimized platforms
compiler --flags build_flags.toml --build=esp32dev,x86_64 --mode=release --target=library --compile-strategy=unity

# Sketch validation - individual compilation for debugging
compiler --flags build_flags.toml --build=uno,esp32dev --mode=debug --target=sketch --compile-strategy=individual
```

### Build Optimization Strategies
```bash
# Maximum speed - library with all optimizations
compiler --flags build_flags.toml --build=x86_64 --mode=release --target=library --compile-strategy=unity --pch=enable --archive=thin --cc="ccache clang++"

# Maximum compatibility - sketch with conservative settings
compiler --flags build_flags.toml --build=uno --mode=debug --target=sketch --compile-strategy=individual --pch=disable --archive=regular

# Balanced development - quick builds with debugging support
compiler --flags build_flags.toml --build=esp32dev --mode=quick-debug --target=sketch --cc="ccache gcc"
```

## Key Improvements

### Core Design Enhancements
✅ **Simple CLI Interface** - Clean command line with minimal cognitive overhead
✅ **Configuration-Target Separation** - TOML for flags, CLI for target selection  
✅ **Zero Magic Composition** - Explicit flag resolution with clear precedence
✅ **Multi-Platform Support** - Build multiple targets in single command
✅ **Intuitive Modes** - Predefined modes for common scenarios (debug, release, quick, quick-debug)

### Build Target Distinction
✅ **Library Builds** - Unity compilation, PCH, thin archives for fast library builds
✅ **Sketch Builds** - Individual files, debugging-friendly compilation for Arduino sketches
✅ **Test Builds** - Individual files, testing features, comprehensive validation
✅ **Target-Specific Defaults** - Appropriate strategies automatically applied per target type

### Compilation Strategy Selection  
✅ **Unity Builds** - Combine source files for faster compilation (ideal for libraries)
✅ **Individual Builds** - Compile each file separately for better debugging (ideal for sketches)
✅ **Smart Defaults** - Target types automatically choose appropriate compilation strategy
✅ **Override Capability** - Manual strategy selection when needed

### Build Speed Optimizations
✅ **Precompiled Headers (PCH)** - Platform, target, and feature-specific header precompilation
✅ **Compiler Caching** - ccache integration with target and mode awareness
✅ **Thin Archives** - Platform-aware archive type selection for faster linking
✅ **Parallel Compilation** - Configurable parallel job support

### Flexible Compiler Invocation
✅ **ccache Support** - `ccache clang++` style compiler wrapper support
✅ **distcc Integration** - Distributed compilation with `distcc gcc` patterns
✅ **Combined Wrappers** - `ccache distcc gcc` for maximum build distribution
✅ **Custom Compiler Paths** - Full path specification for non-standard toolchain locations

### Enhanced Features & Modes
✅ **Quick-Debug Mode** - Light optimization with enhanced debugging support
✅ **Composable Features** - Mix and match features without configuration complexity
✅ **Platform Constraints** - Automatic platform limitation handling (e.g., thin archives)
✅ **Feature-Aware PCH** - Header precompilation adapts to enabled features

### Developer Experience
✅ **Auto-Detection** - Sensible defaults for toolchain, strategy, and optimization selection
✅ **CI/CD Friendly** - Scriptable interface perfect for automation and build pipelines
✅ **Developer Focused** - Optimized workflows for library development, sketch debugging, and testing
✅ **Build Strategy Guidance** - Clear documentation and examples for common scenarios

### Advanced Capabilities  
✅ **Multi-Target Support** - Library, sketch, and test builds for same platform
✅ **Build Optimization Strategies** - From maximum speed to maximum compatibility
✅ **Development Workflow Optimization** - Fast iteration, debugging, and performance testing modes
✅ **Professional Build Features** - Unity builds, PCH, ccache, thin archives, and distcc

This enhanced design addresses all modern C++ build requirements while maintaining the simple, predictable interface that scales from quick iteration to complex multi-platform builds. The addition of build targets, compilation strategies, and build optimizations transforms this from a basic flag composition system into a comprehensive, professional-grade build configuration system that handles library development, sketch debugging, testing, and CI/CD automation with equal sophistication.
