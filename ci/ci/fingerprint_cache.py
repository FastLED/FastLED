"""
Fingerprint Cache Feature

A two-layer file change detection system that efficiently determines if source files
have been modified by combining fast modification time checks with slower but accurate
MD5 hash verification.
"""

import hashlib
import json
import os
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional


@dataclass
class CacheEntry:
    """Cache entry storing file modification time and content hash."""

    modification_time: float
    md5_hash: str


@dataclass
class FingerprintCacheConfig:
    """Configuration for fingerprint cache behavior."""

    cache_file: Path
    hash_algorithm: str
    ignore_patterns: list[str]
    max_cache_size: int


class FingerprintCache:
    """
    Two-layer file change detection using modification time and content hashing.

    Provides efficient change detection by:
    1. Fast modification time comparison (microsecond performance)
    2. Accurate content verification via MD5 hashing when needed
    """

    def __init__(self, cache_file: Path, modtime_only: bool = False):
        """
        Initialize fingerprint cache.

        Args:
            cache_file: Path to JSON cache file
            modtime_only: When True, disable hashing and rely solely on modtime.
        """
        self.cache_file = cache_file
        self._modtime_only: bool = modtime_only
        self.cache = self._load_cache()

    def _load_cache(self) -> Dict[str, CacheEntry]:
        """
        Load cache from JSON file, return empty dict if file doesn't exist.

        Returns:
            Dictionary mapping file paths to cache entries
        """
        if not self.cache_file.exists():
            return {}

        try:
            with open(self.cache_file, "r") as f:
                data = json.load(f)

            # Convert JSON dict to CacheEntry objects
            cache: Dict[str, CacheEntry] = {}
            for file_path, entry_data in data.items():
                cache[file_path] = CacheEntry(
                    modification_time=entry_data["modification_time"],
                    md5_hash=entry_data["md5_hash"],
                )
            return cache
        except (json.JSONDecodeError, KeyError, TypeError):
            # Cache corrupted - start fresh
            return {}

    def _save_cache(self) -> None:
        """Save current cache state to JSON file."""
        # Ensure cache directory exists
        self.cache_file.parent.mkdir(parents=True, exist_ok=True)

        # Convert CacheEntry objects to JSON-serializable dict
        data = {}
        for file_path, entry in self.cache.items():
            data[file_path] = {
                "modification_time": entry.modification_time,
                "md5_hash": entry.md5_hash,
            }

        with open(self.cache_file, "w") as f:
            json.dump(data, f, indent=2)

    def _compute_md5(self, file_path: Path) -> str:
        """
        Compute MD5 hash of file content.

        Args:
            file_path: Path to file to hash

        Returns:
            MD5 hash as hexadecimal string

        Raises:
            IOError: If file cannot be read
        """
        try:
            hasher = hashlib.md5()
            with open(file_path, "rb") as f:
                # Read in chunks to handle large files efficiently
                for chunk in iter(lambda: f.read(8192), b""):
                    hasher.update(chunk)
            return hasher.hexdigest()
        except IOError as e:
            raise IOError(f"Cannot read file {file_path}: {e}")

    def has_changed(self, src_path: Path, previous_modtime: float) -> bool:
        """
        Determine if a source file has changed since the last known modification time.

        Two-layer verification process:
        1. Fast modification time check - if times match, return False immediately
        2. Content verification - compute/use cached MD5 hash for accurate comparison

        Args:
            src_path: Path to the source file to check
            previous_modtime: Previously known modification time (Unix timestamp)

        Returns:
            True if the file has changed, False if unchanged

        Raises:
            FileNotFoundError: If source file doesn't exist
        """
        if not src_path.exists():
            raise FileNotFoundError(f"Source file not found: {src_path}")

        current_modtime = os.path.getmtime(src_path)

        # Optional mode: strictly use modification time only (no hashing)
        # This is required for toolchains that invalidate PCH on any newer
        # dependency regardless of content changes (e.g., Clang).
        if self._modtime_only:
            # Treat as changed only when file is newer than the reference time
            # (keeps behavior stable when reference is an external artifact's mtime)
            return current_modtime > previous_modtime

        # Layer 1: Quick modification time check
        if current_modtime == previous_modtime:
            return False  # No change detected

        file_key = str(src_path.resolve())  # Use absolute path as key

        # Layer 2: Content verification via hash comparison
        if file_key in self.cache:
            cached_entry = self.cache[file_key]
            previous_hash = (
                cached_entry.md5_hash
            )  # Store previous hash before potential update

            if current_modtime == cached_entry.modification_time:
                # File is cached with current modtime - use cached hash
                current_hash = cached_entry.md5_hash
            else:
                # Cache is stale - compute new hash
                current_hash = self._compute_md5(src_path)
                self._update_cache_entry(src_path, current_modtime, current_hash)

            # Compare current hash with previous cached hash
            # If they match, content hasn't actually changed despite modtime difference
            return current_hash != previous_hash

        else:
            # File not in cache - compute hash and cache it
            current_hash = self._compute_md5(src_path)
            self._update_cache_entry(src_path, current_modtime, current_hash)
            return True  # Assume changed since we have no previous state to compare

    def _update_cache_entry(
        self, file_path: Path, modification_time: float, md5_hash: str
    ) -> None:
        """
        Update cache with new entry and save to disk.

        Args:
            file_path: Path to file being cached
            modification_time: File modification time
            md5_hash: File content hash
        """
        file_key = str(file_path.resolve())
        self.cache[file_key] = CacheEntry(
            modification_time=modification_time, md5_hash=md5_hash
        )
        self._save_cache()

    def get_cache_stats(self) -> dict[str, int]:
        """
        Get cache statistics.

        Returns:
            Dictionary with cache size and other metrics
        """
        return {
            "total_entries": len(self.cache),
            "cache_file_exists": self.cache_file.exists(),
            "cache_file_size_bytes": self.cache_file.stat().st_size
            if self.cache_file.exists()
            else 0,
        }

    def clear_cache(self) -> None:
        """Clear all cache entries and remove cache file."""
        self.cache.clear()
        if self.cache_file.exists():
            self.cache_file.unlink()


def has_changed(src_path: Path, previous_modtime: float, cache_file: Path) -> bool:
    """
    Convenience function implementing the main API.

    Args:
        src_path: Path to the source file to check
        previous_modtime: Previously known modification time (Unix timestamp)
        cache_file: Path to the fingerprint cache file

    Returns:
        True if the file has changed, False if unchanged

    Raises:
        FileNotFoundError: If source file doesn't exist
    """
    cache = FingerprintCache(cache_file)
    return cache.has_changed(src_path, previous_modtime)
