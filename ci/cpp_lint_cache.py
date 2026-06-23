from ci.util.global_interrupt_handler import handle_keyboard_interrupt


#!/usr/bin/env python3
"""
C++ Linting Cache Integration

Monitors C++ source files (src/, examples/, and tests/) to determine if
C++ linting (clang-format + custom linters) needs to be re-run.

Uses zccache-fingerprint (Rust/blake3) for fast change detection.
"""

import sys
from pathlib import Path

from ci.fingerprint import TwoLayerFingerprintCache
from ci.util.color_output import print_cache_hit, print_cache_miss
from ci.util.dependency_loader import DependencyManifest


# Default glob patterns for C++ linting (used when DependencyManifest is unavailable)
_DEFAULT_CPP_LINT_GLOBS = [
    "src/**/*.cpp",
    "src/**/*.h",
    "src/**/*.hpp",
    "src/**/*.c",
    "examples/**/*.ino",
    "examples/**/*.cpp",
    "examples/**/*.h",
    "examples/**/*.hpp",
    "tests/**/*.cpp",
    "tests/**/*.hpp",
    "ci/lint_cpp/*.py",
]


def _get_cpp_lint_patterns() -> tuple[list[str], list[str]]:
    """
    Get glob include/exclude patterns for C++ lint monitoring.

    Loads patterns from centralized dependencies.json manifest with
    fallback to hardcoded defaults.

    Returns:
        Tuple of (include_patterns, exclude_patterns).
    """
    try:
        manifest = DependencyManifest()
        include = manifest.get_globs("cpp_lint")
        exclude = manifest.get_excludes("cpp_lint")
        # Also monitor linter scripts
        if "ci/lint_cpp/*.py" not in include:
            include.append("ci/lint_cpp/*.py")
        return include, exclude
    except (FileNotFoundError, KeyError):
        return list(_DEFAULT_CPP_LINT_GLOBS), []


def _get_cpp_lint_cache() -> TwoLayerFingerprintCache:
    """Get the two-layer fingerprint cache for C++ linting."""
    cache_dir = Path(".cache")
    return TwoLayerFingerprintCache(cache_dir, "cpp_lint")


def check_cpp_files_changed() -> bool:
    """
    Check if C++ files have changed since the last successful lint run.

    Uses zccache-fingerprint for blake3-based change detection.

    Returns:
        True if files changed and C++ linting should run
        False if no changes detected and linting can be skipped
    """
    include, exclude = _get_cpp_lint_patterns()

    cache = _get_cpp_lint_cache()
    needs_update = cache.check_needs_update(include=include, exclude=exclude)

    if needs_update:
        print_cache_miss("C++ files changed - running linting")
        return True
    else:
        print_cache_hit("No C++ changes detected - skipping linting")
        return False


def mark_cpp_lint_success() -> None:
    """
    Mark the current C++ lint run as successful.

    Uses the safe pre-computed fingerprint pattern - saves the fingerprint
    that was computed before linting started.
    """
    cache = _get_cpp_lint_cache()

    try:
        cache.mark_success()
        print("🔧 C++ lint success fingerprint updated")
    except RuntimeError as e:
        print(f"⚠️  C++ fingerprint update failed: {e}")
        print(
            "    This indicates check_cpp_files_changed() was not called before linting"
        )


def invalidate_cpp_cache() -> None:
    """Invalidate the cache when lint fails, forcing a re-run next time."""
    cache = _get_cpp_lint_cache()

    try:
        cache.invalidate()
        print_cache_miss("C++ lint cache invalidated - will re-run linting next time")
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print_cache_miss(f"Failed to invalidate C++ cache: {e}")


def main() -> int:
    """Main entry point for C++ lint cache checking."""
    if len(sys.argv) < 2:
        print("Usage: python ci/cpp_lint_cache.py [check|success|failure]")
        return 1

    command = sys.argv[1]

    if command == "check":
        # Check if linting is needed
        needs_lint = check_cpp_files_changed()
        return 0 if needs_lint else 1  # Exit 1 means skip linting

    elif command == "success":
        # Mark lint as successful
        mark_cpp_lint_success()
        return 0

    elif command == "failure":
        # Invalidate cache on failure
        invalidate_cpp_cache()
        return 0

    else:
        print(f"Unknown command: {command}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
