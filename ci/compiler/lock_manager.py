from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


"""
Lock management for FastLED PlatformIO builds.

⚠️  DEPRECATION WARNING:
   GlobalPackageLock is deprecated and will be removed in a future release.
   Use the daemon-based package installation system instead:

   from ci.util.pio_package_client import ensure_packages_installed

   See ci/debug_attached.py for usage example.
"""

import time
import warnings
from pathlib import Path
from typing import Optional

import fasteners

from ci.compiler.path_manager import FastLEDPaths
from ci.util.global_interrupt_handler import is_interrupted


class GlobalPackageLock:
    """A process lock for global package installation per board. Acquired once during first build and released after completion."""

    def __init__(self, platform_name: str) -> None:
        self.platform_name = platform_name

        # Use centralized path management
        self.paths = FastLEDPaths(platform_name)
        self.lock_file = self.paths.global_package_lock_file
        self._file_lock = fasteners.InterProcessLock(str(self.lock_file))
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
                print(
                    f"Detected stale lock file for {self.platform_name} at {self.lock_file}. Removing..."
                )
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

    def acquire(self) -> None:
        """Acquire the global package installation lock for this board."""
        if self._is_acquired:
            return  # Already acquired

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
                    # Stale lock was removed, try to acquire immediately
                    print("Stale lock removed, retrying acquisition...")
                last_stale_check = elapsed

            # Try to acquire with very short timeout (non-blocking)
            try:
                success = self._file_lock.acquire(blocking=True, timeout=0.1)
                if success:
                    self._is_acquired = True
                    print(
                        f"Acquired global package lock for platform {self.platform_name}"
                    )
                    return
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise  # MANDATORY: Always re-raise KeyboardInterrupt
            except Exception:
                # Handle timeout or other exceptions as failed acquisition (continue loop)
                pass  # Continue the loop to check elapsed time and try again

            # Check if we should show warning (after 1 second)
            if not warning_shown and elapsed >= 1.0:
                yellow = "\033[33m"
                reset = "\033[0m"
                print(
                    f"{yellow}Platform {self.platform_name} is waiting to acquire global package lock at {self.lock_file.parent}{reset}"
                )
                warning_shown = True

            # Check for timeout (after 5 seconds)
            if elapsed >= 5.0:
                raise TimeoutError(
                    f"Failed to acquire global package lock for platform {self.platform_name} within 5 seconds. "
                    f"Lock file: {self.lock_file}. "
                    f"This may indicate another process is installing packages or a deadlock occurred."
                )

            # Small sleep to prevent excessive CPU usage while allowing interrupts
            time.sleep(0.1)

    def release(self) -> None:
        """Release the global package installation lock."""
        if not self._is_acquired:
            return  # Not acquired

        try:
            self._file_lock.release()
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
            pass  # Ignore errors during cleanup


class PlatformLock:
    """Platform-specific build lock."""

    def __init__(self, lock_file: Path) -> None:
        self.lock_file_path = lock_file
        self.lock = fasteners.InterProcessLock(str(self.lock_file_path))
        self.is_locked = False

    def _check_stale_lock(self) -> bool:
        """Check if lock file is stale (no process holding it) and remove if stale.

        Returns:
            True if lock was stale and removed, False otherwise
        """
        from ci.util.lock_handler import (
            find_processes_locking_path,
            is_psutil_available,
        )

        if not self.lock_file_path.exists():
            return False

        if not is_psutil_available():
            return False  # Can't check, assume not stale

        try:
            # Check if any process has the lock file open
            locking_pids = find_processes_locking_path(self.lock_file_path)

            if not locking_pids:
                # No process holds the lock, it's stale
                print(
                    f"Detected stale platform lock file at {self.lock_file_path}. Removing..."
                )
                try:
                    self.lock_file_path.unlink()
                    print(f"Removed stale lock file: {self.lock_file_path}")
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

    def acquire(self) -> None:
        """Acquire the platform lock."""
        if self.is_locked:
            return  # Already acquired

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
                    # Stale lock was removed, try to acquire immediately
                    print("Stale platform lock removed, retrying acquisition...")
                last_stale_check = elapsed

            # Try to acquire with very short timeout (non-blocking)
            try:
                success = self.lock.acquire(blocking=True, timeout=0.1)
                if success:
                    self.is_locked = True
                    print(f"Acquired platform lock: {self.lock_file_path}")
                    return
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
                raise  # MANDATORY: Always re-raise KeyboardInterrupt
            except Exception:
                # Handle timeout or other exceptions as failed acquisition (continue loop)
                pass  # Continue the loop to check elapsed time and try again

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
                    f"Lock file: {self.lock_file_path}. "
                    f"This may indicate another process is holding the lock or a deadlock occurred."
                )

            # Small sleep to prevent excessive CPU usage while allowing interrupts
            time.sleep(0.1)

    def release(self) -> None:
        """Release the platform lock."""
        if self.is_locked:
            try:
                self.lock.release()
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
                self.lock.release()
                self.is_locked = False
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception:
                pass  # Ignore errors during cleanup
