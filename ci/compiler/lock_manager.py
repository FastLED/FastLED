from ci.util.global_interrupt_handler import handle_keyboard_interrupt


"""Per-platform build lock for FastLED board builds."""

import os
import platform
import time
import warnings
from pathlib import Path
from typing import Optional

from ci.util.global_interrupt_handler import is_interrupted
from ci.util.lock_database import LockDatabase


class PlatformLock:
    """Platform-specific build lock."""

    def __init__(self, lock_file: Path) -> None:
        self.lock_file_path = lock_file
        # Use global lock DB for platform locks
        home_dir = Path.home()
        db_path = home_dir / ".fastled" / "locks.db"
        self._db = LockDatabase(db_path)
        self._lock_name = f"platform:{lock_file}"
        self.is_locked = False

    def _check_stale_lock(self) -> bool:
        """Check if lock is stale (dead process) and remove if stale.

        Returns:
            True if lock was stale and removed, False otherwise
        """
        try:
            if self._db.is_lock_stale(self._lock_name):
                print(
                    f"Detected stale platform lock at {self.lock_file_path}. Removing..."
                )
                if self._db.break_stale_lock(self._lock_name):
                    print(f"Removed stale lock file: {self.lock_file_path}")
                    return True
            return False
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            print(f"Warning: Could not check for stale lock: {e}")
            return False

    def acquire(self) -> None:
        """Acquire the platform lock."""
        if self.is_locked:
            return  # Already acquired

        my_pid = os.getpid()
        hostname = platform.node()
        start_time = time.time()
        warning_shown = False
        last_stale_check = 0.0

        while True:
            # Check for keyboard interrupt
            if is_interrupted():
                print("\nKeyboardInterrupt: Aborting platform lock acquisition")
                raise KeyboardInterrupt()

            elapsed = time.time() - start_time

            # Check for stale lock periodically (every ~1 second)
            if elapsed - last_stale_check >= 1.0:
                if self._check_stale_lock():
                    print("Stale platform lock removed, retrying acquisition...")
                last_stale_check = elapsed

            # Try to acquire
            try:
                success = self._db.try_acquire(
                    self._lock_name,
                    my_pid,
                    hostname,
                    f"platform:{self.lock_file_path}",
                    mode="write",
                )
                if success:
                    self.is_locked = True
                    print(f"Acquired platform lock: {self.lock_file_path}")
                    return
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except Exception:
                pass  # Continue the loop

            # Check if we should show warning (after 1 second)
            if not warning_shown and elapsed >= 1.0:
                yellow = "\033[33m"
                reset = "\033[0m"
                print(
                    f"{yellow}Waiting to acquire platform lock at {self.lock_file_path}{reset}"
                )
                warning_shown = True

            # Check for timeout (after 5 seconds)
            if elapsed >= 5.0:
                raise TimeoutError(
                    f"Failed to acquire platform lock within 5 seconds. "
                    f"Lock: {self.lock_file_path}. "
                    f"This may indicate another process is holding the lock or a deadlock occurred."
                )

            time.sleep(0.1)

    def release(self) -> None:
        """Release the platform lock."""
        if self.is_locked:
            try:
                self._db.release(self._lock_name, os.getpid())
                self.is_locked = False
                print(f"Released platform lock: {self.lock_file_path}")
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except Exception as e:
                print(f"Warning: Failed to release platform lock: {e}")

    def __enter__(self) -> "PlatformLock":
        """Context manager entry - acquire the lock."""
        self.acquire()
        return self

    def __exit__(
        self,
        exc_type: Optional[type[BaseException]],
        exc_val: Optional[BaseException],
        exc_tb: Optional[object],
    ) -> None:
        """Context manager exit - release the lock."""
        self.release()

    def __del__(self) -> None:
        """Ensure lock is released on deletion."""
        if self.is_locked:
            try:
                self._db.release(self._lock_name, os.getpid())
                self.is_locked = False
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except Exception:
                pass
