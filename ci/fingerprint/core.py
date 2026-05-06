#!/usr/bin/env python3
"""
Core Fingerprint Cache Implementations

This module provides three fingerprint cache strategies:

1. FingerprintCache: Legacy two-layer cache (mtime + MD5) for individual file tracking
2. TwoLayerFingerprintCache: Glob-pattern based change detection using zccache-fingerprint CLI (blake3)
3. HashFingerprintCache: Aggregate hash change detection using zccache-fingerprint CLI (blake3)

TwoLayerFingerprintCache and HashFingerprintCache delegate to the Rust-based
``zccache-fingerprint`` CLI for fast blake3 hashing with mtime fast-paths.
The CLI handles file locking, crash-safe pending patterns, and smart touch handling.

All caches follow the safe pattern:
- check_needs_update(): Compute and store fingerprint before processing
- mark_success(): Save the pre-computed fingerprint (immune to race conditions)
"""

import hashlib
import json
import os
import shutil
import subprocess
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Optional


# ==============================================================================
# Legacy FingerprintCache (from ci/ci/fingerprint_cache.py)
# ==============================================================================


@dataclass(slots=True)
class CacheEntry:
    """Cache entry storing file modification time and content hash."""

    modification_time: float
    md5_hash: str


@dataclass(slots=True)
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


@dataclass(slots=True)
class PendingFingerprint:
    """Pre-computed fingerprint data stored before processing starts."""

    timestamp: float
    file_count: int
    files: dict[str, dict[str, float | str]]


# ==============================================================================
# zccache CLI helpers
# ==============================================================================

_ZCCACHE_BIN: str | None = None
_ZCCACHE_FP_BIN: str | None = None


def _find_zccache() -> str:
    """Locate the main zccache binary (for daemon-backed ``fp`` subcommand)."""
    global _ZCCACHE_BIN
    if _ZCCACHE_BIN is not None:
        return _ZCCACHE_BIN
    found = shutil.which("zccache")
    if found is not None:
        _ZCCACHE_BIN = found
    return found or ""


def _find_zccache_fp() -> str:
    """Locate the standalone zccache-fp binary (fallback)."""
    global _ZCCACHE_FP_BIN
    if _ZCCACHE_FP_BIN is not None:
        return _ZCCACHE_FP_BIN
    found = shutil.which("zccache-fp")
    if found is None:
        raise FileNotFoundError(
            "zccache-fp binary not found on PATH. Install it with: pip install zccache"
        )
    _ZCCACHE_FP_BIN = found
    return found


def _run_zccache(
    args: list[str], *, check: bool = False, timeout: float = 60.0
) -> subprocess.CompletedProcess[str]:
    """Run a zccache fingerprint CLI command.

    Tries the daemon-backed ``zccache fp`` first (<1ms on cache hit).
    Falls back to standalone ``zccache-fp`` if the daemon is unavailable.

    Args:
        args: Arguments after ``fp`` / the binary name.
        check: If True, raise on non-zero exit.
        timeout: Subprocess timeout in seconds.

    Returns:
        CompletedProcess with captured stdout/stderr.
    """
    # Try daemon-backed path first: ``zccache fp <args>``
    zccache_bin = _find_zccache()
    if zccache_bin:
        cmd = [zccache_bin, "fp"] + args
        result = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            check=False,
            timeout=timeout,
        )
        # exit 0 (run) or 1 (skip) are valid; exit 2+ means daemon error
        if result.returncode in (0, 1):
            if check and result.returncode not in (0, 1):
                raise subprocess.CalledProcessError(
                    result.returncode, cmd, result.stdout, result.stderr
                )
            return result

    # Fallback to standalone: ``zccache-fp <args>``
    cmd = [_find_zccache_fp()] + args
    return subprocess.run(
        cmd,
        capture_output=True,
        text=True,
        check=check,
        timeout=timeout,
    )


# ==============================================================================
# TwoLayerFingerprintCache — backed by zccache-fingerprint (blake3)
# ==============================================================================


