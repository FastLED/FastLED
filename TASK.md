# TASK: Transform Examples to DLL Architecture

## Executive Summary

**Objective**: Apply the same DLL transformation used for tests (commit b1314eaf8) to example executables, converting ~96 standalone executables into DLLs loaded by a shared runner.

**Expected Benefits**:
- 10-15% build time improvement (system libraries linked once, not 96 times)
- Reduced total binary size (no library duplication)
- Consistent architecture with tests
- Centralized crash handler management

**Complexity**: Medium (wrapper generation and setup()/loop() model add complexity beyond tests)

---

## Background Context

### Current Example Architecture
```
example-Blink.exe (links: libfastled.a + pthread + dbghelp + psapi + libunwind)
example-DemoReel100.exe (links: libfastled.a + pthread + dbghelp + psapi + libunwind)
...
example-<name>.exe (links: libfastled.a + pthread + system libs)
```

Total: ~96 examples, each with full system library linking

### Target DLL Architecture
```
example_runner.exe (links: pthread + dbghelp + psapi + libunwind + crash_handler)
  ├─ loads example-Blink.dll (links: libfastled.a only)
  ├─ loads example-DemoReel100.dll (links: libfastled.a only)
  └─ loads example-<name>.dll (links: libfastled.a only)
```

**Convenience copies**: `example-Blink.exe` (copy of runner) auto-loads `example-Blink.dll`

---

## Implementation Plan

### Phase 1: Create Core Infrastructure (Iteration 3)

#### Step 1.1: Create examples/shared/ directory
```bash
mkdir -p examples/shared
```

#### Step 1.2: Create example_runner.cpp
**File**: `examples/shared/example_runner.cpp`

**Based on**: `tests/shared/runner.cpp` (exact copy with `run_tests` → `run_example`)

**Key changes from test runner**:
- Replace `RunTestsFunc` with `RunExampleFunc`
- Replace `run_tests` function name with `run_example`
- Keep all other functionality identical (DLL loading, arg parsing, crash handler setup)

**Verification**:
- Copy `tests/shared/runner.cpp` to `examples/shared/example_runner.cpp`
- Do find-replace: `run_tests` → `run_example`, `RunTestsFunc` → `RunExampleFunc`
- Update comment at top to reference "example DLLs" instead of "test DLLs"

---

#### Step 1.3: Create example_dll_main.hpp
**File**: `src/platforms/example_dll_main.hpp`

**Purpose**: Provides `run_example()` export function for DLL mode

**Implementation**:
```cpp
/*
 * DLL entry point for Arduino examples.
 * Exports run_example() function that can be called by example_runner.exe
 */

#pragma once

#include "stub_main.hpp"

// Export function for DLL mode - called by example_runner.exe
extern "C" {
#ifdef _WIN32
    __declspec(dllexport)
#else
    __attribute__((visibility("default")))
#endif
    int run_example(int argc, const char** argv) {
        (void)argc;  // Suppress unused parameter warning
        (void)argv;  // Examples don't typically use command-line args

        // Run setup() once, then loop() until keep_going() returns false
        fl::stub_main::setup();
        while (fl::stub_main::next_loop()) {
            // Loop continues based on FASTLED_STUB_MAIN_FAST_EXIT
        }

        return 0;
    }
}
```

**Verification**:
- File created at `src/platforms/example_dll_main.hpp`
- Does NOT modify existing `stub_main.hpp` (minimizes risk)
- Reuses existing `fl::stub_main::setup()` and `fl::stub_main::next_loop()`

---

#### Step 1.4: Update generate_wrapper.py for DLL Mode
**File**: `ci/util/generate_wrapper.py`

**Current content** (from read):
```python
def main():
    if len(sys.argv) != 4:
        print(
            "Usage: generate_wrapper.py <output_file> <example_name> <ino_file>",
            file=sys.stderr,
        )
        sys.exit(1)

    output_file = sys.argv[1]
    example_name = sys.argv[2]
    ino_file = sys.argv[3]

    content = f"""// Auto-generated wrapper for example: {example_name}
// This file includes the Arduino sketch and provides main()

#include "{ino_file}"
#include "platforms/stub_main.hpp"
"""

    with open(output_file, "w") as f:
        f.write(content)

    print(f"Generated wrapper: {output_file}")
```

