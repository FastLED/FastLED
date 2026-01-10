#!/usr/bin/env python3
"""
Two-Layer Fingerprint Cache

Efficient file change detection using:
1. Layer 1: Fast modification time checks (microsecond performance)
2. Layer 2: Content MD5 hashing (only when mtime changed)

Key optimization: If content matches despite mtime change (e.g., touch, git checkout),
updates the cached mtime to avoid redundant hashing on subsequent checks.
"""
# pyright: reportUnknownVariableType=false

import hashlib
import json
import time
from pathlib import Path
from typing import Any, cast

import fasteners


class TwoLayerFingerprintCache:
    """
    Two-layer file change detection with smart mtime updates.

    Provides efficient change detection by:
    1. Fast modification time comparison (skip if unchanged)
    2. Accurate content verification via MD5 hashing (when mtime differs)
    3. Smart mtime updates when content matches but mtime changed
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
                    files_value: Any = data["files"]
                    if isinstance(files_value, dict):
                        files_raw: dict[str, Any] = files_value
                        return cast(dict[str, dict[str, float | str]], files_raw)
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
