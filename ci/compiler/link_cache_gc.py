#!/usr/bin/env python3
"""
Link Cache Garbage Collection for FastLED Build System

This module provides garbage collection for the link cache directory (.build/link_cache/)
to prevent unbounded growth of cached executables.

Strategy:
- Per-example limit: Keep max N most recent versions per example
- Age-based cleanup: Remove entries older than N days
- Size-based limit: Optional total cache size cap
- LRU tracking: Use modification time to determine least recently used

Author: FastLED Build System
"""

import os
import re
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Set, Tuple


@dataclass
class CacheEntry:
    """Represents a single cached executable"""

    path: Path
    example_name: str
    cache_key: str
    size_bytes: int
    mtime: float

    @property
    def age_days(self) -> float:
        """Age of the cache entry in days"""
        return (time.time() - self.mtime) / 86400


@dataclass
class GCStats:
    """Statistics from a garbage collection run"""

    files_removed: int = 0
    bytes_freed: int = 0
    files_kept: int = 0
    bytes_kept: int = 0

    def __str__(self) -> str:
        freed_mb = self.bytes_freed / (1024 * 1024)
        kept_mb = self.bytes_kept / (1024 * 1024)
        return (
            f"GC Stats: Removed {self.files_removed} files ({freed_mb:.1f} MB), "
            f"Kept {self.files_kept} files ({kept_mb:.1f} MB)"
        )


@dataclass
class CachePolicy:
    """Configuration for cache garbage collection"""

    max_versions_per_example: int = 3
    max_age_days: int = 7
    max_total_size_mb: int = 1024  # 1GB default
    preserve_at_least_one: bool = True

    @classmethod
    def from_toml(cls, config: Dict[str, Any]) -> "CachePolicy":
        """Create policy from TOML configuration"""
        cache_config: Dict[str, Any] = config.get("cache", {})
        return cls(
            max_versions_per_example=cache_config.get("max_versions_per_example", 3),
            max_age_days=cache_config.get("max_age_days", 7),
            max_total_size_mb=cache_config.get("max_total_size_mb", 1024),
            preserve_at_least_one=cache_config.get("preserve_at_least_one", True),
        )


class LinkCacheGC:
    """Garbage collector for link cache directory"""

    # Pattern to extract example name and cache key from filename
    # Format: ExampleName_1234567890abcdef.exe
    CACHE_FILE_PATTERN = re.compile(r"^(.+)_([0-9a-f]{16})\.exe$")

    def __init__(self, cache_dir: Path, policy: CachePolicy | None = None):
        """
        Initialize garbage collector

        Args:
            cache_dir: Path to .build/link_cache directory
            policy: Cache policy configuration (uses defaults if None)
        """
        self.cache_dir = cache_dir
        self.policy = policy or CachePolicy()

    def scan_cache(self) -> Dict[str, List[CacheEntry]]:
        """
        Scan cache directory and group entries by example name

        Returns:
            Dictionary mapping example names to list of cache entries
        """
        if not self.cache_dir.exists():
            return {}

        entries_by_example: Dict[str, List[CacheEntry]] = {}

        for file_path in self.cache_dir.glob("*.exe"):
            match = self.CACHE_FILE_PATTERN.match(file_path.name)
            if not match:
                continue

            example_name = match.group(1)
            cache_key = match.group(2)

            try:
                stat_info = file_path.stat()
                entry = CacheEntry(
                    path=file_path,
                    example_name=example_name,
                    cache_key=cache_key,
                    size_bytes=stat_info.st_size,
                    mtime=stat_info.st_mtime,
                )

                if example_name not in entries_by_example:
                    entries_by_example[example_name] = []
                entries_by_example[example_name].append(entry)
            except OSError:
                # Skip files we can't stat
                continue

        # Sort each example's entries by modification time (newest first)
        for entries in entries_by_example.values():
            entries.sort(key=lambda e: e.mtime, reverse=True)

        return entries_by_example

    def identify_removable_entries(
        self, entries_by_example: Dict[str, List[CacheEntry]]
    ) -> Tuple[List[CacheEntry], List[CacheEntry]]:
        """
        Identify which cache entries should be removed

        Args:
            entries_by_example: Cache entries grouped by example name

        Returns:
            Tuple of (entries_to_remove, entries_to_keep)
        """
        to_remove: List[CacheEntry] = []
        to_keep: List[CacheEntry] = []

        for example_name, entries in entries_by_example.items():
            # Apply per-example version limit
            if len(entries) > self.policy.max_versions_per_example:
                # Keep the N most recent, remove the rest
                keep_count = self.policy.max_versions_per_example
                to_keep.extend(entries[:keep_count])
                to_remove.extend(entries[keep_count:])
            else:
                to_keep.extend(entries)

        # Apply age-based cleanup (but respect preserve_at_least_one)
        aged_out: List[CacheEntry] = []
        for entry in to_keep[:]:
            if entry.age_days > self.policy.max_age_days:
                # Check if this is the last version for this example
                example_entries = [
                    e for e in to_keep if e.example_name == entry.example_name
                ]
                if len(example_entries) > 1 or not self.policy.preserve_at_least_one:
                    to_keep.remove(entry)
                    aged_out.append(entry)

        to_remove.extend(aged_out)

        # Apply size-based limit (remove oldest first until under limit)
        total_size_bytes = sum(e.size_bytes for e in to_keep)
        max_size_bytes = self.policy.max_total_size_mb * 1024 * 1024

        if total_size_bytes > max_size_bytes:
            # Sort by age (oldest first)
            to_keep_sorted = sorted(to_keep, key=lambda e: e.mtime)

            while total_size_bytes > max_size_bytes and to_keep_sorted:
                # Check if we can remove without violating preserve_at_least_one
                oldest = to_keep_sorted[0]
                example_entries = [
                    e for e in to_keep_sorted if e.example_name == oldest.example_name
                ]

                if len(example_entries) > 1 or not self.policy.preserve_at_least_one:
                    to_keep_sorted.remove(oldest)
                    to_remove.append(oldest)
                    total_size_bytes -= oldest.size_bytes
                else:
                    # Can't remove this one, try next
                    break

            to_keep = to_keep_sorted

        return to_remove, to_keep

    def run_gc(self, dry_run: bool = False, verbose: bool = False) -> GCStats:
        """
        Run garbage collection on link cache

        Args:
            dry_run: If True, don't actually delete files (just report what would be done)
            verbose: If True, print detailed information

        Returns:
            GCStats object with statistics from the run
        """
        stats = GCStats()

        # Scan cache directory
        entries_by_example = self.scan_cache()

        if not entries_by_example:
            if verbose:
                print(f"[CACHE GC] Cache directory is empty: {self.cache_dir}")
            return stats

        # Identify what to remove
        to_remove, to_keep = self.identify_removable_entries(entries_by_example)

        # Update statistics
        stats.files_kept = len(to_keep)
        stats.bytes_kept = sum(e.size_bytes for e in to_keep)
        stats.files_removed = len(to_remove)
        stats.bytes_freed = sum(e.size_bytes for e in to_remove)

        # Print summary
        if verbose:
            total_examples = len(entries_by_example)
            total_files = stats.files_kept + stats.files_removed
            print(
                f"[CACHE GC] Scanned {total_files} cached files for {total_examples} examples"
            )
            print(
                f"[CACHE GC] Policy: max {self.policy.max_versions_per_example} versions, max {self.policy.max_age_days} days"
            )

        # Remove files
        if to_remove:
            if dry_run:
                if verbose:
                    print(
                        f"[CACHE GC] DRY RUN: Would remove {stats.files_removed} files ({stats.bytes_freed / (1024 * 1024):.1f} MB)"
                    )
                    if verbose:
                        # Show first few files that would be removed
                        for entry in to_remove[:5]:
                            print(
                                f"[CACHE GC]   - {entry.path.name} (age: {entry.age_days:.1f} days)"
                            )
                        if len(to_remove) > 5:
                            print(f"[CACHE GC]   ... and {len(to_remove) - 5} more")
            else:
                removed_count = 0
                for entry in to_remove:
                    try:
                        entry.path.unlink()
                        removed_count += 1
                    except OSError as e:
                        if verbose:
                            print(
                                f"[CACHE GC] Warning: Failed to remove {entry.path.name}: {e}"
                            )

                if verbose:
                    print(
                        f"[CACHE GC] Removed {removed_count} files ({stats.bytes_freed / (1024 * 1024):.1f} MB)"
                    )
        else:
            if verbose:
                print(f"[CACHE GC] No files to remove (cache within policy limits)")

        return stats


