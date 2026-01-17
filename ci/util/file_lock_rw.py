from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
File Locking API Wrapper with Stale Lock Detection

Provides a unified, simplified API for inter-process file locking.
Wraps the fasteners library with PID-based stale lock detection and auto-recovery.

Features:
- Fine-grained per-resource locking
- 5-second default timeout for download operations
- PID tracking to detect crashed processes
- Automatic stale lock recovery
- Cross-platform (Windows/Linux/macOS)
"""

import json
import logging
import os
import platform
import time
from datetime import datetime
from pathlib import Path
from types import TracebackType
from typing import Any

import fasteners


logger = logging.getLogger(__name__)


def is_process_alive(pid: int) -> bool:
    """
    Check if a process with given PID is still running (cross-platform).

    Args:
        pid: Process ID to check

    Returns:
        True if process exists, False otherwise
    """
    if pid <= 0:
        return False

    try:
        # Unix/Linux/macOS: send signal 0 (doesn't actually send signal, just checks)
        if platform.system() != "Windows":
            os.kill(pid, 0)
            return True
        else:
            # Windows: Try to open process handle
            import ctypes

            kernel32 = ctypes.windll.kernel32
            PROCESS_QUERY_INFORMATION = 0x0400
            handle = kernel32.OpenProcess(PROCESS_QUERY_INFORMATION, False, pid)
            if handle:
                kernel32.CloseHandle(handle)
                return True
            return False
    except (OSError, ProcessLookupError):
        return False
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        logger.warning(f"Error checking if PID {pid} is alive: {e}")
        return False  # Assume dead if we can't check


def _write_lock_metadata(lock_file_path: Path, operation: str = "lock") -> None:
    """
    Write metadata for lock file (PID, timestamp, operation).

    Args:
        lock_file_path: Path to the lock file
        operation: Description of the operation holding the lock
    """
    metadata_file = lock_file_path.with_suffix(lock_file_path.suffix + ".pid")
    metadata = {
        "pid": os.getpid(),
        "timestamp": datetime.now().isoformat(),
        "operation": operation,
        "hostname": platform.node(),
    }

    try:
        with open(metadata_file, "w") as f:
            json.dump(metadata, f, indent=2)
    except OSError as e:
        logger.warning(f"Failed to write lock metadata {metadata_file}: {e}")


def _read_lock_metadata(lock_file_path: Path) -> dict[str, Any] | None:
    """
    Read metadata from lock file.

    Args:
        lock_file_path: Path to the lock file

    Returns:
        Dictionary with metadata, or None if file doesn't exist
    """
    metadata_file = lock_file_path.with_suffix(lock_file_path.suffix + ".pid")

    if not metadata_file.exists():
        return None

    try:
        with open(metadata_file, "r") as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError) as e:
        logger.warning(f"Failed to read lock metadata {metadata_file}: {e}")
        return None


def _remove_lock_metadata(lock_file_path: Path) -> None:
    """
    Remove metadata file for lock.

    Args:
        lock_file_path: Path to the lock file
    """
    metadata_file = lock_file_path.with_suffix(lock_file_path.suffix + ".pid")

    try:
        if metadata_file.exists():
            metadata_file.unlink()
    except OSError as e:
        logger.warning(f"Failed to remove lock metadata {metadata_file}: {e}")


def is_lock_stale(lock_file_path: Path) -> bool:
    """
    Check if a lock file is stale (owner process is dead).

    Args:
        lock_file_path: Path to the lock file

    Returns:
        True if lock is stale, False if still active or can't determine
    """
    if not lock_file_path.exists():
        return False  # No lock file = not stale, just absent

    metadata = _read_lock_metadata(lock_file_path)
    if metadata is None:
        # ⚠️ WARNING: Lock has no metadata (foreign lock or corrupted)
        # Use age-based heuristic: locks older than 30 minutes are considered stale
        # This prevents immortal locks from PlatformIO or other systems
        try:
            age_seconds = time.time() - lock_file_path.stat().st_mtime
            age_minutes = age_seconds / 60

            if age_seconds > 1800:  # 30 minutes
                logger.warning(
                    f"Lock {lock_file_path} has no metadata and is {age_minutes:.1f} minutes old, treating as STALE"
                )
                return True
            else:
                logger.warning(
                    f"Lock {lock_file_path} has no metadata but is recent ({age_minutes:.1f} minutes old), assuming active"
                )
                return False
        except OSError as e:
            logger.warning(
                f"Failed to stat lock file {lock_file_path}: {e}, assuming active"
            )
            return False

    pid = metadata.get("pid")
    if pid is None:
        logger.warning(f"Lock {lock_file_path} has no PID in metadata")
        return False

    # Check if process is alive
    alive = is_process_alive(pid)

    if not alive:
        logger.info(f"Lock {lock_file_path} is stale (PID {pid} not running)")

    return not alive


def break_stale_lock(lock_file_path: Path) -> bool:
    """
    Remove a stale lock file and its metadata.

    Args:
        lock_file_path: Path to the lock file

    Returns:
        True if lock was broken, False if failed or lock is not stale
    """
    if not is_lock_stale(lock_file_path):
        return False

    try:
        # Remove lock file
        if lock_file_path.exists():
            lock_file_path.unlink()
            logger.info(f"Broke stale lock: {lock_file_path}")

        # Remove metadata
        _remove_lock_metadata(lock_file_path)

        return True
    except OSError as e:
        logger.error(f"Failed to break stale lock {lock_file_path}: {e}")
        return False


class FileLock:
    """
    Inter-process file lock wrapper with stale lock detection.

    Provides a context manager for acquiring/releasing locks safely.
    Automatically detects and breaks stale locks (dead processes).
    """

    def __init__(
        self,
        lock_file_path: Path,
        timeout: float | None = None,
        operation: str = "lock",
    ):
        """
        Initialize file lock.

        Args:
            lock_file_path: Path to the lock file
            timeout: Optional timeout in seconds (None = block indefinitely)
            operation: Description of the operation (for debugging)
        """
        self.lock_file_path = Path(lock_file_path)
        self.timeout = timeout
        self.operation = operation
        self._lock: fasteners.InterProcessLock | None = None

    def __enter__(self) -> "FileLock":
        """Acquire the lock with stale lock detection and retry."""
        # Ensure lock file directory exists
        self.lock_file_path.parent.mkdir(parents=True, exist_ok=True)

        # Create and acquire lock
        self._lock = fasteners.InterProcessLock(str(self.lock_file_path))

        if self.timeout is not None:
            # Try to acquire with timeout
            acquired = self._lock.acquire(blocking=True, timeout=self.timeout)

            if not acquired:
                # Timeout - check if lock is stale
                logger.info(
                    f"Lock acquisition timed out after {self.timeout}s, checking for stale lock"
                )

                if is_lock_stale(self.lock_file_path):
                    # Break stale lock and retry once
                    logger.info("Detected stale lock, breaking and retrying")
                    if break_stale_lock(self.lock_file_path):
                        # Retry acquisition
                        acquired = self._lock.acquire(
                            blocking=True, timeout=self.timeout
                        )
                        if not acquired:
                            raise TimeoutError(
                                f"Failed to acquire lock {self.lock_file_path} even after breaking stale lock"
                            )
                    else:
                        raise TimeoutError(
                            f"Failed to break stale lock {self.lock_file_path}"
                        )
                else:
                    # Lock is held by active process
                    metadata = _read_lock_metadata(self.lock_file_path)
                    if metadata:
                        pid = metadata.get("pid", "unknown")
                        operation = metadata.get("operation", "unknown")
                        raise TimeoutError(
                            f"Failed to acquire lock {self.lock_file_path} within {self.timeout}s. "
                            f"Lock held by active process (PID {pid}, operation: {operation})"
                        )
                    else:
                        raise TimeoutError(
                            f"Failed to acquire lock {self.lock_file_path} within {self.timeout}s"
                        )
        else:
            # No timeout - block indefinitely
            self._lock.acquire()

        # Write PID metadata after acquiring lock
        _write_lock_metadata(self.lock_file_path, self.operation)

        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_val: BaseException | None,
        exc_tb: TracebackType | None,
    ) -> bool:
        """Release the lock and clean up metadata."""
        if self._lock:
            # Remove metadata before releasing lock
            _remove_lock_metadata(self.lock_file_path)

            # Release lock
            self._lock.release()

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
    return FileLock(lock_file, timeout=timeout, operation=operation)


def read_lock(
    file_path: Path, timeout: float | None = None, operation: str = "read"
) -> FileLock:
    """
    Create a read lock for a file.

    Note: fasteners only provides exclusive locks, so this behaves
    identically to write_lock(). Provided for API compatibility.

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
    # Note: fasteners.InterProcessLock is exclusive, so read/write are the same
    lock_file = file_path.parent / f".{file_path.name}.lock"
    return FileLock(lock_file, timeout=timeout, operation=operation)


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
