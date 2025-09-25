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
from typing import List

# Import the new hash-based cache system
from ci.util.hash_fingerprint_cache import HashFingerprintCache


def get_directory_files(directory: Path) -> List[Path]:
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
    all_files: List[Path] = []
    for file_path in directory.glob("**/*"):
        if file_path.is_file():
            all_files.append(file_path)

    return sorted(all_files, key=str)


def _get_js_lint_cache() -> HashFingerprintCache:
    """Get the hash fingerprint cache for JS linting."""
    cache_dir = Path(".cache")
    return HashFingerprintCache(cache_dir, "js_lint")


def check_js_files_changed() -> bool:
    """
    Check if the wasm/compiler directory has changed since the last lint run.

    Returns:
        True if directory changed and linting should run
        False if no changes detected and linting can be skipped
    """
    # Monitor the wasm/compiler directory
    compiler_dir = Path("src/platforms/wasm/compiler")

    if not compiler_dir.exists():
        print("⚠️  Directory src/platforms/wasm/compiler not found")
        return False

    # Get all files in the directory
    file_paths = get_directory_files(compiler_dir)

    if not file_paths:
        print("📝 No files found in src/platforms/wasm/compiler/ - triggering lint")
        return True

    # Check if files have changed using hash cache
    cache = _get_js_lint_cache()

    if cache.is_valid(file_paths):
        print("✅ No changes in src/platforms/wasm/compiler/ - skipping lint")
        return False
    else:
        # Count JS files for informational purposes
        js_files = [f for f in file_paths if f.suffix == ".js"]
        print(f"🔄 Changes detected in src/platforms/wasm/compiler/")
        print(
            f"   Found {len(js_files)} JavaScript files, {len(file_paths)} total files"
        )
        return True


def mark_lint_success() -> None:
    """Mark the current lint run as successful."""
    compiler_dir = Path("src/platforms/wasm/compiler")

    if not compiler_dir.exists():
        print("⚠️  Cannot update fingerprint - compiler directory not found")
        return

    # Get all files in the directory
    file_paths = get_directory_files(compiler_dir)

    if not file_paths:
        print("⚠️  No files found to cache fingerprint for")
        return

    # Mark as successful using hash cache
    cache = _get_js_lint_cache()
    success = cache.mark_success(file_paths)

    if success:
        print("📝 Lint success fingerprint updated")
    else:
        print("⚠️  Fingerprint update failed - files changed during lint process")


def invalidate_cache() -> None:
    """Invalidate the cache when lint/test fails, forcing a re-run next time."""
    cache = _get_js_lint_cache()

    try:
        cache.invalidate()
        print("🔄 Lint cache invalidated due to failure - will re-run next time")
    except KeyboardInterrupt:
        _thread.interrupt_main()
        raise
    except Exception as e:
        print(f"⚠️  Failed to invalidate cache: {e}")


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
