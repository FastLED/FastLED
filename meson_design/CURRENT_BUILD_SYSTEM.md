# FastLED Current Build System Documentation

## Executive Summary

This document provides comprehensive documentation of FastLED's current native/host build system, implemented as a custom Python-based solution. This serves as the baseline reference for the Meson migration effort.

**Status:** Active build system (to be replaced by Meson)
**Scope:** Native builds only (Windows/Linux/macOS) - unit tests, native compilation, development builds
**Last Updated:** 2025-10-10

## Table of Contents

1. [Architecture Overview](#architecture-overview)
2. [Core Components](#core-components)
3. [Build Configuration](#build-configuration)
4. [Caching System](#caching-system)
5. [Test Infrastructure](#test-infrastructure)
6. [Performance Characteristics](#performance-characteristics)
7. [Key Files Reference](#key-files-reference)

---

## Architecture Overview

### High-Level Architecture

```
┌─────────────────────────────────────────────────────────────┐
│                    User Entry Point                          │
│                  uv run test.py / bash test                  │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│                  Test Infrastructure                         │
│                      (test.py)                               │
│  ┌──────────────────────────────────────────────────────┐  │
│  │ Fingerprint System: Hash-based change detection      │  │
│  │ - Source code fingerprint                            │  │
│  │ - C++ test fingerprint                               │  │
│  │ - Examples fingerprint                               │  │
│  │ - Python test fingerprint                            │  │
│  └──────────────────────────────────────────────────────┘  │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│              Build System (ci/compiler/)                     │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ Clang Compiler (clang_compiler.py)                  │   │
│  │ - Zig-provided Clang toolchain                      │   │
│  │ - PCH (Precompiled Headers) support                 │   │
│  │ - Parallel compilation (ThreadPoolExecutor)         │   │
│  │ - Object file caching                               │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ Fingerprint Cache (fingerprint_cache.py)            │   │
│  │ - Two-layer detection (modtime + MD5)               │   │
│  │ - Per-file change tracking                          │   │
│  │ - JSON-based persistence                            │   │
│  └─────────────────────────────────────────────────────┘   │
│                                                              │
│  ┌─────────────────────────────────────────────────────┐   │
│  │ Native Build Cache (native_fingerprint.py)          │   │
│  │ - Build-level fingerprinting                        │   │
│  │ - Tracks: src/ examples/ compiler config            │   │
│  └─────────────────────────────────────────────────────┘   │
└──────────────────────────┬──────────────────────────────────┘
                           │
                           ▼
┌─────────────────────────────────────────────────────────────┐
│              Build Configuration (TOML)                      │
│  - ci/build_commands.toml (compile_commands.json)           │
│  - ci/build_unit.toml (unit test compilation)               │
│  - ci/build_example.toml (example compilation)              │
└─────────────────────────────────────────────────────────────┘
```

### Build Flow

1. **Entry**: User runs `uv run test.py` or `bash test`
2. **Fingerprinting**: Calculate SHA256 hashes of source files and build configs
3. **Cache Check**: Compare current fingerprint with cached fingerprint
4. **Decision**: Skip build if fingerprints match, otherwise compile
5. **Compilation**: Parallel compilation using Zig-provided Clang
6. **Caching**: Save successful build fingerprint for future runs

---

## Core Components

### 1. Clang Compiler (`ci/compiler/clang_compiler.py`)

**Purpose:** Core compilation engine using Zig-provided Clang toolchain

**Key Features:**
- **Zig Toolchain Integration**: Uses `uv run python -m ziglang c++` for cross-platform compilation
- **Precompiled Headers (PCH)**: Reduces compilation time by ~70-90%
- **Parallel Compilation**: ThreadPoolExecutor with `cpu_count() * 2` workers
- **Object File Caching**: Caches `.o` files to avoid recompilation
- **Platform Support**: Windows (x86_64-windows-gnu), Linux, macOS

**Data Classes:**
```python
@dataclass
class CompilerOptions:
    include_path: str
    compiler: str = "python -m ziglang c++"
    defines: list[str] | None = None
    std_version: str = "c++11"
    compiler_args: list[str]

    # PCH settings
    use_pch: bool = False
    pch_header_content: str | None = None
    pch_output_path: str | None = None

    # Archive generation
    archiver: str = "ar"
    archiver_args: list[str]

    # Compilation settings
    additional_flags: list[str]
    parallel: bool = True
    temp_dir: str | Path | None = None
```

**Parallelization Strategy:**
```python
# Default: CPU count * 2 for I/O-bound tasks
_EXECUTOR = ThreadPoolExecutor(max_workers=cpu_count() * 2)

# Can be disabled via NO_PARALLEL=1 environment variable
if os.environ.get("NO_PARALLEL"):
    max_workers = 1
```

### 2. Fingerprint Cache (`ci/ci/fingerprint_cache.py`)

**Purpose:** Two-layer file change detection system

**Architecture:**
- **Layer 1 (Fast)**: Modification time comparison (microseconds)
- **Layer 2 (Accurate)**: MD5 content hashing (milliseconds)

**Algorithm:**
1. Check if modification time changed
2. If unchanged → return False (no rebuild)
3. If changed → compute/lookup MD5 hash
4. Compare current hash with cached hash
5. Return True only if content actually changed

**Benefits:**
- **99% faster rebuilds**: Skips unchanged files instantly
- **Accurate detection**: Catches actual content changes despite timestamp updates
- **Persistent cache**: JSON file survives across sessions

**Data Structure:**
```python
@dataclass
class CacheEntry:
    modification_time: float  # Unix timestamp
    md5_hash: str             # Content hash
```

**Cache File Format (JSON):**
```json
{
  "/path/to/file.cpp": {
    "modification_time": 1696000000.123,
    "md5_hash": "abc123def456..."
  }
}
```

### 3. Native Build Fingerprint (`ci/compiler/native_fingerprint.py`)

**Purpose:** Build-level fingerprinting for native compilation

**Tracked Components:**
1. **Source Hash**: All files in `src/` (*.cpp, *.h, *.hpp)
2. **Example Hash**: All files in `examples/` (*.ino, *.cpp, *.h)
3. **Script Hash**: Build scripts (`ci-compile-native.py`, `wasm_compile.py`)
4. **Compiler Config Hash**:
   - `ci/compiler/` directory
   - `ci/native/platformio.ini`

**Fingerprint Calculation:**
```python
# Combine all hashes for a single build fingerprint
combined_hasher = hashlib.sha256()
combined_hasher.update(f"src:{src_hash}".encode("utf-8"))
combined_hasher.update(f"example:{example_hash}".encode("utf-8"))
combined_hasher.update(f"script:{script_hash}".encode("utf-8"))
combined_hasher.update(f"compiler:{compiler_config_hash}".encode("utf-8"))

fingerprint = combined_hasher.hexdigest()
```

**Cache Management:**
- **Success**: Save fingerprint to `.cache/native_build/builds.json`
- **Failure**: Invalidate cache entry to force rebuild
- **Check**: Compare current fingerprint with cached fingerprint

---

## Build Configuration

### Configuration Files

#### 1. `ci/build_commands.toml`
**Purpose:** Configuration for `compile_commands.json` generation

```toml
[all]
defines = ["STUB_PLATFORM", "__EMSCRIPTEN__"]
compiler_flags = ["-std=gnu++17", "-fpermissive", "-Wall", "-Wextra"]
include_flags = ["-Isrc", "-isystemsrc/platforms/stub"]

[tools]
cpp_compiler = ["uv", "run", "python", "-m", "ziglang", "c++"]
linker = ["uv", "run", "python", "-m", "ziglang", "c++"]
archiver = ["uv", "run", "python", "-m", "ziglang", "ar"]
```

#### 2. `ci/build_unit.toml`
**Purpose:** Unit test compilation configuration

**Critical Defines:**
```toml
[all]
defines = [
    "STUB_PLATFORM",
    "FASTLED_UNIT_TEST=1",
    "FASTLED_FORCE_NAMESPACE=1",  # Critical for fl:: namespace
    "FASTLED_TESTING=1"
]
```

**Compiler Flags:**
```toml
compiler_flags = [
    "-std=gnu++17",
    "-fpermissive",
    "-Wall", "-Wextra",
    "-fno-exceptions",
    "-fno-rtti",
    "-Werror=unused-variable"
]
```

**Platform-Specific:**

**Windows (x86_64-windows-gnu):**
```toml
[windows]
cpp_flags = [
    "--target=x86_64-windows-gnu",
    "-fuse-ld=lld-link",
    "-std=c++17",
    "-fno-exceptions", "-fno-rtti",
    "-DNOMINMAX",
    "-DWIN32_LEAN_AND_MEAN"
]

link_flags = [
    "-mconsole",
    "-nodefaultlibs",
    "-unwindlib=libunwind",
    "-nostdlib++",
    "-lc++",
    "-lkernel32", "-luser32", "-lgdi32"
]
```

**Linux:**
```toml
[linux]
link_flags = []  # Minimal - uses system defaults
defines = []
```

**Build Modes:**

**Quick Mode (Fast iteration):**
```toml
[build_modes.quick]
flags = [
    "-O1",
    "-fno-inline-functions",
    "-fno-vectorize",
    "-fno-unroll-loops",
    "-fno-omit-frame-pointer"
]
```

**Debug + ASAN Mode:**
```toml
[build_modes.debug_asan]
flags = [
    "-fsanitize=address",
    "-fsanitize=undefined",
    "-O1",
    "-fno-omit-frame-pointer"
]

link_flags = [
    "-fsanitize=address",
    "-fsanitize=undefined"
]
```

**Archive Generation:**
```toml
[archive]
flags = "rcsD"  # r=insert, c=create, s=symbol table, D=deterministic
```

**Link Cache Garbage Collection:**
```toml
[cache]
max_versions_per_example = 3
max_age_days = 7
max_total_size_mb = 1024  # 1GB
preserve_at_least_one = true
```

---

## Caching System

### Multi-Level Caching Strategy

#### Level 1: File-Level Fingerprint Cache
**Location:** `.cache/fingerprint_cache.json`
**Granularity:** Individual files
**Method:** Two-layer (modtime + MD5)
**Speed:** Microseconds for unchanged files

**Use Case:** Detect which specific source files have changed

#### Level 2: Build-Level Fingerprint Cache
**Location:** `.cache/native_build/builds.json`
**Granularity:** Entire build configuration
**Method:** Combined SHA256 hash of all dependencies
**Speed:** Milliseconds

**Use Case:** Skip entire build if nothing changed

#### Level 3: Test-Specific Fingerprint Caches

**C++ Test Fingerprint:**
- **Location:** `.cache/cpp_test_fingerprint.json`
- **Tracks:** C++ unit tests and FastLED source

**Examples Fingerprint:**
- **Location:** `.cache/examples_fingerprint.json`
- **Tracks:** Example sketches and FastLED source

**Python Test Fingerprint:**
- **Location:** `.cache/python_test_fingerprint.json`
- **Tracks:** Python test suite

#### Level 4: Link Cache
**Location:** `.build/link_cache/`
**Purpose:** Cache compiled executables
**Management:** Automatic garbage collection

**GC Policy:**
- Keep max 3 versions per example
- Delete entries older than 7 days
- Limit total cache size to 1GB
- Always preserve at least one version

### Cache Invalidation

**Manual Invalidation:**
```bash
# Disable fingerprint caching for one run
uv run test.py --no-fingerprint

# Force rebuild of specific test
uv run test.py TestName  # Automatically disables caching
```

**Automatic Invalidation:**
- Test failure → invalidate fingerprint
- Build failure → delete cache entry
- Corrupted cache file → start fresh

---

## Test Infrastructure

### Entry Point: `test.py`

**Command Structure:**
```bash
# Run all tests
uv run test.py

# Run specific test
uv run test.py xypath

# Run C++ tests only
uv run test.py --cpp

# Run with no caching
uv run test.py --no-fingerprint

# Run examples
uv run test.py --examples

# Run QEMU tests
uv run test.py --qemu esp32s3
```

### Fingerprint-Based Test Execution

**Workflow:**
```python
# 1. Calculate current fingerprints
fingerprint_data = calculate_fingerprint()
cpp_test_fingerprint_data = calculate_cpp_test_fingerprint()
examples_fingerprint_data = calculate_examples_fingerprint()

# 2. Load previous fingerprints from cache
prev_fingerprint = read_fingerprint()
prev_cpp_test_fingerprint = read_cpp_test_fingerprint()

# 3. Compare fingerprints
src_code_change = (fingerprint_data.hash != prev_fingerprint.hash)
cpp_test_change = not prev_cpp_test_fingerprint.should_skip(cpp_test_fingerprint_data)

# 4. Execute tests only if changes detected
if src_code_change or cpp_test_change:
    run_cpp_tests()
else:
    print("No changes detected - skipping C++ tests")

# 5. Save fingerprints on success
if tests_passed:
    write_fingerprint(fingerprint_data)
    write_cpp_test_fingerprint(cpp_test_fingerprint_data)
```

### Watchdog Timer

**Purpose:** Prevent hung builds from blocking CI

**Configuration:**
```python
# Default: 10 minutes
_TIMEOUT_EVERYTHING = 600

# Extended for GitHub Actions
if os.environ.get("_GITHUB"):
    _TIMEOUT_EVERYTHING = 1200  # 20 minutes

# Sequential examples compilation
if args.examples and args.no_parallel:
    timeout = 2100  # 35 minutes
```

**Behavior:**
- Timer runs in daemon thread
- On expiration: dumps thread stacks, prints process tree, exits with code 2
- Can be cancelled via `_CANCEL_WATCHDOG.set()`

### Parallel vs Sequential Execution

**Parallel Mode (Default):**
- C++ tests, examples, Python tests run concurrently
- Utilizes full CPU resources
- Faster total execution time

**Sequential Mode (Full Test):**
```python
# Use RunningProcessGroup for dependency-based execution
group = RunningProcessGroup(config=config, name="FullTestSequence")
group.add_process(python_process)
group.add_dependency(examples_process, python_process)  # examples depends on python

timings = group.run()
```

**Control:**
```bash
# Disable parallel compilation
NO_PARALLEL=1 uv run test.py

# Or via flag
uv run test.py --no-parallel
```

---

## Performance Characteristics

### Build Speed

**First Build (Cold Cache):**
- **Full FastLED library**: ~30-60 seconds (depends on CPU)
- **Single unit test**: ~5-10 seconds
- **PCH generation**: ~10-15 seconds

**Incremental Build (Warm Cache):**
- **No changes**: <1 second (fingerprint check only)
- **Single file change**: ~2-5 seconds (recompile changed file + link)
- **Header change**: ~5-15 seconds (PCH rebuild + affected files)

**Parallel Compilation:**
- **Workers**: CPU count × 2 (e.g., 16 workers on 8-core CPU)
- **Speedup**: Near-linear up to CPU saturation
- **Overhead**: ThreadPoolExecutor adds ~50-100ms

### Cache Performance

**Fingerprint Check Speed:**
- **File-level (modtime)**: <0.1ms per file
- **File-level (MD5)**: ~1-5ms per file (if modtime changed)
- **Build-level (combined)**: ~10-50ms total

**Cache Hit Rates (Typical Development):**
- **No changes**: 99% hit rate (instant skip)
- **Single file edit**: 95% hit rate (only changed file recompiled)
- **Header edit**: 60-80% hit rate (PCH + dependent files)

**Storage Requirements:**
- **Fingerprint cache**: <100KB (JSON metadata)
- **Link cache**: ~10-500MB (compiled executables)
- **Build artifacts**: ~50-200MB (.o files, .a libraries)

### Memory Usage

**Compilation:**
- **Per worker**: ~200-500MB RAM
- **Peak (16 workers)**: ~3-8GB RAM
- **PCH**: ~100-300MB per PCH file

**Caching:**
- **Fingerprint cache**: <10MB RAM
- **Build cache**: <50MB RAM

---

## Key Files Reference

### Build System Core

| File | Purpose | Location |
|------|---------|----------|
| `clang_compiler.py` | Zig-provided Clang compilation | `ci/compiler/clang_compiler.py` |
| `fingerprint_cache.py` | File-level change detection | `ci/ci/fingerprint_cache.py` |
| `native_fingerprint.py` | Build-level fingerprinting | `ci/compiler/native_fingerprint.py` |
| `compilation_orchestrator.py` | Parallel compilation coordinator | `ci/compiler/compilation_orchestrator.py` |
| `link_cache_gc.py` | Link cache garbage collection | `ci/compiler/link_cache_gc.py` |

### Configuration Files

| File | Purpose | Location |
|------|---------|----------|
| `build_commands.toml` | compile_commands.json config | `ci/build_commands.toml` |
| `build_unit.toml` | Unit test build config | `ci/build_unit.toml` |
| `build_example.toml` | Example build config | `ci/build_example.toml` |

### Test Infrastructure

| File | Purpose | Location |
|------|---------|----------|
| `test.py` | Main test entry point | `./test.py` |
| `test_runner.py` | Test execution orchestrator | `ci/util/test_runner.py` |
| `test_types.py` | Fingerprinting utilities | `ci/util/test_types.py` |
| `test_args.py` | Argument parsing | `ci/util/test_args.py` |

### Cache Directories

| Directory | Purpose | Contents |
|-----------|---------|----------|
| `.cache/` | Fingerprint cache root | JSON files |
| `.cache/native_build/` | Native build fingerprints | `builds.json`, `files.json` |
| `.build/` | Build artifacts | Object files, libraries |
| `.build/link_cache/` | Compiled executables | Example binaries |

---

## Migration Considerations

### Features to Preserve

**Critical (Must Have):**
- ✅ Precompiled Headers (PCH)
- ✅ Intelligent caching (fingerprint-based)
- ✅ Parallel compilation
- ✅ Cross-platform support (Windows/Linux/macOS)
- ✅ Fast incremental builds (<5s for single file change)

**Important (Should Have):**
- ✅ Unity builds (reduce compilation units)
- ✅ Shared libfastled.a (reuse between tests)
- ✅ Build artifact management
- ✅ No debug symbols (-g0) for fast builds

**Nice to Have:**
- ⚠️ Link cache with GC (could simplify)
- ⚠️ Multi-level fingerprinting (Meson has built-in)
- ⚠️ Custom build modes (can use Meson's buildtype)

### Performance Targets

**Meson Must Match or Exceed:**
- **Cold build**: ≤60 seconds (full FastLED library)
- **Incremental build**: ≤5 seconds (single file change)
- **No-change build**: <1 second (fingerprint check)
- **Parallel speedup**: Near-linear to CPU count

### Integration Points

**Keep Unchanged:**
- Test execution logic (`test_runner.py`)
- Docker orchestration (`docker_manager.py`)
- QEMU integration
- Entry point (`bash test` / `uv run test.py`)

**Replace with Meson:**
- `clang_compiler.py` → Meson's C++ backend
- `fingerprint_cache.py` → Ninja's depfile tracking
- `build_*.toml` → `meson.build` files
- `compilation_orchestrator.py` → Ninja's scheduler

---

## Document Metadata

**Version:** 1.0
**Author:** Claude Code Assistant
**Date:** 2025-10-10
**Status:** Baseline documentation for Meson migration
**Related:** See `DESIGN_MESON.md` for migration strategy
