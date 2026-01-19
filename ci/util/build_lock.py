from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Build lock utility for coordinating concurrent build operations.

This module provides file-based locking to prevent concurrent build operations
from conflicting. Used for project-local locks (e.g., libfastled_build).

Note: Device locking is now handled by fbuild (daemon-based).

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
            # Global lock (system-wide)
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

    _stale_lock_warned: bool = False

    def _check_stale_lock(self) -> bool:
        """Check if lock file is stale (no process holding it) and remove if stale.

        Uses a fast approach: skip expensive process scanning and instead rely on
        file age heuristics. Locks older than 10 minutes with no activity are
        considered stale.

        Returns:
            True if lock was stale and removed, False otherwise
        """
        if not self.lock_file.exists():
            return False

        try:
            # Check lock file age - if older than 10 minutes, consider it stale
            lock_mtime = self.lock_file.stat().st_mtime
            lock_age = time.time() - lock_mtime
            max_lock_age = 600  # 10 minutes

            if lock_age > max_lock_age:
                if not BuildLock._stale_lock_warned:
                    BuildLock._stale_lock_warned = True
                    print(
                        f"Detected stale lock at {self.lock_file} (age: {lock_age:.0f}s). Attempting removal..."
                    )
                try:
                    self.lock_file.unlink()
                    print(f"Removed stale lock file: {self.lock_file}")
                    BuildLock._stale_lock_warned = False  # Reset for next time
                    return True
                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise
                except Exception:
                    # Silently fail - the lock is held by another process
                    # which will release it when done
                    return False
            return False
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception:
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
    import time as _time

    from ci.util.timestamp_print import ts_print

    lock = BuildLock("libfastled_build")

    # Only show lock messages when there's contention (acquisition takes > 1s)
    # This reduces output noise during normal operation
    lock_start = _time.time()
    if not lock.acquire(timeout=timeout):
        raise TimeoutError(
            f"Failed to acquire libfastled build lock after {timeout} seconds"
        )
    lock_duration = _time.time() - lock_start

    try:
        if lock_duration > 1.0:
            ts_print(f"🔒 Lock acquired after {lock_duration:.1f}s wait")
        yield lock
    finally:
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
