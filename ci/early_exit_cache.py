"""Ultra-early-exit cache helpers — stdlib only, no ci.* imports.

These functions provide fast-path detection for cached test results and build state
BEFORE running parse_args() or importing heavy modules (ci/__init__.py, test_discovery,
fingerprint manager, etc.).

Intentional duplication: These duplicate a small subset of ci/util/cache_utils.py and
ci/meson/test_discovery.py. Duplication is deliberate: importing those modules costs
~84ms (ci/__init__.py loads global_interrupt_handler + console_utf8; test_discovery
imports hashlib + subprocess). Inlining this ~240 lines of stdlib-only logic eliminates
that overhead entirely, bringing the ultra-early-exit path from ~40ms to ~2ms.

See test.py:_argv_ultra_early_exit() for usage examples.
"""

from __future__ import annotations

import _thread
import json
import os
import sys
from pathlib import Path


# Directories to skip when scanning for source file mtimes.
SKIP_DIR_NAMES = frozenset(
    [
        ".git",
        ".venv",
        ".venv_new",
        "node_modules",
        "__pycache__",
        ".pio",
        "emscripten",
        "bin",
    ]
)
SKIP_DIR_PREFIXES = (".build", "build", "builddir", ".")

# Source file extensions used in fast-path file scans.
CPP_EXTS = frozenset([".cpp", ".h", ".hpp", ".c", ".ino"])
PY_EXTS = frozenset([".py"])


def max_file_mtime(root: Path, exts: frozenset = CPP_EXTS) -> float:
    """Return max mtime of any source file under root (stdlib only).

    Recursively scans root directory tree and returns the maximum modification
    time of any source file matching the given extensions. Skips directories
    in SKIP_DIR_NAMES and those starting with prefixes in SKIP_DIR_PREFIXES.

    Uses os.scandir() + stack-based traversal (not recursive) to minimize
    memory overhead on large directory trees (e.g., tests/ with 329+ files).

    Args:
        root: Root directory to scan.
        exts: Frozenset of file extensions to match (default: C++ extensions).

    Returns:
        Maximum file mtime (float), or 0.0 if no files match.
    """

    def _should_skip_dir(name: str) -> bool:
        """Check if directory should be skipped from traversal."""
        return name in SKIP_DIR_NAMES or any(
            name.startswith(pfx) for pfx in SKIP_DIR_PREFIXES
        )

    max_mtime = 0.0
    stack = [str(root)]
    while stack:
        try:
            with os.scandir(stack.pop()) as it:
                for entry in it:
                    try:
                        if entry.is_dir(follow_symlinks=False):
                            if not _should_skip_dir(entry.name):
                                stack.append(entry.path)
                        elif entry.is_file(follow_symlinks=False):
                            _, ext = os.path.splitext(entry.name)
                            if ext.lower() in exts:
                                mtime = entry.stat(follow_symlinks=False).st_mtime
                                max_mtime = max(max_mtime, mtime)
                    except OSError:
                        pass
        except OSError:
            pass
    return max_mtime


def max_file_mtime_flat(root: Path, exts: frozenset = CPP_EXTS) -> float:
    """Return max mtime of source files in root directory only (no recursion).

    Used to check shared test headers in tests/ root without scanning all subdirs.
    Much faster than max_file_mtime() for large directory trees.

    Args:
        root: Directory to scan (non-recursive).
        exts: Frozenset of file extensions to match (default: C++ extensions).

    Returns:
        Maximum file mtime (float), or 0.0 if no files match.
    """
    max_mtime = 0.0
    try:
        with os.scandir(str(root)) as it:
            for entry in it:
                try:
                    if entry.is_file(follow_symlinks=False):
                        _, ext = os.path.splitext(entry.name)
                        if ext.lower() in exts:
                            mtime = entry.stat(follow_symlinks=False).st_mtime
                            max_mtime = max(max_mtime, mtime)
                except OSError:
                    pass
    except OSError:
        pass
    return max_mtime


