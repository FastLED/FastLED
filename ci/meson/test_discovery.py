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

    This queries the Meson build system for all test targets containing the
    test name substring, enabling fuzzy matching even when the build directory
    is freshly created and no executables exist yet.

    Args:
        build_dir: Meson build directory
        test_name: Test name to search for (e.g., "async")

    Returns:
        List of matching target names (e.g., ["fl_async", "fl_async_logger"])
    """
    if not build_dir.exists():
        return []

    try:
        # Query Meson for all targets
        result = subprocess.run(
            [get_meson_executable(), "introspect", str(build_dir), "--targets"],
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
        targets = json.loads(result.stdout)

        # Filter targets:
        # 1. Must be an executable (type == "executable")
        # 2. Must be in tests/ directory
        # 3. Must contain the test name substring (case-insensitive)
        test_name_lower = test_name.lower()
        candidates: list[str] = []

        for target in targets:
            if target.get("type") != "executable":
                continue

            target_name = target.get("name", "")
            # Check if this is a test executable (defined in tests/ directory)
            defined_in = target.get("defined_in", "")
            if "tests" not in defined_in.lower():
                continue

            # Check if target name contains the test name substring
            if test_name_lower in target_name.lower():
                candidates.append(target_name)

        return candidates

    except (subprocess.SubprocessError, json.JSONDecodeError, OSError) as e:
        _ts_print(f"[MESON] Warning: Failed to get fuzzy test candidates: {e}")
        return []


def get_source_files_hash(source_dir: Path) -> tuple[str, list[str]]:
    """
    Get hash of all .cpp/.h source and test files in src/ and tests/ directories.

    This detects when source or test files are added or removed, which requires
    Meson reconfiguration to update the build graph (especially for test discovery).

    Args:
        source_dir: Project root directory

    Returns:
        Tuple of (hash_string, sorted_file_list)
    """
    try:
        # Recursively discover all .cpp and .h files in src/ directory
        src_path = source_dir / "src"
        source_files = sorted(
            str(p.relative_to(source_dir)) for p in src_path.rglob("*.cpp")
        )
        source_files.extend(
            sorted(str(p.relative_to(source_dir)) for p in src_path.rglob("*.h"))
        )

        # Recursively discover all .cpp and .h files in tests/ directory
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
