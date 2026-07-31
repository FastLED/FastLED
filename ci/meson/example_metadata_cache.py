"""
Example metadata caching for Meson build system.

This module provides hash-based caching to avoid re-running example discovery scripts
when example files haven't changed. It tracks all example file paths and contents,
and invalidates the cache only when examples are added, deleted, or modified.

The hash is deliberately content-based rather than mtime-based: ``git checkout``,
``git worktree add`` and branch switches rewrite example mtimes with byte-identical
content, and an mtime hash treated every one of those as an example change — which
cost a full ~20-30s meson reconfigure each time (issue #3761).

Usage:
  # Check cache validity (exit 0 if valid, 1 if invalid)
  python example_metadata_cache.py --check examples/ .build/meson-quick/

  # Update cache with new metadata
  python example_metadata_cache.py --update examples/ .build/meson-quick/ <metadata>

  # Force invalidate cache
  python example_metadata_cache.py --invalidate .build/meson-quick/
"""

import argparse
import hashlib
import json
import os
import sys
from pathlib import Path

from ci.meson.cache_utils import (
    compute_files_content_hash,
    should_skip_scan_dir,
)


CACHE_FILENAME = "example_metadata.cache"


_EXAMPLE_SOURCE_EXTS = (".ino", ".cpp", ".h")


def iter_example_files(examples_dir: Path) -> list[Path]:
    """
    Return every file whose content the example metadata depends on.

    That is the ``.ino`` / ``.cpp`` / ``.h`` tree under *examples_dir* plus the
    discovery script itself, deduplicated and in a stable order.

    Build-artifact directories (``.build``, ``.fbuild``, ``build``, ``bin``,
    ``.vscode``, ...) are pruned via :func:`should_skip_scan_dir`. They are
    already invisible to ``discover_examples_all.py`` (which skips dot-dirs)
    and to the mtime fast path (which uses the same predicate), so hashing
    them would make a plain ``bash compile <board> --examples Blink`` look like
    an example-source change and force a needless reconfigure — the very
    false-positive class issue #3761 is about.

    Args:
        examples_dir: Root directory containing example files

    Returns:
        Deduplicated list of paths, sorted within each extension group
    """
    by_ext: dict[str, list[Path]] = {ext: [] for ext in _EXAMPLE_SOURCE_EXTS}

    for dirpath, dirnames, filenames in os.walk(examples_dir):
        # Prune in place so os.walk never descends into build artifacts.
        dirnames[:] = [d for d in dirnames if not should_skip_scan_dir(d)]
        for name in filenames:
            ext = os.path.splitext(name)[1]
            if ext in by_ext:
                by_ext[ext].append(Path(dirpath) / name)

    # Preserve the historical grouping: all .ino, then .cpp, then .h.
    example_files: list[Path] = []
    for ext in _EXAMPLE_SOURCE_EXTS:
        example_files.extend(sorted(by_ext[ext]))

    # Also include the discovery script itself so changes to it invalidate the cache
    this_script = Path(__file__).parent / "discover_examples_all.py"
    if this_script.is_file():
        example_files.append(this_script)

    # Deduplicate while preserving order
    seen: set[Path] = set()
    unique_files: list[Path] = []
    for f in example_files:
        if f not in seen:
            seen.add(f)
            unique_files.append(f)

    return unique_files


def max_example_files_mtime(examples_dir: Path) -> float:
    """
    Return the newest mtime across every example file (0.0 if there are none).

    This is the cheap probe that guards the full content hash: an in-place edit
    bumps the edited file's mtime, so a tree whose files are all older than a
    known-good marker cannot have been edited since. It does *not* observe
    additions or deletions — pair it with :func:`get_max_dir_mtime`, which
    catches those via the parent directory's mtime.
    """
    newest = 0.0
    for f in iter_example_files(examples_dir):
        try:
            mtime = f.stat().st_mtime
        except OSError:
            continue
        if mtime > newest:
            newest = mtime
    return newest


def compute_example_files_hash(examples_dir: Path) -> str:
    """
    Compute a hash of all example file paths and contents.

    This detects examples being added, deleted, or modified without re-parsing
    all example files. See the module docstring for why this hashes content
    rather than mtimes (issue #3761).

    Args:
        examples_dir: Root directory containing example files

    Returns:
        SHA256 hash of all example file paths + contents
    """
    # Paths outside examples_dir (the discovery script) fall back to their
    # absolute path inside the helper, so they still contribute uniquely.
    return compute_files_content_hash(iter_example_files(examples_dir), examples_dir)


def load_cache(build_dir: Path) -> dict[str, str | float] | None:
    """
    Load cached example metadata from build directory.

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


def save_cache(build_dir: Path, examples_hash: str, metadata: str) -> None:
    """
    Save example metadata to cache file.

    Args:
        build_dir: Meson build directory
        examples_hash: Hash of example file metadata
        metadata: Example metadata output from discovery script
    """
    import time

    cache_file = build_dir / CACHE_FILENAME
    build_dir.mkdir(parents=True, exist_ok=True)

    cache = {"hash": examples_hash, "timestamp": time.time(), "metadata": metadata}

    with open(cache_file, "w", encoding="utf-8") as f:
        json.dump(cache, f, indent=2)


def main() -> None:
    """Main entry point for example metadata cache operations."""
    parser = argparse.ArgumentParser(
        description="Manage example metadata cache for Meson build system"
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
        "examples_dir",
        nargs="?",
        type=Path,
        help="Examples directory (required for --check, --update)",
    )
    parser.add_argument(
        "build_dir",
        nargs="?",
        type=Path,
        help="Build directory (required for all operations)",
    )
    parser.add_argument(
        "metadata", nargs="?", type=str, help="Example metadata (required for --update)"
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
        if not args.examples_dir or not args.build_dir:
            print("Error: --check requires <examples_dir> <build_dir>", file=sys.stderr)
            sys.exit(1)

        # Load cached metadata
        cache: dict[str, str | float] | None = load_cache(args.build_dir)
        if cache is None:
            # Cache miss - exit with failure, no output
            sys.exit(1)
        assert cache is not None

        # Compute current hash
        current_hash = compute_example_files_hash(args.examples_dir)

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
        if not args.examples_dir or not args.build_dir or not args.metadata:
            print(
                "Error: --update requires <examples_dir> <build_dir> <metadata>",
                file=sys.stderr,
            )
            sys.exit(1)

        # Compute hash and save cache
        examples_hash = compute_example_files_hash(args.examples_dir)
        save_cache(args.build_dir, examples_hash, args.metadata)
        sys.exit(0)


if __name__ == "__main__":
    main()
