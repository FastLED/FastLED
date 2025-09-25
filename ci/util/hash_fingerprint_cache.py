#!/usr/bin/env python3
"""
Hash-based Fingerprint Cache

A generic caching system that generates a single hash from file paths and modification times.
Uses file locking for concurrent access and compare-and-swap operations for safe updates.
"""

import hashlib
import json
import os
import time
from pathlib import Path
from typing import Any, Dict, List, Optional

import fasteners


class HashFingerprintCache:
    """
    Hash-based fingerprint cache with file locking for concurrent access.

    Generates a single SHA256 hash from all provided file paths and their modification times.
    Uses compare-and-swap operations to prevent race conditions when multiple processes
    are validating and updating the same cache.
    """

    def __init__(self, cache_dir: Path, subpath: str):
        """
        Initialize hash fingerprint cache.

        Args:
            cache_dir: Base cache directory (e.g., Path(".cache"))
            subpath: Subdirectory name for this cache (e.g., "js_lint", "native_build")
        """
        self.cache_dir = cache_dir
        self.subpath = subpath

        # Create cache directory structure
        self.fingerprint_dir = cache_dir / "fingerprint"
        self.fingerprint_dir.mkdir(parents=True, exist_ok=True)

        # Cache and lock file paths
        self.cache_file = self.fingerprint_dir / f"{subpath}.json"
        self.lock_file = str(self.fingerprint_dir / f"{subpath}.lock")

    def _get_file_hash_data(self, file_paths: List[Path]) -> str:
        """
        Generate hash data from file paths and modification times.

        Args:
            file_paths: List of file paths to include in hash

        Returns:
            SHA256 hash as hexadecimal string
        """
        hasher = hashlib.sha256()

        # Sort paths for consistent ordering
        sorted_paths = sorted(file_paths, key=str)

        for file_path in sorted_paths:
            if file_path.exists() and file_path.is_file():
                # Include file path and modification time
                hasher.update(str(file_path.resolve()).encode("utf-8"))
                hasher.update(str(os.path.getmtime(file_path)).encode("utf-8"))
            else:
                # File doesn't exist - include path with special marker
                hasher.update(str(file_path.resolve()).encode("utf-8"))
                hasher.update(b"MISSING_FILE")

        # Include count of files to detect changes in file list
        hasher.update(f"file_count:{len(file_paths)}".encode("utf-8"))

        return hasher.hexdigest()

    def get_current_hash(self, file_paths: List[Path]) -> str:
        """
        Get current hash for the given file paths.

        Args:
            file_paths: List of file paths to hash

        Returns:
            SHA256 hash of current state
        """
        return self._get_file_hash_data(file_paths)

    def _read_cache_data(self) -> Optional[Dict[str, Any]]:
        """
        Read cache data from JSON file (should be called within lock context).

        Returns:
            Cache data dict or None if file doesn't exist or is corrupted
        """
        if not self.cache_file.exists():
            return None

        try:
            with open(self.cache_file, "r") as f:
                return json.load(f)
        except (json.JSONDecodeError, OSError):
            return None

    def _write_cache_data(self, data: Dict[str, Any]) -> None:
        """
        Write cache data to JSON file (should be called within lock context).

        Args:
            data: Cache data to write
        """
        try:
            with open(self.cache_file, "w") as f:
                json.dump(data, f, indent=2)
        except OSError as e:
            raise RuntimeError(f"Failed to write cache file {self.cache_file}: {e}")

    def is_valid(self, file_paths: List[Path]) -> bool:
        """
        Check if the current file state matches the cached hash.

        Args:
            file_paths: List of file paths to validate

        Returns:
            True if cached hash matches current state, False otherwise
        """
        current_hash = self.get_current_hash(file_paths)

        # Use read lock for validation
        lock = fasteners.InterProcessLock(self.lock_file)
        with lock:
            cache_data = self._read_cache_data()
            if cache_data is None:
                return False

            cached_hash = cache_data.get("hash")
            return cached_hash == current_hash

    def mark_success(self, file_paths: List[Path]) -> bool:
        """
        Mark validation as successful using compare-and-swap operation.

        This method prevents race conditions by checking if the hash has changed
        between validation and update. If another process updated the cache
        in the meantime, this operation will fail.

        Args:
            file_paths: List of file paths that were successfully validated

        Returns:
            True if cache was updated successfully, False if hash changed
            (indicating another process updated the cache)
        """
        current_hash = self.get_current_hash(file_paths)
        current_time = time.time()

        # Use write lock for compare-and-swap operation
        lock = fasteners.InterProcessLock(self.lock_file)
        with lock:
            # Read current cache state
            cache_data = self._read_cache_data()

            if cache_data is not None:
                # Check if hash changed since our validation
                cached_hash = cache_data.get("hash")
                if cached_hash != current_hash:
                    # Hash changed - another process updated the cache
                    return False

            # Hash matches or no previous cache - safe to update
            new_cache_data = {
                "hash": current_hash,
                "timestamp": current_time,
                "subpath": self.subpath,
                "file_count": len(file_paths),
            }

            self._write_cache_data(new_cache_data)
            return True

    def invalidate(self) -> None:
        """
        Invalidate the cache by removing the cache file.

        This forces the next validation to return False.
        """
        lock = fasteners.InterProcessLock(self.lock_file)
        with lock:
            if self.cache_file.exists():
                try:
                    self.cache_file.unlink()
                except OSError as e:
                    raise RuntimeError(
                        f"Failed to invalidate cache file {self.cache_file}: {e}"
                    )

    def get_cache_info(self) -> Optional[Dict[str, Any]]:
        """
        Get information about the current cache state.

        Returns:
            Dict with cache information or None if no cache exists
        """
        lock = fasteners.InterProcessLock(self.lock_file)
        with lock:
            cache_data = self._read_cache_data()
            if cache_data is None:
                return None

            return {
                "hash": cache_data.get("hash"),
                "timestamp": cache_data.get("timestamp"),
                "subpath": cache_data.get("subpath"),
                "file_count": cache_data.get("file_count"),
                "cache_file": str(self.cache_file),
                "cache_exists": self.cache_file.exists(),
            }


