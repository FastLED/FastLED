"""Lightweight cache/state helpers for Meson build system.

This module intentionally imports ONLY stdlib modules (json, os, platform, Path)
so that it can be imported cheaply (< 5ms) inside ultra-early exit fast-paths
in test.py without loading heavy dependencies (rich, typeguard, running_process, etc.).

All functions here are pure data-reading/writing utilities that gate build skip
decisions. They must remain free of any non-stdlib imports.
"""

import json
import os
import platform
from pathlib import Path
from typing import Optional


# ---------------------------------------------------------------------------
# Directory / file scanning helpers
# ---------------------------------------------------------------------------

# Directories to skip when scanning for source file mtimes.
# These are build artifacts, caches, and VCS internals that don't
# contain developer-edited source files.
_SKIP_DIR_NAMES = frozenset(
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
_SKIP_DIR_PREFIXES = (".build", "build", "builddir", ".")

# Source file extensions to check for modification
_SOURCE_EXTS = frozenset([".cpp", ".h", ".hpp", ".c", ".ino"])

# Python source file extensions (for ci/ scanning)
_PY_SOURCE_EXTS = frozenset([".py"])


def _should_skip_scan_dir(name: str) -> bool:
    """Return True if this directory should be excluded from source file scanning."""
    if name in _SKIP_DIR_NAMES:
        return True
    for prefix in _SKIP_DIR_PREFIXES:
        if name.startswith(prefix):
            return True
    return False


def get_max_dir_mtime(root: Path) -> float:
    """
    Return the maximum mtime of any SOURCE directory under *root* (root included).

    Walk only directory entries (skipping files) to detect whether any subdirectory
    has been modified since a marker file was written.  Adding or removing a file
    always updates the PARENT directory's mtime on NTFS, ext4, and APFS, so this
    gives an O(#dirs) change proxy vs O(#files) for a full rglob.

    Uses os.scandir for efficiency — on Windows, DirEntry.stat() reuses
    FindFirstFile data (no extra syscall per directory entry).

    Excludes non-source directories (``__pycache__``, ``.git``, etc.) that are
    updated by tools (Python bytecode caching, version control) without representing
    developer code changes.  Pruning them prevents false-positive cache invalidation.

    Returns 0.0 when root does not exist or any OS error occurs.
    """
    max_mtime = 0.0
    stack = [str(root)]
    while stack:
        current = stack.pop()
        try:
            mtime = os.stat(current).st_mtime
            if mtime > max_mtime:
                max_mtime = mtime
        except OSError:
            continue
        try:
            with os.scandir(current) as it:
                for entry in it:
                    try:
                        if entry.is_dir(follow_symlinks=False):
                            if not _should_skip_scan_dir(entry.name):
                                stack.append(entry.path)
                    except OSError:
                        pass
        except OSError:
            pass
    return max_mtime


def _get_max_source_file_mtime(
    root: Path, exts: Optional[frozenset[str]] = None
) -> float:
    """Return max mtime of any source file under root, skipping build directories.

    Uses os.scandir() for efficiency. On Windows, DirEntry.stat() uses cached
    metadata from FindNextFile (no extra syscall per file). On Linux, scandir
    similarly batches directory reads.

    Detects BOTH structural changes (file add/remove) AND content modifications
    (file writes), unlike the directory-mtime-only approach.

    Args:
        root: Root directory to scan
        exts: Set of file extensions to check (default: _SOURCE_EXTS for C++ files).
              Pass _PY_SOURCE_EXTS to scan Python files instead.

    Returns 0.0 on missing root or any OS error.
    """
    if exts is None:
        exts = _SOURCE_EXTS
    max_mtime = 0.0
    stack = [str(root)]
    while stack:
        current = stack.pop()
        try:
            with os.scandir(current) as it:
                for entry in it:
                    try:
                        name = entry.name
                        if entry.is_dir(follow_symlinks=False):
                            if not _should_skip_scan_dir(name):
                                stack.append(entry.path)
                        elif entry.is_file(follow_symlinks=False):
                            _, ext = os.path.splitext(name)
                            if ext.lower() in exts:
                                mtime = entry.stat(follow_symlinks=False).st_mtime
                                if mtime > max_mtime:
                                    max_mtime = mtime
                    except OSError:
                        pass
        except OSError:
            pass
    return max_mtime


# ---------------------------------------------------------------------------
# Ninja-skip state (gates ninja invocation when nothing changed)
# ---------------------------------------------------------------------------


def _get_ninja_skip_state_file(build_dir: Path) -> Path:
    """Return path to the ninja skip state cache file."""
    return build_dir / ".ninja_skip_state.json"


def check_ninja_skip(build_dir: Path, target: str) -> bool:
    """Check if the ninja invocation can be skipped for this target.

    Returns True if all of these hold:
    1. build.ninja mtime matches saved state (no meson reconfiguration)
    2. src/ max source file mtime <= saved value (no src/ code changes)
    3. tests/ max source file mtime <= saved value (no test source modifications)
    4. The target's output file exists with the same mtime as when last built

    Overhead: ~20-50ms (scandir over src/ + tests/ source files + stat calls + JSON read).
    Savings when fired: ~2-3s (skips the full ninja startup + dependency graph load).

    Correctness:
    - test source edits (alloca.cpp modified): detected via tests/ file mtime scan
    - src/ code changes: detected via src/ file mtime scan (not libfastled.a proxy,
      because libfastled.a is only updated AFTER ninja runs)
    - meson reconfiguration: detected via build.ninja mtime
    - external DLL modification: detected via output file mtime check
    """
    state_file = _get_ninja_skip_state_file(build_dir)
    if not state_file.exists():
        return False

    try:
        with open(state_file, "r", encoding="utf-8") as f:
            saved = json.load(f)

        # 1. Check build.ninja mtime (detects meson reconfiguration)
        build_ninja = build_dir / "build.ninja"
        if not build_ninja.exists():
            return False
        bn_mtime = build_ninja.stat().st_mtime
        if abs(bn_mtime - saved.get("build_ninja_mtime", -1.0)) > 0.001:
            return False

        # 2a. Check meson.build files (detects build config changes)
        cwd = Path.cwd()
        _meson_files = [
            cwd / "meson.build",
            cwd / "tests" / "meson.build",
            cwd / "ci" / "meson" / "native" / "meson.build",
            cwd / "ci" / "meson" / "wasm" / "meson.build",
            cwd / "examples" / "meson.build",
        ]
        _meson_max = 0.0
        for mf in _meson_files:
            try:
                _meson_max = max(_meson_max, mf.stat().st_mtime)
            except OSError:
                pass
        if _meson_max > saved.get("meson_max_file_mtime", -1.0) + 0.001:
            return False  # meson.build file modified since last build

        # 2b. Check src/ max source file mtime (detects src/ code changes).
        # Previously used libfastled.a mtime as a proxy, but libfastled.a is only
        # updated AFTER ninja runs. Since this check fires BEFORE ninja, we must
        # scan src/ files directly to detect content modifications.
        src_max_file_mtime = _get_max_source_file_mtime(cwd / "src")
        if src_max_file_mtime > saved.get("src_max_file_mtime", -1.0) + 0.001:
            return False  # src/ source file modified since last build

        # 3. Check tests/ max source file mtime.
        # Uses os.scandir() for efficiency (batches metadata reads).
        # Correctly detects both content modifications and structural changes.
        tests_max_file_mtime = _get_max_source_file_mtime(cwd / "tests")
        if tests_max_file_mtime > saved.get("tests_max_file_mtime", -1.0) + 0.001:
            return False  # Test source file modified since last build

        # 4. Check target-specific output file mtime (sanity / external-edit check)
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


def save_ninja_skip_state(build_dir: Path, target: str) -> None:
    """Save build state after successful ninja run to enable skip on next run.

    Stores:
    - build.ninja mtime (gate for meson reconfiguration)
    - libfastled.a mtime (gate for src/ code changes)
    - tests/ max source file mtime (gate for test source modifications)
    - Per-target: output file path + mtime (sanity gate)

    Called only after a successful compilation, so overhead (~30-50ms) is acceptable.
    """
    try:
        # Derive the output file path from the target name.
        # Strip "tests/" prefix if present (meson sometimes uses qualified names).
        stem = target.removeprefix("tests/")
        is_windows = platform.system() == "Windows"
        ext = ".dll" if is_windows else ".so"
        output_path = build_dir / "tests" / f"{stem}{ext}"
        if not output_path.exists():
            # Fall back to .exe for special targets (runner, profile tests)
            exe_path = build_dir / "tests" / f"{stem}.exe"
            if exe_path.exists():
                output_path = exe_path
            else:
                return  # Can't determine output file — don't save state

        state_file = _get_ninja_skip_state_file(build_dir)

        # Load existing state to preserve other targets
        existing: dict = {}
        if state_file.exists():
            try:
                with open(state_file, "r", encoding="utf-8") as f:
                    existing = json.load(f)
            except (json.JSONDecodeError, OSError):
                existing = {}

        # Update global build-state gates
        build_ninja = build_dir / "build.ninja"
        cwd = Path.cwd()
        existing["build_ninja_mtime"] = (
            build_ninja.stat().st_mtime if build_ninja.exists() else 0.0
        )

        # src/ max source file mtime: detects src/ code changes directly.
        # Previously used libfastled.a as a proxy, but file-level scanning
        # is needed because the skip check fires BEFORE ninja rebuilds libfastled.a.
        existing["src_max_file_mtime"] = _get_max_source_file_mtime(cwd / "src")

        # tests/ max source file mtime: detects test source modifications
        existing["tests_max_file_mtime"] = _get_max_source_file_mtime(cwd / "tests")

        # meson.build max mtime: detects build configuration changes
        _meson_files = [
            cwd / "meson.build",
            cwd / "tests" / "meson.build",
            cwd / "ci" / "meson" / "native" / "meson.build",
            cwd / "ci" / "meson" / "wasm" / "meson.build",
            cwd / "examples" / "meson.build",
        ]
        _meson_max = 0.0
        for mf in _meson_files:
            try:
                _meson_max = max(_meson_max, mf.stat().st_mtime)
            except OSError:
                pass
        existing["meson_max_file_mtime"] = _meson_max

        # Update per-target state
        existing.setdefault("targets", {})[target] = {
            "output_path": str(output_path),
            "output_mtime": output_path.stat().st_mtime,
        }

        with open(state_file, "w", encoding="utf-8") as f:
            json.dump(existing, f)

    except (OSError, json.JSONDecodeError):
        pass  # Non-critical — next run will fall back to ninja normally


# ---------------------------------------------------------------------------
# Per-test result cache (gates test re-execution when DLL unchanged)
# ---------------------------------------------------------------------------


def _get_test_result_cache_file(build_dir: Path) -> Path:
    """Return path to the test result cache file."""
    return build_dir / ".test_result_cache.json"


def check_test_result_cached(
    build_dir: Path, test_name: str, artifact_path: Path
) -> bool:
    """Check if the test result is cached and the test can be skipped.

    Returns True if:
    1. A cache entry exists for this test name
    2. The artifact (DLL/exe) exists at the saved path
    3. The artifact mtime matches the cached value (+-0.001s)
    4. The last result was "pass"

    Rationale for DLL mtime as the sole gate:
    - src/ changes -> libfastled.a rebuilt (ar_content_preserving) -> DLL relinked -> mtime changes
    - test source changes -> DLL recompiled + relinked -> mtime changes
    - meson reconfiguration -> DLL rebuilt -> mtime changes
    Any code change that would affect test behavior also changes the DLL mtime.

    Overhead: ~3ms (one JSON read + one stat call). Savings: ~2s per cached run.
    """
    cache_file = _get_test_result_cache_file(build_dir)
    if not cache_file.exists():
        return False
    if not artifact_path.exists():
        return False

    try:
        with open(cache_file, "r", encoding="utf-8") as f:
            cache = json.load(f)

        entry = cache.get(test_name)
        if not entry:
            return False

        if entry.get("result") != "pass":
            return False

        saved_mtime = entry.get("artifact_mtime", -1.0)
        current_mtime = artifact_path.stat().st_mtime
        if abs(current_mtime - saved_mtime) > 0.001:
            return False

        return True

    except (OSError, json.JSONDecodeError, KeyError, TypeError, ValueError):
        return False


def save_test_result_state(
    build_dir: Path, test_name: str, artifact_path: Path, passed: bool
) -> None:
    """Save test result to cache for future skip decisions.

    Records artifact path + mtime and pass/fail result.
    Called after each test run (success or failure) so the next run can skip
    re-execution when the artifact is unchanged and the last result was "pass".
    """
    cache_file = _get_test_result_cache_file(build_dir)
    try:
        existing: dict = {}
        if cache_file.exists():
            try:
                with open(cache_file, "r", encoding="utf-8") as f:
                    existing = json.load(f)
            except (json.JSONDecodeError, OSError):
                existing = {}

        if artifact_path.exists():
            existing[test_name] = {
                "artifact_path": str(artifact_path),
                "artifact_mtime": artifact_path.stat().st_mtime,
                "result": "pass" if passed else "fail",
            }

        with open(cache_file, "w", encoding="utf-8") as f:
            json.dump(existing, f)

    except (OSError, json.JSONDecodeError):
        pass  # Non-critical — next run will fall back to running normally


# ---------------------------------------------------------------------------
# Full test-suite run cache (gates full re-run when nothing changed)
# ---------------------------------------------------------------------------


def get_full_run_cache_file(build_dir: Path) -> Path:
    """Return path to the full test suite run cache file."""
    return build_dir / ".full_run_cache.json"


def check_full_run_cache(
    build_dir: Path,
    include_examples: bool = False,
) -> Optional[tuple[int, int, float]]:
    """Check if full test suite passed with current build state.

    Returns (num_passed, num_tests, duration_seconds) if conditions match, else None.

    Uses same gate conditions as check_ninja_skip:
    1. build.ninja mtime unchanged (no meson reconfiguration)
    2. libfastled.a mtime unchanged (no src/ code changes)
    3. tests/ max source file mtime unchanged (no test source modifications)
    4. Last result was "pass"
    5. [Optional] examples/ max source file mtime unchanged (when include_examples=True)

    Args:
        build_dir: Meson build directory
        include_examples: If True, also verify examples/ haven't changed.
                         Use this for 'bash test --cpp' (includes examples).

    Overhead: ~30-60ms (scandir over tests/ [+ examples/] + 2 stat calls + JSON read).
    Savings: ~3s per cached 'bash test --cpp' run (skips fingerprint + imports).
    """
    cache_file = get_full_run_cache_file(build_dir)
    if not cache_file.exists():
        return None

    try:
        with open(cache_file, "r", encoding="utf-8") as f:
            saved = json.load(f)

        if saved.get("result") != "pass":
            return None

        # 1. Check build.ninja mtime (detects meson reconfiguration)
        build_ninja = build_dir / "build.ninja"
        if not build_ninja.exists():
            return None
        if (
            abs(build_ninja.stat().st_mtime - saved.get("build_ninja_mtime", -1.0))
            > 0.001
        ):
            return None

        # 2a. Check meson.build files (detects build config changes).
        # _max_meson_build_mtime raises on an unreadable (as opposed to
        # absent) build file rather than under-reporting the max, which here
        # would read as "unchanged" and serve a stale entry. The enclosing
        # except turns that into a cache miss -- fail closed.
        cwd = Path.cwd()
        _meson_max = _max_meson_build_mtime(cwd)
        if _meson_max > saved.get("meson_max_file_mtime", -1.0) + 0.001:
            return None

        # 2b. Check src/ max source file mtime (detects src/ code changes).
        # Previously used libfastled.a mtime as a proxy, but libfastled.a is only
        # updated AFTER ninja runs. Since this check fires BEFORE ninja, we must
        # scan src/ files directly to detect content modifications.
        src_max_mtime = _get_max_source_file_mtime(cwd / "src")
        if src_max_mtime > saved.get("src_max_file_mtime", -1.0) + 0.001:
            return None

        # 3. Check tests/ max source file mtime.
        tests_max_mtime = _get_max_source_file_mtime(cwd / "tests")
        if tests_max_mtime > saved.get("tests_max_file_mtime", -1.0) + 0.001:
            return None

        # 4. [Optional] Check examples/ max source file mtime.
        # Used by 'bash test --cpp' which includes example compilation.
        if include_examples:
            examples_max_mtime = _get_max_source_file_mtime(cwd / "examples")
            if examples_max_mtime > saved.get("examples_max_file_mtime", -1.0) + 0.001:
                return None

        num_passed = int(saved.get("num_passed", 0))
        num_tests = int(saved.get("num_tests", 0))
        duration = float(saved.get("duration", 0.0))

        return (num_passed, num_tests, duration)

    except (OSError, json.JSONDecodeError, KeyError, TypeError, ValueError):
        return None


# The full set of source-tree watermarks a full-run cache entry is gated on.
# Persisting a partial set would leave an ungated directory, so writes require
# all four.
_WATERMARK_KEYS = frozenset(
    {
        "src_max_file_mtime",
        "tests_max_file_mtime",
        "examples_max_file_mtime",
        "meson_max_file_mtime",
    }
)


def _meson_build_files(root: Path) -> "list[Path]":
    """The build-definition files whose mtimes gate every cache entry."""
    return [
        root / "meson.build",
        root / "tests" / "meson.build",
        root / "ci" / "meson" / "native" / "meson.build",
        root / "ci" / "meson" / "wasm" / "meson.build",
        root / "examples" / "meson.build",
    ]


def _max_meson_build_mtime(root: Path) -> float:
    """Max mtime across the build-definition files. Fails closed.

    A missing file is expected and skipped -- not every checkout has, say,
    examples/meson.build. Any OTHER stat error (permissions, I/O) is raised
    rather than swallowed.

    Swallowing those would silently produce a LOW maximum, and low is the
    dangerous direction: on the check side the comparison is
    `now > saved -> invalidate`, so an under-reported "now" reads as
    "unchanged" and serves a stale cache entry for a build definition that
    actually changed. That is the same silent-staleness failure this module's
    watermarks exist to prevent, so it must not be reintroduced by an
    over-broad except.
    """
    max_mtime = 0.0
    for mf in _meson_build_files(root):
        try:
            max_mtime = max(max_mtime, mf.stat().st_mtime)
        except FileNotFoundError:
            continue  # optional build file, genuinely absent
        except OSError as e:
            raise OSError(f"cannot stat build definition {mf}: {e}") from e
    return max_mtime


def capture_source_watermarks(cwd: Optional[Path] = None) -> "dict[str, float]":
    """Snapshot source-tree mtimes to gate a test run that is about to start.

    Must be called BEFORE the run whose result it will describe. Scanning at
    save time instead is wrong: a file added while the suite was running gets
    folded into the watermark, so the cache claims that file's state was tested
    when it never was, and every later run skips on a stale pass. Capturing up
    front leaves such a file strictly newer than the watermark, so the next run
    correctly rebuilds. See the test-discovery counterpart in
    meson_setup_phases._write_test_list_watermark.

    Raises:
        OSError: if a build-definition file exists but cannot be stat'd. The
            caller must not fall back to a partial value -- see
            _max_meson_build_mtime.
    """
    root = cwd if cwd is not None else Path.cwd()
    return {
        "src_max_file_mtime": _get_max_source_file_mtime(root / "src"),
        "tests_max_file_mtime": _get_max_source_file_mtime(root / "tests"),
        "examples_max_file_mtime": _get_max_source_file_mtime(root / "examples"),
        "meson_max_file_mtime": _max_meson_build_mtime(root),
    }


def save_full_run_result(
    build_dir: Path,
    num_passed: int,
    num_tests: int,
    duration: float,
    watermarks: "Optional[dict[str, float]]" = None,
) -> None:
    """Save full test suite result for future fast-path.

    Called after a successful full unit-test suite run (all tests passed).
    Stores build-state gates so future runs can skip fingerprint + imports
    when nothing has changed. Also captures examples/ mtime so the
    'bash test --cpp' ultra-early exit can correctly detect example changes.

    Args:
        watermarks: Source mtimes from capture_source_watermarks(), taken
            BEFORE the run. REQUIRED and must be complete -- if omitted or
            partial this fails closed and returns WITHOUT persisting a cache
            entry. There is deliberately no scan-now fallback: scanning at save
            time cannot distinguish "unchanged" from "changed while we were
            running", so it would cache a pass over files that were never
            tested. Callers that cannot supply a full set simply forgo the
            fast path for one run.
    """
    cache_file = get_full_run_cache_file(build_dir)
    try:
        build_ninja = build_dir / "build.ninja"

        # A complete pre-run watermark is mandatory. There is deliberately no
        # scan-now fallback: scanning here (after the run) is the original bug
        # -- it folds files added during the run into the watermark and marks
        # them as covered by a pass they never took part in. If the caller
        # cannot supply a full set, skip persisting entirely. The cost is one
        # extra full run next time; the alternative is a silent stale green.
        if watermarks is None or not _WATERMARK_KEYS <= watermarks.keys():
            return
        marks = {k: watermarks[k] for k in _WATERMARK_KEYS}

        data = {
            "result": "pass",
            "num_passed": num_passed,
            "num_tests": num_tests,
            "duration": duration,
            "build_ninja_mtime": (
                build_ninja.stat().st_mtime if build_ninja.exists() else 0.0
            ),
            **marks,
        }

        with open(cache_file, "w", encoding="utf-8") as f:
            json.dump(data, f)

    except (OSError, json.JSONDecodeError):
        pass  # Non-critical — next run will fall back to normal execution
