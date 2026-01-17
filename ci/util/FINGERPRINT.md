# FastLED Fingerprint System

⚠️ **IMPORTANT**: The fingerprint system has been refactored and centralized in `ci/fingerprint/`.

**For detailed documentation, see:** [`ci/fingerprint/README.md`](../../fingerprint/README.md)

This file provides a quick reference for common operations. The fingerprint system provides intelligent caching to skip redundant test runs when code hasn't changed. It uses multi-layered change detection combining fast modification time checks with accurate content hashing.

## Quick Reference

```bash
# Normal usage (fingerprinting enabled by default)
uv run test.py --cpp                    # Uses .cache/fingerprint/cpp_test_quick.json
uv run test.py --examples               # Uses .cache/fingerprint/examples_quick.json

# Build mode separation (separate caches per mode)
uv run test.py --cpp --build-mode quick     # cpp_test_quick.json
uv run test.py --cpp --build-mode debug     # cpp_test_debug.json
uv run test.py --cpp --build-mode release   # cpp_test_release.json

# Disable fingerprinting
uv run test.py --no-fingerprint         # Force full rebuild
uv run test.py --force                  # Same as --no-fingerprint

# Clear cache manually
rm -rf .cache/fingerprint/
```

## Cache Directory Structure

```
.cache/fingerprint/
├── all.json                  # Entire codebase fingerprint
├── cpp_test_quick.json       # C++ tests (quick mode)
├── cpp_test_debug.json       # C++ tests (debug mode with sanitizers)
├── cpp_test_release.json     # C++ tests (release mode)
├── examples_quick.json       # Examples (quick mode)
├── examples_debug.json       # Examples (debug mode)
├── examples_release.json     # Examples (release mode)
└── python_test.json          # Python tests (no build modes)
```

## File Format

Each fingerprint JSON file contains:

```json
{
  "hash": "a1b2c3d4e5f6...",
  "elapsed_seconds": "0.25",
  "status": "success"
}
```

- `hash`: SHA256 of all monitored files (path + content)
- `elapsed_seconds`: Time to compute the fingerprint
- `status`: `"success"` or `"failure"` from the last run

## How It Works

### Skip Logic

Tests are **skipped** when:
1. Hash matches previous run (no code changes)
2. Previous status was `"success"`

Tests are **run** when:
1. Hash differs (code changed)
2. Previous status was `"failure"` (retry failed tests)
3. No previous cache exists (first run)
4. `--no-fingerprint` or `--force` flag used

### What Each Fingerprint Monitors

| Fingerprint | Monitored Files |
|-------------|-----------------|
| `all` | `src/**/*.{h,cpp,hpp}` |
| `cpp_test` | `src/**/*`, `tests/**/*`, `meson.build`, `tests/meson.build`, debug flag |
| `examples` | `src/**/*`, `examples/**/*.{ino,h,cpp,hpp}`, `meson.build`, example scripts |
| `python_test` | `ci/tests/**/*.py`, `ci/**/*.py`, `pyproject.toml`, `uv.lock` |

### Build Mode Separation

Build modes (`quick`, `debug`, `release`) create separate cache files because:

- **Quick mode** (`-O0 -g1`): Fast iteration, minimal debug info
- **Debug mode** (`-O0 -g3 -fsanitize=address,undefined`): Full sanitizers, different binary
- **Release mode** (`-O2`): Optimized, different binary

Switching modes without separate caches would cause unnecessary cache invalidation.

## Architecture

**⚠️ NOTE**: The core fingerprint implementations have been moved to `ci/fingerprint/`.

### Core Components

```
ci/fingerprint/               # NEW: Centralized fingerprint system
├── __init__.py              # Package exports
├── README.md                # Comprehensive documentation
├── core.py                  # FingerprintCache, TwoLayerFingerprintCache, HashFingerprintCache
├── config.py                # Cache configuration and presets
└── rules.py                 # Invalidation rules and policies

ci/util/
├── fingerprint.py           # FingerprintManager - high-level coordination
└── test_types.py            # FingerprintResult dataclass, calculate_* functions
```

### FingerprintManager (fingerprint.py)

High-level manager coordinating all fingerprint operations:

