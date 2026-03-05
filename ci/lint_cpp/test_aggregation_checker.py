#!/usr/bin/env python3
"""
Lint check: Validate test aggregation structure.

Excluded test subdirectories (defined in tests/test_config.py) must have a
parent .cpp aggregator that #includes every .hpp file in the subdirectory.

Rules:
  1. Every .hpp file in an excluded test subdirectory must be #included by
     a parent .cpp aggregator file.
  2. Parent aggregators must not include .cpp files from subdirectories —
     those should be .hpp.

The parent aggregator is found by convention: for excluded directory
  tests/fl/codec/
the aggregator is
  tests/fl/codec.cpp
(i.e., the directory name + .cpp in the parent directory).
"""

from __future__ import annotations

import re
from pathlib import Path

from ci.util.check_files import FileContent, FileContentChecker
from ci.util.paths import PROJECT_ROOT


TESTS_DIR = PROJECT_ROOT / "tests"

# Only check consolidated test directories (under tests/fl/) that follow
# the aggregation pattern. Infrastructure exclusions (tests/shared,
# tests/data, tests/profile, etc.) don't have aggregators.
_AGGREGATED_TEST_DIRS: set[Path] | None = None


def _get_aggregated_test_dirs() -> set[Path]:
    """Get excluded test dirs that follow the aggregation pattern.

    Only returns dirs under tests/fl/ — these are the ones that must have
    a parent .cpp aggregator. Infrastructure dirs (tests/shared, tests/data,
    etc.) are excluded from discovery for other reasons.
    """
    global _AGGREGATED_TEST_DIRS
    if _AGGREGATED_TEST_DIRS is not None:
        return _AGGREGATED_TEST_DIRS
    import importlib.util

    config_path = TESTS_DIR / "test_config.py"
    try:
        spec = importlib.util.spec_from_file_location("test_config", config_path)
        if spec is None or spec.loader is None:
            raise ImportError(f"Cannot load {config_path}")
        mod = importlib.util.module_from_spec(spec)
        spec.loader.exec_module(mod)
        all_excluded: set[Path] = mod.EXCLUDED_TEST_DIRS  # type: ignore[attr-defined]
    except (ImportError, AttributeError, FileNotFoundError):
        _AGGREGATED_TEST_DIRS = set()
        return _AGGREGATED_TEST_DIRS

    # Filter to only dirs under tests/fl/ (the aggregation pattern dirs)
    fl_tests = TESTS_DIR / "fl"
    _AGGREGATED_TEST_DIRS = {d for d in all_excluded if d.is_relative_to(fl_tests)}
    return _AGGREGATED_TEST_DIRS


# Pattern matching #include "subdir/file.ext"
INCLUDE_PATTERN = re.compile(r'^\s*#include\s+"([^"]+)"')


def _find_aggregator(excluded_dir: Path) -> Path | None:
    """Find the parent .cpp aggregator for an excluded directory.

    Convention: tests/fl/codec/ -> tests/fl/codec.cpp
    """
    aggregator = excluded_dir.with_suffix(".cpp")
    if aggregator.exists():
        return aggregator
    return None


def _parse_includes(aggregator: Path) -> set[str]:
    """Parse all #include paths from an aggregator file."""
    includes: set[str] = set()
    try:
        content = aggregator.read_text(encoding="utf-8")
    except OSError:
        return includes
    for line in content.splitlines():
        m = INCLUDE_PATTERN.match(line)
        if m:
            includes.add(m.group(1))
    return includes


def _collect_included_files(excluded_dir: Path) -> tuple[Path | None, set[Path]]:
    """Collect all files included by any ancestor aggregator.

    Walks up from the excluded dir, checking each ancestor's .cpp aggregator.
    Resolves include paths to actual file paths so we can match regardless
    of how the include path is written.

    Returns (primary_aggregator, set_of_included_absolute_file_paths).
    """
    aggregator = _find_aggregator(excluded_dir)
    included_files: set[Path] = set()

    # Check the direct aggregator and all ancestors
    check_dir = excluded_dir
    while check_dir.is_relative_to(TESTS_DIR) and check_dir != TESTS_DIR:
        agg = check_dir.with_suffix(".cpp")
        if agg.exists():
            agg_dir = agg.parent
            for inc_path in _parse_includes(agg):
                # Resolve include relative to the aggregator's directory
                resolved = (agg_dir / inc_path).resolve()
                if resolved.exists():
                    included_files.add(resolved)
        check_dir = check_dir.parent

    return aggregator, included_files


