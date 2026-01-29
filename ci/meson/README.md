# Meson Build System Integration Package

This directory contains the complete Meson build system integration for FastLED, refactored from the monolithic `ci/util/meson_runner.py` (2,760 lines) into focused modules for better maintainability.

## Package Structure

The `ci.meson` package is organized into 9 focused modules following a tier-based dependency hierarchy:

### Tier 1: Leaf Modules (No Internal Dependencies)

- **`output.py`** (~100 lines) - Output formatting and colored printing
  - Functions: `_print_success()`, `_print_error()`, `_print_warning()`, `_print_info()`, `_print_banner()`

- **`io_utils.py`** (~90 lines) - Safe file I/O operations
  - Functions: `atomic_write_text()`, `write_if_different()`

- **`compiler.py`** (~150 lines) - Compiler detection and Meson executable resolution
  - Functions: `get_meson_executable()`, `check_meson_installed()`, `get_compiler_version()`

### Tier 2: Basic Operations

- **`test_discovery.py`** (~140 lines) - Test discovery and fuzzy matching
  - Functions: `get_fuzzy_test_candidates()`, `get_source_files_hash()`

### Tier 3: Core Operations

- **`build_config.py`** (~650 lines) - Build configuration and setup
  - Types: `CleanupResult` dataclass
  - Functions: `cleanup_build_artifacts()`, `detect_system_llvm_tools()`, `setup_meson_build()`, `perform_ninja_maintenance()`

- **`compile.py`** (~280 lines) - Compilation execution with IWYU support
  - Functions: `compile_meson()`, `_create_error_context_filter()`

- **`test_execution.py`** (~150 lines) - Individual test execution
  - Types: `MesonTestResult` dataclass
  - Functions: `run_meson_test()`

### Tier 4: Advanced Operations

- **`streaming.py`** (~300 lines) - Parallel streaming compilation and testing
  - Functions: `stream_compile_and_run_tests()`

### Tier 5: Orchestration

- **`runner.py`** (~450 lines) - Main orchestration and CLI entry point
  - Functions: `run_meson_build_and_test()`, `main()`

## Migration Guide

The old `ci.util.meson_runner` module is now a backward compatibility shim that shows a deprecation warning. Update your imports as follows:

```python
# OLD (deprecated)
from ci.util.meson_runner import MesonTestResult
from ci.util.meson_runner import run_meson_build_and_test
from ci.util.meson_runner import setup_meson_build
from ci.util.meson_runner import _print_banner

# NEW (modular imports)
from ci.meson.test_execution import MesonTestResult
from ci.meson.runner import run_meson_build_and_test
from ci.meson.build_config import setup_meson_build
from ci.meson.output import _print_banner

# OR use package-level exports for common functions
from ci.meson import (
    MesonTestResult,
    run_meson_build_and_test,
    setup_meson_build,
    check_meson_installed,
    get_meson_executable,
    perform_ninja_maintenance,
    _print_banner,
)
```

### Updated Client Files

The following files have been migrated to the new module structure:
- ✅ `ci/util/meson_example_runner.py`
- ✅ `ci/util/test_args.py`
- ✅ `ci/util/test_runner.py`

## Public API

The `ci.meson` package exports these public APIs:

| Export | Source Module | Description |
|--------|---------------|-------------|
| `MesonTestResult` | `test_execution` | Test result dataclass with success/duration/counts |
| `run_meson_build_and_test()` | `runner` | Main build and test orchestration function |
| `setup_meson_build()` | `build_config` | Set up Meson build directory with configuration |
| `perform_ninja_maintenance()` | `build_config` | Periodic Ninja dependency database optimization |
| `check_meson_installed()` | `compiler` | Check if Meson is installed and accessible |
| `get_meson_executable()` | `compiler` | Resolve Meson executable path (venv preferred) |
| `_print_banner()` | `output` | Print section separator banner |

## Dependency Graph

```
runner.py (Tier 5)
  ├─> streaming.py (Tier 4)
  │   ├─> compile.py (Tier 3)
  │   ├─> test_execution.py (Tier 3)
  │   ├─> compiler.py (Tier 1)
  │   └─> output.py (Tier 1)
  ├─> build_config.py (Tier 3)
  │   ├─> test_discovery.py (Tier 2)
  │   │   └─> compiler.py (Tier 1)
  │   ├─> io_utils.py (Tier 1)
  │   ├─> compiler.py (Tier 1)
  │   └─> output.py (Tier 1)
  ├─> compile.py (Tier 3)
  ├─> test_execution.py (Tier 3)
  └─> test_discovery.py (Tier 2)
```

## Helper Scripts

This directory also contains Python helper scripts used by the Meson build system for file discovery and source organization.

## Active Scripts

### `rglob.py` - Recursive File Pattern Matching

Platform-agnostic recursive glob utility for discovering source files.

