"""
Source file metadata caching for Meson build system.

This module provides hash-based caching to avoid re-running source discovery scripts
when source files haven't changed. It tracks all .cpp file metadata (path + mtime + size)
and invalidates the cache only when sources are added, deleted, or modified.

Usage:
  # Check cache validity (exit 0 if valid, 1 if invalid)
  python src_metadata_cache.py --check src/ .build/meson-quick/

  # Update cache with new metadata
  python src_metadata_cache.py --update src/ .build/meson-quick/ <metadata>

  # Force invalidate cache
  python src_metadata_cache.py --invalidate .build/meson-quick/
"""

import argparse
import hashlib
import json
import sys
from pathlib import Path


CACHE_FILENAME = "src_metadata.cache"


def compute_src_files_hash(src_dir: Path, pattern: str = "*.cpp") -> str:
    """
    Compute a hash of all source file metadata (path + mtime + size).

    This provides a fast way to detect if any source files have been added,
    deleted, or modified without re-parsing all source files.

    Args:
        src_dir: Root directory containing source files
        pattern: File pattern to match (default: "*.cpp")

    Returns:
        SHA256 hash of all source file metadata
    """
    # Find all files matching pattern recursively
    source_files = sorted(src_dir.rglob(pattern))

    # Create hash input from file metadata
    hash_input: list[str] = []
    for f in source_files:
        if f.is_file():
            from os import stat_result

            stat: stat_result = f.stat()
            rel_path: str = f.relative_to(src_dir).as_posix()
            # Include path, mtime, and size in hash
            hash_input.append(f"{rel_path}:{stat.st_mtime:.6f}:{stat.st_size}")

    # Compute SHA256 hash
    hash_str = "\n".join(hash_input)
    return hashlib.sha256(hash_str.encode()).hexdigest()


def load_cache(build_dir: Path) -> dict[str, str | float] | None:
    """
    Load cached source metadata from build directory.

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
        with open(cache_file, "r", encoding="utf-8") as f:
            cache = json.load(f)

        # Validate cache structure
        required_keys = ["hash", "timestamp", "metadata"]
        if not all(key in cache for key in required_keys):
            return None

        return cache
    except (json.JSONDecodeError, OSError):
        return None


def save_cache(build_dir: Path, src_hash: str, metadata: str) -> None:
    """
    Save source metadata to cache file.

    Args:
        build_dir: Meson build directory
        src_hash: Hash of source file metadata
        metadata: Source metadata output from discovery script
    """
    import time

    cache_file = build_dir / CACHE_FILENAME
    build_dir.mkdir(parents=True, exist_ok=True)

    cache = {"hash": src_hash, "timestamp": time.time(), "metadata": metadata}

    with open(cache_file, "w", encoding="utf-8") as f:
        json.dump(cache, f, indent=2)


def main() -> None:
    """Main entry point for source metadata cache operations."""
    parser = argparse.ArgumentParser(
        description="Manage source metadata cache for Meson build system"
    )

    # Mutually exclusive operation modes
    group = parser.add_mutually_exclusive_group(required=True)
    group.add_argument(
        "--check",
        action="store_true",
        help="Check if cache is valid (exit 0 if valid, 1 if invalid)",
    )
    group.add_argument(
        "--update", action="store_true", help="Update cache with new metadata"
    )
    group.add_argument(
        "--invalidate", action="store_true", help="Force invalidate cache"
    )

    # Positional arguments
    parser.add_argument(
        "src_dir",
        nargs="?",
        type=Path,
        help="Source directory (required for --check, --update)",
    )
    parser.add_argument(
        "build_dir",
        nargs="?",
        type=Path,
        help="Build directory (required for all operations)",
    )
    parser.add_argument(
        "metadata", nargs="?", type=str, help="Source metadata (required for --update)"
    )
    parser.add_argument(
        "--pattern",
        type=str,
        default="*.cpp",
        help="File pattern to match (default: *.cpp)",
    )

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
        if not args.src_dir or not args.build_dir:
            print("Error: --check requires <src_dir> <build_dir>", file=sys.stderr)
            sys.exit(1)

        # Load cached metadata
        cache: dict[str, str | float] | None = load_cache(args.build_dir)
        if cache is None:
            # Cache miss - exit with failure, no output
            sys.exit(1)

        # Compute current hash
        current_hash = compute_src_files_hash(args.src_dir, args.pattern)

        # Check if hash matches
        if cache["hash"] == current_hash:
            # Cache hit - output cached metadata and exit success
            metadata_str: str = str(cache["metadata"])
            print(metadata_str)
            sys.exit(0)
        else:
            # Cache invalid - exit with failure, no output
            sys.exit(1)

    elif args.update:
        if not args.src_dir or not args.build_dir or not args.metadata:
            print(
                "Error: --update requires <src_dir> <build_dir> <metadata>",
                file=sys.stderr,
            )
            sys.exit(1)

        # Compute hash and save cache
        src_hash = compute_src_files_hash(args.src_dir, args.pattern)
        save_cache(args.build_dir, src_hash, args.metadata)
        sys.exit(0)


if __name__ == "__main__":
    main()
