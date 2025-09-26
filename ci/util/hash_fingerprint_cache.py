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

    def _store_pending_fingerprint(
        self, hash_value: str, timestamp: float, file_count: int
    ) -> None:
        """Store pending fingerprint data in a temporary cache file."""
        pending_file = self.cache_file.with_suffix(".pending")
        pending_data = {
            "hash": hash_value,
            "timestamp": timestamp,
            "file_count": file_count,
            "subpath": self.subpath,
        }

        try:
            with open(pending_file, "w") as f:
                json.dump(pending_data, f, indent=2)
        except OSError as e:
            # If we can't store pending data, that's okay - we'll fall back to force update
            pass

    def _load_pending_fingerprint(self) -> Optional[Dict[str, Any]]:
        """Load pending fingerprint data from temporary cache file."""
        pending_file = self.cache_file.with_suffix(".pending")
        if not pending_file.exists():
            return None

        try:
            with open(pending_file, "r") as f:
                return json.load(f)
        except (json.JSONDecodeError, OSError):
            return None

    def _clear_pending_fingerprint(self) -> None:
        """Remove the pending fingerprint file."""
        pending_file = self.cache_file.with_suffix(".pending")
        if pending_file.exists():
            try:
                pending_file.unlink()
            except OSError:
                pass

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

        Note: This method is deprecated in favor of check_needs_update().
        Use check_needs_update() for new code as it follows the safe pattern.

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

    def check_needs_update(self, file_paths: List[Path]) -> bool:
        """
        Check if files need to be processed and store fingerprint for later use.

        This is the safe pattern: compute fingerprint before processing, store it
        in the cache file temporarily, and use the stored fingerprint in mark_success()
        regardless of file changes during processing.

        Args:
            file_paths: List of file paths to check

        Returns:
            True if processing is needed, False if cache is valid
        """
        current_hash = self.get_current_hash(file_paths)
        current_time = time.time()

        # Check if cache is valid
        lock = fasteners.InterProcessLock(self.lock_file)
        with lock:
            cache_data = self._read_cache_data()
            if cache_data is None:
                # No cache - store pending fingerprint and return needs update
                self._store_pending_fingerprint(
                    current_hash, current_time, len(file_paths)
                )
                return True

            cached_hash = cache_data.get("hash")
            needs_update = cached_hash != current_hash

            if needs_update:
                # Store pending fingerprint for successful completion
                self._store_pending_fingerprint(
                    current_hash, current_time, len(file_paths)
                )

            return needs_update

    def mark_success(self) -> None:
        """
        Mark processing as successful using the pre-computed fingerprint.

        This method uses the fingerprint stored by check_needs_update(),
        making it immune to file changes during processing. Works across
        process boundaries by storing pending data in a temporary file.
        """
        # Try to load pending fingerprint from file (cross-process safe)
        fingerprint_data = self._load_pending_fingerprint()

        # Fall back to in-memory pending fingerprint (single-process case)
        if fingerprint_data is None and hasattr(self, "_pending_fingerprint"):
            pending_fp: Dict[str, Any] = getattr(self, "_pending_fingerprint")
            fingerprint_data = pending_fp.copy()
            fingerprint_data["subpath"] = self.subpath

        if fingerprint_data is None:
            raise RuntimeError(
                "mark_success() called without prior check_needs_update()"
            )

        # Save using write lock
        lock = fasteners.InterProcessLock(self.lock_file)
        with lock:
            self._write_cache_data(fingerprint_data)

        # Clean up stored fingerprints
        self._clear_pending_fingerprint()
        if hasattr(self, "_pending_fingerprint"):
            delattr(self, "_pending_fingerprint")

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

        # First check - should need update (no cache)
        needs_update = cache.check_needs_update(test_files)
        print(f"Initial check needs update: {needs_update}")

        # Get current hash
        current_hash = cache.get_current_hash(test_files)
        print(f"Current hash: {current_hash[:16]}...")

        # Mark as successful
        cache.mark_success()
        print("Mark success completed")

        # Second check - should not need update now
        needs_update2 = cache.check_needs_update(test_files)
        print(f"Second check needs update: {needs_update2}")

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

        # Third check - should need update now
        needs_update3 = cache.check_needs_update(test_files)
        print(f"Check after modification needs update: {needs_update3}")

        print("\nDemo completed successfully!")