**Changes needed**:
Replace the content generation to use conditional compilation:

```python
    content = f"""// Auto-generated wrapper for example: {example_name}
// This file includes the Arduino sketch and provides main() or DLL export

#include "{ino_file}"

#ifdef EXAMPLE_DLL_MODE
// DLL mode: Use export function from example_dll_main.hpp
#include "platforms/example_dll_main.hpp"
#else
// Standalone mode: Use standard stub_main.hpp with main()
#include "platforms/stub_main.hpp"
#endif
"""
```

**Verification**:
- Wrapper supports both modes via `-DEXAMPLE_DLL_MODE` flag
- No mode flag = standalone executable (existing behavior)
- With mode flag = DLL export (new behavior)

---

### Phase 2: Update Build System (Iteration 4)

#### Step 2.1: Add example_runner.exe to examples/meson.build

**Location**: After line 122 (after stub_main_src definition), add runner build:

```meson
# Build crash handler library for example_runner.exe
# Reuse the crash handler from tests (same implementation)
crash_handler_deps_example = []
if libunwind_dep.found()
  crash_handler_deps_example += [libunwind_dep]
endif

crash_handler_example = static_library('crash_handler_example',
  project_root / 'tests' / 'shared' / 'crash_handler_main.cpp',
  include_directories: [src_dir, stub_dir],
  cpp_args: ['-DENABLE_CRASH_HANDLER', '-DFASTLED_STUB_IMPL'],
  dependencies: crash_handler_deps_example,
  install: false
)

# Build example_runner.exe (static, generic DLL loader)
# This executable loads an example DLL and calls its run_example() function
# It will be copied to each example directory as example-<name>.exe
#
# OPTIMIZATION: System libraries (pthread, dbghelp, psapi) are linked here (once)
# instead of in each example-X.dll (96 times), significantly reducing build time.
# The example_runner.exe provides these symbols to the loaded DLL at runtime.
# Crash handler is also setup in example_runner.exe before loading any DLL.
example_runner_link_args = ['-static', '-static-libgcc', '-static-libstdc++']
example_runner_deps = []
if build_machine.system() == 'windows'
  # Windows: Add system libraries that example DLLs depend on
  example_runner_link_args += [
    '-lpthread',    # POSIX threads (winpthreads) - required by FastLED
    '-ldbghelp',    # Debug helper library (for stack traces in crash handler)
    '-lpsapi',      # Process status API (for memory info in crash handler)
  ]
else
  # Unix: Add pthread and libunwind (if available)
  example_runner_link_args += ['-pthread']
  if libunwind_dep.found()
    example_runner_deps += [libunwind_dep]
  endif
endif

example_runner_exe = executable('example_runner',
  'shared' / 'example_runner.cpp',
  cpp_args: [
    '-std=c++11',
    '-O2',
    '-g',
    # Suppress warnings from Windows SDK headers
    '-Wno-pragma-pack',
  ],
  link_args: example_runner_link_args,
  link_with: [crash_handler_example],  # Link crash handler library
  dependencies: example_runner_deps,
  install: false
)
```

**Verification**:
- Runner builds successfully
- Links against crash handler library
- System libraries included in runner (not DLLs)

---

#### Step 2.2: Transform example executable() to shared_library()

**Location**: examples/meson.build, line 198-207 (the exe = executable(...) block)

**Current code**:
```meson
    exe = executable(
        'example-' + example_name,
        all_sources,  # Wrapper + additional .cpp files
        include_directories: all_includes,
        link_with: fastled_lib,
        cpp_args: example_compile_args + debug_cpp_args,
        link_args: example_link_args,
        build_by_default: false,  # Only build when explicitly requested
        install: false
    )
```

