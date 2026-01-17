#!/usr/bin/env python3
"""
Core Fingerprint Cache Implementations

This module provides three fingerprint cache strategies:

1. FingerprintCache: Legacy two-layer cache (mtime + MD5) for individual file tracking
2. TwoLayerFingerprintCache: Efficient two-layer detection (mtime + MD5) for batch file tracking
3. HashFingerprintCache: Single SHA256 hash for all files, optimized for concurrent access

All caches follow the safe pattern:
- check_needs_update(): Compute and store fingerprint before processing
- mark_success(): Save the pre-computed fingerprint (immune to race conditions)
"""

import hashlib
import json
import os
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional, cast

import fasteners


# ==============================================================================
# Legacy FingerprintCache (from ci/ci/fingerprint_cache.py)
# ==============================================================================


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

    NOTE: This is the legacy implementation. For new code, prefer TwoLayerFingerprintCache
    which supports batch operations and the safe pre-computed fingerprint pattern.
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

    def _load_cache(self) -> dict[str, CacheEntry]:
        """
        Load cache from JSON file, return empty dict if file doesn't exist.

        Uses try-except instead of exists() check to avoid TOCTOU race condition.
        This is the same pattern used in meson_runner.py for test_list_cache.

        Returns:
            Dictionary mapping file paths to cache entries
        """
        try:
            with open(self.cache_file, "r") as f:
                data = json.load(f)

            # Convert JSON dict to CacheEntry objects
            cache: dict[str, CacheEntry] = {}
            for file_path, entry_data in data.items():
                cache[file_path] = CacheEntry(
                    modification_time=entry_data["modification_time"],
                    md5_hash=entry_data["md5_hash"],
                )
            return cache
        except FileNotFoundError:
            # Cache file doesn't exist yet - start fresh
            return {}
        except (json.JSONDecodeError, KeyError, TypeError, IOError, OSError):
            # Cache corrupted or inaccessible - start fresh
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
            raise IOError(f"Cannot read file {file_path}: {e}") from e

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
            "cache_file_size_bytes": (
                self.cache_file.stat().st_size if self.cache_file.exists() else 0
            ),
        }

    def get_cached_files(self) -> list[Path]:
        """
        Get list of all files currently tracked in the cache.

        Returns:
            List of Path objects for all files in the cache
        """
        return [Path(file_path) for file_path in self.cache.keys()]

    def check_for_deleted_files(self, base_path: Optional[Path] = None) -> list[Path]:
        """
        Check for files that are cached but no longer exist on disk.

        Args:
            base_path: Optional base path to limit search scope. If provided, only
                      checks for deleted files under this path.

        Returns:
            List of Path objects for files that are cached but missing from disk
        """
        deleted_files: list[Path] = []

        for file_path_str in self.cache.keys():
            file_path = Path(file_path_str)

            # If base_path is provided, only check files under that path
            if base_path is not None:
                try:
                    file_path.relative_to(base_path.resolve())
                except ValueError:
                    # File is not under base_path, skip it
                    continue

            # Check if the cached file still exists
            if not file_path.exists():
                deleted_files.append(file_path)

        return deleted_files

    def remove_deleted_files(self, deleted_files: list[Path]) -> None:
        """
        Remove entries for deleted files from the cache.

        Args:
            deleted_files: List of deleted file paths to remove from cache
        """
        for file_path in deleted_files:
            file_key = str(file_path.resolve())
            if file_key in self.cache:
                del self.cache[file_key]

        # Save the updated cache if any entries were removed
        if deleted_files:
            self._save_cache()

    def clear_cache(self) -> None:
        """Clear all cache entries and remove cache file."""
        self.cache.clear()
        if self.cache_file.exists():
            self.cache_file.unlink()


# ==============================================================================
# TwoLayerFingerprintCache (from ci/util/two_layer_fingerprint_cache.py)
# ==============================================================================