def check() -> tuple[bool, list[str]]:
    """Run full aggregation check across all excluded test directories.

    Returns:
        (success, violations) tuple.
    """
    violations: list[str] = []
    excluded_dirs = _get_aggregated_test_dirs()

    for excluded_dir in sorted(excluded_dirs):
        if not excluded_dir.exists():
            continue

        aggregator, included_files = _collect_included_files(excluded_dir)
        if aggregator is None:
            rel = excluded_dir.relative_to(PROJECT_ROOT)
            violations.append(
                f"Missing aggregator: {rel.as_posix()}.cpp "
                f"(no parent .cpp file for excluded directory {rel.as_posix()}/)"
            )
            continue

        # Check every .hpp file in the excluded directory and subdirs
        for hpp_file in sorted(excluded_dir.rglob("*.hpp")):
            resolved = hpp_file.resolve()
            if resolved not in included_files:
                rel_agg = aggregator.relative_to(PROJECT_ROOT)
                rel_file = hpp_file.relative_to(PROJECT_ROOT)
                violations.append(
                    f"{rel_agg.as_posix()}: orphaned test file not "
                    f"#included by any aggregator: {rel_file.as_posix()}"
                )

        # Check for .cpp includes in the direct aggregator (should be .hpp)
        for inc_path in sorted(_parse_includes(aggregator)):
            inc_resolved = (aggregator.parent / inc_path).resolve()
            if inc_resolved.is_relative_to(excluded_dir) and inc_path.endswith(".cpp"):
                rel_agg = aggregator.relative_to(PROJECT_ROOT)
                violations.append(
                    f'{rel_agg.as_posix()}: #include "{inc_path}" should use .hpp '
                    f"(included test files must use .hpp extension)"
                )

    return len(violations) == 0, violations


def check_single_file(file_path: Path) -> tuple[bool, list[str]]:
    """Check a single file for aggregation violations.

    If the file is:
    - An aggregator .cpp: verify it includes all .hpp children
    - An .hpp in an excluded dir: verify some aggregator includes it
    """
    violations: list[str] = []
    excluded_dirs = _get_aggregated_test_dirs()
    resolved = file_path.resolve()

    # Case 1: File is an aggregator .cpp for an excluded dir
    for excluded_dir in excluded_dirs:
        aggregator = _find_aggregator(excluded_dir)
        if aggregator and aggregator.resolve() == resolved:
            _, included_files = _collect_included_files(excluded_dir)
            for hpp_file in sorted(excluded_dir.rglob("*.hpp")):
                if hpp_file.resolve() not in included_files:
                    rel_agg = aggregator.relative_to(PROJECT_ROOT)
                    rel_file = hpp_file.relative_to(PROJECT_ROOT)
                    violations.append(
                        f"{rel_agg.as_posix()}: orphaned {rel_file.as_posix()}"
                    )
            for inc_path in sorted(_parse_includes(aggregator)):
                inc_resolved = (aggregator.parent / inc_path).resolve()
                if inc_resolved.is_relative_to(excluded_dir) and inc_path.endswith(
                    ".cpp"
                ):
                    rel_agg = aggregator.relative_to(PROJECT_ROOT)
                    violations.append(
                        f'{rel_agg.as_posix()}: #include "{inc_path}" should use .hpp'
                    )
            return len(violations) == 0, violations

    # Case 2: File is an .hpp inside an excluded directory
    for excluded_dir in excluded_dirs:
        if not resolved.is_relative_to(excluded_dir):
            continue
        aggregator, included_files = _collect_included_files(excluded_dir)
        if not aggregator:
            rel = excluded_dir.relative_to(PROJECT_ROOT)
            violations.append(f"Missing aggregator: {rel.as_posix()}.cpp")
            return False, violations
        if resolved not in included_files:
            rel_agg = aggregator.relative_to(PROJECT_ROOT)
            rel_file = file_path.relative_to(PROJECT_ROOT)
            violations.append(f"{rel_agg.as_posix()}: orphaned {rel_file.as_posix()}")
        return len(violations) == 0, violations

    # Not relevant to aggregation
    return True, []


class TestAggregationChecker(FileContentChecker):
    """FileContentChecker wrapper for integration with run_all_checkers.py."""

    def __init__(self) -> None:
        self.violations: dict[str, list[tuple[int, str]]] = {}

    def should_process_file(self, file_path: str) -> bool:
        if "/tests/" not in file_path.replace("\\", "/"):
            return False
        return file_path.endswith((".cpp", ".hpp"))

    def check_file_content(self, file_content: FileContent) -> list[str]:
        success, violations = check_single_file(Path(file_content.path))
        if not success:
            self.violations[file_content.path] = [(0, v) for v in violations]
        return []


def main() -> int:
    """Run standalone check."""
    success, violations = check()
    if success:
        print("✅ Test aggregation structure OK")
        return 0
    print(f"❌ Found {len(violations)} test aggregation violation(s):")
    for v in violations:
        print(f"  {v}")
    return 1


if __name__ == "__main__":
    import sys

    sys.exit(main())
