# Meson Build System Helper Scripts

This directory contains Python helper scripts used by the Meson build system for file discovery and source organization.

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
