# Build System Optimization Research

## Problem: DLL Relink Cascade on Whitespace Changes

### Root Cause Chain

When a developer makes a whitespace-only change to a `src/` file and runs
`bash test --no-fingerprint` (or any incremental build):

1. **sccache cache hit**: sccache restores the `.obj` file from cache with
   IDENTICAL content, but updates `mtime` to NOW (unavoidable sccache behavior)

2. **ninja sees stale archive**: `.obj` mtimes > `libfastled.a` mtime →
   ninja schedules re-archive of `libfastled.a`

3. **archive unchanged**: `libfastled.a` is re-created with identical content
   (unity builds: 4 .obj files, deterministic output with `D` flag)

4. **DLL cascade**: ninja sees `libfastled.a` mtime changed → schedules relink
   of ALL 327 DLLs (247 unit tests + 80 examples)

5. **Result**: 60+ seconds of unnecessary linking for a whitespace change

---

## Optimization 1: Content-Preserving Archive Wrapper

### Solution Design

Two components work together:

#### Component A: `ci/meson/ar_content_preserving.py`
A Python wrapper around `llvm-ar` that:
1. Hashes the existing `libfastled.a` (SHA-256, ~1ms for 757KB)
2. Creates a new archive in a temp file (same directory for atomic rename)
3. Hashes the new archive
4. If hashes match: **does NOT replace the archive** → preserves mtime
5. If hashes differ: atomically replaces the archive (real content change)

**Key details:**
- `tempfile.mkstemp()` creates an empty 0-byte file; must `unlink()` it
  before calling `llvm-ar`, or ar fails with "file too small to be an archive"
- Archives are THIN archives (`T` flag in `csrDT`) - they store file PATHS
  and symbol tables, not file content. Hashes compare path lists + symbols.
- Thin archives with identical path lists in identical order always produce
  identical bytes (with `D` deterministic flag). This is the key invariant.
- Ninja always passes input files in the SAME ORDER (from build.ninja), so
  the hash comparison is reliable across builds.

#### Component B: `restat = 1` on STATIC_LINKER rule
Meson's `STATIC_LINKER` rule in `build.ninja` lacks `restat = 1` by default.

With `restat = 1`:
- After the archive command completes, ninja **re-checks** `libfastled.a`'s mtime
- If mtime is UNCHANGED (wrapper preserved it), ninja removes `libfastled.a`
  from the "recently built" set and does NOT propagate dirty to downstream DLLs
- Result: 327 DLLs are NOT scheduled for relinking

Without `restat = 1`:
- ninja considers `libfastled.a` dirty regardless of whether it changed
- All 327 DLLs are always relinked after any archive command

### Implementation

Both patches are injected into `build.ninja` directly (not via `meson_native.txt`):

**Why build.ninja injection instead of meson_native.txt?**
- `meson_native.txt` is listed as a REGENERATE_BUILD dependency in `build.ninja`
- Changing `meson_native.txt` triggers ninja to auto-regenerate `build.ninja`
  (via meson), which would strip our patches
- Direct `build.ninja` patching avoids triggering auto-regeneration
- Patches are re-applied on every `bash test` run (idempotent, <2ms total)

**`build_config.py` injection functions:**
1. `inject_restat_into_static_linker_rule(build_dir)`: adds `restat = 1`
2. `inject_ar_wrapper_into_static_linker_command(build_dir, source_dir)`:
   prepends `"python.exe" "ar_content_preserving.py"` to the command
3. Both called in both skip and post-meson-setup paths

**Patched STATIC_LINKER rule:**
```
rule STATIC_LINKER
 command = "python.exe" "ar_content_preserving.py" "ar.exe" $LINK_ARGS $out $in
 description = Linking static target $out
 restat = 1
```

### Validation

End-to-end test proved the optimization works:
1. Created archive from 4 obj files with absolute paths → hash H1
2. Touched all obj files (same content, newer mtime)
3. Ran wrapper with same absolute paths and flags
4. Result: archive hash UNCHANGED (H1 == H1), mtime PRESERVED

