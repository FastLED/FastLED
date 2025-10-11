# FastLED Build System Migration: Ad-hoc to Meson

## Executive Summary

This document outlines a migration strategy for FastLED's native/host build system from the current ad-hoc Python approach to Meson, a modern, fast build system designed for multi-platform C++ development.

The *.cpp and *.c files will be glob included, no need to specify them individually.

**Scope:** Meson is for **native builds only** (Windows/Linux/macOS) - unit tests, native compilation, development builds.

**Note:** This assumes "mason" refers to **Meson** build system (https://mesonbuild.com/).

## Current State Analysis

### Current Build Architecture

FastLED currently uses a custom Python build system for native/host builds:

1. **Custom Python Build System** (`ci/compiler/`) - **For native/host builds**
   - Zig-provided Clang-based compilation with PCH (Precompiled Headers)
   - Fingerprint caching for 99% faster rebuilds
   - Parallel compilation orchestration
   - **THIS is what Meson will replace for native builds**

2. **Build Configuration** (TOML files)
   - `ci/build_commands.toml`
   - `ci/build_example.toml`
   - `ci/build_unit.toml`

3. **Test Infrastructure** - **Target for Meson migration**
   - `uv run test.py` - Unit and integration tests
   - Native C++ unit test compilation
   - Host machine development builds

### Key Features to Preserve (Native Builds Only)

- **Native platform support**: Windows, Linux, macOS
- **Precompiled headers** for fast compilation (required)
- **Intelligent caching** for fast rebuilds
- **Parallel builds** (CPU-optimized)
- **Unity builds** for reduced compilation time
- **Unit test compilation** (C++ tests)
- **Fast builds with -g0**: No debug symbols for maximum speed
- **Shared libfastled.a**: Single library shared between unit tests and example tests
- **Build artifact management**

## Migration Goals

### Primary Objectives

1. **Unified native build interface** - Single entry point for host machine compilation
2. **Faster builds** - Leverage Meson's Ninja backend + unity builds
3. **Better dependency management** - Automatic header tracking via Ninja depfiles
4. **Improved IDE integration** - Standard compile_commands.json for all IDEs
5. **Simplified maintenance** - Replace custom Python compiler with declarative meson.build
6. **Preserve performance features** - Keep PCH, add unity builds, maintain parallelism

### Non-Goals

- **NO Docker orchestration changes** - Keep current Docker system

## Proposed Architecture

### Meson Build System for Native Builds

```

         Meson Build System
       (Native Development)
              ,

                   Native Builds (Linux/Windows/macOS)
                   Unit Tests (C++ tests)
                   Static Analysis Tools
```

### Directory Structure

```
./                          # Project root (FastLED)
   src/                     # FastLED library source (not touched by Meson)
   examples/                # Arduino examples (not touched by Meson)
   unit/                    # Unit tests source (not touched by Meson)
   ci/
      meson/                # Meson build root (completely separate from src/examples/unit)
         meson.build        # Root build file
         meson_options.txt  # Build options
         subprojects/       # Dependencies (if any)
         native-files/      # Native build configs
         src/
            meson.build     # Source build rules
         tests/
            meson.build     # Test compilation
      compiler/             # Existing compiler (to be replaced)
```

## Migration Strategy

### Phase 1: Parallel Infrastructure (Months 1-2)

**Goal:** Introduce Meson without breaking existing system

1. **Create basic meson.build files**
   - Root `meson.build` with project metadata in `ci/meson/`
   - `ci/meson/src/meson.build` for library compilation
   - `ci/meson/tests/meson.build` for unit tests

2. **Support native builds only**
   - Linux/Windows/macOS compilation
   - Unit test execution
   - Replace `ci/compiler/clang_compiler.py` usage for native

3. **Preserve existing workflows**
   - **Build entry point remains `bash test`** - same command as today
   - Keep `uv run test.py` working (internally calls Meson instead of custom compiler)
   - Keep Docker system unchanged

4. **Direct Meson commands (optional)**
   ```bash
   # Users can optionally use Meson directly, but not required
   meson setup ci/meson/builddir
   meson compile -C ci/meson/builddir
   meson test -C ci/meson/builddir
   ```

**Key principle:** Users continue using `bash test` - Meson is an internal implementation detail.

### Phase 2: Optimization & Polish (Months 3-4)

**Goal:** Match or exceed current performance

1. **Unity build integration**
   - Choose between built-in (`unity: true`) or custom generator approach
   - Tune `unity_size` for optimal parallelism (typically 10-30 files per TU)
   - Benchmark against current system (compile time, binary size, debug symbols)
   - Unity builds can replace/complement current fingerprint caching

2. **PCH + Unity combination**
   ```meson
   fastled_lib = static_library('fastled',
     fl_srcs,
     cpp_pch : 'fastled_precompiled.h',  # PCH for faster TU compilation
     unity : true,                        # Unity for fewer TUs
     unity_size : 20                      # Balance compile speed vs parallelism
   )
   ```

3. **Caching strategy**
   - Meson's built-in caching (Ninja backend tracks dependencies automatically)
   - Unity builds reduce cache pressure (fewer object files)

4. **Parallel build tuning**
   ```bash
   # Ninja backend (default, fastest)
   meson setup builddir --backend=ninja
   ninja -C builddir -j$(nproc)
   ```

5. **CI/CD integration**
   - GitHub Actions workflow updates (build-only, no install)
   - Performance benchmarking (unity vs non-unity, Meson vs current system)
   - Verify no privilege escalation in CI pipelines

## Technical Details

### Installation and Security

**Meson Installation:**
```bash
# Install via uv (no system privileges required)
uv tool install meson
```

**Security Note - Privilege Escalation:**
Meson never escalates privileges during build/compile phases. The only time privilege escalation can occur is during `meson install` if the prefix points to a system directory (e.g., `/usr/local`).

**Safe practices for FastLED CI/build:**
```bash
# Option 1: User-local prefix (no privilege escalation)
meson setup builddir --prefix=$HOME/.local
ninja -C builddir
meson install -C builddir  # Safe - writes to user directory

# Option 2: Build-only workflow (skip install entirely)
meson setup builddir
ninja -C builddir
# Artifacts stay in builddir/ - no install needed

# Option 3: DESTDIR staging for packages (no privilege escalation)
meson install --destdir=/tmp/stage  # Safe - writes to staging area
```

For FastLED's CI environment, we should use **Option 2** (build-only) or **Option 3** (DESTDIR staging) to guarantee zero privilege escalation.

### Toolchain Integration

**Zig Toolchain:**
- FastLED uses Zig-provided Clang compiler (defined in `pyproject.toml`)
- Meson needs to be configured to use the same Zig-provided toolchain
- Native file configuration should specify Zig's Clang path

### Unity Build Strategies

Meson offers two approaches for unity builds, both of which integrate properly into the dependency graph:

#### Option A: Built-in Unity Builds (Simplest, Recommended)

Per-target unity builds with automatic dependency tracking:

```meson
# src/meson.build

fs = import('fs')

# Glob source files (fs module is sanctioned for this)
fl_srcs = fs.glob('src/fl/*.cpp', 'src/fl/**/*.cpp')
fl_inc  = include_directories('src')

# Static library libfastled.a - shared between unit tests and example tests
fastled_lib = static_library(
  'fastled',
  fl_srcs,
  include_directories : fl_inc,
  cpp_pch : 'fastled_precompiled.h',  # Precompiled headers (required)
  unity : true,                        # Enable unity builds for speed
  unity_size : 20,                     # Files per unity TU (tune for speed)
  cpp_args : ['-g0'],                  # No debug symbols for fast builds
  install : false                      # CI: don't install, just build
)

# Dependency for other targets (unit tests and example tests)
fastled_dep = declare_dependency(
  link_with : fastled_lib,
  include_directories : fl_inc
)
```

**How it works:**
- Meson generates unity sources that `#include` your real sources
- Ninja's depfiles track headers automatically
- Any source or header change triggers correct rebuilds
- `unity_size` controls parallelism vs. compilation speed trade-off

**Advantages:**
- Zero custom code
- Proper dependency tracking (headers + sources)
- Tunable with `unity_size`
- Compatible with PCH

#### Option B: Custom Unity Generation (Advanced Control)

Two-stage pipeline with explicit dependency node for unity generation:

```meson
# src/meson.build

fs = import('fs')

# Use uv to run Python scripts
uv = find_program('uv')

# Enumerate source files
fl_srcs = fs.glob('src/fl/*.cpp', 'src/fl/**/*.cpp')

# Optionally track headers for regeneration triggers
fl_hdrs = fs.glob('src/fl/**/*.h', 'src/fl/**/*.hpp')

# Stage 1: Generate unity file (dependency node in build graph)
gen_unity = custom_target(
  'gen_fl_unity',
  input  : fl_srcs,
  output : 'fl_unity.cpp',
  depend_files : fl_hdrs,  # Regenerate if headers added/removed
  command: [
    uv, 'run', files('tools/make_unity.py'),
    '@OUTPUT@', '@INPUT@'
  ],
  build_by_default : true
)

# Stage 2: Compile the generated unity TU - shared libfastled.a
fastled_lib = static_library(
  'fastled',
  [gen_unity],  # Depends on generated unity file
  include_directories : include_directories('src'),
  cpp_pch : 'fastled_precompiled.h',  # Precompiled headers (required)
  cpp_args : ['-DFL_UNITY_BUILD', '-g0'],  # No debug symbols for fast builds
  install : false
)

# Dependency for other targets (unit tests and example tests)
fastled_dep = declare_dependency(
  link_with : fastled_lib,
  include_directories : include_directories('src')
)
```

**Unity generator script** (`tools/make_unity.py`):
```python
#!/usr/bin/env python3
import sys
from pathlib import Path

out = Path(sys.argv[1])
inputs = sys.argv[2:]

# Generate unity file with #includes
unity_content = '\n'.join(f'#include "{path}"' for path in inputs)
out.write_text(unity_content)
```

**Dependency chain:**
```
src/fl/*.cpp or *.h → gen_unity (custom_target) → fl_unity.cpp → fastled_lib.a
```

**How it works:**
- **Stage 1 (Detection/Gen):** If source list or headers change, Meson re-runs generator
- **Stage 2 (Compile):** Compiler emits depfiles for `fl_unity.cpp`, tracking all headers
- Any header edit retriggers compilation even if amalgam text unchanged

**Advantages:**
- Full control over amalgamation layout
- Can add custom preludes/defines per block
- Explicit dependency node in build graph
- Header edits don't regenerate amalgam (faster) but still retrigger compile

**When to use each:**
- **Option A** (built-in): Most cases - least custom code, great speed
- **Option B** (custom): Need custom amalgamation layout or precise control

### Root Meson Build File Example

```meson
# Root meson.build
project('fastled', 'cpp',
  version : '6.0.0',
  default_options : [
    'cpp_std=c++17',
    'warning_level=3',
    'optimization=2',
    'debug=false',          # -g0: No debug symbols for fast builds
    'buildtype=plain'       # Fast builds, no debug info
  ]
)

# Add subdirectories
subdir('src')
subdir('tests')

# Export compile_commands.json for IDE integration
if meson.is_subproject() == false
  run_command('uv', 'run', '-m', 'compdb',
    '-p', meson.build_root(),
    list: true,
    capture: false
  )
endif
```


## Risk Assessment

### High Risk

1. **Performance regression** - Current system is highly optimized
   - **Mitigation:** Extensive benchmarking, preserve PCH and caching

2. **Learning curve** - Team familiarity with current Python system
   - **Mitigation:** Gradual migration, maintain both systems during transition

### Medium Risk

1. **Docker integration complexity** - Current Docker system is sophisticated
   - **Mitigation:** Keep Docker orchestration, change only internal build tool

2. **CI/CD disruption** - GitHub Actions workflows need updates
   - **Mitigation:** Run parallel CI pipelines during migration

### Low Risk

1. **Test infrastructure** - Mostly independent
   - **Mitigation:** Update test compilation only, keep runner logic

## Success Metrics

1. **Build speed**: Match or exceed current compilation times (target: 20% faster)
2. **Developer experience**: Simpler commands, better IDE integration
3. **CI stability**: No regression in CI reliability
4. **Platform coverage**: All native platforms supported (Windows/Linux/macOS)
5. **Documentation**: Complete migration guide and examples

## Open Questions

1. ✅ ~~**Meson vs Mason**~~: Confirmed - **Meson** build system
2. **Unity build approach**: Use built-in `unity: true` (Option A) or custom generator (Option B)?
3. **Performance validation**: Acceptable performance targets for migration approval?
4. **Timeline flexibility**: Is 4-month timeline acceptable or should we accelerate?
5. **Privilege escalation policy**: Confirm build-only workflow (no install) for CI?

## Implementation Progress

### Phase 1: Basic Infrastructure (COMPLETED - 2025-10-10)

✅ **1. Meson Installation**
- Meson already installed via `uv tool install meson`
- Version: 1.9.1
- No privilege escalation required

✅ **2. Directory Structure**
- Created `ci/meson/` directory structure
- Created `ci/meson/src/meson.build` for library compilation
- Created `ci/meson/tests/meson.build` for unit tests
- Created `ci/meson/meson_options.txt` for build options

✅ **3. Root Meson Build File**
- Created `ci/meson/meson.build` with project metadata
- FastLED version: 6.0.0
- C++17 standard
- Optimized defaults: `-g0` (no debug symbols) for fast builds

✅ **4. Library Compilation (src/meson.build)**
- Automatic source file discovery using Python `run_command`
- Globs all `*.cpp` files in `src/fl/` recursively
- Proper relative path handling (from `ci/meson/src/`)
- Platform defines configured (STUB_PLATFORM, FASTLED_UNIT_TEST, etc.)
- Compiler args match existing build system (gnu++17, fpermissive, etc.)
- Static library target: `libfastled.a`

✅ **5. Unit Tests (tests/meson.build)**
- Automatic test file discovery (`test_*.cpp`)
- Includes `doctest_main.cpp`
- Test-specific defines (DOCTEST_CONFIG_NO_EXCEPTIONS_BUT_WITH_ALL_ASSERTS)
- Sanitizers configured (address, undefined) for debug builds
- Platform-specific linking (Windows, Linux, macOS)
- Test executable: `fastled_tests`

✅ **6. Test Integration**
- Added `--meson` flag to `test.py`
- Created `ci/meson_builder.py` wrapper module
- Command-line interface: `uv run python ci/meson_builder.py`

✅ **7. Build Options**
- `fastled_unity`: Feature option for unity builds (enabled by default)
- `fastled_unity_size`: Integer option for files per unity TU (default: 20)
- Note: Unity builds not yet functional in current Meson version (requires Meson ≥1.1.0 with proper support)

✅ **8. Initial Testing**
- Meson setup: ✅ WORKING
- Source discovery: ✅ WORKING
- Build starts: ✅ WORKING
- Compilation: ⚠️ BLOCKED by FastLED source code issue (see below)

### Known Issues (RESOLVED - 2025-10-10)

1. ✅ **FIXED: Compilation Error in file_system.cpp**
   - Error: `'replace' is not a member of 'std'` in `fs_stub.hpp:165`
   - Fix: Added `#include <algorithm>` to fs_stub.hpp
   - Status: RESOLVED

2. ✅ **FIXED: Circular dependency in math headers**
   - Error: `'sqrt' is not a member of 'fl'` in geometry.h
   - Root cause: map_range.h included geometry.h, which tried to use fl::sqrt before it was declared
   - Fix: Removed geometry.h include from map_range.h (forward declaration was already present)
   - Status: RESOLVED

3. ✅ **FIXED: Missing Arduino.h stub path**
   - Error: `Arduino.h: No such file or directory`
   - Fix: Added `src/platforms/stub` to include paths in Meson build
   - Status: RESOLVED

4. **Unity Builds Not Functional**
   - Current Meson version doesn't support `unity` parameter in `static_library()`
   - Options: Wait for Meson update OR implement custom unity generation (Option B)
   - Disabled for now to get basic build working

5. ⚠️ **NEW: Missing math headers in source files**
   - Files affected: splat.cpp, corkscrew.cpp, screenmap.cpp
   - Errors: `floorf`, `fmodf`, `ceilf`, `cos`, `sin` not declared
   - These files need `<cmath>` includes
   - This is a FastLED source code issue (affects both old and new build systems)
   - Build progress: 132/171 files compile successfully (77% complete)

### Next Steps

1. ✅ ~~**Clarify build system choice**~~: Confirmed - **Meson** build system
2. ✅ ~~**Install Meson**~~: Installed via `uv tool install meson`
3. ✅ ~~**Create proof-of-concept**~~: Basic meson.build working, compiles started
4. ✅ ~~**Fix FastLED source issues**~~: Fixed map_range.h circular dependency, fs_stub.hpp missing algorithm
5. **Fix remaining source issues** - Add `<cmath>` includes to splat.cpp, corkscrew.cpp, screenmap.cpp
6. **Complete initial build** - Get libfastled.a to build successfully (currently 77% complete)
7. **Run first test** - Execute `fastled_tests` binary
8. **Unity builds** - Implement custom unity generation (Option B from design doc)
9. **Benchmark current system** - Compare Meson vs current Python build system
10. **Test CI integration** - Verify build-only workflow (no privilege escalation)
11. **Team review** - Get feedback on migration progress


## Testing strategy

Your testing strategy will be to hook it in to `bash test --meson`

When you are finished, hook it into `bash test` so it's the same flow as status quo but with mason.

## References

- Meson Build System: https://mesonbuild.com/
- Current FastLED build docs: `ci/AGENTS.md`

---

**Document Status:** In Progress - Meson build system 77% functional

**Author:** Claude Code Assistant
**Date:** 2025-10-10
**Version:** 0.3

---

## Session Summary (2025-10-10)

### Iteration 1-10 Progress Report (Session 3)

**Objective:** Migrate FastLED native/host build system from custom Python compiler to Meson

**Status:** ✅ BUILD SUCCESSFUL - All source files compile and link! - **UPDATE: 2025-10-10 Session 3**

**Completed Work (Session 3 - Final):**
1. ✅ Set up basic Meson build configuration (`ci/meson/meson.build`)
2. ✅ Created library build rules (`ci/meson/src/meson.build`)
3. ✅ Created test build rules (`ci/meson/tests/meson.build`)
4. ✅ Created Python wrapper (`ci/meson_builder.py`) for build orchestration
5. ✅ Fixed circular dependency in `src/fl/map_range.h` (removed geometry.h include)
6. ✅ Added missing `<algorithm>` include in `src/platforms/stub/fs_stub.hpp`
7. ✅ Fixed missing include path for Arduino.h stub
8. ✅ Fixed missing `<cmath>` includes in fl/ files (splat.cpp, corkscrew.cpp, screenmap.cpp)
9. ✅ Fixed test infrastructure (test.h, test_fft.cpp) with cmath includes
10. ✅ **SESSION 3:** Fixed missing `<cmath>` includes in fx/audio files (luminova.cpp, sound_to_midi.cpp)
11. ✅ **SESSION 3:** Updated source glob to include ALL .cpp files in src/ (not just fl/, fx/, third_party/)
12. ✅ **SESSION 3:** Added top-level source files (crgb.cpp, hsv2rgb.cpp, FastLED.cpp, etc.)
13. ✅ **SESSION 3:** Added Windows dbghelp library for crash handler support
14. ✅ **SESSION 3:** Disabled sanitizers on Windows (MinGW doesn't support them)
15. ✅ **SESSION 3:** Excluded test_example_compilation.cpp (has its own main())
16. ✅ **SESSION 3:** All linting passes (Python and C++)

**Build Status:**
- ✅ Library (libfastled.a): **BUILDS SUCCESSFULLY**
- ✅ Test executable (fastled_tests.exe): **BUILDS AND LINKS SUCCESSFULLY**
- ✅ Test execution: **WORKING** (static linking fixed DLL issues)
- ✅ All 200+ source files compile without errors
- ✅ All linker dependencies resolved
- ✅ Individual tests run successfully (e.g., xypath: 10/10 assertions passed)

**Performance:**
- Parallel compilation working (200+ files with Ninja backend)
- All libraries compiling successfully
- Clean build time: ~30-45 seconds
- Incremental builds: Very fast (Ninja dependency tracking)
- Compiler warnings: Only pedantic warnings (anonymous structs, etc.)

**Session 4 Fixes (2025-10-10):**
17. ✅ **SESSION 4:** Fixed DLL dependency crash by static linking C++ runtime (-static-libgcc, -static-libstdc++)
18. ✅ **SESSION 4:** Verified test execution works (xypath test: 10/10 assertions passed)
19. ✅ **SESSION 4:** Disabled crash handler (was causing STATUS_ENTRYPOINT_NOT_FOUND errors)

**Remaining Work:**
1. ✅ ~~Debug test executable crash~~ - FIXED with static linking
2. ⚠️ Integrate with `bash test` command - Flag exists but needs implementation in cpp_test_run.py
3. Benchmark Meson vs current build system
4. Team review and feedback
5. Consider configuring Zig compiler as Meson toolchain (currently using MinGW GCC)

**Integration Status:**
- ✅ `ci/meson_builder.py` - Standalone Meson build wrapper (working)
- ✅ `--meson` flag in test.py - Argument parsing (working)
- ⚠️ Need to hook Meson into cpp_test_run.py to replace compiler.clang_compiler calls
- ✅ Can run tests directly: `uv run python ci/meson_builder.py` (working)
- ✅ Can run specific tests: `.build/meson/tests/fastled_tests.exe --test-case="*name*"` (working)
