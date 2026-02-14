from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""
Lock management for FastLED PlatformIO builds.

⚠️  DEPRECATION WARNING:
   GlobalPackageLock is deprecated and will be removed in a future release.
   Use the daemon-based package installation system instead:

   from ci.util.pio_package_client import ensure_packages_installed

   See ci/debug_attached.py for usage example.
"""

import os
import platform
import time
import warnings
from pathlib import Path
from typing import Optional

from ci.compiler.path_manager import FastLEDPaths
from ci.util.global_interrupt_handler import is_interrupted
from ci.util.lock_database import LockDatabase


class GlobalPackageLock:
    """A process lock for global package installation per board. Acquired once during first build and released after completion."""

    def __init__(self, platform_name: str) -> None:
        self.platform_name = platform_name

        # Use centralized path management
        self.paths = FastLEDPaths(platform_name)
        # Use global lock DB (~/.fastled/locks.db)
        home_dir = Path.home()
        db_path = home_dir / ".fastled" / "locks.db"
        self._db = LockDatabase(db_path)
        self._lock_name = f"global_package:{platform_name}"
        # Keep lock_file for backward compatibility
        self.lock_file = self.paths.global_package_lock_file
        self._is_acquired = False

    def _check_stale_lock(self) -> bool:
        """Check if lock is stale (dead process) and remove if stale.

        Returns:
            True if lock was stale and removed, False otherwise
        """
        try:
            if self._db.is_lock_stale(self._lock_name):
                print(f"Detected stale lock for {self.platform_name}. Removing...")
                if self._db.break_stale_lock(self._lock_name):
                    print(f"Removed stale lock for {self.platform_name}")
                    return True
            return False
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            print(f"Warning: Could not check for stale lock: {e}")
            return False

    def acquire(self) -> None:
        """Acquire the global package installation lock for this board."""
        if self._is_acquired:
            return  # Already acquired

        my_pid = os.getpid()
        hostname = platform.node()
        start_time = time.time()
        warning_shown = False
        last_stale_check = 0.0

        while True:
            # Check for keyboard interrupt
            if is_interrupted():
                print(
                    f"\nKeyboardInterrupt: Aborting lock acquisition for platform {self.platform_name}"
                )
                raise KeyboardInterrupt()

            elapsed = time.time() - start_time

            # Check for stale lock periodically (every ~1 second)
            if elapsed - last_stale_check >= 1.0:
                if self._check_stale_lock():
                    print("Stale lock removed, retrying acquisition...")
                last_stale_check = elapsed

            # Try to acquire
            try:
                success = self._db.try_acquire(
                    self._lock_name,
                    my_pid,
                    hostname,
                    f"global_package:{self.platform_name}",
                    mode="write",
                )
                if success:
                    self._is_acquired = True
                    print(
                        f"Acquired global package lock for platform {self.platform_name}"
                    )
                    return
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception:
                pass  # Continue the loop

            # Check if we should show warning (after 1 second)
            if not warning_shown and elapsed >= 1.0:
                yellow = "\033[33m"
                reset = "\033[0m"
                print(
                    f"{yellow}Platform {self.platform_name} is waiting to acquire global package lock{reset}"
                )
                warning_shown = True

            # Check for timeout (after 5 seconds)
            if elapsed >= 5.0:
                raise TimeoutError(
                    f"Failed to acquire global package lock for platform {self.platform_name} within 5 seconds. "
                    f"This may indicate another process is installing packages or a deadlock occurred."
                )

            time.sleep(0.1)

    def release(self) -> None:
        """Release the global package installation lock."""
        if not self._is_acquired:
            return

        try:
            self._db.release(self._lock_name, os.getpid())
            self._is_acquired = False
            print(f"Released global package lock for platform {self.platform_name}")
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            warnings.warn(
                f"Failed to release global package lock for {self.platform_name}: {e}"
            )

    def is_acquired(self) -> bool:
        """Check if the lock is currently acquired."""
        return self._is_acquired

    def __enter__(self) -> "GlobalPackageLock":
        """Context manager entry - acquire the lock."""
        self.acquire()
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc_val: BaseException | None,
        exc_tb: object,
    ) -> None:
        """Context manager exit - release the lock."""
        self.release()

    def __del__(self) -> None:
        """Ensure lock is released when object is garbage collected."""
        try:
            self.release()
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception:
            pass


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
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
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
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
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
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
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
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception:
                pass
