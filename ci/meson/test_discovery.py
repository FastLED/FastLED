"""Test discovery, fuzzy matching, and source file hashing for Meson build system."""

import hashlib
import json
import subprocess
from dataclasses import dataclass
from pathlib import Path

from ci.meson.compiler import get_meson_executable
from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.timestamp_print import ts_print as _ts_print


@dataclass(slots=True)
class SplitSourceHashes:
    src_hash: str
    tests_hash: str
    src_files: list[str]
    test_files: list[str]


def _load_introspect_tests_cache(build_dir: Path) -> list[str] | None:
    """
    Load cached test names from disk if the cache is still valid.

    The cache is stored in ``<build_dir>/.meson_introspect_tests_cache.json``
    as a plain JSON array of test name strings (NOT the full meson introspect
    output).  Storing only names keeps the file small (~6 KB vs ~180 KB),
    reducing parse time from ~100 ms to ~5 ms.

    The cache is considered valid when its mtime is at least as recent as
    ``<build_dir>/build.ninja``.  build.ninja is rewritten whenever meson
    reconfigures (adding or removing tests), so its mtime is a reliable
    invalidation signal for the registered test list.

    Returns:
        List of registered test name strings on cache hit, or None on miss/error.
    """
    cache_path = build_dir / ".meson_introspect_tests_cache.json"
    build_ninja = build_dir / "build.ninja"
    if not cache_path.exists() or not build_ninja.exists():
        return None
    try:
        cache_mtime = cache_path.stat().st_mtime
        ninja_mtime = build_ninja.stat().st_mtime
        if cache_mtime < ninja_mtime:
            return None  # build.ninja is newer → cache is stale
        data = json.loads(cache_path.read_text(encoding="utf-8"))
        # Validate: must be a list of strings (names-only format)
        if isinstance(data, list) and (not data or isinstance(data[0], str)):
            return data
        # Stale full-dict format from a previous version → treat as miss
        return None
    except (OSError, json.JSONDecodeError):
        return None


def _save_introspect_tests_cache(build_dir: Path, test_names: list[str]) -> None:
    """Persist test names to the cache file as a compact JSON array.

    Stores only test name strings (not the full meson introspect dict) to
    keep the file small (~6 KB) and parse time minimal (~5 ms).
    """
    cache_path = build_dir / ".meson_introspect_tests_cache.json"
    try:
        cache_path.write_text(json.dumps(test_names), encoding="utf-8")
    except OSError:
        pass  # Non-critical: fast-path simply won't fire on the next run


def get_fuzzy_test_candidates(build_dir: Path, test_name: str) -> list[str]:
    """
    Get fuzzy match candidates for a test name using meson introspect.

    This queries the Meson build system for all registered tests containing the
    test name substring, enabling fuzzy matching even when the build directory
    is freshly created and no executables exist yet.

    Uses ``meson introspect --tests`` which returns registered test names directly,
    avoiding issues with target type filtering (tests are shared libraries, not
    executables, in the DLL-based test architecture).

    Performance: the full ``meson introspect --tests`` subprocess (~700 ms) is
    expensive.  Results are cached in ``<build_dir>/.meson_introspect_tests_cache.json``
    and reused on subsequent runs as long as ``build.ninja`` has not changed.
    This saves ~700 ms per specific-test invocation (e.g., ``bash test alloca``).

    Args:
        build_dir: Meson build directory
        test_name: Test name to search for (e.g., "async", "s16x16")

    Returns:
        List of matching test names (e.g., ["fl_async", "fl_fixed_point_s16x16"])
    """
    if not build_dir.exists():
        return []

    # Fast-path: use cached test names when build.ninja hasn't changed.
    # Measured cost of meson introspect --tests: ~700 ms on Windows.
    # Cache invalidation: build.ninja mtime updates on every meson reconfigure,
    # which is the only event that changes the registered test list.
    # Cache stores only test name strings (~6 KB) for fast JSON parsing (~5 ms).
    test_names: list[str] | None = _load_introspect_tests_cache(build_dir)

    if test_names is None:
        # Cache miss: run meson introspect subprocess
        try:
            # Query Meson for all registered tests (not targets)
            # --tests returns test registrations which have the correct names
            # regardless of whether the underlying target is exe, shared_library, etc.
            result = subprocess.run(
                [get_meson_executable(), "introspect", str(build_dir), "--tests"],
                capture_output=True,
                text=True,
                encoding="utf-8",
                errors="replace",
                timeout=10,
                check=False,
            )

            # SELF-HEALING: Detect missing/corrupted intro files
            # When intro-targets.json, intro-tests.json or other introspection files are missing,
            # meson introspect fails with "is missing! Directory is not configured yet?"
            # This indicates the build directory is in a corrupted state.
            if result.returncode != 0:
                stderr_output = result.stderr.lower()
                stdout_output = result.stdout.lower()

                # Check for missing intro files error patterns
                missing_intro_patterns = [
                    "intro-targets.json",
                    "intro-tests.json",
                    "is missing",
                    "directory is not configured",
                    "not a valid build directory",
                    "introspection file",
                ]

                if any(
                    pattern in stderr_output or pattern in stdout_output
                    for pattern in missing_intro_patterns
                ):
                    _ts_print("")
                    _ts_print("=" * 80)
                    _ts_print(
                        "[MESON] ⚠️  BUILD DIRECTORY CORRUPTION DETECTED - AUTO-HEALING"
                    )
                    _ts_print("=" * 80)
                    _ts_print("[MESON] Missing or corrupted Meson introspection files:")
                    if result.stderr.strip():
                        # Show first 200 chars of error
                        error_preview = result.stderr.strip()[:200]
                        _ts_print(f"[MESON]   {error_preview}")
                    _ts_print("[MESON]")
                    _ts_print("[MESON] This typically happens when:")
                    _ts_print(
                        "[MESON]   1. Build directory was partially created/deleted"
                    )
                    _ts_print("[MESON]   2. Meson setup was interrupted mid-execution")
                    _ts_print(
                        "[MESON]   3. File system errors during build directory creation"
                    )
                    _ts_print("[MESON]")
                    _ts_print(
                        "[MESON] 🔧 Auto-fix: Forcing reconfiguration to rebuild intro files"
                    )
                    _ts_print("=" * 80)
                    _ts_print("")

                    # Create marker file to trigger reconfiguration
                    # The setup code in build_config.py will detect this and reconfigure
                    intro_corruption_marker = build_dir / ".intro_corruption_detected"
                    try:
                        intro_corruption_marker.touch()
                        _ts_print(
                            "[MESON] ✅ Created corruption marker for auto-healing"
                        )
                    except (OSError, IOError) as e:
                        _ts_print(
                            f"[MESON] Warning: Could not create corruption marker: {e}"
                        )

                return []

            # Parse JSON, extract only names, and persist compact cache for future runs
            all_tests = json.loads(result.stdout)
            test_names = [t.get("name", "") for t in all_tests if t.get("name")]
            _save_introspect_tests_cache(build_dir, test_names)

        except (subprocess.SubprocessError, json.JSONDecodeError, OSError) as e:
            _ts_print(f"[MESON] Warning: Failed to get fuzzy test candidates: {e}")
            return []

    # Pyright guard: test_names is guaranteed non-None here (all None paths return early above)
    if test_names is None:
        return []

    # Filter tests whose name contains the test_name substring (case-insensitive)
    test_name_lower = test_name.lower()
    candidates: list[str] = []

    for registered_name in test_names:
        if test_name_lower in registered_name.lower():
            candidates.append(registered_name)

    return candidates