def load_test_names(build_dir: Path) -> list:
    """Load cached test names from disk (stdlib only, no ci.* imports).

    Mirrors ci/meson/test_discovery._load_introspect_tests_cache() without
    importing that module (which pulls hashlib, subprocess, global_interrupt_handler).

    Args:
        build_dir: Meson build directory (e.g., .build/meson-quick/).

    Returns:
        List of cached test names, or empty list if cache is invalid.
    """
    cache_path = build_dir / ".meson_introspect_tests_cache.json"
    build_ninja = build_dir / "build.ninja"
    if not cache_path.exists() or not build_ninja.exists():
        return []
    try:
        if cache_path.stat().st_mtime < build_ninja.stat().st_mtime:
            return []
        data = json.loads(cache_path.read_text(encoding="utf-8"))
        if isinstance(data, list) and (not data or isinstance(data[0], str)):
            return data
        return []
    except (OSError, json.JSONDecodeError):
        return []


def ninja_skip(
    build_dir: Path,
    target: str,
    narrow_test_dir: Path | None = None,
) -> bool:
    """Check if ninja invocation can be skipped (stdlib only, no ci.* imports).

    Verifies that:
      1. Cached state file exists and is valid
      2. build.ninja hasn't been modified
      3. Source files (src/ and tests/) haven't been modified
      4. Target output file hasn't been modified

    Mirrors ci/meson/cache_utils.check_ninja_skip() without importing that module.

    Args:
        build_dir: Meson build directory.
        target: Meson target name (e.g. "fl_alloca").
        narrow_test_dir: If provided, narrows the tests/ scan to:
            - tests/ root (flat, no recursion) for shared headers like test.h
            - narrow_test_dir (recursive) for target-specific sources
            This reduces scan cost when running a specific test (~78% fewer files).

    Returns:
        True if ninja build can be skipped (no source changes detected),
        False if rebuild is needed.
    """
    state_file = build_dir / ".ninja_skip_state.json"
    if not state_file.exists():
        return False
    try:
        with open(state_file, "r", encoding="utf-8") as f:
            saved = json.load(f)
        build_ninja = build_dir / "build.ninja"
        if not build_ninja.exists():
            return False
        if (
            abs(build_ninja.stat().st_mtime - saved.get("build_ninja_mtime", -1.0))
            > 0.001
        ):
            return False
        # Check src/ max source file mtime to detect src/ code changes.
        # Previously used libfastled.a mtime as a proxy, but libfastled.a is only
        # updated AFTER ninja runs. Since this check fires BEFORE ninja, we must
        # scan src/ files directly to detect content modifications.
        src_max = max_file_mtime(Path("src"))
        if src_max > saved.get("src_max_file_mtime", -1.0) + 0.001:
            return False
        # Check tests/ max source file mtime to detect test source modifications.
        # When narrow_test_dir is provided (specific test run), narrow the scan to:
        #   - tests/ root (flat scan) for shared headers (test.h, etc.)
        #   - narrow_test_dir subtree for target-specific sources
        # This saves ~15-40ms vs scanning all 329+ test files (~74 files vs 329).
        saved_tests_max = saved.get("tests_max_file_mtime", -1.0)
        if narrow_test_dir is not None:
            tests_max = max(
                max_file_mtime_flat(Path("tests")),
                max_file_mtime(narrow_test_dir),
            )
        else:
            tests_max = max_file_mtime(Path("tests"))
        if tests_max > saved_tests_max + 0.001:
            return False
        target_data = saved.get("targets", {}).get(target)
        if not target_data:
            return False
        output_path = Path(target_data["output_path"])
        if not output_path.exists():
            return False
        if abs(output_path.stat().st_mtime - target_data["output_mtime"]) > 0.001:
            return False
        return True
    except (OSError, json.JSONDecodeError, KeyError, TypeError, ValueError):
        return False


