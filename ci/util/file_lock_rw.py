from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""
File Locking API Wrapper with Stale Lock Detection

Provides a unified, simplified API for inter-process file locking.
Uses a centralized SQLite database for lock state with PID-based stale
lock detection and auto-recovery.

Features:
- True reader-writer lock semantics (multiple concurrent readers, exclusive writer)
- Atomic lock operations (SQLite transactions)
- Fine-grained per-resource locking
- 5-second default timeout for download operations
- PID tracking to detect crashed processes
- Automatic stale lock recovery
- Cross-platform (Windows/Linux/macOS)
"""

import logging
import os
import platform
import time
from pathlib import Path
from types import TracebackType
from typing import Any

from ci.util.file_lock_rw_util import is_process_alive
from ci.util.lock_database import LockDatabase, get_lock_database


logger = logging.getLogger(__name__)

# Re-export is_process_alive so existing callers don't break
__all__ = [
    "is_process_alive",
    "is_lock_stale",
    "break_stale_lock",
    "FileLock",
    "write_lock",
    "read_lock",
    "custom_lock",
    "download_lock",
    "_write_lock_metadata",
    "_read_lock_metadata",
    "_remove_lock_metadata",
]


def _write_lock_metadata(lock_file_path: Path, operation: str = "lock") -> None:
    """No-op: metadata is stored in the SQLite database.

    Kept for backward compatibility with callers that reference this function.
    """
    pass


def _read_lock_metadata(lock_file_path: Path) -> dict[str, Any] | None:
    """Read metadata from lock database for backward compatibility.

    Args:
        lock_file_path: Path to the lock file

    Returns:
        Dictionary with metadata, or None if no lock is held
    """
    db = get_lock_database(lock_file_path)
    lock_name = str(lock_file_path)
    holders = db.get_lock_info(lock_name)
    if not holders:
        return None

    # Return first holder's info in the legacy format
    holder = holders[0]
    return {
        "pid": holder["owner_pid"],
        "timestamp": str(holder["acquired_at"]),
        "operation": holder["operation"],
        "hostname": holder["hostname"],
    }


def _remove_lock_metadata(lock_file_path: Path) -> None:
    """No-op: metadata cleanup is handled by release().

    Kept for backward compatibility with callers that reference this function.
    """
    pass


def is_lock_stale(lock_file_path: Path) -> bool:
    """
    Check if a lock is stale (all holder processes are dead).

    Args:
        lock_file_path: Path to the lock file

    Returns:
        True if lock is stale, False if still active or no lock exists
    """
    db = get_lock_database(lock_file_path)
    lock_name = str(lock_file_path)
    return db.is_lock_stale(lock_name)


def break_stale_lock(lock_file_path: Path) -> bool:
    """
    Remove stale lock entries (dead PIDs) from the database.

    Args:
        lock_file_path: Path to the lock file

    Returns:
        True if stale lock was broken, False otherwise
    """
    db = get_lock_database(lock_file_path)
    lock_name = str(lock_file_path)
    return db.break_stale_lock(lock_name)


class FileLock:
    """
    Inter-process file lock wrapper backed by SQLite database.

    Provides a context manager for acquiring/releasing locks safely.
    Automatically detects and breaks stale locks (dead processes).
    Supports both read and write lock modes.
    """

    def __init__(
        self,
        lock_file_path: Path,
        timeout: float | None = None,
        operation: str = "lock",
        mode: str = "write",
    ):
        """
        Initialize file lock.

        Args:
            lock_file_path: Path to the lock file (used as lock name)
            timeout: Optional timeout in seconds (None = block indefinitely)
            operation: Description of the operation (for debugging)
            mode: Lock mode - 'read' or 'write' (default: 'write')
        """
        self.lock_file_path = Path(lock_file_path)
        self.timeout = timeout
        self.operation = operation
        self._mode = mode
        self._lock_name = str(self.lock_file_path)
        self._db: LockDatabase = get_lock_database(self.lock_file_path)
        self._acquired = False

    def __enter__(self) -> "FileLock":
        """Acquire the lock with stale lock detection and retry."""
        from ci.util.global_interrupt_handler import is_interrupted

        my_pid = os.getpid()
        hostname = platform.node()
        start_time = time.time()
        stale_check_done = False

        while True:
            if is_interrupted():
                raise KeyboardInterrupt()

            # Try to acquire
            acquired = self._db.try_acquire(
                self._lock_name, my_pid, hostname, self.operation, mode=self._mode
            )

            if acquired:
                self._acquired = True
                return self

            # Check for stale locks (once per acquisition attempt cycle)
            if not stale_check_done:
                if self._db.is_lock_stale(self._lock_name):
                    logger.info("Detected stale lock, breaking and retrying")
                    self._db.break_stale_lock(self._lock_name)
                    stale_check_done = True
                    continue  # Retry immediately after breaking stale lock

            # Check timeout
            if self.timeout is not None:
                elapsed = time.time() - start_time
                if elapsed >= self.timeout:
                    # Build informative error message
                    holders = self._db.get_lock_info(self._lock_name)
                    if holders:
                        holder = holders[0]
                        raise TimeoutError(
                            f"Failed to acquire lock {self._lock_name} within {self.timeout}s. "
                            f"Lock held by active process (PID {holder['owner_pid']}, "
                            f"operation: {holder['operation']})"
                        )
                    else:
                        raise TimeoutError(
                            f"Failed to acquire lock {self._lock_name} within {self.timeout}s"
                        )

            # Periodically check for stale locks
            elapsed = time.time() - start_time
            if elapsed > 1.0:
                stale_check_done = False  # Re-enable stale checks

            # Small sleep to prevent busy-waiting
            time.sleep(0.1)

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_val: BaseException | None,
        exc_tb: TracebackType | None,
    ) -> bool:
        """Release the lock."""
        if self._acquired:
            self._db.release(self._lock_name, os.getpid())
            self._acquired = False

        return False


def write_lock(
    file_path: Path, timeout: float | None = None, operation: str = "write"
) -> FileLock:
    """
    Create a write lock for a file.

    Args:
        file_path: Path to the file to lock
        timeout: Optional timeout in seconds
        operation: Description of the operation

    Returns:
        FileLock context manager

    Example:
        with write_lock(Path("data.json"), timeout=5.0) as lock:
            # Write to file safely
            with open("data.json", "w") as f:
                json.dump(data, f)
    """
    lock_file = file_path.parent / f".{file_path.name}.lock"
    return FileLock(lock_file, timeout=timeout, operation=operation, mode="write")


def read_lock(
    file_path: Path, timeout: float | None = None, operation: str = "read"
) -> FileLock:
    """
    Create a read lock for a file.

    Allows multiple concurrent readers. Blocks only if a writer holds the lock.

    Args:
        file_path: Path to the file to lock
        timeout: Optional timeout in seconds
        operation: Description of the operation

    Returns:
        FileLock context manager

    Example:
        with read_lock(Path("data.json"), timeout=5.0) as lock:
            # Read from file safely
            with open("data.json", "r") as f:
                data = json.load(f)
    """
    lock_file = file_path.parent / f".{file_path.name}.lock"
    return FileLock(lock_file, timeout=timeout, operation=operation, mode="read")


def custom_lock(
    lock_file_path: Path, timeout: float | None = None, operation: str = "custom"
) -> FileLock:
    """
    Create a lock with a custom lock file path.

    Useful when you want to lock a resource that isn't a file,
    or when you need a specific lock file location.

    Args:
        lock_file_path: Explicit path to the lock file
        timeout: Optional timeout in seconds
        operation: Description of the operation

    Returns:
        FileLock context manager

    Example:
        lock_path = cache_dir / "download.lock"
        with custom_lock(lock_path, timeout=5.0, operation="download") as lock:
            # Perform locked operation
            download_file(url, dest)
    """
    return FileLock(lock_file_path, timeout=timeout, operation=operation)


def download_lock(artifact_dir: Path, timeout: float = 5.0) -> FileLock:
    """
    Create a lock for download operations with stale detection.

    This is the recommended lock for toolchain/package downloads.
    Uses 5-second timeout by default with automatic stale lock recovery.

    Args:
        artifact_dir: Cache artifact directory being downloaded to
        timeout: Timeout in seconds (default: 5.0)

    Returns:
        FileLock context manager

    Example:
        cache_dir = Path("~/.platformio/global_cache")
        artifact_dir = cache_dir / "toolchain-xyz"

        with download_lock(artifact_dir) as lock:
            # Download inside lock
            download_file(url, artifact_dir / "toolchain.zip")
            # Write breadcrumb inside lock
            write_breadcrumb(artifact_dir / "info.json", data)
    """
    lock_file = artifact_dir / ".download.lock"
    return FileLock(lock_file, timeout=timeout, operation="download")