class TwoLayerFingerprintCache:
    """
    Two-layer file change detection with smart mtime updates.

    Provides efficient change detection by:
    1. Fast modification time comparison (skip if unchanged)
    2. Accurate content verification via MD5 hashing (when mtime differs)
    3. Smart mtime updates when content matches but mtime changed

    This is the recommended cache for lint and test operations.
    """

    def __init__(self, cache_dir: Path, subpath: str):
        """
        Initialize two-layer fingerprint cache.

        Args:
            cache_dir: Base cache directory (e.g., Path(".cache"))
            subpath: Subdirectory name for this cache (e.g., "python_lint", "cpp_lint")
        """
        self.cache_dir = cache_dir
        self.subpath = subpath

        # Create cache directory structure
        self.fingerprint_dir = cache_dir / "fingerprint"
        self.fingerprint_dir.mkdir(parents=True, exist_ok=True)

        # Cache and lock file paths
        self.cache_file = self.fingerprint_dir / f"{subpath}.json"
        self.lock_file = str(self.fingerprint_dir / f"{subpath}.lock")

        # Pending fingerprint data (stored before linting starts)
        self._pending_fingerprint: (
            dict[str, float | int | dict[str, dict[str, float | str]]] | None
        ) = None

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
            raise IOError(f"Cannot read file {file_path}: {e}") from e

    def _read_cache_data(self) -> dict[str, dict[str, float | str]]:
        """
        Read cache data from JSON file (should be called within lock context).

        Returns:
            Cache data dict mapping absolute paths to {mtime, hash} or empty dict
        """
        if not self.cache_file.exists():
            return {}

        try:
            with open(self.cache_file, "r") as f:
                data: Any = json.load(f)
                # Ensure data is a dict (backward compatibility)
                if isinstance(data, dict) and "files" in data:
                    files_value: Any = data["files"]  # type: ignore[reportUnknownVariableType]
                    if isinstance(files_value, dict):
                        return cast(dict[str, dict[str, float | str]], files_value)
                if isinstance(data, dict):
                    return cast(dict[str, dict[str, float | str]], data)
                return {}
        except (json.JSONDecodeError, OSError):
            # Cache corrupted - start fresh
            return {}

    def _write_cache_data(self, cache_data: dict[str, dict[str, float | str]]) -> None:
        """
        Write cache data to JSON file (should be called within lock context).

        Args:
            cache_data: Per-file cache data to write
        """
        try:
            with open(self.cache_file, "w") as f:
                json.dump({"files": cache_data}, f, indent=2)
        except OSError as e:
            raise RuntimeError(
                f"Failed to write cache file {self.cache_file}: {e}"
            ) from e

    def _save_pending_fingerprint(
        self, fingerprint: dict[str, float | int | dict[str, dict[str, float | str]]]
    ) -> None:
        """Save pending fingerprint to file for cross-process access."""
        pending_file = self.cache_file.with_suffix(".pending")
        try:
            with open(pending_file, "w") as f:
                json.dump(fingerprint, f, indent=2)
        except OSError:
            # Non-fatal - will fall back to in-memory if needed
            pass

    def _load_pending_fingerprint(
        self,
    ) -> dict[str, float | int | dict[str, dict[str, float | str]]] | None:
        """Load pending fingerprint from file."""
        pending_file = self.cache_file.with_suffix(".pending")
        if not pending_file.exists():
            return None

        try:
            with open(pending_file, "r") as f:
                data: Any = json.load(f)
                return cast(
                    dict[str, float | int | dict[str, dict[str, float | str]]], data
                )
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

    def check_needs_update(self, file_paths: list[Path]) -> bool:
        """
        Check if files need to be processed using two-layer detection.

        Layer 1: Fast mtime check - if all mtimes match cached values, skip
        Layer 2: Content hash - compute MD5 only for files with changed mtimes

        Smart optimization: If content matches despite mtime change, updates
        the cached mtime to avoid redundant hashing on next run.

        Args:
            file_paths: List of file paths to check

        Returns:
            True if processing is needed, False if cache is valid
        """
        lock = fasteners.InterProcessLock(self.lock_file)
        with lock:
            cache_data = self._read_cache_data()
            needs_update = False
            updated_cache = cache_data.copy()

            for file_path in file_paths:
                if not file_path.exists():
                    # File deleted - needs update
                    needs_update = True
                    file_key = str(file_path.resolve())
                    if file_key in updated_cache:
                        del updated_cache[file_key]
                    continue

                file_key = str(file_path.resolve())
                current_mtime = file_path.stat().st_mtime

                if file_key in cache_data:
                    cached_entry = cache_data[file_key]
                    cached_mtime = cached_entry.get("mtime", 0)
                    cached_hash = cached_entry.get("hash", "")

                    # Layer 1: Fast mtime check
                    if current_mtime == cached_mtime:
                        # No change - keep in cache as-is
                        continue

                    # Layer 2: Content verification (mtime changed)
                    current_hash = self._compute_md5(file_path)

                    if current_hash == cached_hash:
                        # Content unchanged despite mtime change (e.g., touch)
                        # Update cached mtime to avoid future hashing
                        updated_cache[file_key] = {
                            "mtime": current_mtime,
                            "hash": cached_hash,
                        }
                        # Don't set needs_update - content is identical
                    else:
                        # Content actually changed
                        updated_cache[file_key] = {
                            "mtime": current_mtime,
                            "hash": current_hash,
                        }
                        needs_update = True
                else:
                    # New file - compute hash and add to cache
                    current_hash = self._compute_md5(file_path)
                    updated_cache[file_key] = {
                        "mtime": current_mtime,
                        "hash": current_hash,
                    }
                    needs_update = True

            # Check for files in cache that are no longer being monitored
            # This detects when files are deleted from disk between runs
            cached_file_keys = set(cache_data.keys())
            current_file_keys = {str(fp.resolve()) for fp in file_paths}
            removed_files = cached_file_keys - current_file_keys

            if removed_files:
                needs_update = True  # File was removed from monitored set
                for file_key in removed_files:
                    if file_key in updated_cache:
                        del updated_cache[file_key]

            # Store pending fingerprint for mark_success()
            self._pending_fingerprint = {
                "timestamp": time.time(),
                "file_count": len(file_paths),
                "files": updated_cache,
            }

            # Save pending fingerprint to file for cross-process access
            self._save_pending_fingerprint(self._pending_fingerprint)

            # If we updated mtimes (touch case), save cache immediately
            # This ensures next run doesn't re-hash these files
            if not needs_update and updated_cache != cache_data:
                self._write_cache_data(updated_cache)

            return needs_update

    def mark_success(self) -> None:
        """
        Mark processing as successful using the pre-computed fingerprint.

        This saves the cache state that was computed during check_needs_update(),
        including any smart mtime updates from the touch optimization.
        """
        # Try to load from file first (cross-process support)
        fingerprint_data = self._load_pending_fingerprint()

        # Fall back to in-memory (single-process case)
        if fingerprint_data is None:
            fingerprint_data = self._pending_fingerprint

        if fingerprint_data is None:
            raise RuntimeError(
                "mark_success() called without prior check_needs_update()"
            )

        lock = fasteners.InterProcessLock(self.lock_file)
        with lock:
            files_data_raw: Any = fingerprint_data.get("files", {})
            # Type-safe extraction - ensure it's the right type
            if isinstance(files_data_raw, dict):
                self._write_cache_data(
                    cast(dict[str, dict[str, float | str]], files_data_raw)
                )

        # Clear pending data (both file and memory)
        self._clear_pending_fingerprint()
        self._pending_fingerprint = None

    def invalidate(self) -> None:
        """
        Invalidate the cache by removing the cache file.

        This forces the next validation to return True (needs update).
        """
        lock = fasteners.InterProcessLock(self.lock_file)
        with lock:
            if self.cache_file.exists():
                try:
                    self.cache_file.unlink()
                except OSError as e:
                    raise RuntimeError(
                        f"Failed to invalidate cache file {self.cache_file}: {e}"
                    ) from e

    def get_cache_info(self) -> dict[str, int | bool | str] | None:
        """
        Get information about the current cache state.

        Returns:
            Dict with cache information or None if no cache exists
        """
        lock = fasteners.InterProcessLock(self.lock_file)
        with lock:
            cache_data = self._read_cache_data()
            if not cache_data:
                return None

            return {
                "file_count": len(cache_data),
                "cache_file": str(self.cache_file),
                "cache_exists": self.cache_file.exists(),
            }