def run_link_cache_gc(
    cache_dir: Path | None = None,
    policy: CachePolicy | None = None,
    dry_run: bool = False,
    verbose: bool = False,
) -> GCStats:
    """
    Convenience function to run link cache garbage collection

    Args:
        cache_dir: Path to cache directory (defaults to .build/link_cache)
        policy: Cache policy (uses defaults if None)
        dry_run: If True, don't actually delete files
        verbose: If True, print detailed information

    Returns:
        GCStats object with statistics
    """
    if cache_dir is None:
        cache_dir = Path(".build/link_cache")

    gc = LinkCacheGC(cache_dir, policy)
    return gc.run_gc(dry_run=dry_run, verbose=verbose)


def load_policy_from_toml(toml_path: Path) -> CachePolicy:
    """
    Load cache policy from TOML configuration file

    Args:
        toml_path: Path to build_unit.toml or similar config file

    Returns:
        CachePolicy loaded from file (or defaults if file doesn't exist)
    """
    if not toml_path.exists():
        return CachePolicy()

    try:
        import tomllib

        with open(toml_path, "rb") as f:
            config = tomllib.load(f)
        return CachePolicy.from_toml(config)
    except Exception:
        # If we can't load config, use defaults
        return CachePolicy()


if __name__ == "__main__":
    """Command-line interface for manual cache GC"""
    import argparse

    parser = argparse.ArgumentParser(
        description="FastLED Link Cache Garbage Collection"
    )
    parser.add_argument(
        "--cache-dir",
        type=Path,
        default=Path(".build/link_cache"),
        help="Path to link cache directory",
    )
    parser.add_argument(
        "--dry-run",
        action="store_true",
        help="Don't actually delete files, just show what would be done",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Print detailed information",
    )
    parser.add_argument(
        "--max-versions",
        type=int,
        default=3,
        help="Maximum versions to keep per example (default: 3)",
    )
    parser.add_argument(
        "--max-age-days",
        type=int,
        default=7,
        help="Maximum age in days (default: 7)",
    )
    parser.add_argument(
        "--max-size-mb",
        type=int,
        default=1024,
        help="Maximum total cache size in MB (default: 1024)",
    )

    args = parser.parse_args()

    # Create policy from command-line arguments
    policy = CachePolicy(
        max_versions_per_example=args.max_versions,
        max_age_days=args.max_age_days,
        max_total_size_mb=args.max_size_mb,
    )

    # Run garbage collection
    stats = run_link_cache_gc(
        cache_dir=args.cache_dir,
        policy=policy,
        dry_run=args.dry_run,
        verbose=args.verbose,
    )

    # Print summary
    print(stats)