```python
class FingerprintManager:
    def __init__(self, cache_dir: Path, build_mode: str = "quick"):
        self.build_mode = build_mode
        self.fingerprint_dir = cache_dir / "fingerprint"

    def check_cpp(self, args: TestArgs) -> bool:
        """Returns True if C++ tests should run"""

    def check_examples(self) -> bool:
        """Returns True if examples should compile"""

    def check_python(self) -> bool:
        """Returns True if Python tests should run"""

    def save_all(self, status: str) -> None:
        """Save all fingerprints with success/failure status"""
```

### FingerprintResult (test_types.py)

```python
@dataclass
class FingerprintResult:
    hash: str
    elapsed_seconds: Optional[str] = None
    status: Optional[str] = None

    def should_skip(self, current: "FingerprintResult") -> bool:
        """Returns True if tests should be skipped"""
        return self.hash == current.hash and self.status == "success"
```

### Fingerprint Calculation Functions (test_types.py)

```python
def calculate_fingerprint() -> FingerprintResult:
    """Fingerprint entire src/ directory"""

def calculate_cpp_test_fingerprint(args: TestArgs) -> FingerprintResult:
    """Fingerprint for C++ tests (includes debug flag)"""

def calculate_examples_fingerprint() -> FingerprintResult:
    """Fingerprint for example compilation"""

def calculate_python_test_fingerprint() -> FingerprintResult:
    """Fingerprint for Python tests"""
```

## Two-Layer Change Detection

The system uses a two-layer approach for efficiency:

### Layer 1: Modification Time (Fast Path)
```python
if current_mtime == cached_mtime:
    return False  # No change - microsecond check
```

### Layer 2: Content Hash (Accurate Path)
```python
if current_mtime != cached_mtime:
    current_hash = compute_md5(file)
    if current_hash == cached_hash:
        # File touched but content unchanged (git checkout, touch, etc.)
        update_cached_mtime()  # Avoid re-hashing next time
        return False
    else:
        return True  # Content actually changed
```

This handles common scenarios efficiently:
- **Unchanged files**: Instant mtime comparison
- **Touched files**: One MD5 computation, then updates mtime cache
- **Modified files**: Detects actual content changes

## Integration with test.py

```python
# From test.py (lines 154-162)
cache_dir = Path(".cache")
build_mode = args.build_mode if args.build_mode else "quick"
fingerprint_manager = FingerprintManager(cache_dir, build_mode=build_mode)

# Check if tests need to run
src_code_change = fingerprint_manager.check_all()
cpp_test_change = fingerprint_manager.check_cpp(args)
examples_change = fingerprint_manager.check_examples()
python_test_change = fingerprint_manager.check_python()

# After tests complete
fingerprint_manager.save_all("success")  # or "failure"
```

## Performance

| Operation | Time |
|-----------|------|
| Cache hit (mtime match) | < 1ms |
| Fingerprint calculation (src/) | ~2-5ms |
| Fingerprint calculation (examples/) | ~10-20ms |
| Full Python fingerprint | ~50-100ms |

## Concurrent Access Safety

The system supports concurrent access via:

1. **File locking**: `fasteners.InterProcessLock` prevents race conditions
2. **Pending pattern**: Fingerprints stored in `.pending` files before processing
3. **Atomic updates**: Compare-and-swap prevents cache corruption

## Troubleshooting

### Tests always run despite no changes
1. Check if previous run failed (`status: "failure"` in cache file)
2. Verify cache file exists in `.cache/fingerprint/`
3. Check for mtime changes (git operations, file syncing)

### Tests skip when they shouldn't
1. Use `--no-fingerprint` to force run
2. Delete specific cache file
3. Check if monitored files are in the fingerprint scope

### Cache not created
1. Ensure `.cache/` directory is writable
2. Check for file permission issues
3. Verify test completes (cache saved on exit)

## Centralized Fingerprint Module

**All fingerprint caches have been centralized in `ci/fingerprint/`.**

### Import Path

**All code must now use:**
```python
from ci.fingerprint import (
    TwoLayerFingerprintCache,
    HashFingerprintCache,
    FingerprintCache,
)
```

The old scattered implementations (`ci.util.two_layer_fingerprint_cache`, `ci.util.hash_fingerprint_cache`, `ci.ci.fingerprint_cache`) have been removed and consolidated into `ci.fingerprint`.

## Adding New Fingerprints

See [`ci/fingerprint/README.md`](../../fingerprint/README.md) for complete documentation on adding new cache types.

Quick summary:

1. Add configuration to `ci/fingerprint/config.py`
2. Add monitored files to `ci/fingerprint/rules.py`
3. Use the predefined cache via `get_cache_config()`