class TwoLayerFingerprintCache:
    """
    Two-layer file change detection backed by the Rust ``zccache-fingerprint`` CLI.

    Provides efficient change detection by:
    1. Fast mtime+size comparison (skip blake3 hash if unchanged)
    2. blake3 content verification when mtime/size differ
    3. Smart touch handling (content unchanged despite mtime change)

    The CLI handles file locking and crash-safe pending patterns internally.
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

        # Cache file path (same location as before for migration transparency)
        self.cache_file = self.fingerprint_dir / f"{subpath}.json"

    def check_needs_update(
        self,
        include: list[str] | None = None,
        exclude: list[str] | None = None,
        *,
        root: str = ".",
        ext: list[str] | None = None,
    ) -> bool:
        """
        Check if files need to be processed using two-layer detection.

        Delegates to ``zccache-fingerprint --cache-type two-layer check``.
        Exit 0 = run needed (files changed).
        Exit 1 = skip (cache hit, no changes).

        Use ``ext`` + ``root`` for fastest scanning (simple suffix match,
        scoped directory walk).  Use ``include`` for glob-pattern matching.

        Args:
            include: Glob patterns for files to monitor (e.g., ["src/**/*.cpp"]).
                     Cannot be combined with ``ext``.
            exclude: Optional patterns to exclude.
            root: Root directory for scanning (default: ".").
            ext: File extensions without dot (e.g., ["h", "cpp"]).
                 Faster than ``include`` — uses simple suffix match.
                 Cannot be combined with ``include``.

        Returns:
            True if processing is needed, False if cache is valid.
        """
        # zccache-fp 1.3.10 fails to canonicalize a relative "." root
        # ("scan error in : cannot canonicalize root"); always pass absolute.
        abs_root = str(Path(root).resolve())
        args = [
            "--cache-file",
            str(self.cache_file),
            "--cache-type",
            "two-layer",
            "check",
            "--root",
            abs_root,
        ]
        if ext:
            for e in ext:
                args.extend(["--ext", e])
        elif include:
            for pattern in include:
                args.extend(["--include", pattern])
        if exclude:
            for pattern in exclude:
                args.extend(["--exclude", pattern])

        result = _run_zccache(args)
        # exit 0 = run needed, exit 1 = skip (cache hit), exit 2+ = error (fail-safe: run)
        return result.returncode != 1

    def mark_success(self) -> None:
        """
        Mark processing as successful using the pre-computed fingerprint.

        Promotes the pending fingerprint (stored by ``check``) to the main cache.
        """
        _run_zccache(
            ["--cache-file", str(self.cache_file), "mark-success"],
            check=True,
        )

    def mark_failure(self) -> None:
        """Mark the previous check as failed, forcing re-run next time."""
        _run_zccache(
            ["--cache-file", str(self.cache_file), "mark-failure"],
            check=True,
        )

    def invalidate(self) -> None:
        """
        Invalidate the cache by deleting the cache file.

        Forces the next check to return True (needs update).
        """
        _run_zccache(
            ["--cache-file", str(self.cache_file), "invalidate"],
            check=True,
        )

    def get_cache_info(self) -> dict[str, int | bool | str] | None:
        """
        Get information about the current cache state.

        Returns:
            Dict with cache information or None if no cache exists.
        """
        if not self.cache_file.exists():
            return None
        return {
            "cache_file": str(self.cache_file),
            "cache_exists": True,
            "subpath": self.subpath,
        }


# ==============================================================================
# HashFingerprintCache — backed by zccache-fingerprint (blake3)
# ==============================================================================


class HashFingerprintCache:
    """
    Aggregate hash-based fingerprint cache backed by ``zccache-fingerprint`` CLI.

    Generates a single blake3 hash from the entire file set.  All-or-nothing:
    if any file changes, the whole set is considered dirty.

    Best for "run all tests" or "compile all examples" style decisions.
    """

    def __init__(
        self,
        cache_dir: Path,
        subpath: str,
        timestamp_file: Optional[Path] = None,
    ):
        """
        Initialize hash fingerprint cache.

        Args:
            cache_dir: Base cache directory (e.g., Path(".cache"))
            subpath: Subdirectory name for this cache (e.g., "js_lint", "native_build")
            timestamp_file: Optional path to write timestamp when changes are detected.
        """
        self.cache_dir = cache_dir
        self.subpath = subpath
        self.timestamp_file = timestamp_file

        # Create cache directory structure
        self.fingerprint_dir = cache_dir / "fingerprint"
        self.fingerprint_dir.mkdir(parents=True, exist_ok=True)

        # Cache file path
        self.cache_file = self.fingerprint_dir / f"{subpath}.json"

    def check_needs_update(
        self,
        include: list[str] | None = None,
        exclude: list[str] | None = None,
        *,
        root: str = ".",
        ext: list[str] | None = None,
    ) -> bool:
        """
        Check if files need to be processed.

        Delegates to ``zccache-fingerprint --cache-type hash check``.
        Exit 0 = run needed.  Exit 1 = skip (cache hit).

        Use ``ext`` + ``root`` for fastest scanning (simple suffix match,
        scoped directory walk).  Use ``include`` for glob-pattern matching.

        Args:
            include: Glob patterns for files to monitor.
                     Cannot be combined with ``ext``.
            exclude: Optional patterns to exclude.
            root: Root directory for scanning (default: ".").
            ext: File extensions without dot (e.g., ["h", "cpp"]).
                 Faster than ``include`` — uses simple suffix match.
                 Cannot be combined with ``include``.

        Returns:
            True if processing is needed, False if cache is valid.
        """
        abs_root = str(Path(root).resolve())
        args = [
            "--cache-file",
            str(self.cache_file),
            "--cache-type",
            "hash",
            "check",
            "--root",
            abs_root,
        ]
        if ext:
            for e in ext:
                args.extend(["--ext", e])
        elif include:
            for pattern in include:
                args.extend(["--include", pattern])
        if exclude:
            for pattern in exclude:
                args.extend(["--exclude", pattern])

        result = _run_zccache(args)

        needs_update = result.returncode != 1

        # Write timestamp file on change (preserves existing feature)
        if needs_update and self.timestamp_file is not None:
            self._write_timestamp_file()

        return needs_update

    def mark_success(self) -> None:
        """Mark processing as successful (promotes pending → main cache)."""
        _run_zccache(
            ["--cache-file", str(self.cache_file), "mark-success"],
            check=True,
        )

    def mark_failure(self) -> None:
        """Mark the previous check as failed, forcing re-run next time."""
        _run_zccache(
            ["--cache-file", str(self.cache_file), "mark-failure"],
            check=True,
        )

    def invalidate(self) -> None:
        """Delete the cache file (forces re-run on next check)."""
        _run_zccache(
            ["--cache-file", str(self.cache_file), "invalidate"],
            check=True,
        )

    def get_cache_info(self) -> Optional[dict[str, Any]]:
        """
        Get information about the current cache state.

        Returns:
            Dict with cache information or None if no cache exists.
        """
        if not self.cache_file.exists():
            return None
        return {
            "cache_file": str(self.cache_file),
            "cache_exists": True,
            "subpath": self.subpath,
        }

    def get_last_change_timestamp(self) -> Optional[float]:
        """
        Get the timestamp of the last detected change.

        Returns:
            Unix timestamp or None if no timestamp file exists.
        """
        if self.timestamp_file is None or not self.timestamp_file.exists():
            return None
        try:
            with open(self.timestamp_file, "r") as f:
                data = json.load(f)
                return data.get("timestamp")
        except (json.JSONDecodeError, OSError):
            return None

    def _write_timestamp_file(self) -> None:
        """Write timestamp file when changes are detected."""
        if self.timestamp_file is None:
            return
        try:
            self.timestamp_file.parent.mkdir(parents=True, exist_ok=True)
            timestamp_data = {
                "timestamp": time.time(),
                "subpath": self.subpath,
            }
            with open(self.timestamp_file, "w") as f:
                json.dump(timestamp_data, f, indent=2)
        except OSError:
            pass
