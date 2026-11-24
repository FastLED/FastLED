"""
Test file discovery for FastLED test suite.

This module finds and reports all test files that should be built and executed.
It's designed to be called by the Meson build system during configuration.

Host doctest suites live in tier directories under tests/:

  tests/unit/**         default tier, built and run by every `bash test`
  tests/integration/**  heavy suites, built and run only on request
                        (`bash test --integration` or `bash test <name>`)

tests/profile/ holds standalone benchmark binaries, not doctest suites, and is
discovered separately by tests/profile/meson.build.

Discovery inside a tier is fully automatic: every *.cpp / *.ino file is a
test, except for files in EXCLUDED_TEST_FILES and directories in
EXCLUDED_TEST_DIRS. .ino is included for parity with the rest of the
codebase (CPP_EXTS, FILE_PATTERNS, fingerprint rules, etc.).
"""

import sys
from itertools import chain
from pathlib import Path


# Extensions discovered as host test sources. Kept in sync with the
# CPP_EXTS frozenset in ci/early_exit_cache.py and ci/meson/cache_utils.py.
TEST_SOURCE_GLOBS = ("*.cpp", "*.ino")

# Doctest tiers, in discovery order. The tier directory is not part of a
# test's name: tests/unit/fl/x.cpp and tests/integration/fl/x.cpp would both
# be fl_x, so a name must exist in only one tier.
UNIT_TIER = "unit"
INTEGRATION_TIER = "integration"
TEST_TIERS = (UNIT_TIER, INTEGRATION_TIER)


def split_tier(test_file_path: str) -> tuple[str, str]:
    """Split ``<tier>/<path>`` into ``(tier, path)``.

    Args:
        test_file_path: Path relative to tests/ (POSIX format), e.g.
            ``unit/fl/algorithm.cpp``.

    Returns:
        The tier and the path inside it, e.g. ``("unit", "fl/algorithm.cpp")``.
    """
    tier, sep, rest = test_file_path.partition("/")
    if sep and tier in TEST_TIERS:
        return tier, rest
    raise ValueError(f"test file is outside every tier directory: {test_file_path}")


def discover_test_files(
    tests_dir: Path,
    excluded_files: set[Path],
    excluded_dirs: set[Path],
    tiers: tuple[str, ...] = TEST_TIERS,
) -> list[str]:
    """
    Discover all test files in the tier directories under tests/.

    Recursively scans tests/<tier>/ for *.cpp and *.ino files, excluding
    infrastructure files and directories.

    Args:
        tests_dir: Root directory containing the tier directories
        excluded_files: Set of full paths to exclude (e.g., {Path(".../tests/unit/misc/coroutine_impl.cpp")})
        excluded_dirs: Set of full directory paths to exclude (e.g., {Path(".../tests/unit/fl/codec")})
        tiers: Tier directories to scan (default: every tier)

    Returns:
        List of paths relative to tests_dir (POSIX format, e.g.
        ``unit/fl/algorithm.cpp``), sorted
    """
    test_files: list[str] = []

    for tier in tiers:
        tier_dir = tests_dir / tier
        if not tier_dir.is_dir():
            continue
        for f in chain.from_iterable(tier_dir.rglob(g) for g in TEST_SOURCE_GLOBS):
            resolved = f.resolve()

            # Skip excluded files
            if resolved in excluded_files:
                continue

            # Skip files inside excluded directories
            if any(resolved.is_relative_to(d) for d in excluded_dirs):
                continue

            # Skip hidden directories and .dir directories
            rel = f.relative_to(tests_dir)
            parent_parts = rel.parts[:-1]  # directory components only
            if any(
                part.startswith(".") or part.endswith(".dir") for part in parent_parts
            ):
                continue

            test_files.append(rel.as_posix())

    # Sort and deduplicate
    return sorted(set(test_files))


# Directories under tests/ that hold doctest sources but are not tiers.
# tests/profile builds its own standalone binaries.
NON_TIER_TEST_DIRS = ("profile",)


def find_untiered_test_sources(tests_dir: Path) -> list[str]:
    """Return doctest sources that sit outside every tier directory.

    Discovery only scans tests/<tier>/, so a test case left at the pre-tier
    layout (e.g. tests/fl/x.cpp after a rebase) would never build or run.
    Callers must treat a non-empty result as an error.
    """
    stray: list[str] = []
    for f in chain.from_iterable(tests_dir.rglob(g) for g in TEST_SOURCE_GLOBS):
        rel = f.relative_to(tests_dir)
        top = rel.parts[0] if len(rel.parts) > 1 else ""
        if top in TEST_TIERS or top in NON_TIER_TEST_DIRS:
            continue
        if any(
            part.startswith(".") or part.endswith(".dir") for part in rel.parts[:-1]
        ):
            continue
        text = f.read_text(encoding="utf-8", errors="replace")
        if "FL_TEST_CASE(" in text or "TEST_CASE(" in text:
            stray.append(rel.as_posix())
    return sorted(stray)


def main() -> None:
    """Main entry point for test discovery."""
    if len(sys.argv) != 2:
        print("Usage: discover_tests.py <tests_directory>", file=sys.stderr)
        sys.exit(1)

    tests_dir = Path(sys.argv[1]).resolve()

    if not tests_dir.exists():
        print(f"Error: Tests directory not found: {tests_dir}", file=sys.stderr)
        sys.exit(1)

    from test_config import EXCLUDED_TEST_DIRS, EXCLUDED_TEST_FILES

    test_files = discover_test_files(tests_dir, EXCLUDED_TEST_FILES, EXCLUDED_TEST_DIRS)

    # Output one file path per line
    for test_file in test_files:
        print(test_file)


if __name__ == "__main__":
    main()