_SOURCE_EXTENSIONS = {".cpp", ".h", ".hpp"}


def _hash_file_list(file_list: list[str]) -> str:
    """Hash a sorted list of file paths into a hex digest."""
    hasher = hashlib.sha256()
    for file_path in file_list:
        hasher.update(file_path.encode("utf-8"))
        hasher.update(b"\n")
    return hasher.hexdigest()


def _discover_files_in_dir(source_dir: Path, subdir: str) -> list[str]:
    """Discover .cpp/.h/.hpp files under source_dir/subdir, return sorted relative paths."""
    dir_path = source_dir / subdir
    if not dir_path.exists():
        return []
    files = sorted(
        str(p.relative_to(source_dir))
        for p in dir_path.rglob("*")
        if p.suffix in _SOURCE_EXTENSIONS and p.is_file()
    )
    return files


def get_source_files_hash(source_dir: Path) -> tuple[str, list[str]]:
    """
    Get combined hash of all .cpp/.h/.hpp files in src/ and tests/ directories.

    Kept for backward compatibility. The combined hash is used as the marker
    value stored in .source_files_hash.

    Returns:
        Tuple of (hash_string, sorted_file_list)
    """
    try:
        src_files = _discover_files_in_dir(source_dir, "src")
        test_files = _discover_files_in_dir(source_dir, "tests")
        all_files = sorted(src_files + test_files)
        return (_hash_file_list(all_files), all_files)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        _ts_print(f"[MESON] Warning: Failed to get source file hash: {e}")
        return ("", [])


def get_split_source_hashes(source_dir: Path) -> SplitSourceHashes:
    """
    Get separate hashes for src/ and tests/ directories.

    Returns:
        SplitSourceHashes with src_hash, tests_hash, src_files, and test_files.
    """
    try:
        src_files = _discover_files_in_dir(source_dir, "src")
        test_files = _discover_files_in_dir(source_dir, "tests")
        return SplitSourceHashes(
            src_hash=_hash_file_list(src_files),
            tests_hash=_hash_file_list(test_files),
            src_files=src_files,
            test_files=test_files,
        )
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        _ts_print(f"[MESON] Warning: Failed to get source file hashes: {e}")
        return SplitSourceHashes(
            src_hash="", tests_hash="", src_files=[], test_files=[]
        )


def list_all_tests(
    filter_pattern: str | None = None, filter_type: str | None = None
) -> None:
    """
    List all available tests (unit tests and examples).

    Args:
        filter_pattern: Optional pattern to filter test names
        filter_type: Optional filter type ("unit_test" or "example")
    """
    from ci.util.smart_selector import discover_all_tests

    # Discover all available tests
    unit_tests, examples = discover_all_tests()

    # Apply filters
    if filter_pattern:
        filter_pattern_lower = filter_pattern.lower()
        unit_tests = [
            (name, path)
            for name, path in unit_tests
            if filter_pattern_lower in name.lower()
        ]
        examples = [
            (name, path)
            for name, path in examples
            if filter_pattern_lower in name.lower()
        ]

    if filter_type == "unit_test":
        examples = []
    elif filter_type == "example":
        unit_tests = []

    # Print results
    _ts_print("Available tests:")
    _ts_print("")

    if unit_tests:
        _ts_print(f"Unit tests ({len(unit_tests)}):")
        for name, path in sorted(unit_tests):
            _ts_print(f"  {name:30} ({path})")
        _ts_print("")

    if examples:
        _ts_print(f"Examples ({len(examples)}):")
        for name, path in sorted(examples):
            _ts_print(f"  {name:30} ({path})")
        _ts_print("")

    total = len(unit_tests) + len(examples)
    _ts_print(f"Total: {total} tests")
