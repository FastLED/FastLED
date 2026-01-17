from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
C++ Linting Cache Integration

Monitors C++ source files (src/ and examples/) to determine if
C++ linting (clang-format + custom linters) needs to be re-run.

Uses the unified safe fingerprint cache system.
"""

import sys
from pathlib import Path

from ci.fingerprint import TwoLayerFingerprintCache
from ci.util.color_output import print_cache_hit, print_cache_miss
from ci.util.dependency_loader import DependencyManifest


def get_cpp_files() -> list[Path]:
    """
    Get all C++ files that should be monitored for changes.

    Loads patterns from centralized dependencies.json manifest.

    Returns:
        List of C++ file paths to monitor
    """
    files: list[Path] = []

    try:
        manifest = DependencyManifest()
        globs = manifest.get_globs("cpp_lint")
        excludes = manifest.get_excludes("cpp_lint")
    except (FileNotFoundError, KeyError):
        # Fallback to hardcoded patterns if manifest not available
        globs = [
            "src/**/*.cpp",
            "src/**/*.h",
            "src/**/*.hpp",
            "src/**/*.c",
            "examples/**/*.ino",
            "examples/**/*.cpp",
            "examples/**/*.h",
            "examples/**/*.hpp",
        ]
        excludes = []

    # Collect all files matching globs
    for glob_pattern in globs:
        for file_path in Path(".").glob(glob_pattern):
            if file_path.is_file():
                # Check exclusions
                skip = False
                for exclude_pattern in excludes:
                    if exclude_pattern in str(file_path):
                        skip = True
                        break

                if not skip:
                    files.append(file_path)

    # Also monitor linter scripts in ci/lint_cpp/
    # If linter logic changes, we should re-run linting
    lint_cpp_dir = Path("ci/lint_cpp")
    if lint_cpp_dir.exists():
        for py_file in lint_cpp_dir.glob("*.py"):
            if py_file.is_file():
                files.append(py_file)

    return sorted(files, key=str)


def _get_cpp_lint_cache() -> TwoLayerFingerprintCache:
    """Get the two-layer fingerprint cache for C++ linting."""
    cache_dir = Path(".cache")
    return TwoLayerFingerprintCache(cache_dir, "cpp_lint")


def check_cpp_files_changed() -> bool:
    """
    Check if C++ files have changed since the last successful lint run.

    Uses the safe pre-computed fingerprint pattern to avoid race conditions.

    Returns:
        True if files changed and C++ linting should run
        False if no changes detected and linting can be skipped
    """
    # Get all C++ files to monitor
    file_paths = get_cpp_files()

    if not file_paths:
        print("🔧 No C++ files found - skipping linting")
        return False

    # Use the safe pattern: check_needs_update stores fingerprint for later use
    cache = _get_cpp_lint_cache()
    needs_update = cache.check_needs_update(file_paths)

    if needs_update:
        print_cache_miss("C++ files changed - running linting")
        print(f"   Monitoring {len(file_paths)} files")
        if len(file_paths) <= 5:
            for f in file_paths:
                print(f"     {f}")
        else:
            print(f"     {file_paths[0]} ... {file_paths[-1]}")
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
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
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
