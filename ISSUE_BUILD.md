# test.py Fails After New Test File Triggers Meson Reconfiguration

## Summary

When a new test file is added to the project (e.g., `tests/fl/fixed_point/s16x16.cpp`), `test.py` fails with exit code 1 on the first run and on several subsequent retries. The failure occurs silently after "Preparing build (quick mode)..." with no error message. The test compiles and passes when built/run manually outside of `test.py`.

## Status: FIXED

Three bugs were identified and fixed:

### Bug 1: `get_fuzzy_test_candidates` filtered for wrong target type

**File**: `ci/meson/test_discovery.py`

The function used `meson introspect --targets` and filtered for `type == "executable"`, but test targets are `shared library` type in the DLL-based test architecture. This meant fuzzy candidate resolution never found any test targets.

**Fix**: Changed to use `meson introspect --tests` which returns registered test names directly, regardless of the underlying target type.

### Bug 2: Runner didn't support DLL-based test execution for specific tests

**File**: `ci/meson/runner.py`

When running a specific test, the runner only looked for `{test_name}.exe`. In the DLL-based test architecture, tests are run as `runner.exe <test.dll>`. When the specific test's runner copy (`.exe`) hadn't been compiled yet, the runner reported "test executable not found."

**Fix**: Added fallback logic: if `{test_name}.exe` doesn't exist, look for the shared `runner.exe` + `{test_name}.dll` combination.

### Bug 3: `test_metadata_cache.py` used non-recursive glob for subdirectories

**File**: `tests/test_metadata_cache.py`

`compute_test_files_hash()` used `glob("*.cpp")` (non-recursive) for subdirectories, so files in nested directories like `fl/fixed_point/s16x16.cpp` were never included in the hash. Also hardcoded `["fl", "ftl", "fx"]` instead of using `TEST_SUBDIRS` from `test_config.py`.

**Fix**: Changed to `rglob("*.cpp")` and imported `TEST_SUBDIRS` from `test_config.py` to stay in sync with test discovery configuration.

### Supporting fix: `test_config.py` subdirectory registration

**File**: `tests/test_config.py`

Added `"fl/fixed_point"` to `TEST_SUBDIRS` so `discover_tests.py` finds tests in the new subdirectory.

## Root Cause Analysis

The test infrastructure has a multi-layer caching system:

1. **Source hash layer** (`build_config.py`): `get_source_files_hash()` tracks all source/test files via `rglob`. Detects adds/removes and triggers `meson setup --reconfigure`.

2. **Test metadata cache** (`test_metadata_cache.py`): Caches the output of `organize_tests.py` using a hash of test file metadata (path + mtime + size). Used by `tests/meson.build` to avoid re-running test discovery on every reconfigure.

3. **Test list cache** (`build_config.py`): `test_list_cache.txt` tracks the list of discovered test files for detecting test adds/removes.

4. **Meson target resolution** (`test_discovery.py`, `runner.py`): Maps user-provided test names to meson build targets and executables.

The original failure chain was:
1. User adds `tests/fl/fixed_point/s16x16.cpp`
2. Source hash detects change → triggers reconfigure
3. During reconfigure, `test_metadata_cache.py --check` recomputes hash but `glob()` misses the nested file → stale cache returned
4. Even after cache issues resolved, `get_fuzzy_test_candidates` filters for `type == "executable"` → never finds the `shared library` test target
5. Runner tries `test_s16x16` and `s16x16` as target names → neither exists (actual name is `fl_fixed_point_s16x16`)
6. Even if compilation succeeds via fuzzy match, runner looks for `.exe` file that doesn't exist yet

---

## Resolved: Fragile Hardcoded Subdirectory Lists

### Problem (was)

Test discovery relied on a manually-maintained `TEST_SUBDIRS` list in `tests/test_config.py` with 12 leaf-directory entries. Every new test subdirectory required updating this list, and forgetting to do so silently hid tests.

### Fix Applied

Replaced `TEST_SUBDIRS` (inclusion list) with `EXCLUDED_TEST_DIRS` (exclusion list) + automatic recursive discovery.

**Before**: `discover_tests.py` used `glob("*.cpp")` per explicitly-listed subdirectory.
**After**: `discover_tests.py` uses a single `rglob("*.cpp")` over the entire `tests/` tree, filtering out:
- Files in `EXCLUDED_TEST_FILES` (`doctest_main.cpp`, `test_runner.cpp`)
- Directories in `EXCLUDED_TEST_DIRS` (`shared`, `testing`, `data`, `manual`, `build`, `builddir`, `x64`, `bin`, `example_compile_direct`, `fastled_js`)
- Hidden directories (names starting with `.` — covers `.build-*`, `__pycache__`)
- MSVC artifact dirs (names ending with `.dir`)

This immediately found **10 previously-invisible tests** in nested subdirectories that were never listed in `TEST_SUBDIRS`:
- `fl/chipsets/clockless_block_generic.cpp`, `fl/chipsets/lcd50.cpp`
- `fl/codec/gif.cpp`, `fl/codec/jpeg.cpp`, `fl/codec/mp3.cpp`, `fl/codec/mpeg1.cpp`, `fl/codec/pixel.cpp`, `fl/codec/vorbis.cpp`
- `fx/video/frame_tracker.cpp`
- `platforms/esp/32/drivers/rmt/rmt_5/rmt5_nibble_lut.cpp`

Adding `tests/any/nested/path/new_test.cpp` now Just Works with zero config changes.

## Environment

- Platform: Windows (MSYS_NT-10.0-19045)
- Meson: 1.10.0
- Compiler: clang 21.1.5 via clang-tool-chain-sccache
- Build mode: quick
