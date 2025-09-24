#!/usr/bin/env python3
"""
JavaScript Linting Cache Integration

Monitors the src/platforms/wasm/compiler directory for any changes
to determine if JS files need re-linting.
"""

import hashlib
import os
import sys
from pathlib import Path


def get_directory_fingerprint(directory: Path) -> str:
    """
    Generate a fingerprint for a directory based on all files' modification times.

    This captures any changes including:
    - File modifications
    - File additions
    - File deletions
    - Subdirectory changes
    """
    if not directory.exists():
        return "directory_not_found"

    hasher = hashlib.sha256()

    # Walk through all files in the directory
    all_files = sorted(directory.glob("**/*"))

    for file_path in all_files:
        if file_path.is_file():
            # Include file path and modification time
            hasher.update(str(file_path.relative_to(directory)).encode("utf-8"))
            hasher.update(str(os.path.getmtime(file_path)).encode("utf-8"))

    # Also include count of files to detect deletions
    hasher.update(f"file_count:{len(all_files)}".encode("utf-8"))

    return hasher.hexdigest()


def get_last_lint_fingerprint(cache_dir: Path) -> str:
    """Get the fingerprint from the last successful lint run."""
    fingerprint_file = cache_dir / "last_lint_fingerprint.txt"

    if fingerprint_file.exists():
        try:
            with open(fingerprint_file, "r") as f:
                return f.read().strip()
        except IOError:
            pass

    return ""  # No previous fingerprint


def update_last_lint_fingerprint(cache_dir: Path, fingerprint: str) -> None:
    """Update the fingerprint after successful lint run."""
    cache_dir.mkdir(parents=True, exist_ok=True)
    fingerprint_file = cache_dir / "last_lint_fingerprint.txt"

    with open(fingerprint_file, "w") as f:
        f.write(fingerprint)


def check_js_files_changed() -> bool:
    """
    Check if the wasm/compiler directory has changed since the last lint run.

    Returns:
        True if directory changed and linting should run
        False if no changes detected and linting can be skipped
    """
    cache_dir = Path(".cache/js-lint")

    # Monitor the wasm/compiler directory
    compiler_dir = Path("src/platforms/wasm/compiler")

    if not compiler_dir.exists():
        print("⚠️  Directory src/platforms/wasm/compiler not found")
        return False

    # Get current directory fingerprint
    current_fingerprint = get_directory_fingerprint(compiler_dir)

    # Get last lint fingerprint
    last_fingerprint = get_last_lint_fingerprint(cache_dir)

    if not last_fingerprint:
        print("📝 No previous lint fingerprint found - triggering lint")
        return True

    if current_fingerprint != last_fingerprint:
        # Count JS files for informational purposes
        js_files = list(compiler_dir.glob("**/*.js"))
        print(f"🔄 Changes detected in src/platforms/wasm/compiler/")
        print(f"   Found {len(js_files)} JavaScript files")
        return True
    else:
        print("✅ No changes in src/platforms/wasm/compiler/ - skipping lint")
        return False


def mark_lint_success() -> None:
    """Mark the current lint run as successful."""
    cache_dir = Path(".cache/js-lint")
    compiler_dir = Path("src/platforms/wasm/compiler")

    if compiler_dir.exists():
        current_fingerprint = get_directory_fingerprint(compiler_dir)
        update_last_lint_fingerprint(cache_dir, current_fingerprint)
        print("📝 Lint success fingerprint updated")
    else:
        print("⚠️  Cannot update fingerprint - compiler directory not found")


def invalidate_cache() -> None:
    """Invalidate the cache when lint/test fails, forcing a re-run next time."""
    cache_dir = Path(".cache/js-lint")
    fingerprint_file = cache_dir / "last_lint_fingerprint.txt"

    if fingerprint_file.exists():
        try:
            fingerprint_file.unlink()
            print("🔄 Lint cache invalidated due to failure - will re-run next time")
        except IOError as e:
            print(f"⚠️  Failed to invalidate cache: {e}")
    else:
        print("📝 No cache to invalidate")


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