**Usage:**
```bash
python rglob.py <directory> <pattern> [exclude_path]
```

**Arguments:**
- `directory`: Root directory to search from
- `pattern`: Glob pattern to match (e.g., `*.cpp`, `*.h`)
- `exclude_path`: Optional path substring to exclude from results

**Output:**
- One file path per line (using forward slashes for cross-platform compatibility)

**Example (from meson.build):**
```meson
# Discover all .cpp files in src/, excluding platforms/shared
discovered_sources = run_command(
  uv_prog, 'run', 'python', 'ci/meson/rglob.py', 'src', '*.cpp', 'platforms/shared',
  check: true,
  capture: true
)
fastled_sources = files(discovered_sources.stdout().strip().split('\n'))
```

**Why this uses Python:** File discovery is an **imperative** operation (search filesystem), not declarative configuration. Meson has no native recursive glob, so Python is the appropriate tool.

## Build Flag Configuration (Centralized in meson.build)

**All compilation and linker flags are now defined directly in `meson.build`** (lines 44-201). This follows Meson best practices by keeping declarative configuration in build files, not external scripts.

### Build Mode Flags (meson.build:100-135)

Build mode is selected via the `build_mode` option (defined in `meson.options`):

| Mode | Optimization | Debug Info | Sanitizers | Use Case |
|------|-------------|-----------|-----------|----------|
| `quick` | `-O0` | `-g1` (minimal) | None | Default - fast compilation, basic stack traces |
| `debug` | `-O0` | `-g3` (full) | ASan + UBSan | Deep debugging with memory error detection |
| `release` | `-O3` | None | None | Production builds with maximum optimization |

**Common flags (all modes):**
- `-std=gnu++11` - GNU C++11 standard (ABI compatibility)
- `-fno-exceptions` - Disable C++ exceptions
- `-fno-rtti` - Disable runtime type information
- `-fno-omit-frame-pointer` - Keep frame pointers for stack traces
- `-fno-strict-aliasing` - Prevent strict aliasing optimizations

**Selecting build mode:**
```bash
# Default mode (quick - fast compilation)
uv run test.py --examples Blink

# Debug mode (full symbols + sanitizers)
uv run test.py --examples Blink --debug

# Manual Meson configuration
meson setup -Dbuild_mode=debug builddir
meson setup -Dbuild_mode=release builddir
```

### Platform-Specific Link Flags (meson.build:155-201)

Platform detection and linker configuration is handled via Meson conditionals:

| Platform | Link Type | Flags |
|----------|-----------|-------|
| Windows | `base` | Console app, custom stdlib (libc++), Windows libs, 16MB stack |
| Windows | `test` | Base flags + debug libraries (dbghelp, psapi) for crash handling |
| Windows | `example` | Same as base |
| Unix/Linux/macOS | all types | `-pthread` (POSIX threads), `-rdynamic` (backtrace symbols) |

### Compilation Flag Organization (meson.build:64-138)

The root `meson.build` organizes compilation flags into three groups for maintainability:

#### 1. Preprocessor Defines (`base_defines`)
Platform and feature configuration macros:
- `-DFASTLED_USE_PROGMEM=0`
- `-DSTUB_PLATFORM`
- `-DARDUINO=10808`
- `-DFASTLED_TESTING` (ABI-critical: controls InlinedMemoryBlock layout)

#### 2. Warning Flags (`base_warnings`)
Code quality enforcement:
- `-Wall`, `-Wextra`
- `-Werror=unused-variable`
- `-Werror=c++14-extensions` (enforce C++11 compatibility)

#### 3. Optimization Flags (`base_optimization`)
Mode-dependent code generation (see Build Mode Flags above)

**Final assembly:**
```meson
base_compile_args = base_optimization + base_defines + base_warnings
unit_test_compile_args = base_compile_args + test_specific_args
example_compile_args = base_compile_args + example_specific_args
```

This separation enables:
- Easy mode switching (debug/quick/release)
- Clear understanding of build configuration
- Selective flag modification without touching unrelated flags

## Deprecated Scripts

See `.deprecated/README.md` for information about previously used Python scripts that generated build and link flags. These have been **moved into `meson.build`** as of Nov 7, 2025.

## Best Practices

Following [CLAUDE.md Meson Best Practices](../../CLAUDE.md):

1. **NO Embedded Python Scripts** - Extract to standalone files in `ci/meson/`
2. **Configuration as Data** - Define settings in Python config files, not meson.build
3. **Extract Complex Logic to Python** - Meson is declarative, Python handles logic
4. **Testable, Type-Checked Scripts** - All scripts should have type hints and be testable

## Testing

To verify the build system changes:
```bash
# Run all tests
uv run test.py

# Run C++ tests only
uv run test.py --cpp

# Compile specific examples
uv run test.py --examples Blink DemoReel100
```