# ==============================================================================
# HashFingerprintCache (from ci/util/hash_fingerprint_cache.py)
# ==============================================================================


class HashFingerprintCache:
    """
    Hash-based fingerprint cache with file locking for concurrent access.

    Generates a single SHA256 hash from all provided file paths and their modification times.
    Uses compare-and-swap operations to prevent race conditions when multiple processes
    are validating and updating the same cache.

    This is optimized for scenarios where you need a single cache validation for
    many files, such as build systems or test runners.
    """

    def __init__(
        self, cache_dir: Path, subpath: str, timestamp_file: Optional[Path] = None
    ):
        """
        Initialize hash fingerprint cache.

        Args:
            cache_dir: Base cache directory (e.g., Path(".cache"))
            subpath: Subdirectory name for this cache (e.g., "js_lint", "native_build")
            timestamp_file: Optional path to write timestamp when changes are detected
        """
        self.cache_dir = cache_dir
        self.subpath = subpath
        self.timestamp_file = timestamp_file

        # Create cache directory structure
        self.fingerprint_dir = cache_dir / "fingerprint"
        self.fingerprint_dir.mkdir(parents=True, exist_ok=True)

        # Cache and lock file paths
        self.cache_file = self.fingerprint_dir / f"{subpath}.json"
        self.lock_file = str(self.fingerprint_dir / f"{subpath}.lock")

    def _get_file_hash_data(self, file_paths: list[Path]) -> str:
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
        except OSError:
            # If we can't store pending data, that's okay - we'll fall back to force update
            pass

    def _load_pending_fingerprint(self) -> Optional[dict[str, Any]]:
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

    def _write_timestamp_file(
        self, timestamp: float, file_count: int, hash_value: str
    ) -> None:
        """
        Write timestamp file when changes are detected.

        Args:
            timestamp: Unix timestamp of when changes were detected
            file_count: Number of files monitored
            hash_value: Current hash value
        """
        if self.timestamp_file is None:
            return

        try:
            # Create parent directory if needed
            self.timestamp_file.parent.mkdir(parents=True, exist_ok=True)

            # Write timestamp data in JSON format
            timestamp_data = {
                "timestamp": timestamp,
                "file_count": file_count,
                "hash": hash_value,
                "subpath": self.subpath,
            }

            with open(self.timestamp_file, "w") as f:
                json.dump(timestamp_data, f, indent=2)
        except OSError:
            # Non-fatal error - log but don't fail
            pass

    def get_last_change_timestamp(self) -> Optional[float]:
        """
        Get the timestamp of the last detected change.

        Returns:
            Unix timestamp or None if no timestamp file exists
        """
        if self.timestamp_file is None or not self.timestamp_file.exists():
            return None

        try:
            with open(self.timestamp_file, "r") as f:
                data = json.load(f)
                return data.get("timestamp")
        except (json.JSONDecodeError, OSError):
            return None

    def get_current_hash(self, file_paths: list[Path]) -> str:
        """
        Get current hash for the given file paths.

        Args:
            file_paths: List of file paths to hash

        Returns:
            SHA256 hash of current state
        """
        return self._get_file_hash_data(file_paths)

    def _read_cache_data(self) -> Optional[dict[str, Any]]:
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

    def _write_cache_data(self, data: dict[str, Any]) -> None:
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

    def is_valid(self, file_paths: list[Path]) -> bool:
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

    def check_needs_update(self, file_paths: list[Path]) -> bool:
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
            pending_fp: dict[str, Any] = getattr(self, "_pending_fingerprint")
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

            # Write timestamp file if configured
            if fingerprint_data and self.timestamp_file is not None:
                self._write_timestamp_file(
                    timestamp=fingerprint_data.get("timestamp", time.time()),
                    file_count=fingerprint_data.get("file_count", 0),
                    hash_value=fingerprint_data.get("hash", ""),
                )

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

    def get_cache_info(self) -> Optional[dict[str, Any]]:
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
