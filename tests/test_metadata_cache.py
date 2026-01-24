"""
Test metadata caching for Meson build system.

This module provides hash-based caching to avoid re-running organize_tests.py
when test files haven't changed. It tracks all test file metadata (path + mtime + size)
and invalidates the cache only when tests are added, deleted, or modified.

Usage:
  # Check cache validity (exit 0 if valid, 1 if invalid)
  python test_metadata_cache.py --check tests/ .build/meson-quick/

  # Update cache with new metadata
  python test_metadata_cache.py --update tests/ .build/meson-quick/ <metadata>

  # Force invalidate cache
  python test_metadata_cache.py --invalidate .build/meson-quick/

  # Detect changes (JSON output)
  python test_metadata_cache.py --detect-changes tests/ .build/meson-quick/
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path
from typing import Dict, List, Set


CACHE_FILENAME = "test_metadata.cache"


def compute_test_files_hash(tests_dir: Path) -> str:
    """
    Compute a hash of all test file metadata (path + mtime + size).

    This provides a fast way to detect if any test files have been added,
    deleted, or modified without re-parsing all test files.

    Args:
        tests_dir: Root directory containing test files

    Returns:
        SHA256 hash of all test file metadata
    """
    # Find all test files using same pattern as discover_tests.py
    test_files: List[Path] = []

    # Find all test_*.cpp files recursively
    for f in sorted(tests_dir.rglob("test_*.cpp")):
        test_files.append(f)

    # Also find *.cpp files in fl/, ftl/, fx/ subdirectories
    for subdir in ["fl", "ftl", "fx"]:
        subdir_path = tests_dir / subdir
        if subdir_path.exists():
            for f in sorted(subdir_path.glob("*.cpp")):
                test_files.append(f)

    # Deduplicate while preserving order
    seen = set()
    unique_files = []
    for f in test_files:
        if f not in seen:
            seen.add(f)
            unique_files.append(f)

    # Create hash input from file metadata
    hash_input = []
    for f in unique_files:
        if f.is_file():
            stat = f.stat()
            rel_path = f.relative_to(tests_dir).as_posix()
            # Include path, mtime, and size in hash
            hash_input.append(f"{rel_path}:{stat.st_mtime:.6f}:{stat.st_size}")

    # Compute SHA256 hash
    hash_str = '\n'.join(hash_input)
    return hashlib.sha256(hash_str.encode()).hexdigest()


def load_cache(build_dir: Path) -> dict | None:
    """
    Load cached test metadata from build directory.

    Args:
        build_dir: Meson build directory (e.g., .build/meson-quick/)

    Returns:
        Cache dictionary with keys: hash, timestamp, metadata
        None if cache doesn't exist or is invalid
    """
    cache_file = build_dir / CACHE_FILENAME
    if not cache_file.exists():
        return None

    try:
        with open(cache_file, 'r', encoding='utf-8') as f:
            cache = json.load(f)

        # Validate cache structure
        required_keys = ['hash', 'timestamp', 'metadata']
        if not all(key in cache for key in required_keys):
            return None

        return cache
    except (json.JSONDecodeError, OSError):
        return None


def save_cache(build_dir: Path, tests_hash: str, metadata: str) -> None:
    """
    Save test metadata to cache file.

    Args:
        build_dir: Meson build directory
        tests_hash: Hash of test file metadata
        metadata: Test metadata output from organize_tests.py
    """
    import time

    cache_file = build_dir / CACHE_FILENAME
    build_dir.mkdir(parents=True, exist_ok=True)

    cache = {
        'hash': tests_hash,
        'timestamp': time.time(),
        'metadata': metadata
    }

    with open(cache_file, 'w', encoding='utf-8') as f:
        json.dump(cache, f, indent=2)


def parse_metadata(metadata: str) -> Set[str]:
    """
    Parse test metadata into a set of test names.

    Args:
        metadata: Test metadata in format "TEST:name:path:category\\n..."

    Returns:
        Set of test names
    """
    test_names = set()
    for line in metadata.strip().split('\n'):
        if line and line.startswith('TEST:'):
            parts = line.split(':')
            if len(parts) >= 2:
                test_names.add(parts[1])
    return test_names


def detect_changes(tests_dir: Path, build_dir: Path) -> Dict[str, List[str] | bool]:
    """
    Detect what changed since last cache.

    Returns:
        {
            "unchanged": True/False,
            "new_tests": ["test_foo", "test_bar"],
            "deleted_tests": ["test_old"],
            "modified_tests": []  # Not implemented yet
        }
    """
    cache = load_cache(build_dir)

    if cache is None:
        # No cache - everything is new
        return {
            "unchanged": False,
            "new_tests": [],
            "deleted_tests": [],
            "modified_tests": []
        }

    # Compute current hash
    current_hash = compute_test_files_hash(tests_dir)

    # If hashes match, nothing changed
    if cache['hash'] == current_hash:
        return {
            "unchanged": True,
            "new_tests": [],
            "deleted_tests": [],
            "modified_tests": []
        }

    # Hash mismatch - need to run organize_tests.py to get actual differences
    # For now, just report that something changed
    return {
        "unchanged": False,
        "new_tests": [],
        "deleted_tests": [],
        "modified_tests": []
    }


def main() -> None:
    """Main entry point for test metadata cache operations."""
    parser = argparse.ArgumentParser(
        description='Manage test metadata cache for Meson build system'
    )

    # Mutually exclusive operation modes
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument('--check', action='store_true',
                       help='Check if cache is valid (exit 0 if valid, 1 if invalid)')
    group.add_argument('--update', action='store_true',
                       help='Update cache with new metadata')
    group.add_argument('--invalidate', action='store_true',
                       help='Force invalidate cache')
    group.add_argument('--detect-changes', action='store_true',
                       help='Detect changes since last cache (JSON output)')

    # Positional arguments
    parser.add_argument('tests_dir', nargs='?', type=Path,
                        help='Tests directory (required for --check, --update, --detect-changes)')
    parser.add_argument('build_dir', nargs='?', type=Path,
                        help='Build directory (required for all operations)')
    parser.add_argument('metadata', nargs='?', type=str,
                        help='Test metadata (required for --update)')

    args = parser.parse_args()

    # Validate arguments based on operation
    if args.invalidate:
        if not args.build_dir:
            print("Error: --invalidate requires <build_dir>", file=sys.stderr)
            sys.exit(1)

        cache_file = args.build_dir / CACHE_FILENAME
        if cache_file.exists():
            cache_file.unlink()
        sys.exit(0)

    elif args.check:
        if not args.tests_dir or not args.build_dir:
            print("Error: --check requires <tests_dir> <build_dir>", file=sys.stderr)
            sys.exit(1)

        # Load cached metadata
        cache = load_cache(args.build_dir)
        if cache is None:
            # Cache miss - exit with failure, no output
            sys.exit(1)

        # Compute current hash
        current_hash = compute_test_files_hash(args.tests_dir)

        # Check if hash matches
        if cache['hash'] == current_hash:
            # Cache hit - output cached metadata and exit success
            print(cache['metadata'])
            sys.exit(0)
        else:
            # Cache invalid - exit with failure, no output
            sys.exit(1)

    elif args.update:
        if not args.tests_dir or not args.build_dir or not args.metadata:
            print("Error: --update requires <tests_dir> <build_dir> <metadata>", file=sys.stderr)
            sys.exit(1)

        # Compute hash and save cache
        tests_hash = compute_test_files_hash(args.tests_dir)
        save_cache(args.build_dir, tests_hash, args.metadata)
        sys.exit(0)

    elif args.detect_changes:
        if not args.tests_dir or not args.build_dir:
            print("Error: --detect-changes requires <tests_dir> <build_dir>", file=sys.stderr)
            sys.exit(1)

        # Detect changes and output JSON
        changes = detect_changes(args.tests_dir, args.build_dir)
        print(json.dumps(changes, indent=2))
        sys.exit(0)


if __name__ == "__main__":
    main()
