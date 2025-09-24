#!/usr/bin/env python3
"""
JavaScript Linting Cache Integration

Uses fingerprint cache to determine if JS files need re-linting by checking
modification times of files in src/platforms/wasm/**/*.js
"""

import os
import sys
import time
from pathlib import Path
from typing import List

from ci.ci.fingerprint_cache import FingerprintCache


def get_js_files() -> List[Path]:
    """Get all JavaScript files in the WASM platform directory."""
    js_files = []
    wasm_dir = Path("src/platforms/wasm")

    if wasm_dir.exists():
        # Find all .js files recursively
        js_files = list(wasm_dir.glob("**/*.js"))

    return sorted(js_files)


def get_last_lint_time(cache_dir: Path) -> float:
    """Get the timestamp of the last successful lint run."""
    timestamp_file = cache_dir / "last_lint_success.txt"

    if timestamp_file.exists():
        try:
            with open(timestamp_file, "r") as f:
                return float(f.read().strip())
        except (ValueError, IOError):
            pass

    # Default to 1 hour ago if no previous run
    return time.time() - 3600


def update_last_lint_time(cache_dir: Path) -> None:
    """Update the timestamp of successful lint run."""
    cache_dir.mkdir(parents=True, exist_ok=True)
    timestamp_file = cache_dir / "last_lint_success.txt"

    with open(timestamp_file, "w") as f:
        f.write(str(time.time()))


def check_js_files_changed() -> bool:
    """
    Check if any JavaScript files have changed since the last lint run.

    Returns:
        True if any files changed and linting should run
        False if no changes detected and linting can be skipped
    """
    cache_dir = Path(".cache/js-lint")
    cache_file = cache_dir / "fingerprint_cache.json"

    # Initialize fingerprint cache
    fingerprint_cache = FingerprintCache(cache_file)

    # Get all JS files
    js_files = get_js_files()

    if not js_files:
        print("⚠️  No JavaScript files found in src/platforms/wasm/")
        return False

    # Get last successful lint time
    last_lint_time = get_last_lint_time(cache_dir)

    print(f"📁 Checking {len(js_files)} JavaScript files for changes...")
    print(f"📅 Last lint run: {time.ctime(last_lint_time)}")

    # Check each file for changes
    changed_files: List[Path] = []
    for js_file in js_files:
        if fingerprint_cache.has_changed(js_file, last_lint_time):
            changed_files.append(js_file)

    if changed_files:
        print(f"🔄 {len(changed_files)} files changed since last lint:")
        for file_path in changed_files:
            print(f"   - {file_path}")
        return True
    else:
        print("✅ No JavaScript files changed - skipping lint")
        return False


def mark_lint_success() -> None:
    """Mark the current lint run as successful."""
    cache_dir = Path(".cache/js-lint")
    update_last_lint_time(cache_dir)
    print("📝 Lint success timestamp updated")


def main() -> int:
    """Main entry point for JS lint cache checking."""
    if len(sys.argv) < 2:
        print("Usage: python ci/js_lint_cache.py [check|success]")
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

    else:
        print(f"Unknown command: {command}")
        return 1


if __name__ == "__main__":
    sys.exit(main())