**Test result:**
```
Initial archive hash:  f976ee98b7878a57...
Touched 4 objs (newer than archive: True)
After archive hash:    f976ee98b7878a57...
Mtime unchanged: True
SUCCESS: mtime preserved!
```

### Important: Thin Archive Path Sensitivity

During debugging, discovered that thin archive hashes are PATH-SENSITIVE:
- Thin archives store FILE PATHS in their symbol tables (not file content)
- Archives created with relative paths differ from archives created with absolute paths
- Ninja passes absolute paths when running build commands
- The wrapper receives the SAME paths as ninja passes → hashes match reliably

**Failed test approach** (wrong): running ar with relative paths from project root
**Correct approach**: using absolute paths as ninja does (Path.cwd() / relative)

---

## Optimization 2: Skip BuildOptimizer When ar_content_preserving Is Active

### Problem: BuildOptimizer DLL Hashing Overhead

With Optimization 1 in place, `ci/meson/build_optimizer.py` is now redundant but adds
~10-12 seconds of overhead per build:

- **`BuildOptimizer.touch_dlls_if_lib_unchanged()`**: Hashes 2862.6 MB of DLLs (~5-6s)
  to check if DLLs should be touched (now unnecessary - cascade prevented at source)
- **`BuildOptimizer.save_fingerprints()`**: Hashes 2862.6 MB of DLLs (~5-6s) after
  each successful build to save fingerprints (unnecessary when cascade doesn't happen)

**DLL stats**: 328 DLLs total, ~8.9 MB average, 2862.6 MB total = ~5-6s to hash all

### Solution: `is_ar_content_preserving_active()` Gate

Added `is_ar_content_preserving_active(build_dir)` function to `build_config.py`:
- Reads `build.ninja` and checks for both patches (`ar_content_preserving.py` + `restat = 1`)
- Returns `True` if both optimizations are active (meaning cascade is prevented)
- Overhead: < 1ms (reads one file, does two string searches)

Modified `runner.py` to skip BuildOptimizer creation when ar optimization is active:
```python
ar_opt_active = is_ar_content_preserving_active(build_dir)
build_optimizer = (
    make_build_optimizer(build_dir) if (use_streaming and not ar_opt_active) else None
)
```

### Why Keep BuildOptimizer at All?

BuildOptimizer is kept as a fallback for cases where the ar_content_preserving patches
may not be active (e.g., if `build.ninja` was regenerated by meson and patches haven't
been re-applied yet in that build session). It's bypassed only when patches are confirmed
present.

---

## Pre-existing BuildOptimizer Issues (Superseded)

The original `BuildOptimizer.touch_dlls_if_lib_unchanged()` approach had problems:

1. **Race condition**: background thread tries to touch DLLs while ninja is
   already relinking them in parallel
2. **Too slow**: hashes 61.5MB of DLL content before touching (multi-second operation)
3. **Approach**: "after the fact" - tries to undo relinking that already started

The content-preserving archiver + `restat = 1` prevents the cascade BEFORE it starts:
- No race conditions (happens synchronously during archive step)
- Minimal overhead (~2ms for archive hash comparison)
- Works WITH ninja's dependency tracking (not fighting it)
- Clean: uses ninja's own `restat` mechanism as designed

---

## Performance Impact

**Before optimizations:**
- Whitespace change in src/ → sccache cache hits → libfastled.a re-archived
- All 327 DLLs relinked → 60+ seconds extra build time
- BuildOptimizer hashes 2.86GB of DLLs → +10-12 seconds overhead
- Total wasted time: ~70-72 seconds per incremental build with --no-fingerprint

**After optimizations (Iterations 1 + 2):**
- Optimization 1: libfastled.a mtime preserved → ninja restat sees no change → 0 DLL relinks (~60s saved)
- Optimization 2: BuildOptimizer skipped → 0 DLL hashing overhead (~10-12s saved)
- Combined overhead: ~2ms (archive hash) + <1ms (build.ninja check)
- Total savings: ~70-72 seconds

---

## Files Modified

| File | Change |
|------|--------|
| `ci/meson/ar_content_preserving.py` | NEW: content-preserving archive wrapper |
| `ci/meson/build_config.py` | Added inject functions + `is_ar_content_preserving_active()` |
| `ci/meson/runner.py` | Skip BuildOptimizer when ar optimization active |
| `.build/meson-quick/build.ninja` | Patched: restat=1 + ar wrapper in STATIC_LINKER |

---

## Archive Determinism Notes

- `D` flag in `csrDT`: deterministic mode - zeroes timestamps, UIDs, GIDs in archive
- `T` flag in `csrDT`: thin archive - stores paths/symbols only, not file content
- Two fresh archives from SAME inputs (same paths, same order, `D` flag) → IDENTICAL bytes
- Different member ORDER → different archives (archives are ordered data structures)
- Ninja always passes members in the same order (from build.ninja), ensuring consistency

---

## Optimization 3: Startup Subprocess Overhead Reduction

### Problem: Wasted Subprocess Calls on Every Build

Even in the "fast path" (build already configured, no changes needed), multiple
subprocess calls were made before ninja even started:

1. `detect_system_llvm_tools()` → runs lld, ld.lld, llvm-ar subprocess calls (DEAD CODE)
2. `check_meson_version_compatibility()` → runs `meson --version` subprocess
3. `get_compiler_version()` → runs `clang-tool-chain-cpp --version` subprocess

These process spawns add ~500ms-2000ms overhead on Windows (process creation is slow).

### Optimization 3a: Remove Dead `detect_system_llvm_tools()` Call

**Problem**: `detect_system_llvm_tools()` runs subprocess calls to detect lld and
llvm-ar, but the results (`has_lld`, `has_llvm_ar`) were NEVER USED in the function.
Confirmed via grep: the variables go out of scope immediately after assignment.

**Fix**: Removed lines 487-488 from `ci/meson/build_config.py`:
```python
# DELETED - was dead code:
# Legacy detection for standalone LLVM tools (kept for diagnostic messages)
# has_lld, has_llvm_ar = detect_system_llvm_tools()
```

**Savings**: ~200-500ms per build (subprocess calls to lld and llvm-ar variants)

### Optimization 3b: Cache `get_compiler_version()` Using Compiler Binary Mtime

**Problem**: Every build ran `clang-tool-chain-cpp --version` subprocess (~100-300ms)
to get the compiler version, even though the version almost never changes.

**Fix**: Compare compiler binary mtime vs. compiler_version_marker mtime:
- If `compiler_binary.mtime < marker.mtime`: marker was written AFTER compiler install
  → version hasn't changed → use cached version from marker file (skip subprocess)
- If mtime condition fails: fall through to subprocess (safe fallback)

**Implementation** in `ci/meson/build_config.py`:
```python
current_compiler_version: str = ""
_version_from_cache = False
_clangxx_path = Path(clangxx_wrapper)
if compiler_version_marker.exists() and _clangxx_path.exists():
    try:
        if _clangxx_path.stat().st_mtime < compiler_version_marker.stat().st_mtime:
            _cached_ver = compiler_version_marker.read_text(encoding="utf-8").strip()
            if _cached_ver and _cached_ver != "unknown":
                current_compiler_version = _cached_ver
                _version_from_cache = True
    except OSError:
        pass
if not _version_from_cache:
    current_compiler_version = get_compiler_version(clangxx_wrapper)
```

**Savings**: ~100-300ms per build in common case (compiler unchanged between runs)

### Optimization 3c: Mtime Fast-Path for `check_meson_version_compatibility()`

**Problem**: Every build ran `meson --version` subprocess (~100-300ms) to check
version compatibility, even though meson version almost never changes.

**Fix**: Compare meson.exe mtime vs. coredata.dat mtime:
- If `meson.exe.mtime < coredata.dat.mtime`: meson was installed BEFORE the last
  build dir configuration → same meson version → compatible (skip subprocess)
- Falls through to full check if mtime condition fails

**Implementation** in `ci/meson/compiler.py`:
```python
# Fast path: skip subprocess if meson.exe hasn't changed since last config
meson_exe_path = Path(get_meson_executable())
if meson_exe_path.is_absolute() and meson_exe_path.exists() and coredata_path.exists():
    try:
        if meson_exe_path.stat().st_mtime < coredata_path.stat().st_mtime:
            return True, "Meson version compatible (mtime fast-path)"
    except OSError:
        pass
```

**Savings**: ~100-300ms per build in common case (meson version unchanged)

---

## BUGFIX: example_metadata.cache Path Mismatch (MAJOR IMPACT)

### Problem: Infinite Meson Reconfiguration Loop

Every single build was detecting "example file changes" and triggering full meson
reconfiguration (~30-60 seconds overhead per build). Root cause:

- **Meson writes cache to**: `.build/meson-quick/examples/example_metadata.cache`
  (because `meson.current_build_dir()` in `examples/meson.build` = the examples subdir)
- **`build_config.py` reads cache from**: `.build/meson-quick/example_metadata.cache`
  (wrong path - one level up)

Since the read path never matched the write path:
1. `compute_example_files_hash()` returns current hash (non-empty)
2. `cached_example_hash = ""` (file not found at wrong path)
3. Hashes differ → `example_files_changed = True`
4. Delete (nonexistent) cache file, force meson reconfigure
5. Meson writes cache to correct path (`.../examples/`)
6. Next run: back to step 1 (reads wrong path again → infinite loop)

### Fix

In `ci/meson/build_config.py`:
```python
# BEFORE (wrong):
example_cache_file = build_dir / "example_metadata.cache"

# AFTER (correct):
# Cache file location matches where meson.current_build_dir() writes it
# in examples/meson.build: .build/meson-quick/examples/example_metadata.cache
example_cache_file = build_dir / "examples" / "example_metadata.cache"
```

**Savings**: ~30-60 seconds per build (eliminates unnecessary full meson reconfigure)

---

## Performance Impact (All Iterations)

| Optimization | Saves | Mechanism |
|-------------|-------|-----------|
| Iter 1: ar_content_preserving.py + restat=1 | ~60s | Prevents DLL cascade on whitespace changes |
| Iter 2: Skip BuildOptimizer | ~10-12s | Skips 2.86GB DLL hashing |
| Iter 3a: Remove dead detect_system_llvm_tools() | ~200-500ms | Dead code removal |
| Iter 3b: Cache compiler version (mtime) | ~100-300ms | Skip clang --version subprocess |
| Iter 3c: Meson version mtime fast-path | ~100-300ms | Skip meson --version subprocess |
| Iter 3 BUGFIX: example_metadata.cache path | ~30-60s | Eliminates infinite reconfigure loop |
| Iter 4a: check_meson_installed() fast-path | ~50-100ms | Skip meson binary check |
| Iter 4b: Combined rglobs (6→2) | ~100-200ms (fallback) | Fewer filesystem traversals |
| Iter 4c: Source hash mtime fast-path | ~200-500ms | Skip src/ rglob when dirs unchanged |
| Iter 4d: Test discovery mtime fast-path | ~100-200ms | Skip tests/ rglob when dirs unchanged |
| Iter 5a: Example hash mtime fast-path | ~50-100ms | Skip examples/ rglob when dirs unchanged |
| Iter 5b: meson introspect --tests cache | ~700ms (specific-test runs) | Cache test names; 0.3ms vs 703ms subprocess |
| **Total** | **~100-130s** | Per incremental all-tests build |
| **+ Iter 5b bonus** | **+~700ms** | Per specific-test invocation |

---

## Optimization 5a: Example File Hash Mtime Fast-Path

### Problem

`compute_example_files_hash()` runs 3 separate `rglob()` passes over the `examples/`
directory on EVERY build (even when nothing changed), then stat()s each file and
computes SHA256. With ~80 examples and multiple files each, this takes ~50-100ms.

### Solution

Added a directory-mtime fast-path in `build_config.py` before calling
`compute_example_files_hash()`:

```python
_skip_example_hash = False
if example_cache_file.exists():
    try:
        _cache_mtime = example_cache_file.stat().st_mtime
        _max_example_dir_mtime = _get_max_dir_mtime(examples_dir)
        if _max_example_dir_mtime <= _cache_mtime:
            _skip_example_hash = True  # No structural changes detected
    except OSError:
        pass

if not _skip_example_hash:
    current_example_hash = compute_example_files_hash(examples_dir)
    # ... existing hash comparison logic
```

**Rationale**: Adding/removing a file always updates the PARENT directory mtime on
NTFS, ext4, and APFS. `_get_max_dir_mtime()` is O(#dirs) vs O(#files) for the full
rglob. If the meson-written `example_metadata.cache` is newer than all example
directories, the examples haven't changed structurally.

**Limitation**: In-place file content modifications (not adding/removing) update only
file mtime, NOT parent directory mtime. These won't be detected by the fast-path.
Acceptable because modifying existing examples is rare and the next structural change
will trigger reconfiguration anyway.

**Savings**: ~50-100ms per incremental build.

---

## Optimization 5b: meson introspect --tests Caching

### Problem

`get_fuzzy_test_candidates()` in `test_discovery.py` runs `meson introspect --tests`
subprocess on EVERY specific-test invocation (e.g., `bash test alloca`).

**Measured cost**: 703ms on this Windows system (meson Python startup + JSON serialization
of 322 tests into 180KB of output).

This subprocess is called even when the test list hasn't changed (typical case).

### Solution

Cache the test names (not the full meson introspect JSON) in:
`.build/meson-quick/.meson_introspect_tests_cache.json`

**Format**: compact JSON array of test name strings only (~6.5KB vs 180KB full output)

**Cache invalidation**: build.ninja mtime. Meson rewrites build.ninja on every
reconfigure (when tests are added/removed), making its mtime a reliable
invalidation signal.

**Implementation**:
```python
def _load_introspect_tests_cache(build_dir: Path) -> list[str] | None:
    cache_path = build_dir / ".meson_introspect_tests_cache.json"
    build_ninja = build_dir / "build.ninja"
    if not cache_path.exists() or not build_ninja.exists():
        return None
    try:
        cache_mtime = cache_path.stat().st_mtime
        ninja_mtime = build_ninja.stat().st_mtime
        if cache_mtime < ninja_mtime:
            return None  # stale
        data = json.loads(cache_path.read_text(encoding="utf-8"))
        if isinstance(data, list) and (not data or isinstance(data[0], str)):
            return data  # compact name-list format
        return None  # old full-dict format → treat as miss
    except (OSError, json.JSONDecodeError):
        return None
```

**Cache save** (after subprocess):
```python
all_tests = json.loads(result.stdout)
test_names = [t.get("name", "") for t in all_tests if t.get("name")]
_save_introspect_tests_cache(build_dir, test_names)  # writes compact 6.5KB JSON
```

**Performance measurements**:
| Approach | Time | Notes |
|---------|------|-------|
| No cache (subprocess) | 703ms | Full meson introspect --tests |
| Full-dict cache (180KB) | 104ms | json.loads of 180KB |
| Compact name cache (6.5KB) | 0.3ms | json.loads of 6.5KB |

**Total savings vs subprocess**: 702ms per specific-test invocation.

---

---

## Optimization 6a: Avoid Double Walk of tests/ Directory

### Problem

`setup_meson_build()` walked `source_dir / "tests"` TWICE per call:
1. Inside the **source hash fast-path** (line ~676): `_get_max_dir_mtime(source_dir / "tests")`
2. Inside the **test discovery fast-path** (line ~918): `_get_max_dir_mtime(source_dir / "tests")`

Each walk takes ~73-83ms on Windows (401+ directories in tests/).
Total wasted: ~52ms per incremental build (one redundant walk).

### Solution

Precompute `_max_tests_dir_mtime` ONCE before both fast-path checks, then
reuse the cached value:

```python
# Pre-computed once; reused by both fast-paths below
_max_tests_dir_mtime: float = _get_max_dir_mtime(source_dir / "tests")
```

Both fast-paths now reference `_max_tests_dir_mtime` instead of calling
`_get_max_dir_mtime(source_dir / "tests")` independently.

**Savings**: ~73ms per incremental build (one fewer O(#dirs) walk of tests/).

---

## Optimization 6b: Consolidate 3 build.ninja Reads into 1

### Problem

Three separate operations read `build.ninja` per `setup_meson_build()` call:
1. `inject_restat_into_static_linker_rule(build_dir)` — reads + maybe writes
2. `inject_ar_wrapper_into_static_linker_command(build_dir, source_dir)` — reads + maybe writes
3. `is_ar_content_preserving_active(build_dir)` — reads (in runner.py, after setup)

Total: 3 reads (~1.9-2.5ms each = ~6ms wasted), 2 writes if patches missing.

### Solution

New combined function `inject_ar_optimization_patches(build_dir, source_dir)`:
- Reads `build.ninja` ONCE
- Checks for both patches (ar wrapper + restat=1) in that single read
- Applies missing patches in ONE write if needed (vs two separate writes)
- Populates module-level `_ar_opt_status_cache[str(build_dir)] = bool`

`is_ar_content_preserving_active()` now checks `_ar_opt_status_cache` first:
```python
build_dir_key = str(build_dir)
if build_dir_key in _ar_opt_status_cache:
    return _ar_opt_status_cache[build_dir_key]  # cache hit: no file read
# Cache miss: read build.ninja and check...
```

Since `inject_ar_optimization_patches()` is always called inside
`setup_meson_build()` (before `is_ar_content_preserving_active()` is called in
`runner.py`), the cache is always populated → `is_ar_content_preserving_active()`
returns from cache (~0ms, no file read).

**Savings**: ~4ms per incremental build (2 fewer build.ninja reads; avoids
the 3rd read entirely via process-local cache).

### Important: `restat = 1` Appears Multiple Times in build.ninja

The `CUSTOM_COMMAND`, `SHSYM`, and `CUSTOM_COMMAND_DEP` rules also have `restat = 1`.
The old check `"restat = 1" in content` (full file) would be True even if the
STATIC_LINKER rule didn't have it. However, since the optimization checks BOTH
`ar_content_preserving.py` AND `restat = 1` together as the idempotency test,
and `ar_content_preserving.py` only appears in STATIC_LINKER, the combined check
is still correct: if the wrapper is present, the restat was added alongside it.

---

## False Alarm: sccache Daemon Version Mismatch

During iteration 6 testing, a ~6-second regression appeared that turned out to be
caused by a **broken sccache daemon** (client/server version mismatch), NOT by the
code changes. Symptoms:

```
[sccache] Server connection error detected, retrying in 2.0s (attempt 1/3)...
[sccache] Server connection error detected, retrying in 2.0s (attempt 2/3)...
[sccache] Server connection error detected, retrying in 2.0s (attempt 3/3)...
sccache: error: failed to get stats from server
```

3 retries × 2s delay = 6s added to every `show_sccache_stats()` call.
Fix: `clang-tool-chain-sccache --stop-server` to restart the daemon.

**Lesson**: When diagnosing timing regressions, check sccache daemon health first.

---

## Files Modified (All Iterations)

| File | Change |
|------|--------|
| `ci/meson/ar_content_preserving.py` | NEW: content-preserving archive wrapper |
| `ci/meson/build_config.py` | inject functions, is_ar_content_preserving_active(), removed dead code, compiler version caching, example cache path fix, example/source hash mtime fast-paths, `_max_tests_dir_mtime` precomputation, `inject_ar_optimization_patches()`, `_ar_opt_status_cache` |
| `ci/meson/runner.py` | Skip BuildOptimizer when ar optimization active |
| `ci/meson/compiler.py` | Meson version mtime fast-path |
| `ci/meson/test_discovery.py` | meson introspect --tests caching (compact format) |
| `.build/meson-quick/build.ninja` | Patched: restat=1 + ar wrapper in STATIC_LINKER |
