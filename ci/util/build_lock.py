#!/usr/bin/env python3
"""
Build lock utility for coordinating concurrent builds of libfastled.a

This module provides file-based locking to prevent concurrent builds
of libfastled.a from unit tests and examples from conflicting.

Uses the 'fasteners' library for cross-platform file locking.
"""

import time
from contextlib import contextmanager
from pathlib import Path
from typing import Generator, Optional

import fasteners


class BuildLock:
    """
    File-based lock for coordinating builds using the fasteners library.

    Provides cross-platform file locking via the fasteners InterProcessLock.
    """

    def __init__(self, lock_name: str = "libfastled_build"):
        """
        Initialize build lock.

        Args:
            lock_name: Name of the lock (creates .lock file with this name)
        """
        # Create lock directory in project root
        project_root = Path(__file__).parent.parent.parent
        lock_dir = project_root / ".build" / "locks"
        lock_dir.mkdir(parents=True, exist_ok=True)

        self.lock_file = lock_dir / f"{lock_name}.lock"
        self._lock: Optional[fasteners.InterProcessLock] = None

    def acquire(self, timeout: float = 300.0) -> bool:
        """
        Acquire the lock, waiting up to timeout seconds.

        Args:
            timeout: Maximum time to wait for lock (seconds)

        Returns:
            True if lock was acquired, False if timeout occurred
        """
        # Create InterProcessLock instance
        self._lock = fasteners.InterProcessLock(str(self.lock_file))

        # Try to acquire with timeout
        return self._lock.acquire(blocking=True, timeout=timeout)

    def release(self) -> None:
        """Release the lock."""
        if self._lock is not None:
            try:
                self._lock.release()
            except Exception:
                pass
            finally:
                self._lock = None

    def __enter__(self) -> "BuildLock":
        """Context manager entry."""
        if not self.acquire():
            raise TimeoutError("Failed to acquire build lock")
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:
        """Context manager exit."""
        self.release()

    def __del__(self) -> None:
        """Ensure lock is released on deletion."""
        self.release()


@contextmanager
def libfastled_build_lock(
    timeout: float = 300.0
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
    lock = BuildLock("libfastled_build")

    print("[BUILD_LOCK] Acquiring libfastled build lock...")
    if not lock.acquire(timeout=timeout):
        raise TimeoutError(
            f"Failed to acquire libfastled build lock after {timeout} seconds"
        )

    try:
        print("[BUILD_LOCK] Lock acquired, proceeding with build")
        yield lock
    finally:
        print("[BUILD_LOCK] Releasing libfastled build lock")
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
        help="How long to hold the lock (seconds)"
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=10.0,
        help="Maximum time to wait for lock (seconds)"
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
