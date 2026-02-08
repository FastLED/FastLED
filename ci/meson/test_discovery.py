"""Test discovery, fuzzy matching, and source file hashing for Meson build system."""

import hashlib
import json
import subprocess
from pathlib import Path

from ci.meson.compiler import get_meson_executable
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.timestamp_print import ts_print as _ts_print


def get_fuzzy_test_candidates(build_dir: Path, test_name: str) -> list[str]:
    """
    Get fuzzy match candidates for a test name using meson introspect.

    This queries the Meson build system for all registered tests containing the
    test name substring, enabling fuzzy matching even when the build directory
    is freshly created and no executables exist yet.

    Uses ``meson introspect --tests`` which returns registered test names directly,
    avoiding issues with target type filtering (tests are shared libraries, not
    executables, in the DLL-based test architecture).

    Args:
        build_dir: Meson build directory
        test_name: Test name to search for (e.g., "async", "s16x16")

    Returns:
        List of matching test names (e.g., ["fl_async", "fl_fixed_point_s16x16"])
    """
    if not build_dir.exists():
        return []

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

        if result.returncode != 0:
            return []

        # Parse JSON output
        tests = json.loads(result.stdout)

        # Filter tests whose name contains the test_name substring (case-insensitive)
        test_name_lower = test_name.lower()
        candidates: list[str] = []

        for test in tests:
            registered_name = test.get("name", "")
            if test_name_lower in registered_name.lower():
                candidates.append(registered_name)

        return candidates

    except (subprocess.SubprocessError, json.JSONDecodeError, OSError) as e:
        _ts_print(f"[MESON] Warning: Failed to get fuzzy test candidates: {e}")
        return []


def get_source_files_hash(source_dir: Path) -> tuple[str, list[str]]:
    """
    Get hash of all .cpp/.h/.hpp source and test files in src/ and tests/ directories.

    This detects when source or test files are added, removed, or renamed (including
    .h -> .hpp renames), which requires Meson reconfiguration to update the build graph.

    Args:
        source_dir: Project root directory

    Returns:
        Tuple of (hash_string, sorted_file_list)
    """
    try:
        # Recursively discover all .cpp, .h, and .hpp files in src/ directory
        # NOTE: .hpp is included to detect .h -> .hpp renames that require reconfiguration
        src_path = source_dir / "src"
        source_files = sorted(
            str(p.relative_to(source_dir)) for p in src_path.rglob("*.cpp")
        )
        source_files.extend(
            sorted(str(p.relative_to(source_dir)) for p in src_path.rglob("*.h"))
        )
        source_files.extend(
            sorted(str(p.relative_to(source_dir)) for p in src_path.rglob("*.hpp"))
        )

        # Recursively discover all .cpp, .h, and .hpp files in tests/ directory
        # This is CRITICAL for detecting when tests are added or removed
        tests_path = source_dir / "tests"
        if tests_path.exists():
            source_files.extend(
                sorted(
                    str(p.relative_to(source_dir)) for p in tests_path.rglob("*.cpp")
                )
            )
            source_files.extend(
                sorted(str(p.relative_to(source_dir)) for p in tests_path.rglob("*.h"))
            )
            source_files.extend(
                sorted(
                    str(p.relative_to(source_dir)) for p in tests_path.rglob("*.hpp")
                )
            )

        # Re-sort the combined list for consistent ordering
        source_files = sorted(source_files)

        # Hash the list of file paths (not contents - just detect add/remove)
        hasher = hashlib.sha256()
        for file_path in source_files:
            hasher.update(file_path.encode("utf-8"))
            hasher.update(b"\n")  # Separator

        return (hasher.hexdigest(), source_files)

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        _ts_print(f"[MESON] Warning: Failed to get source file hash: {e}")
        return ("", [])


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