def test_cached(build_dir: Path, test_name: str, artifact_path: Path) -> bool:
    """Check if test result is cached and test can be skipped (stdlib only).

    Verifies that:
      1. Test result cache file exists
      2. Test artifact (DLL/SO) file exists
      3. Cached result was "pass"
      4. Artifact hasn't been recompiled since cache write

    Mirrors ci/meson/cache_utils.check_test_result_cached() without importing
    that module.

    Args:
        build_dir: Meson build directory.
        test_name: Test name (e.g. "fl_alloca").
        artifact_path: Path to test artifact (DLL or SO file).

    Returns:
        True if test result is cached and artifact hasn't changed,
        False if test needs to be re-run.
    """
    cache_file = build_dir / ".test_result_cache.json"
    if not cache_file.exists() or not artifact_path.exists():
        return False
    try:
        with open(cache_file, "r", encoding="utf-8") as f:
            cache = json.load(f)
        entry = cache.get(test_name)
        if not entry or entry.get("result") != "pass":
            return False
        if (
            abs(artifact_path.stat().st_mtime - entry.get("artifact_mtime", -1.0))
            > 0.001
        ):
            return False
        return True
    except (OSError, json.JSONDecodeError, KeyError, TypeError, ValueError):
        return False


def full_run_cache(
    build_dir: Path, include_examples: bool = False
) -> tuple[int, int, float] | None:
    """Check if full test suite result is cached (stdlib only, no ci.* imports).

    Verifies that:
      1. Full run cache file exists
      2. Last run result was "pass"
      3. build.ninja hasn't been modified
      4. Source files (src/, tests/, and optionally examples/) haven't changed

    Mirrors ci/meson/cache_utils.check_full_run_cache() without importing
    that module.

    Args:
        build_dir: Meson build directory.
        include_examples: If True, also check examples/ for changes.

    Returns:
        Tuple of (num_passed, num_tests, duration) on cache hit, else None.
    """
    cache_file = build_dir / ".full_run_cache.json"
    if not cache_file.exists():
        return None
    try:
        with open(cache_file, "r", encoding="utf-8") as f:
            saved = json.load(f)
        if saved.get("result") != "pass":
            return None
        build_ninja = build_dir / "build.ninja"
        if not build_ninja.exists():
            return None
        if (
            abs(build_ninja.stat().st_mtime - saved.get("build_ninja_mtime", -1.0))
            > 0.001
        ):
            return None
        # Check src/ max source file mtime (detects src/ code changes).
        # Previously used libfastled.a mtime as a proxy, but libfastled.a is only
        # updated AFTER ninja runs. Scanning src/ files directly is correct.
        src_max = max_file_mtime(Path("src"))
        if src_max > saved.get("src_max_file_mtime", -1.0) + 0.001:
            return None
        tests_max = max_file_mtime(Path("tests"))
        if tests_max > saved.get("tests_max_file_mtime", -1.0) + 0.001:
            return None
        if include_examples:
            ex_max = max_file_mtime(Path("examples"))
            if ex_max > saved.get("examples_max_file_mtime", -1.0) + 0.001:
                return None
        return (
            int(saved.get("num_passed", 0)),
            int(saved.get("num_tests", 0)),
            float(saved.get("duration", 0.0)),
        )
    except (OSError, json.JSONDecodeError, KeyError, TypeError, ValueError):
        return None


def check_single_test_cached(test_name_raw: str, build_dir: Path) -> bool:
    """Check if a single named test is fully cached (build + result).

    Performs the full pipeline:
      1. Load cached test names from introspect cache
      2. Normalize test name and find a match
      3. Locate the test artifact (DLL/SO)
      4. Verify ninja build can be skipped (no source changes)
      5. Verify test result is cached and passing

    This is the shared logic used by both argv_ultra_early_exit() (pre-parse_args)
    and the post-parse_args early exit in main().

    Args:
        test_name_raw: Raw test name string (e.g., "xypath", "tests/fl/alloca").
        build_dir: Meson build directory (e.g., .build/meson-quick/).

    Returns:
        True if the test is fully cached and can be skipped, False otherwise.
    """
    cached_names = load_test_names(build_dir)
    if not cached_names:
        return False

    # Normalize: strip "tests/" prefix and convert path separators to underscores
    name_lower = test_name_raw.lower()
    if name_lower.startswith("tests/") or name_lower.startswith("tests\\"):
        name_lower = name_lower[len("tests/") :]
    name_normalized = name_lower.replace("/", "_").replace("\\", "_")
    stem = Path(test_name_raw).stem.lower()

    # Try multiple candidate names (exact, test_ prefixed, stem-only)
    candidates = dict.fromkeys(
        [name_normalized, f"test_{name_normalized}", stem, f"test_{stem}"]
    )
    cache_set = set(cached_names)
    matched: str | None = None
    for cn in candidates:
        if cn in cache_set:
            matched = cn
            break
    if not matched:
        return False

    ext = ".dll" if os.name == "nt" else ".so"
    dll = build_dir / "tests" / f"{matched}{ext}"
    if not dll.exists():
        return False

    # Determine the test's source directory to narrow the tests/ scan.
    # For "tests/fl/alloca" → narrow to tests/fl/ (+ tests/ root for shared headers).
    # This reduces the file scan from ~329 files to ~74 files (~78% faster).
    test_raw_path = Path(test_name_raw)
    if test_raw_path.parts and test_raw_path.parts[0].lower() == "tests":
        narrow_dir: Path | None = test_raw_path.parent
    else:
        narrow_dir = Path("tests") / test_raw_path.parent
    # Only use narrow scan when the directory exists and differs from tests/ root
    if narrow_dir is not None and (
        not narrow_dir.exists() or narrow_dir == Path("tests")
    ):
        narrow_dir = None

    return ninja_skip(build_dir, matched, narrow_test_dir=narrow_dir) and test_cached(
        build_dir, matched, dll
    )


