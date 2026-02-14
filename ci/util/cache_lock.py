from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""
Cache-Specific Locking API

Provides higher-level locking functions specifically for cache operations.
Handles fine-grained per-artifact locking for concurrent downloads.
Uses SQLite-backed lock database for centralized lock management.
"""

import logging
from dataclasses import dataclass
from pathlib import Path

from ci.util.file_lock_rw import FileLock
from ci.util.lock_database import get_lock_database
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

    Uses the SQLite database to clear locks, plus scans for legacy .lock files.

    Args:
        cache_dir: Global cache directory

    Returns:
        Number of locks broken
    """
    if not cache_dir.exists():
        return 0

    broken_count = 0

    # Clear locks from the database
    db = get_lock_database()
    all_locks = db.list_all_locks()
    for lock in all_locks:
        lock_name = lock["lock_name"]
        # Only clear locks that relate to paths under this cache_dir
        if str(cache_dir) in lock_name:
            db.force_break(lock_name)
            broken_count += 1
            logger.info(f"Force-broke lock: {lock_name}")

    # Also clean up any legacy .lock files on disk
    for lock_file in cache_dir.rglob("*.lock"):
        try:
            if lock_file.exists():
                lock_file.unlink()
                logger.info(f"Removed legacy lock file: {lock_file}")
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
    """
    if not cache_dir.exists():
        return 0

    # Clean stale locks from the database
    db = get_lock_database()
    cleaned_count = db.cleanup_stale_locks()

    # Also clean up any legacy .lock files on disk
    from ci.util.file_lock_rw import break_stale_lock as _break_stale_lock_legacy

    for lock_file in cache_dir.rglob("*.lock"):
        try:
            # Check for legacy .lock.pid metadata files
            pid_file = lock_file.with_suffix(lock_file.suffix + ".pid")
            if pid_file.exists():
                if _break_stale_lock_legacy(lock_file):
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
        List of LockInfo objects
    """
    if not cache_dir.exists():
        return []

    locks: list[LockInfo] = []

    # Query database for all locks
    db = get_lock_database()
    all_db_locks = db.list_all_locks()

    from ci.util.file_lock_rw_util import is_process_alive

    for lock in all_db_locks:
        # Filter to locks relevant to this cache_dir
        lock_name = lock["lock_name"]
        if str(cache_dir) in lock_name:
            pid = lock["owner_pid"]
            stale = not is_process_alive(pid)
            locks.append(
                LockInfo(
                    path=lock_name,
                    pid=pid,
                    operation=lock["operation"],
                    timestamp=str(lock["acquired_at"]),
                    is_stale=stale,
                    hostname=lock["hostname"],
                )
            )

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

    db = get_lock_database(lock_file)
    lock_name = str(lock_file)

    # Check if held in database
    if not db.is_held(lock_name):
        return False

    # Check if it's stale
    if db.is_lock_stale(lock_name):
        return False

    return True
