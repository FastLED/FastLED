from ci.util.global_interrupt_handler import handle_keyboard_interrupt


"""
Build lock utility for coordinating concurrent build operations.

This module provides SQLite-backed locking to prevent concurrent build operations
from conflicting. Used for project-local locks (e.g., libfastled_build).

Note: Device locking is now handled by fbuild (daemon-based).

Uses a centralized SQLite database with PID-based stale lock detection for robust recovery.
"""

import os
import platform
import time
from contextlib import contextmanager
from pathlib import Path
from types import TracebackType
from typing import Generator, Optional

from ci.util.lock_database import LockDatabase


# Default timeout for lock acquisition (seconds).
# After this period of failing to acquire, diagnostics are printed and the caller exits.
LOCK_TIMEOUT_S: float = 120.0

# Grace period (seconds) before printing diagnostics and failing.
# Allows brief lock contention (e.g., concurrent build finishing) to resolve.
_LOCK_GRACE_S: float = 2.0


def _is_process_alive_psutil(pid: int) -> bool:
    """Check if a process is alive using psutil (more reliable than kernel32 on Windows)."""
    try:
        import psutil

        return psutil.pid_exists(pid)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        # Fallback to the existing kernel32/os.kill checker
        from ci.util.file_lock_rw_util import is_process_alive

        return is_process_alive(pid)


def _get_process_info(pid: int) -> str:
    """Get process name, exe path, and cwd via psutil. Returns descriptive string."""
    try:
        import psutil

        proc = psutil.Process(pid)
        name = proc.name()
        exe = proc.exe()
        try:
            cwd = proc.cwd()
        except (psutil.AccessDenied, OSError):
            cwd = "<access denied>"
        cmdline = " ".join(proc.cmdline()[:4])
        return f"name={name}, exe={exe}, cwd={cwd}, cmd={cmdline}"
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        return f"<unable to query process: {e}>"


def _dump_blocking_stacks(lock_name: str, elapsed: float) -> None:
    """Dump thread stacks when blocked on lock acquisition."""
    try:
        from ci.util.test_env import dump_thread_stacks

        yellow = "\033[33m"
        reset = "\033[0m"
        print(
            f"{yellow}[LOCK] Blocked on '{lock_name}' for {elapsed:.0f}s - dumping stacks:{reset}"
        )
        dump_thread_stacks()
    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt

        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        pass  # Best-effort diagnostic


class BuildLock:
    """
    SQLite-backed lock for coordinating operations with PID tracking.

    Provides cross-platform locking with PID-based stale lock detection.
    Automatically recovers from crashed processes via stale lock breaking.
    Supports both project-local locks (.cache/locks.db) and system-wide global
    locks (~/.fastled/locks.db) depending on the use case.
    """

    def __init__(self, lock_name: str = "libfastled_build", use_global: bool = False):
        """
        Initialize build lock.

        Args:
            lock_name: Name of the lock
            use_global: If True, use ~/.fastled/locks.db (system-wide). If False, use .cache/locks.db (project-local)
        """
        if use_global:
            home_dir = Path.home()
            db_path = home_dir / ".fastled" / "locks.db"
        else:
            project_root = Path(__file__).parent.parent.parent
            db_path = project_root / ".cache" / "locks.db"

        self._db = LockDatabase(db_path)
        self._lock_name = f"build:{lock_name}"
        self.lock_name = lock_name
        # Keep lock_file attribute for backward compatibility (logging/display)
        self.lock_file = db_path
        self._is_acquired = False

    _stale_lock_warned: bool = False

    def _check_stale_lock(self) -> bool:
        """Check if lock is stale (dead process) and remove if stale.

        Uses psutil for process liveness checks (more reliable than kernel32 on Windows).

        Returns:
            True if lock was stale and removed, False otherwise
        """
        try:
            holders = self._db.get_lock_info(self._lock_name)
            if not holders:
                return False

            # Check all holders with psutil (more reliable than kernel32)
            all_dead = all(
                not _is_process_alive_psutil(h["owner_pid"]) for h in holders
            )
            if not all_dead:
                return False

            if not BuildLock._stale_lock_warned:
                BuildLock._stale_lock_warned = True
                dead_pids = [h["owner_pid"] for h in holders]
                print(
                    f"Detected stale lock '{self._lock_name}' (dead PIDs: {dead_pids}). Removing..."
                )

            # Force-break since we already verified all holders are dead
            if self._db.force_break(self._lock_name):
                print(f"Removed stale lock: {self._lock_name}")
                BuildLock._stale_lock_warned = False
                return True

            return False
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception:
            return False

    def acquire(self, timeout: float = LOCK_TIMEOUT_S) -> bool:
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

        # Check for stale lock BEFORE attempting acquisition
        self._check_stale_lock()

        my_pid = os.getpid()
        hostname = platform.node()
        start_time = time.time()
        warning_shown = False
        last_stale_check = 0.0

        while True:
            if is_interrupted():
                print("\nKeyboardInterrupt: Aborting lock acquisition")
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
                    f"build:{self.lock_name}",
                    mode="write",
                )
                if success:
                    self._is_acquired = True
                    return True
            except KeyboardInterrupt as ki:
                handle_keyboard_interrupt(ki)
                raise
            except Exception:
                pass  # Continue the loop

            # After grace period, print diagnostics and keep waiting
            if not warning_shown and elapsed >= _LOCK_GRACE_S:
                yellow = "\033[33m"
                reset = "\033[0m"
                print(f"{yellow}Waiting for lock '{self._lock_name}'{reset}")
                # Show who holds the lock with psutil process info
                try:
                    holders = self._db.get_lock_info(self._lock_name)
                    for h in holders:
                        held_secs = time.time() - h["acquired_at"]
                        proc_info = _get_process_info(h["owner_pid"])
                        print(
                            f"{yellow}  Held by PID {h['owner_pid']} "
                            f"(held {held_secs:.0f}s, {proc_info}){reset}"
                        )
                except KeyboardInterrupt as ki:
                    handle_keyboard_interrupt(ki)
                    raise
                except Exception:
                    pass
                _dump_blocking_stacks(self._lock_name, elapsed)
                warning_shown = True

            # Check for timeout
            if elapsed >= timeout:
                return False

            time.sleep(0.1)

    def release(self) -> None:
        """Release the lock."""
        if not self._is_acquired:
            return

        try:
            self._db.release(self._lock_name, os.getpid())
            self._is_acquired = False
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception:
            pass

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
def libfastled_build_lock(
    timeout: float = LOCK_TIMEOUT_S,
) -> Generator[BuildLock, None, None]:
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

    lock_start = _time.time()
    if not lock.acquire(timeout=timeout):
        # Include holder info with psutil process details in error message
        holder_info = ""
        try:
            holders = lock._db.get_lock_info(lock._lock_name)
            if holders:
                parts: list[str] = []
                for h in holders:
                    held_secs = _time.time() - h["acquired_at"]
                    proc_info = _get_process_info(h["owner_pid"])
                    parts.append(
                        f"PID {h['owner_pid']} (held {held_secs:.0f}s, {proc_info})"
                    )
                holder_info = f". Held by: {', '.join(parts)}"
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception:
            pass
        actual_elapsed = _time.time() - lock_start
        raise TimeoutError(
            f"Failed to acquire libfastled build lock after {actual_elapsed:.0f}s (timeout={timeout:.0f}s){holder_info}"
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
