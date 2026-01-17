# FastLED Fingerprint Cache System

Centralized fingerprinting and caching system for efficient change detection across the FastLED build system.

## Overview

The fingerprint cache system has been refactored and centralized in `ci/fingerprint/` to provide:

1. **Unified API**: All caches use consistent interfaces (`check_needs_update()`, `mark_success()`, `invalidate()`)
2. **Centralized Configuration**: Cache settings and rules defined in one place
3. **Multiple Strategies**: Choose the right cache type for your use case
4. **Safe Pattern**: Pre-computed fingerprints immune to race conditions

## Module Structure

```
ci/fingerprint/
├── __init__.py       # Package exports
├── README.md         # This file
├── core.py           # Core cache implementations
├── config.py         # Cache configuration and presets
└── rules.py          # Invalidation rules and policies
```

## Quick Start

### Using Predefined Caches

```python
from ci.fingerprint import TwoLayerFingerprintCache
from ci.fingerprint.config import get_cache_config

# Get configuration for C++ linting cache
config = get_cache_config("cpp_lint")

# Create cache instance
cache = TwoLayerFingerprintCache(config.cache_dir, config.name)

# Check if files need processing
file_paths = [Path("src/main.cpp"), Path("src/utils.h")]
if cache.check_needs_update(file_paths):
    # Files changed - run linting
    run_linting(file_paths)
    # Mark success after linting completes
    cache.mark_success()
else:
    print("No changes detected - skipping linting")
```

### Build Mode Support

For caches that depend on build mode (quick/debug/release):

```python
from ci.fingerprint.config import get_cache_config

# Get configuration with build mode
config = get_cache_config("cpp_test", build_mode="debug")

# Cache file will be: .cache/fingerprint/cpp_test_debug.json
print(config.get_cache_filename())  # "cpp_test_debug.json"
```

## Cache Types

### 1. TwoLayerFingerprintCache (Recommended for Linting)

Efficient two-layer detection (mtime + MD5) for batch file operations.

**Use when:**
- Monitoring many files (dozens to hundreds)
- Need fast cache validation (< 1ms for unchanged files)
- Files may be touched without content changes (git operations)

**Features:**
- Layer 1: Fast mtime comparison
- Layer 2: MD5 content verification
- Smart mtime updates (avoids rehashing touched files)
- File locking for concurrent access

**Example:**
```python
from ci.fingerprint import TwoLayerFingerprintCache

cache = TwoLayerFingerprintCache(Path(".cache"), "cpp_lint")
needs_update = cache.check_needs_update(file_paths)
```

### 2. HashFingerprintCache (Recommended for Build Systems)

Single SHA256 hash for all files, optimized for concurrent access.

**Use when:**
- Need single cache decision for entire project
- Concurrent processes may validate simultaneously
- Timestamp tracking is useful

**Features:**
- Single SHA256 hash of all file paths + mtimes
- File locking prevents race conditions
- Optional timestamp file for change detection
- Compare-and-swap for safe updates

**Example:**
```python
from ci.fingerprint import HashFingerprintCache

cache = HashFingerprintCache(Path(".cache"), "examples")
needs_update = cache.check_needs_update(file_paths)
```

### 3. FingerprintCache (Legacy)

Original per-file tracking system. Kept for backward compatibility.

**Use when:**
- Existing code depends on it
- Need per-file change tracking API

**Features:**
- Per-file mtime + MD5 cache
- `has_changed(path, previous_mtime)` API
- No file locking (single-process only)

**Note:** For new code, prefer `TwoLayerFingerprintCache`.

## Configuration System

### Predefined Cache Configurations

The `config.py` module defines standard caches:

| Cache Name | Type | Build Modes | Description |
|------------|------|-------------|-------------|
| `cpp_lint` | TwoLayer | No | C++ linting (clang-format + custom) |
| `python_lint` | TwoLayer | No | Python type checking (pyright) |
| `js_lint` | TwoLayer | No | JavaScript/TypeScript linting |
| `cpp_test` | Hash | Yes | C++ unit tests |
| `python_test` | Hash | No | Python unit tests |
| `examples` | Hash | Yes | Example compilation |
| `all` | Hash | No | Entire src/ directory |

### Custom Cache Configuration

```python
from ci.fingerprint.config import CacheConfig, CacheType, BuildMode
from pathlib import Path

config = CacheConfig(
    name="my_custom_cache",
    cache_type=CacheType.TWO_LAYER,
    cache_dir=Path(".cache"),
    build_mode=BuildMode.DEBUG,
    description="My custom cache for XYZ"
)

# Cache file: .cache/fingerprint/my_custom_cache_debug.json
cache_path = config.get_cache_path()
```

## Invalidation Rules

The `rules.py` module centralizes cache invalidation logic.

### Skip Logic

Operations are **skipped** when:
1. Hash matches previous run (no code changes)
2. Previous status was `"success"`
3. Not forced by user (`--no-fingerprint` flag)