**New code** (replace entire block):
```meson
    # Build example as a shared library (DLL)
    # NOTE: System dependencies moved to example_runner.exe
    example_dll = shared_library(
        'example-' + example_name,
        all_sources,  # Wrapper + additional .cpp files
        include_directories: all_includes,
        link_with: fastled_lib,
        cpp_args: example_compile_args + debug_cpp_args + ['-DEXAMPLE_DLL_MODE'],
        link_args: dll_link_args,  # Minimal linking (defined in root meson.build)
        name_prefix: '',  # Remove 'lib' prefix on Windows (produce 'example-Blink.dll' not 'libexample-Blink.dll')
        build_by_default: false,  # Only build when explicitly requested
        install: false
    )

    # Copy example_runner.exe to example directory as example-<name>.exe
    # This allows running `example-Blink.exe` which auto-loads `example-Blink.dll`
    example_runner_copy = custom_target(
        'example-' + example_name + '_runner',
        input: example_runner_exe,
        output: 'example-' + example_name + '.exe',
        command: ['cp', '@INPUT@', '@OUTPUT@'],
        depends: [example_dll],  # Ensure DLL is built first
        build_by_default: false
    )

    # Register as test target: runner loads the DLL and executes it
    # Add to 'examples' suite for easy filtering
    test('example-' + example_name, example_runner_exe,
        args: [example_dll.full_path()],
        suite: 'examples',
        timeout: 60
    )

    # Track both DLL and runner for alias target
    example_executables += [example_dll, example_runner_copy]
```

**Key changes**:
1. `executable()` → `shared_library()` with `name_prefix: ''`
2. Added `-DEXAMPLE_DLL_MODE` to cpp_args
3. Changed `link_args` from `example_link_args` to `dll_link_args`
4. Added `custom_target` to copy runner
5. Updated test registration to use runner + DLL path
6. Track both DLL and runner copy in example_executables

**Verification**:
- Examples build as DLLs
- Runner copies created with correct names
- Test registration works with runner + DLL

---

#### Step 2.3: Verify dll_link_args exists in root meson.build

**Check**: Root `meson.build` should already have `dll_link_args` from test transformation (commit b1314eaf8)

**Location**: Root meson.build (already exists from test transformation)

**Expected content**:
```meson
# DLL-specific link arguments: Minimize linking by moving libraries to runner.exe
if is_windows
  dll_link_args = [
    '-mconsole',                # Console application subsystem
    '-Wl,--stack,16777216',     # 16MB stack (for large arrays)
    '-Wl,--no-undefined',       # Catch undefined symbols at link time
  ]
else
  dll_link_args = [
    '-rdynamic',                # Export symbols for backtrace_symbols()
    '-Wl,--no-undefined',       # Catch undefined symbols at link time
  ]
endif
```

**Action**: Verify this exists. If not, it's an error (should be from commit b1314eaf8).

**Verification**:
- `dll_link_args` variable exists in root meson.build
- Examples can reference it via parent scope

---

### Phase 3: Incremental Testing (Iteration 5)

#### Step 3.1: Test single example (Blink)

**Commands**:
```bash
# Clean build
rm -rf builddir
meson setup builddir

# Build just Blink
ninja -C builddir example-Blink.dll
ninja -C builddir example-Blink.exe

# Verify files exist
ls -lh builddir/examples/example-Blink.dll
ls -lh builddir/examples/example-Blink.exe

# Run via runner (explicit DLL path)
builddir/example_runner.exe builddir/examples/example-Blink.dll

# Run via convenience copy (auto-loads DLL)
builddir/examples/example-Blink.exe
```

**Expected output**:
- Blink.dll builds successfully (~200KB, small)
- example-Blink.exe is a copy of example_runner.exe
- Both invocation methods work identically
- Example executes 5 loop iterations (FASTLED_STUB_MAIN_FAST_EXIT)
- Returns exit code 0

**Troubleshooting**:
- If "Failed to load DLL": Check DLL actually built, check paths
- If "Failed to find run_example()": Check EXAMPLE_DLL_MODE flag, check example_dll_main.hpp included
- If linker errors: Check dll_link_args applied, check system libs NOT in DLL link

---