def create_cache(cache_dir: Path, subpath: str) -> HashFingerprintCache:
    """
    Convenience function to create a hash fingerprint cache.

    Args:
        cache_dir: Base cache directory
        subpath: Subdirectory name for this cache

    Returns:
        HashFingerprintCache instance
    """
    return HashFingerprintCache(cache_dir, subpath)


if __name__ == "__main__":
    # Demo/test the hash fingerprint cache
    import tempfile

    print("Hash Fingerprint Cache Demo")
    print("=" * 40)

    # Create temporary test files
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)

        # Create test files
        test_file1 = temp_path / "test1.txt"
        test_file2 = temp_path / "test2.txt"

        test_file1.write_text("Hello, World!")
        test_file2.write_text("Testing cache")

        test_files = [test_file1, test_file2]

        # Create cache
        cache_dir = temp_path / ".cache"
        cache = HashFingerprintCache(cache_dir, "demo")

        print(f"Test files: {[str(f) for f in test_files]}")

        # First validation - should be invalid (no cache)
        is_valid = cache.is_valid(test_files)
        print(f"Initial validation: {is_valid}")

        # Get current hash
        current_hash = cache.get_current_hash(test_files)
        print(f"Current hash: {current_hash[:16]}...")

        # Mark as successful
        success = cache.mark_success(test_files)
        print(f"Mark success result: {success}")

        # Second validation - should be valid now
        is_valid2 = cache.is_valid(test_files)
        print(f"Second validation: {is_valid2}")

        # Get cache info
        cache_info = cache.get_cache_info()
        if cache_info:
            print(f"Cache info:")
            print(f"  Hash: {cache_info['hash'][:16]}...")
            print(f"  File count: {cache_info['file_count']}")
            print(f"  Cache file: {cache_info['cache_file']}")

        # Modify a file
        print("\nModifying test file...")
        test_file1.write_text("Modified content!")

        # Third validation - should be invalid now
        is_valid3 = cache.is_valid(test_files)
        print(f"Validation after modification: {is_valid3}")

        print("\nDemo completed successfully!")
