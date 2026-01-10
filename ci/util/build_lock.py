from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly

#!/usr/bin/env python3
"""
Build lock utility for coordinating concurrent operations.

This module provides file-based locking to prevent concurrent operations
from conflicting. Supports both project-local locks (for builds) and
system-wide global locks (for shared resources like attached devices).

Uses the 'fasteners' library for cross-platform file locking.
"""

import time
from contextlib import contextmanager
from pathlib import Path
from types import TracebackType
from typing import Generator, Optional

import fasteners


class BuildLock:
    """
    File-based lock for coordinating operations using the fasteners library.

    Provides cross-platform file locking via the fasteners InterProcessLock.
    Supports both project-local locks (.build/locks) and system-wide global
    locks (~/.fastled/locks) depending on the use case.
    """

    def __init__(self, lock_name: str = "libfastled_build", use_global: bool = False):
        """
        Initialize build lock.

        Args:
            lock_name: Name of the lock (creates .lock file with this name)
            use_global: If True, use ~/.fastled/locks (system-wide). If False, use .build/locks (project-local)
        """
        if use_global:
            # Global lock (system-wide resource like attached device)
            home_dir = Path.home()
            lock_dir = home_dir / ".fastled" / "locks"
        else:
            # Project-local lock (project-specific resource like build cache)
            project_root = Path(__file__).parent.parent.parent
            lock_dir = project_root / ".build" / "locks"

        lock_dir.mkdir(parents=True, exist_ok=True)

        self.lock_file = lock_dir / f"{lock_name}.lock"
        self._lock: Optional[fasteners.InterProcessLock] = None
        self._is_acquired = False

    def _check_stale_lock(self) -> bool:
        """Check if lock file is stale (no process holding it) and remove if stale.

        Returns:
            True if lock was stale and removed, False otherwise
        """
        from ci.util.lock_handler import (
            find_processes_locking_path,
            is_psutil_available,
        )

        if not self.lock_file.exists():
            return False

        if not is_psutil_available():
            return False  # Can't check, assume not stale

        try:
            # Check if any process has the lock file open
            locking_pids = find_processes_locking_path(self.lock_file)

            if not locking_pids:
                # No process holds the lock, it's stale
                print(f"Detected stale lock at {self.lock_file}. Removing...")
                try:
                    self.lock_file.unlink()
                    print(f"Removed stale lock file: {self.lock_file}")
                    return True
                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise
                except Exception as e:
                    print(f"Warning: Could not remove stale lock file: {e}")
                    return False
            return False
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Warning: Could not check for stale lock: {e}")
            return False

    def acquire(self, timeout: float = 300.0) -> bool:
        """
        Acquire the lock, waiting up to timeout seconds.

        Args:
            timeout: Maximum time to wait for lock (seconds)

        Returns:
            True if lock was acquired, False if timeout occurred

        Raises:
            KeyboardInterrupt: If user interrupts during lock acquisition
        """
        from ci.util.global_interrupt_handler import is_interrupted

        if self._is_acquired:
            return True  # Already acquired

        # Create InterProcessLock instance
        self._lock = fasteners.InterProcessLock(str(self.lock_file))

        start_time = time.time()
        warning_shown = False
        last_stale_check = 0.0

        while True:
            # Check for keyboard interrupt
            if is_interrupted():
                print("\nKeyboardInterrupt: Aborting lock acquisition")
                raise KeyboardInterrupt()

            elapsed = time.time() - start_time

            # Check for stale lock periodically (every ~1 second)
            if elapsed - last_stale_check >= 1.0:
                if self._check_stale_lock():
                    # Stale lock was removed, try to acquire immediately
                    print("Stale lock removed, retrying acquisition...")
                last_stale_check = elapsed

            # Try to acquire with very short timeout (non-blocking)
            try:
                success = self._lock.acquire(blocking=True, timeout=0.1)
                if success:
                    self._is_acquired = True
                    return True
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
                raise  # MANDATORY: Always re-raise KeyboardInterrupt
            except Exception:
                # Handle timeout or other exceptions as failed acquisition (continue loop)
                pass  # Continue the loop to check elapsed time and try again

            # Check if we should show warning (after 2 seconds)
            if not warning_shown and elapsed >= 2.0:
                yellow = "\033[33m"
                reset = "\033[0m"
                print(f"{yellow}Waiting to acquire lock at {self.lock_file}...{reset}")
                warning_shown = True

            # Check for timeout
            if elapsed >= timeout:
                return False

            # Small sleep to prevent excessive CPU usage while allowing interrupts
            time.sleep(0.1)

    def release(self) -> None:
        """Release the lock."""
        if not self._is_acquired:
            return  # Not acquired

        if self._lock is not None:
            try:
                self._lock.release()
                self._is_acquired = False
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception:
                pass
            finally:
                self._lock = None

    def __enter__(self) -> "BuildLock":
        """Context manager entry."""
        if not self.acquire():
            raise TimeoutError("Failed to acquire build lock")
        return self

    def __exit__(
        self,
        exc_type: Optional[type[BaseException]],
        exc_val: Optional[BaseException],
        exc_tb: Optional[TracebackType],
    ) -> None:
        """Context manager exit."""
        self.release()

    def __del__(self) -> None:
        """Ensure lock is released on deletion."""
        self.release()


@contextmanager
def libfastled_build_lock(timeout: float = 300.0) -> Generator[BuildLock, None, None]:
    """
    Context manager for acquiring libfastled build lock.

    Usage:
        with libfastled_build_lock():
            # Build libfastled.a here
            pass

    Args:
        timeout: Maximum time to wait for lock (seconds)

    Yields:
        BuildLock instance

    Raises:
        TimeoutError: If lock cannot be acquired within timeout
    """
    from ci.util.timestamp_print import ts_print

    lock = BuildLock("libfastled_build")

    ts_print("🔒 Acquiring libfastled build lock...")
    if not lock.acquire(timeout=timeout):
        raise TimeoutError(
            f"Failed to acquire libfastled build lock after {timeout} seconds"
        )

    try:
        ts_print("🔒 Lock acquired, proceeding with build")
        yield lock
    finally:
        ts_print("[BUILD_LOCK] Releasing libfastled build lock")
        lock.release()


# Example usage
if __name__ == "__main__":
    import argparse
    import sys

    parser = argparse.ArgumentParser(description="Test build lock mechanism")
    parser.add_argument(
        "--duration",
        type=float,
        default=5.0,
        help="How long to hold the lock (seconds)",
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="Maximum time to wait for lock (seconds)",
    )

    args = parser.parse_args()

    print(f"Attempting to acquire lock (timeout={args.timeout}s)...")

    try:
        with libfastled_build_lock(timeout=args.timeout):
            print(f"Lock acquired! Holding for {args.duration} seconds...")
            time.sleep(args.duration)
            print("Releasing lock...")
    except TimeoutError as e:
        print(f"Failed to acquire lock: {e}")
        sys.exit(1)

    print("Done!")