Operations **run** when:
1. Hash differs (code changed)
2. Previous status was `"failure"` (retry failed tests)
3. No previous cache exists (first run)
4. User forced operation (`--no-fingerprint`)

### Monitored Files

Each cache defines what files it monitors:

```python
from ci.fingerprint.rules import CacheInvalidationRules

# Get monitored files for C++ linting
patterns = CacheInvalidationRules.get_monitored_files_for_cache("cpp_lint")
# Returns: ["src/**/*.cpp", "src/**/*.h", "examples/**/*.ino", ...]

# Get exclude patterns
excludes = CacheInvalidationRules.excludes_for_cache("cpp_lint")
# Returns: [".cache/", ".build/", ".venv/", ...]
```

## Safe Pattern: Pre-Computed Fingerprints

All caches follow the safe pattern to avoid race conditions:

```python
# 1. Compute and store fingerprint BEFORE processing
if cache.check_needs_update(file_paths):
    # Fingerprint is stored internally

    # 2. Run the operation (files may change during this)
    run_operation()

    # 3. Save the pre-computed fingerprint (immune to changes during step 2)
    cache.mark_success()
```

**Why this works:**
- Fingerprint is computed before processing starts
- Stored in temporary `.pending` file
- `mark_success()` saves the pre-computed value
- Race-safe even if files change during processing

## Migration Guide

### Migrating from Old Imports

**Before:**
```python
from ci.util.two_layer_fingerprint_cache import TwoLayerFingerprintCache
from ci.util.hash_fingerprint_cache import HashFingerprintCache
from ci.ci.fingerprint_cache import FingerprintCache
```

**After:**
```python
from ci.fingerprint import (
    TwoLayerFingerprintCache,
    HashFingerprintCache,
    FingerprintCache,
)
```

### Migrating Cache Configuration

**Before:**
```python
cache_dir = Path(".cache")
cache = TwoLayerFingerprintCache(cache_dir, "cpp_lint")
```

**After:**
```python
from ci.fingerprint.config import get_cache_config

config = get_cache_config("cpp_lint")
cache = TwoLayerFingerprintCache(config.cache_dir, config.name)
```

## Performance

| Operation | Time | Notes |
|-----------|------|-------|
| Cache hit (mtime match) | < 1ms | Fast path - no hashing |
| Fingerprint calculation (src/) | ~2-5ms | TwoLayer or Hash |
| Fingerprint calculation (examples/) | ~10-20ms | More files to process |
| Full Python fingerprint | ~50-100ms | Includes dependency scanning |

## Concurrent Access

Both `TwoLayerFingerprintCache` and `HashFingerprintCache` support concurrent access:

- **File locking**: Uses `fasteners.InterProcessLock`
- **Pending pattern**: Temporary `.pending` files prevent corruption
- **Atomic updates**: Compare-and-swap prevents race conditions

**Safe for:**
- Multiple test runners in parallel
- CI/CD pipelines with concurrent jobs
- Background linting + active development

## Troubleshooting

### Cache always invalidates despite no changes

**Possible causes:**
1. Previous run failed (status: "failure" in cache file)
2. Git operations changed file mtimes
3. File syncing tools touched files

**Solutions:**
- Check cache file: `.cache/fingerprint/<name>.json`
- Use `--no-fingerprint` to force rebuild and clear state
- TwoLayer cache automatically handles touched files (updates mtime without invalidation)

### Cache not created after successful run

**Possible causes:**
1. `.cache/` directory not writable
2. Process terminated before `mark_success()` called
3. Exception during operation

**Solutions:**
- Ensure `.cache/` exists and is writable
- Check that `mark_success()` is called in finally block or error handler
- Review operation logs for exceptions

## Adding New Caches

To add a new cache type:

1. **Add configuration** in `config.py`:
   ```python
   "my_cache": CacheConfig(
       name="my_cache",
       cache_type=CacheType.TWO_LAYER,
       cache_dir=cache_dir,
       description="My cache description",
   ),
   ```

2. **Add monitored files** in `rules.py`:
   ```python
   "my_cache": [
       "my_dir/**/*.ext",
       "config.yaml",
   ],
   ```

3. **Use in code**:
   ```python
   from ci.fingerprint.config import get_cache_config
   from ci.fingerprint import TwoLayerFingerprintCache

   config = get_cache_config("my_cache")
   cache = TwoLayerFingerprintCache(config.cache_dir, config.name)
   ```

## Design Principles

1. **Fail-safe**: Corrupted cache → rebuild (never skip incorrectly)
2. **Concurrent-safe**: Multiple processes can safely validate simultaneously
3. **Race-safe**: Pre-computed fingerprints immune to changes during processing
4. **Fast path optimized**: Unchanged files validated in < 1ms
5. **Consistent API**: All caches follow same pattern (`check_needs_update`, `mark_success`, `invalidate`)