#### Step 3.2: Test example with additional sources

**Example to test**: `xypath` (has subdirectory sources)

**Why**: Tests that additional .cpp files are handled correctly in DLL mode

**Commands**:
```bash
ninja -C builddir example-xypath.dll
ninja -C builddir example-xypath.exe
builddir/examples/example-xypath.exe
```

**Expected behavior**:
- Builds successfully with subdirectory sources
- Runs without errors

---

#### Step 3.3: Test all examples

**Command**:
```bash
uv run test.py --examples
```

**Expected output**:
- All ~96 examples compile as DLLs
- All runner copies created
- All tests pass (via meson test)
- Total build time faster than before (~10-15% improvement)

**Success criteria**:
- Zero compilation failures
- Zero runtime failures
- Measurable build time improvement

**If failures occur**:
- Check specific example for platform-specific code
- Verify EXAMPLE_DLL_MODE handling
- Check include paths and source discovery

---

### Phase 4: Validation and Cleanup (Iteration 6)

#### Step 4.1: Verify binary sizes

**Check**:
```bash
# Total size of all example DLLs
du -sh builddir/examples/*.dll

# Size of runner
du -sh builddir/example_runner.exe

# Compare to old architecture (if backup exists)
```

**Expected**:
- Individual DLLs smaller (no system libs)
- Runner larger (has system libs + crash handler)
- Total size reduced

---

#### Step 4.2: Verify link args optimization

**Check**: Example DLLs should NOT link pthread, dbghelp, psapi directly

**Command**:
```bash
# On Windows with objdump
objdump -p builddir/examples/example-Blink.dll | grep -i "DLL Name"
```

**Expected imports**:
- Should see: `msvcrt.dll`, `KERNEL32.dll`, `libgcc_s_seh-1.dll`, `libstdc++-6.dll`
- Should NOT see: `libpthread` explicit imports (provided by runner at runtime)

---

#### Step 4.3: Run full regression test suite

**Commands**:
```bash
# Build everything
ninja -C builddir

# Run all tests (examples + unit tests)
uv run test.py --cpp
uv run test.py --examples

# Run linter
bash lint
```

**Success criteria**:
- All tests pass
- No new linter errors
- No regressions in functionality

---

#### Step 4.4: Update documentation

**File**: `examples/meson.build` (comment block at top)

**Update** the "HOST-BASED COMPILATION" section to mention DLL architecture:

```meson
# HOST-BASED EXAMPLE COMPILATION (Fast, using STUB platform + DLL architecture)
# ============================================================================
# Compiles all Arduino examples for the host platform using STUB backend.
# Examples are built as DLLs loaded by example_runner.exe for optimal build performance.
# System libraries (pthread, dbghelp, psapi, libunwind) are linked once in the runner,
# not separately in each of the 96 example DLLs.
```

**File**: `INVESTIGATION.md`

**Add section** at the end:

```markdown
## Implementation Complete

The example DLL transformation was successfully implemented in iterations 3-6.
See commit history for details. Examples now use the same DLL architecture as tests,
resulting in 10-15% build time improvement and reduced binary size.
```

---

### Phase 5: Performance Benchmarking (Iteration 7)

#### Step 5.1: Measure build time improvement

**Benchmark**:
```bash
# Clean build (before)
rm -rf builddir
time meson setup builddir && time ninja -C builddir examples-host

# Record time
```

**Record**:
- Setup time: X seconds
- Build time: Y seconds
- Total: X + Y seconds

**Compare** to INVESTIGATION.md baseline (if available)

---

#### Step 5.2: Measure binary size reduction

**Calculate**:
```bash
# Total size of example DLLs
TOTAL_DLL=$(du -sb builddir/examples/*.dll | awk '{sum+=$1} END {print sum}')

# Size of runner
RUNNER=$(du -sb builddir/example_runner.exe | awk '{print $1}')

# Total
echo "Total size: $(($TOTAL_DLL + $RUNNER)) bytes"
```

**Compare** to old architecture total

---

## Rollback Strategy

If critical issues arise during implementation:

