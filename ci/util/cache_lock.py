from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Cache-Specific Locking API

Provides higher-level locking functions specifically for cache operations.
Handles fine-grained per-artifact locking for concurrent downloads.
"""

import logging
from dataclasses import dataclass
from pathlib import Path

from ci.util.file_lock_rw import (
    FileLock,
    _read_lock_metadata,
    break_stale_lock,
    is_lock_stale,
)
from ci.util.url_utils import sanitize_url_for_path


logger = logging.getLogger(__name__)


@dataclass
class LockInfo:
    """Container for lock metadata information."""

    path: str
    pid: int | None
    operation: str
    timestamp: str | None
    is_stale: bool
    hostname: str | None

    def __str__(self) -> str:
        """Human-readable lock information."""
        status = "STALE" if self.is_stale else "ACTIVE"
        pid_str = f"PID {self.pid}" if self.pid else "unknown PID"
        return f"{status}: {self.path} ({pid_str}, {self.operation})"


def acquire_artifact_lock(
    cache_dir: Path, url: str, operation: str = "download", timeout: float = 5.0
) -> FileLock:
    """
    Acquire lock for a specific cache artifact directory.

    Provides fine-grained locking per toolchain/package URL.
    Multiple different packages can download concurrently.

    Args:
        cache_dir: Global cache directory
        url: URL being downloaded/accessed
        operation: Operation name (download/delete/update) for debugging
        timeout: Lock timeout in seconds (default: 5.0)

    Returns:
        FileLock context manager with stale lock recovery

    Example:
        with acquire_artifact_lock(cache_dir, url, operation="download") as lock:
            # Download inside lock
            download_file(url, dest)
            # Write breadcrumb inside lock
            write_breadcrumb(path, data)
    """
    # Get artifact directory for this URL
    cache_key = str(sanitize_url_for_path(url))
    artifact_dir = cache_dir / cache_key

    # Ensure artifact directory exists
    artifact_dir.mkdir(parents=True, exist_ok=True)

    # Use download_lock for consistency
    lock_file = artifact_dir / ".download.lock"
    return FileLock(lock_file, timeout=timeout, operation=operation)


def force_unlock_cache(cache_dir: Path) -> int:
    """
    Break ALL locks in cache directory (manual recovery).

    Use this for recovery after system crashes or when locks are stuck.
    Does NOT check if processes are alive - breaks all locks unconditionally.

    Args:
        cache_dir: Global cache directory

    Returns:
        Number of locks broken

    Example:
        broken = force_unlock_cache(Path("~/.platformio/global_cache"))
        logger.info(f"Broke {broken} locks")
    """
    if not cache_dir.exists():
        return 0

    broken_count = 0

    # Find all .lock files recursively
    for lock_file in cache_dir.rglob("*.lock"):
        try:
            # Remove lock file
            if lock_file.exists():
                lock_file.unlink()
                logger.info(f"Force-broke lock: {lock_file}")
                broken_count += 1

            # Remove associated .pid file
            pid_file = lock_file.with_suffix(lock_file.suffix + ".pid")
            if pid_file.exists():
                pid_file.unlink()

        except OSError as e:
            logger.warning(f"Failed to remove lock file {lock_file}: {e}")

    return broken_count


def cleanup_stale_locks(cache_dir: Path) -> int:
    """
    Find and remove only stale locks (PID-based detection).

    Safe to run anytime - only removes locks from dead processes.

    Args:
        cache_dir: Global cache directory

    Returns:
        Number of stale locks cleaned

    Example:
        cleaned = cleanup_stale_locks(Path("~/.platformio/global_cache"))
        logger.info(f"Cleaned {cleaned} stale locks")
    """
    if not cache_dir.exists():
        return 0

    cleaned_count = 0

    # Find all .lock files recursively
    for lock_file in cache_dir.rglob("*.lock"):
        try:
            if break_stale_lock(lock_file):
                cleaned_count += 1
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            logger.warning(f"Error checking lock {lock_file}: {e}")

    return cleaned_count


def list_active_locks(cache_dir: Path) -> list[LockInfo]:
    """
    List all active locks in cache directory with metadata.

    Args:
        cache_dir: Global cache directory

    Returns:
        List of LockInfo objects containing:
        - path: Path to lock file
        - pid: Process ID holding lock (None if unknown)
        - operation: Operation name
        - timestamp: When lock was acquired (None if unknown)
        - is_stale: Whether lock is from dead process
        - hostname: Host where lock was created (None if unknown)

    Example:
        locks = list_active_locks(Path("~/.platformio/global_cache"))
        for lock in locks:
            print(lock)  # Uses LockInfo.__str__ for formatting
    """
    if not cache_dir.exists():
        return []

    locks: list[LockInfo] = []

    # Find all .lock files recursively
    for lock_file in cache_dir.rglob("*.lock"):
        try:
            metadata = _read_lock_metadata(lock_file)
            if metadata is None:
                # Lock exists but no metadata
                locks.append(
                    LockInfo(
                        path=str(lock_file),
                        pid=None,
                        operation="unknown",
                        timestamp=None,
                        is_stale=False,  # Can't determine, assume active
                        hostname=None,
                    )
                )
                continue

            # Check if stale
            stale = is_lock_stale(lock_file)

            locks.append(
                LockInfo(
                    path=str(lock_file),
                    pid=metadata.get("pid"),
                    operation=metadata.get("operation", "unknown"),
                    timestamp=metadata.get("timestamp"),
                    is_stale=stale,
                    hostname=metadata.get("hostname"),
                )
            )

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            logger.warning(f"Error reading lock {lock_file}: {e}")

    return locks


def is_artifact_locked(cache_dir: Path, url: str) -> bool:
    """
    Check if an artifact is currently locked.

    Args:
        cache_dir: Global cache directory
        url: URL to check

    Returns:
        True if artifact is locked by an active process, False otherwise
    """
    cache_key = str(sanitize_url_for_path(url))
    artifact_dir = cache_dir / cache_key
    lock_file = artifact_dir / ".download.lock"

    if not lock_file.exists():
        return False

    # Check if lock is stale
    if is_lock_stale(lock_file):
        return False  # Stale lock = not actively locked

    return True