def argv_ultra_early_exit(start_time: float) -> None:
    """Check if test result is cached without running parse_args().

    This fires BEFORE the smart_selector filesystem scan (~200-500ms) and before
    all fingerprint checks. It reads sys.argv directly to detect these cases:

    Case 0 - No flags (bash test with no arguments):
      Default mode runs unit tests, examples, Python tests, and WASM.
      If all 4 primary fingerprints are fresh and status="success", exit immediately.
      Saves ~0.46s by bypassing parse_args() and heavy module imports.

    Case 1 - Specific test: 'bash test <test_name>' with no cache-busting flags.
      Checks introspect cache + ninja skip + test result cache.
      If all match, exits immediately (~0.12s vs ~1s).

    Case 2 - Full unit test suite: 'bash test --cpp' or 'bash test --unit'.
      Checks full run cache (build.ninja + src/ + tests/ mtimes).
      If all match, exits immediately (~0.15s vs ~3.4s).

    Case 3 - Examples-only flag (--examples alone, no specific examples):
      Checks examples fingerprint mtime fast-path without instantiating FingerprintManager.
      Saves ~7s by bypassing parse_args() and all heavy imports.

    Case 4 - Python-tests-only flag (--py alone):
      Checks Python test fingerprint mtime fast-path without instantiating FingerprintManager.
      Saves ~0.5s per cached run.

    Correctness: Bails out if any of these flags are present (they change semantics):
      --clean, --force, --no-fingerprint, --debug, --build-mode, --docker, --run,
      --qemu, --examples, --py, --full, --check, -h/--help, --list-tests

    Args:
        start_time: Time when test.py started (used for elapsed time reporting).
    """
    argv = sys.argv[1:]

    # --- CASE 0: No flags (bash test with no arguments) ---
    # Default mode runs unit tests, examples, Python tests, and WASM.
    # If all 4 primary fingerprints are fresh and status="success", exit immediately.
    # Saves ~0.46s by bypassing parse_args() and heavy module imports.
    if not argv:
        try:
            # Use FILE-level mtime scanning (not directory mtime) to correctly detect
            # content-only modifications to source files. Directory mtimes do NOT update
            # when file content changes on NTFS/ext4, so directory-based checks can
            # produce false "nothing changed" cache hits (correctness bug).
            # File-level scanning adds ~70-120ms but is necessary for correctness.
            _fps_c0 = [
                Path(".cache/fingerprint/all.json"),
                Path(".cache/fingerprint/cpp_test_quick.json"),
                Path(".cache/fingerprint/examples_quick.json"),
                Path(".cache/fingerprint/python_test.json"),
            ]
            _mtimes_c0: list[float] = []
            for _fp_c0 in _fps_c0:
                try:
                    _st_c0 = _fp_c0.stat()
                except FileNotFoundError:
                    return
                with open(_fp_c0) as _f_c0:
                    if json.load(_f_c0).get("status") != "success":
                        return
                _mtimes_c0.append(_st_c0.st_mtime)
            _mt_all_c0, _mt_cpp_c0, _mt_ex_c0, _mt_py_c0 = _mtimes_c0
            # src/ is used by all, cpp_test, and examples fingerprints
            _min_src_c0 = min(_mt_all_c0, _mt_cpp_c0, _mt_ex_c0)

            # Parallel directory scans: os.scandir()+stat() release the GIL so
            # multiple OS threads can do true concurrent I/O. On cold OS cache
            # (after system restart), each scan takes ~20-50ms; parallel reduces
            # total scan time from ~80-120ms to ~25-35ms. On warm cache (~3ms
            # each), savings are ~7ms. threading is imported lazily here (and NOT
            # at module level) so CASE 1-4 paths avoid paying the ~2.5ms cost.
            import threading  # noqa: PLC0415 - lazy import for startup performance

            _scan_res_c0: list = [None, None, None, None]  # [src, tests, ex, ci]

            def _scan_c0(idx: int, root: Path, exts: frozenset = CPP_EXTS) -> None:
                try:
                    _scan_res_c0[idx] = max_file_mtime(root, exts)
                except KeyboardInterrupt:
                    _thread.interrupt_main()
                except Exception:
                    _scan_res_c0[idx] = float("inf")

            _threads_c0 = [
                threading.Thread(target=_scan_c0, args=(0, Path("src")), daemon=True),
                threading.Thread(target=_scan_c0, args=(1, Path("tests")), daemon=True),
                threading.Thread(
                    target=_scan_c0, args=(2, Path("examples")), daemon=True
                ),
                threading.Thread(
                    target=_scan_c0, args=(3, Path("ci"), PY_EXTS), daemon=True
                ),
            ]
            for _t_c0 in _threads_c0:
                _t_c0.start()
            for _t_c0 in _threads_c0:
                _t_c0.join()
            _max_src_c0, _max_tests_c0, _max_ex_c0, _max_ci_c0 = _scan_res_c0

            if _max_src_c0 > _min_src_c0:
                return  # src/ source file modified after fingerprint write
            if _max_tests_c0 > _mt_cpp_c0:
                return  # tests/ source file modified after cpp fingerprint write
            if _max_ex_c0 > _mt_ex_c0:
                return  # examples/ source file modified after examples fingerprint write
            if _max_ci_c0 > _mt_py_c0:
                return  # ci/ Python file modified after python fingerprint write
            print(
                "✓ Fingerprint cache valid - skipping all tests (no changes detected)"
            )
            import time  # noqa: PLC0415 - lazy import

            print(f"Total: {time.time() - start_time:.2f}s")
            sys.exit(0)
        except KeyboardInterrupt:
            _thread.interrupt_main()
        except Exception:
            pass
        return  # Fall through to normal execution

    # --- CASE 2: Full unit-test-suite flags (--cpp, --unit alone) ---
    # Check full run cache when only --cpp or --unit is passed (no other flags).
    # This avoids ~3s of fingerprint + imports for the fully-cached case.
    # --cpp includes examples, so we also verify examples/ haven't changed.
    # --unit is unit-tests only (no examples check needed).
    _FULL_RUN_FLAGS = frozenset(["--cpp", "--unit"])
    if set(argv) <= _FULL_RUN_FLAGS and len(argv) >= 1:
        try:
            _build_mode_fr = "quick"
            _build_dir_fr = Path(".build") / f"meson-{_build_mode_fr}"
            # For --cpp: also verify examples/ haven't changed (--cpp includes examples)
            _include_examples_fr = "--cpp" in argv
            _cached_fr = full_run_cache(
                _build_dir_fr, include_examples=_include_examples_fr
            )
            if _cached_fr:
                import time  # noqa: PLC0415 - lazy import

                _np_fr, _nt_fr, _dur_fr = _cached_fr
                print(
                    f"✓ Fingerprint cache valid - skipping C++ unit tests "
                    f"[{_np_fr}/{_nt_fr} passed in {_dur_fr:.2f}s]"
                )
                print(f"Total: {time.time() - start_time:.2f}s")
                sys.exit(0)
        except KeyboardInterrupt:
            _thread.interrupt_main()
        except Exception:
            pass  # Fall through to normal execution
        return  # Let normal path handle --cpp/--unit (avoids bail-flags check below)

    # --- CASE 3: Examples-only flag (--examples alone, no specific examples) ---
    # Check examples fingerprint mtime fast-path without instantiating FingerprintManager
    # or importing test_runner. Saves ~7s by bypassing parse_args() and all heavy imports.
    # Only fires when exactly ["--examples"] is passed (no specific example names, no flags).
    if argv == ["--examples"]:
        try:
            _fp_file = Path(".cache/fingerprint/examples_quick.json")
            if _fp_file.exists():
                _fp_mtime = _fp_file.stat().st_mtime
                # Use FILE-level mtime scan (not directory mtime) to correctly detect
                # content-only modifications. Directory mtimes do not update when file
                # content changes on NTFS/ext4 (correctness fix from iteration 16).
                _max_mtime = max(
                    max_file_mtime(Path("src")),
                    max_file_mtime(Path("examples")),
                )
                if _max_mtime <= _fp_mtime:
                    with open(_fp_file) as _f:
                        _fp_data = json.load(_f)
                    if _fp_data.get("status") == "success":
                        import time  # noqa: PLC0415 - lazy import

                        print(
                            "✓ Fingerprint cache valid - skipping examples (no changes detected)"
                        )
                        print(f"Total: {time.time() - start_time:.2f}s")
                        sys.exit(0)
        except KeyboardInterrupt:
            _thread.interrupt_main()
        except Exception:
            pass  # Fall through to normal execution
        return  # Let normal path handle --examples (avoids bail-flags check below)

    # --- CASE 4: Python-tests-only flag (--py alone) ---
    # Check Python test fingerprint mtime fast-path without instantiating FingerprintManager.
    # Python tests depend only on ci/ directory changes. Saves ~0.5s per cached run.
    if argv == ["--py"]:
        try:
            _fp_file_py = Path(".cache/fingerprint/python_test.json")
            if _fp_file_py.exists():
                _fp_mtime_py = _fp_file_py.stat().st_mtime
                # Use FILE-level .py scan (not directory mtime) for correctness.
                # Directory mtimes do not update when file content changes on NTFS/ext4,
                # so directory-based checks can miss content-only Python file modifications.
                _max_mtime_py = max_file_mtime(Path("ci"), PY_EXTS)
                if _max_mtime_py <= _fp_mtime_py:
                    with open(_fp_file_py) as _f_py:
                        _fp_data_py = json.load(_f_py)
                    if _fp_data_py.get("status") == "success":
                        import time  # noqa: PLC0415 - lazy import

                        print(
                            "✓ Fingerprint cache valid - skipping Python tests (no changes detected)"
                        )
                        print(f"Total: {time.time() - start_time:.2f}s")
                        sys.exit(0)
        except KeyboardInterrupt:
            _thread.interrupt_main()
        except Exception:
            pass  # Fall through to normal execution
        return  # Let normal path handle --py (avoids bail-flags check below)

    # Bail out if any cache-busting or mode-changing flags are present
    _BAIL_FLAGS = frozenset(
        [
            "--clean",
            "--force",
            "--no-fingerprint",
            "--debug",
            "--build-mode",
            "--docker",
            "--run",
            "--qemu",
            "--examples",
            "--py",
            "--full",
            "--check",
            "-h",
            "--help",
            "--list-tests",
            "--list",
            "--unit",
            "--cpp",
        ]
    )
    for a in argv:
        if a in _BAIL_FLAGS or (a.startswith("--build-mode=") or a.startswith("-")):
            return

    # Only proceed if exactly one positional argument (the test name)
    positional = [a for a in argv if not a.startswith("-")]
    if len(positional) != 1:
        return

    test_name_raw = positional[0]
    # Avoid treating file paths or flags as test names
    if not test_name_raw or test_name_raw.startswith("-"):
        return

    try:
        _build_dir = Path(".build") / "meson-quick"
        if check_single_test_cached(test_name_raw, _build_dir):
            import time  # noqa: PLC0415 - lazy import

            print("✅ All tests passed (1/1 cached)")
            print(f"Total: {time.time() - start_time:.2f}s")
            sys.exit(0)
    except KeyboardInterrupt:
        _thread.interrupt_main()
    except Exception:
        pass  # If ultra-early check fails, fall through to normal execution
