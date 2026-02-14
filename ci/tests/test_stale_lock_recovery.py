#!/usr/bin/env python3
"""
Stale Lock Recovery Test Suite

Tests for verifying stale lock detection and recovery mechanisms.
Uses SQLite-backed lock database for lock state management.
"""

import os
import tempfile
import time
import unittest
from pathlib import Path

import pytest

from ci.util.file_lock_rw import (
    FileLock,
    break_stale_lock,
    is_lock_stale,
    is_process_alive,
)
from ci.util.lock_database import LockDatabase


class TestStaleLockRecovery(unittest.TestCase):
    """Test suite for stale lock detection and recovery."""

    def setUp(self) -> None:
        """Create temporary directory for test locks."""
        self.temp_dir = Path(tempfile.mkdtemp())
        self.lock_path = self.temp_dir / "test.lock"
        self.db_path = self.temp_dir / "test_locks.db"
        self.db = LockDatabase(self.db_path)
        os.environ["FASTLED_LOCK_DB_PATH"] = str(self.db_path)

    def tearDown(self) -> None:
        """Clean up test files."""
        import shutil

        os.environ.pop("FASTLED_LOCK_DB_PATH", None)
        from ci.util.lock_database import _db_cache

        _db_cache.clear()
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    @pytest.mark.serial
    def test_stale_lock_detection_with_fake_pid(self) -> None:
        """Simulated stale lock with non-existent PID should be detected."""
        fake_pid = 99999999

        # Verify the fake PID doesn't actually exist
        self.assertFalse(
            is_process_alive(fake_pid),
            f"Fake PID {fake_pid} should not exist before test",
        )

        # Create stale lock in DB with fake PID
        self.db.try_acquire(
            str(self.lock_path), fake_pid, "test_host", "test_simulated_stale"
        )

        # Verify lock is detected as stale
        self.assertTrue(
            is_lock_stale(self.lock_path),
            f"Lock with non-existent PID {fake_pid} should be stale",
        )

        # Verify can be broken
        result = break_stale_lock(self.lock_path)
        self.assertTrue(result, "Should successfully break stale lock with fake PID")

        # Verify lock is removed from DB
        self.assertFalse(
            self.db.is_held(str(self.lock_path)),
            "Lock should be removed from DB after breaking",
        )

    @pytest.mark.serial
    def test_auto_recovery_on_timeout(self) -> None:
        """FileLock should auto-break stale lock on timeout and retry."""
        fake_pid = 99999998

        # Create stale lock in DB
        self.db.try_acquire(
            str(self.lock_path), fake_pid, "test_host", "test_auto_recovery"
        )

        # Verify lock is stale
        self.assertTrue(
            is_lock_stale(self.lock_path), "Lock should be stale (fake PID)"
        )

        # Try to acquire lock with timeout - should succeed after breaking stale lock
        start_time = time.time()
        acquired = False
        try:
            with FileLock(
                self.lock_path, timeout=10.0, operation="test_auto_recovery_2"
            ):
                acquired = True
                duration = time.time() - start_time
                print(f"Lock acquired after {duration:.2f}s (expected < 1s)")
        except TimeoutError:
            self.fail("Failed to acquire lock - auto-recovery did not work")

        self.assertTrue(
            acquired, "Should successfully acquire lock after auto-recovery"
        )

        # Verify lock was broken and re-acquired quickly
        duration = time.time() - start_time
        self.assertLess(
            duration,
            2.0,
            f"Auto-recovery took too long ({duration:.2f}s), should be < 2s",
        )

    @pytest.mark.serial
    def test_manual_recovery(self) -> None:
        """break_stale_lock() should successfully remove stale locks."""
        fake_pid = 99999997

        # Create stale lock in DB
        self.db.try_acquire(
            str(self.lock_path), fake_pid, "test_host", "test_manual_recovery"
        )

        # Verify lock is stale
        self.assertTrue(is_lock_stale(self.lock_path), "Lock should be stale")

        # Manually break stale lock
        result = break_stale_lock(self.lock_path)
        self.assertTrue(result, "break_stale_lock() should return True for stale locks")

        # Verify lock is removed from DB
        self.assertFalse(
            self.db.is_held(str(self.lock_path)),
            "Lock should be removed from DB after breaking",
        )

    @pytest.mark.serial
    def test_break_stale_lock_refuses_active_lock(self) -> None:
        """break_stale_lock() should refuse to break active locks."""
        # Create an active lock (held by this process)
        with FileLock(self.lock_path, operation="test_refuse_active"):
            # Verify lock is NOT stale (this process is alive)
            self.assertFalse(
                is_lock_stale(self.lock_path),
                "Lock should NOT be stale (held by active process)",
            )

            # Try to break lock - should fail
            result = break_stale_lock(self.lock_path)
            self.assertFalse(
                result, "break_stale_lock() should refuse to break active locks"
            )

            # Verify lock still held
            self.assertTrue(
                self.db.is_held(str(self.lock_path)),
                "Lock should still be held in DB (not broken)",
            )


if __name__ == "__main__":
    unittest.main()
