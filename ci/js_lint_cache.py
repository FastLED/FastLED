#!/usr/bin/env python3
"""
JavaScript Linting Cache Integration

Monitors the src/platforms/wasm/compiler directory for any changes
to determine if JS files need re-linting.

Refactored to use the new hash-based fingerprint cache system.
"""

import _thread
import sys
from pathlib import Path

# Import the two-layer fingerprint cache (mtime + content hash)
from ci.fingerprint import TwoLayerFingerprintCache
from ci.util.color_output import print_cache_hit, print_cache_miss


def get_directory_files(directory: Path) -> list[Path]:
    """
    Get all files in a directory for fingerprint hashing.

    This captures all files including:
    - All file types
    - Files in subdirectories
    - Directory structure
    """
    if not directory.exists():
        return []

    # Get all files (not directories) recursively
    all_files: list[Path] = []
    for file_path in directory.glob("**/*"):
        if file_path.is_file():
            all_files.append(file_path)

    return sorted(all_files, key=str)


def _get_js_lint_cache() -> TwoLayerFingerprintCache:
    """Get the two-layer fingerprint cache for JS linting."""
    cache_dir = Path(".cache")
    return TwoLayerFingerprintCache(cache_dir, "js_lint")


def check_js_files_changed() -> bool:
    """
    Check if JavaScript/TypeScript directories have changed since the last lint run.

    Monitors:
    - src/platforms/wasm/compiler (JavaScript files)
    - ci/docker_utils/avr8js (TypeScript files)

    Uses the safe pre-computed fingerprint pattern to avoid race conditions.

    Returns:
        True if directories changed and linting should run
        False if no changes detected and linting can be skipped
    """
    # Monitor both JavaScript and TypeScript directories
    directories = [
        Path("src/platforms/wasm/compiler"),
        Path("ci/docker_utils/avr8js"),
    ]

    all_file_paths: list[Path] = []
    missing_dirs: list[str] = []

    for directory in directories:
        if not directory.exists():
            missing_dirs.append(str(directory))
            continue
        all_file_paths.extend(get_directory_files(directory))

    if missing_dirs:
        print(f"⚠️  Directories not found: {', '.join(missing_dirs)}")

    if not all_file_paths:
        print("📝 No files found in monitored directories - triggering lint")
        return True

    # Use the safe pattern: check_needs_update stores fingerprint for later use
    cache = _get_js_lint_cache()
    needs_update = cache.check_needs_update(all_file_paths)

    if needs_update:
        # Count JS and TS files for informational purposes
        js_files = [f for f in all_file_paths if f.suffix == ".js"]
        ts_files = [f for f in all_file_paths if f.suffix == ".ts"]
        print_cache_miss("Changes detected in JavaScript/TypeScript directories")
        print(
            f"   Found {len(js_files)} JavaScript files, {len(ts_files)} TypeScript files, {len(all_file_paths)} total files"
        )
        return True
    else:
        print_cache_hit(
            "No changes in JavaScript/TypeScript directories - skipping lint"
        )
        return False


def mark_lint_success() -> None:
    """
    Mark the current lint run as successful.

    Uses the safe pre-computed fingerprint pattern - saves the fingerprint
    that was computed before linting started, regardless of file changes
    during the linting process.
    """
    cache = _get_js_lint_cache()

    try:
        cache.mark_success()
        print("📝 Lint success fingerprint updated")
    except RuntimeError as e:
        print(f"⚠️  Fingerprint update failed: {e}")
        print(
            "    This indicates check_js_files_changed() was not called before linting"
        )


def invalidate_cache() -> None:
    """Invalidate the cache when lint/test fails, forcing a re-run next time."""
    cache = _get_js_lint_cache()

    try:
        cache.invalidate()
        print_cache_miss(
            "Lint cache invalidated due to failure - will re-run next time"
        )
    except KeyboardInterrupt:
        _thread.interrupt_main()
        raise
    except Exception as e:
        print_cache_miss(f"Failed to invalidate cache: {e}")


def main() -> int:
    """Main entry point for JS lint cache checking."""
    if len(sys.argv) < 2:
        print("Usage: python ci/js_lint_cache.py [check|success|failure]")
        return 1

    command = sys.argv[1]

    if command == "check":
        # Check if linting is needed
        needs_lint = check_js_files_changed()
        return 0 if needs_lint else 1  # Exit 1 means skip linting

    elif command == "success":
        # Mark lint as successful
        mark_lint_success()
        return 0

    elif command == "failure":
        # Invalidate cache on failure
        invalidate_cache()
        return 0

    else:
        print(f"Unknown command: {command}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
