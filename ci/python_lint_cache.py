from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Python Linting Cache Integration

Monitors Python source files and configuration to determine if
pyright type checking needs to be re-run.

Uses the unified safe fingerprint cache system.
"""

import sys
from pathlib import Path

# Import the two-layer fingerprint cache (mtime + content hash)
from ci.fingerprint import TwoLayerFingerprintCache
from ci.util.color_output import print_cache_hit, print_cache_miss


def get_python_files() -> list[Path]:
    """
    Get all Python files that should be monitored for changes.

    Returns:
        List of Python file paths to monitor
    """
    files: list[Path] = []

    # Add test.py
    test_py = Path("test.py")
    if test_py.exists():
        files.append(test_py)

    # Add Python files in ci/ directory (excluding ci/tmp/ and ci/wasm/)
    ci_dir = Path("ci")
    if ci_dir.exists():
        for py_file in ci_dir.glob("**/*.py"):
            # Skip excluded directories
            if "ci/tmp/" in str(py_file) or "ci/wasm/" in str(py_file):
                continue
            files.append(py_file)

    # Add pyproject.toml (pyright configuration)
    pyproject = Path("pyproject.toml")
    if pyproject.exists():
        files.append(pyproject)

    return sorted(files, key=str)


def _get_python_lint_cache() -> TwoLayerFingerprintCache:
    """Get the two-layer fingerprint cache for Python linting."""
    cache_dir = Path(".cache")
    return TwoLayerFingerprintCache(cache_dir, "python_lint")


def check_python_files_changed() -> bool:
    """
    Check if Python files have changed since the last successful lint run.

    Uses the safe pre-computed fingerprint pattern to avoid race conditions.

    Returns:
        True if files changed and pyright should run
        False if no changes detected and pyright can be skipped
    """
    # Get all Python files to monitor
    file_paths = get_python_files()

    if not file_paths:
        print("📝 No Python files found - skipping pyright")
        return False

    # Use the safe pattern: check_needs_update stores fingerprint for later use
    cache = _get_python_lint_cache()
    needs_update = cache.check_needs_update(file_paths)

    if needs_update:
        print_cache_miss("Python files changed - running pyright")
        print(f"   Monitoring {len(file_paths)} files")
        if len(file_paths) <= 5:
            for f in file_paths:
                print(f"     {f}")
        else:
            print(f"     {file_paths[0]} ... {file_paths[-1]}")
        return True
    else:
        print_cache_hit("No Python changes detected - skipping pyright")
        return False


def mark_python_lint_success() -> None:
    """
    Mark the current Python lint run as successful.

    Uses the safe pre-computed fingerprint pattern - saves the fingerprint
    that was computed before linting started.
    """
    cache = _get_python_lint_cache()

    try:
        cache.mark_success()
        print("📝 Python lint success fingerprint updated")
    except RuntimeError as e:
        print(f"⚠️  Python fingerprint update failed: {e}")
        print(
            "    This indicates check_python_files_changed() was not called before linting"
        )


def invalidate_python_cache() -> None:
    """Invalidate the cache when lint/test fails, forcing a re-run next time."""
    cache = _get_python_lint_cache()

    try:
        cache.invalidate()
        print_cache_miss(
            "Python lint cache invalidated - will re-run pyright next time"
        )
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print_cache_miss(f"Failed to invalidate Python cache: {e}")


def main() -> int:
    """Main entry point for Python lint cache checking."""
    if len(sys.argv) < 2:
        print("Usage: python ci/python_lint_cache.py [check|success|failure]")
        return 1

    command = sys.argv[1]

    if command == "check":
        # Check if pyright is needed
        needs_lint = check_python_files_changed()
        return 0 if needs_lint else 1  # Exit 1 means skip pyright

    elif command == "success":
        # Mark lint as successful
        mark_python_lint_success()
        return 0

    elif command == "failure":
        # Invalidate cache on failure
        invalidate_python_cache()
        return 0

    else:
        print(f"Unknown command: {command}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
