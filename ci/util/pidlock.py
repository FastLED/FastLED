#!/usr/bin/env python3
"""
PID-based file locking mechanism.

This module provides a PIDLock class that uses PID files to detect
and clean up stale locks left by crashed processes.
"""

import atexit
import os
import signal
import threading
import time
import warnings
from pathlib import Path
from typing import Any, Dict, Optional, Set

import psutil
from filelock import FileLock as OriginalFileLock
from filelock import Timeout as FileLockTimeout


# Global registry to track active locks for cleanup
_active_locks: Set["PIDLock"] = set()
_lock_registry_lock = threading.Lock()


class PIDLock:
    """A file lock implementation with PID-based stale lock detection and cleanup.

    This class wraps the filelock.FileLock with additional functionality to
    detect and clean up stale locks left by crashed processes using PID files.

    Example:
        with PIDLock("resource.lock", timeout=30):
            # Critical section - only one process can execute this
            pass
    """

    def __init__(self, lock_file: str | Path, timeout: float = -1) -> None:
        """Initialize the PIDLock.

        Args:
            lock_file: Path to the lock file
            timeout: Default timeout for lock acquisition (-1 = wait forever)
        """
        self.lock_file = Path(lock_file)
        self.pid_file = self.lock_file.with_suffix(self.lock_file.suffix + ".pid")
        self.timeout = timeout
        self._our_pid = os.getpid()

        # Create the underlying FileLock
        self._file_lock = OriginalFileLock(str(self.lock_file))
        self._is_acquired = False
        self._lock = threading.RLock()  # Thread-safe operations

        # Signal handling state
        self._original_handlers: Dict[int, Any] = {}
        self._signal_handlers_installed = False

    def acquire(self, timeout: Optional[float] = None) -> bool:
        """Acquire the lock with stale lock detection.

        Args:
            timeout: Timeout in seconds. None uses the default timeout.
                    -1 means wait forever, 0 means try once.

        Returns:
            True if lock was acquired, False if timeout occurred (for timeout=0).

        Raises:
            TimeoutError: If timeout > 0 and lock could not be acquired.
        """
        if timeout is None:
            timeout = self.timeout

        with self._lock:
            if self._is_acquired:
                return True

            # Check for and clean up stale locks
            self._cleanup_stale_lock()

            # Acquire the underlying file lock
            try:
                if timeout == 0:
                    # Non-blocking attempt
                    success = self._file_lock.acquire(timeout=0.01)
                    if not success:
                        return False
                elif timeout == -1:
                    # Wait forever
                    self._file_lock.acquire()
                else:
                    # Wait with timeout
                    self._file_lock.acquire(timeout=timeout)

                # Lock acquired successfully - write our PID
                self._write_pid_file()
                self._is_acquired = True

                # Install scoped signal handlers
                self._install_signal_handlers()

                # Register this lock for cleanup
                with _lock_registry_lock:
                    _active_locks.add(self)

                return True

            except FileLockTimeout:
                if timeout > 0:
                    raise Timeout(
                        f"Could not acquire lock {self.lock_file} within {timeout} seconds"
                    )
                return False

    def release(self) -> None:
        """Release the lock and clean up PID file."""
        with self._lock:
            if not self._is_acquired:
                return

            try:
                # Remove from active locks registry
                with _lock_registry_lock:
                    _active_locks.discard(self)

                # Uninstall scoped signal handlers
                self._uninstall_signal_handlers()

                # Clean up PID file
                self._remove_pid_file()

                # Release the underlying lock
                self._file_lock.release()

                self._is_acquired = False

            except Exception as e:
                warnings.warn(f"Error releasing lock {self.lock_file}: {e}")

    def _cleanup_stale_lock(self) -> None:
        """Check for and clean up stale locks."""
        if not self.pid_file.exists():
            return

        try:
            # Read the PID from the PID file
            pid_content = self.pid_file.read_text().strip()
            if not pid_content:
                # Empty PID file - remove it
                self._remove_pid_file()
                return

            try:
                lock_pid = int(pid_content)
            except ValueError:
                # Invalid PID format - remove stale files
                self._remove_pid_file()
                self._remove_lock_file()
                return

            # Check if the process is still alive
            if not psutil.pid_exists(lock_pid):
                # Process is dead - clean up stale lock
                self._remove_pid_file()
                self._remove_lock_file()
            # If process is alive, the lock is valid and we'll wait for it

        except Exception as e:
            warnings.warn(f"Error checking stale lock {self.lock_file}: {e}")
            # On error, try to clean up anyway
            try:
                self._remove_pid_file()
                self._remove_lock_file()
            except Exception:
                pass

    def _write_pid_file(self) -> None:
        """Write our PID to the PID file."""
        try:
            self.pid_file.parent.mkdir(parents=True, exist_ok=True)
            self.pid_file.write_text(str(self._our_pid))
        except Exception as e:
            warnings.warn(f"Could not write PID file {self.pid_file}: {e}")

    def _remove_pid_file(self) -> None:
        """Remove the PID file."""
        try:
            if self.pid_file.exists():
                self.pid_file.unlink()
        except Exception as e:
            warnings.warn(f"Could not remove PID file {self.pid_file}: {e}")

    def _remove_lock_file(self) -> None:
        """Remove the lock file (for stale lock cleanup)."""
        try:
            if self.lock_file.exists():
                self.lock_file.unlink()
        except Exception as e:
            warnings.warn(f"Could not remove lock file {self.lock_file}: {e}")

    def _force_cleanup(self) -> None:
        """Force cleanup of lock files (called during shutdown)."""
        try:
            # Only clean up if we're the owner of this lock
            if self._is_acquired:
                self._remove_pid_file()
                # Don't remove the main lock file - let filelock handle it

            # Remove from registry
            with _lock_registry_lock:
                _active_locks.discard(self)

        except Exception:
            pass  # Ignore all errors during forced cleanup

    def _install_signal_handlers(self) -> None:
        """Install signal handlers for this lock instance."""
        if self._signal_handlers_installed:
            return

        def cleanup_and_exit(signum: int, frame: Any) -> None:
            """Clean up this lock and re-raise the signal."""
            try:
                self._force_cleanup()
            except Exception:
                pass  # Ignore cleanup errors

            # Re-raise the signal to allow normal termination
            if signum == signal.SIGINT:
                # For SIGINT (Ctrl+C), raise KeyboardInterrupt to allow normal Python handling
                raise KeyboardInterrupt()
            else:
                # For other signals, restore default handler and re-raise
                signal.signal(signum, signal.SIG_DFL)
                os.kill(os.getpid(), signum)

        # Install signal handlers and store originals
        signals_to_handle: list[int] = []
        if hasattr(signal, "SIGTERM"):
            signals_to_handle.append(signal.SIGTERM)
        if hasattr(signal, "SIGINT"):
            signals_to_handle.append(signal.SIGINT)
        if os.name == "nt" and hasattr(signal, "SIGBREAK"):
            signals_to_handle.append(signal.SIGBREAK)

        for sig in signals_to_handle:
            try:
                self._original_handlers[sig] = signal.signal(sig, cleanup_and_exit)
            except (ValueError, OSError):
                # Signal not supported or can't be handled
                pass

        self._signal_handlers_installed = True

    def _uninstall_signal_handlers(self) -> None:
        """Restore original signal handlers."""
        if not self._signal_handlers_installed:
            return

        for sig, original_handler in self._original_handlers.items():
            try:
                signal.signal(sig, original_handler)
            except (ValueError, OSError):
                # Signal not supported or can't be restored
                pass

        self._original_handlers.clear()
        self._signal_handlers_installed = False

    def is_locked(self) -> bool:
        """Check if the lock is currently held by a living process."""
        if not self.lock_file.exists():
            return False

        if not self.pid_file.exists():
            # Lock file exists but no PID file - might be stale
            return True  # Conservative assumption

        try:
            pid_content = self.pid_file.read_text().strip()
            if not pid_content:
                return False

            lock_pid = int(pid_content)
            return psutil.pid_exists(lock_pid)

        except (ValueError, OSError):
            return False

    def __enter__(self):
        """Context manager entry."""
        self.acquire()
        return self

    def __exit__(self, exc_type: Any, exc_val: Any, exc_tb: Any) -> None:
        """Context manager exit."""
        self.release()

    def __del__(self):
        """Destructor - ensure cleanup."""
        if hasattr(self, "_is_acquired") and self._is_acquired:
            try:
                self.release()
            except Exception:
                pass
        # Always try to clean up signal handlers
        if hasattr(self, "_signal_handlers_installed"):
            try:
                self._uninstall_signal_handlers()
            except Exception:
                pass


# Compatibility aliases
FileLock = PIDLock
StaleFileLock = PIDLock  # Legacy alias


class Timeout(Exception):
    """Exception raised when lock acquisition times out."""

    pass