### Rollback Point 1 (After Phase 1)
**If**: Core infrastructure doesn't compile

**Action**:
```bash
rm examples/shared/example_runner.cpp
rm src/platforms/example_dll_main.hpp
git checkout ci/util/generate_wrapper.py
```

### Rollback Point 2 (After Phase 2)
**If**: Build system changes break examples

**Action**:
```bash
git checkout examples/meson.build
# Remove runner build from meson.build manually
```

### Complete Rollback
**If**: DLL architecture causes unfixable issues

**Action**:
```bash
git checkout examples/meson.build
git checkout ci/util/generate_wrapper.py
rm -rf examples/shared/
rm src/platforms/example_dll_main.hpp
```

---

## Success Criteria

### Must Have (Blocking)
- [ ] All 96 examples compile as DLLs without errors
- [ ] All examples run successfully via runner
- [ ] Zero functionality regressions
- [ ] No new linter errors

### Should Have (Non-blocking)
- [ ] 10-15% build time improvement measured
- [ ] Binary size reduction measured
- [ ] Documentation updated

### Nice to Have
- [ ] Consistent build time across iterations (demonstrating stability)
- [ ] Zero warnings during compilation

---

## Key Files Reference

### Files to Create
1. `examples/shared/example_runner.cpp` (based on tests/shared/runner.cpp)
2. `src/platforms/example_dll_main.hpp` (new DLL export header)

### Files to Modify
1. `ci/util/generate_wrapper.py` (add EXAMPLE_DLL_MODE support)
2. `examples/meson.build` (transform executable → shared_library, add runner build)

### Files to Reference (Do NOT modify)
1. `tests/shared/runner.cpp` (template for example_runner.cpp)
2. `tests/shared/crash_handler_main.cpp` (reuse for examples)
3. `tests/meson.build` (reference implementation)
4. `src/platforms/stub_main.hpp` (reuse fl::stub_main:: functions)
5. `meson.build` (root - dll_link_args already defined)

### Files to Read for Context
1. `INVESTIGATION.md` (background on DLL transformation)
2. `CLAUDE.md` (project coding standards)
3. `.loop/MOTIVATION.md` (performance expectations)

---

## Execution Notes for Agents

### Iteration 3 (Infrastructure)
**Focus**: Create runner, export header, update wrapper script
**Verify**: Files compile independently
**Time**: ~15-20 minutes

### Iteration 4 (Build System)
**Focus**: Update meson.build for DLL architecture
**Verify**: Builds complete without errors
**Time**: ~20-30 minutes

### Iteration 5 (Testing)
**Focus**: Incremental testing (single example → all examples)
**Verify**: Functionality matches original
**Time**: ~30-40 minutes

### Iteration 6 (Validation)
**Focus**: Regression testing, documentation
**Verify**: No regressions, docs updated
**Time**: ~20-30 minutes

### Iteration 7 (Benchmarking)
**Focus**: Measure improvements, document results
**Verify**: Performance gains documented
**Time**: ~10-15 minutes

**Total estimated time**: 95-135 minutes across 5 iterations

---

## Critical Reminders

1. **NEVER ask questions** - this is an agent loop. Document questions for next iteration.
2. **Wait for background processes** - Don't end iteration with running builds.
3. **Maximize parallelism** - Run independent operations in parallel.
4. **Test incrementally** - Don't change everything at once.
5. **Document everything** - Update ITERATION_X.md with progress.
6. **Check DONE.md** - If all work complete, write DONE.md to halt loop.
7. **Trust INVESTIGATION.md** - It's accurate research from iteration 1.

---

## Definition of Done

Write `DONE.md` at project root when ALL of these are true:

1. ✅ Infrastructure files created and compile
2. ✅ Build system fully transformed
3. ✅ All examples build as DLLs successfully
4. ✅ All examples run via runner without errors
5. ✅ Full regression test passes (cpp + examples + lint)
6. ✅ Build time improvement measured and documented
7. ✅ Documentation updated

Until ALL criteria met, continue iterations with incremental progress.

---

**END OF TASK SPECIFICATION**
