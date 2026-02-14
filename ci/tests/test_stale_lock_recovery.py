#!/usr/bin/env python3
"""
Stale Lock Recovery Test Suite

Tests for verifying stale lock detection and recovery mechanisms.
Demonstrates the issue of stale locks from force-killed processes
and validates recovery solutions.

Test Scenarios:
1. Force-kill creates stale lock (PID metadata remains but process is dead)
2. Auto-recovery on timeout (FileLock breaks stale lock and retries)
3. Manual recovery via break_stale_lock()
4. CLI tool recovery via lock_admin.py
"""

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


class TestStaleLockRecovery(unittest.TestCase):
    """Test suite for stale lock detection and recovery."""

    def setUp(self):
        """Create temporary directory for test locks."""
        self.temp_dir = Path(tempfile.mkdtemp())
        self.lock_path = self.temp_dir / "test.lock"

    def tearDown(self):
        """Clean up test files and any surviving child processes."""
        # Clean up lock files
        if self.lock_path.exists():
            try:
                self.lock_path.unlink()
            except OSError:
                pass

        metadata_file = self.lock_path.with_suffix(".lock.pid")
        if metadata_file.exists():
            try:
                metadata_file.unlink()
            except OSError:
                pass

        # Clean up temp directory
        try:
            self.temp_dir.rmdir()
        except OSError:
            pass

    @pytest.mark.serial  # Can't run in parallel
    def test_stale_lock_detection_with_fake_pid(self):
        """Simulated stale lock with non-existent PID should be detected."""
        import json

        # Manually create a stale lock file with a PID that doesn't exist
        # Use a very high PID that's unlikely to exist (99999999)
        fake_pid = 99999999

        # Verify the fake PID doesn't actually exist
        self.assertFalse(
            is_process_alive(fake_pid),
            f"Fake PID {fake_pid} should not exist before test",
        )

        # Create lock file
        self.lock_path.touch()

        # Create metadata file with fake PID
        metadata_file = self.lock_path.with_suffix(".lock.pid")
        metadata = {
            "pid": fake_pid,
            "timestamp": time.time(),
            "operation": "test_simulated_stale",
            "hostname": "test_host",
        }
        with open(metadata_file, "w") as f:
            json.dump(metadata, f)

        # Verify lock file and metadata exist
        self.assertTrue(self.lock_path.exists(), "Lock file should exist")
        self.assertTrue(metadata_file.exists(), "Metadata file should exist")

        # Verify lock is detected as stale
        self.assertTrue(
            is_lock_stale(self.lock_path),
            f"Lock with non-existent PID {fake_pid} should be stale",
        )

        # Verify can be broken
        result = break_stale_lock(self.lock_path)
        self.assertTrue(result, "Should successfully break stale lock with fake PID")

        # Verify files are removed
        self.assertFalse(
            self.lock_path.exists(), "Lock file should be removed after breaking"
        )
        self.assertFalse(
            metadata_file.exists(), "Metadata file should be removed after breaking"
        )

    @pytest.mark.serial  # Can't run in parallel
    def test_auto_recovery_on_timeout(self):
        """FileLock should auto-break stale lock on timeout and retry."""
        import json

        # Create a simulated stale lock (fake PID)
        fake_pid = 99999998

        # Create lock file
        self.lock_path.touch()

        # Create metadata file with fake PID
        metadata_file = self.lock_path.with_suffix(".lock.pid")
        metadata = {
            "pid": fake_pid,
            "timestamp": time.time(),
            "operation": "test_auto_recovery",
            "hostname": "test_host",
        }
        with open(metadata_file, "w") as f:
            json.dump(metadata, f)

        # Verify lock is stale
        self.assertTrue(self.lock_path.exists(), "Lock file should exist")
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

        # Verify lock was broken and re-acquired (should be fast, < 2 seconds)
        duration = time.time() - start_time
        self.assertLess(
            duration,
            2.0,
            f"Auto-recovery took too long ({duration:.2f}s), should be < 2s",
        )

    @pytest.mark.serial  # Can't run in parallel
    def test_manual_recovery(self):
        """break_stale_lock() should successfully remove stale locks."""
        import json

        # Create a simulated stale lock (fake PID)
        fake_pid = 99999997

        # Create lock file
        self.lock_path.touch()

        # Create metadata file with fake PID
        metadata_file = self.lock_path.with_suffix(".lock.pid")
        metadata = {
            "pid": fake_pid,
            "timestamp": time.time(),
            "operation": "test_manual_recovery",
            "hostname": "test_host",
        }
        with open(metadata_file, "w") as f:
            json.dump(metadata, f)

        # Verify lock is stale
        self.assertTrue(self.lock_path.exists(), "Lock file should exist")
        self.assertTrue(is_lock_stale(self.lock_path), "Lock should be stale")

        # Manually break stale lock
        result = break_stale_lock(self.lock_path)
        self.assertTrue(result, "break_stale_lock() should return True for stale locks")

        # Verify lock and metadata are removed
        self.assertFalse(
            self.lock_path.exists(), "Lock file should be removed after breaking"
        )

        self.assertFalse(
            metadata_file.exists(), "Metadata file should be removed after breaking"
        )

    @pytest.mark.serial  # Can't run in parallel
    def test_break_stale_lock_refuses_active_lock(self):
        """break_stale_lock() should refuse to break active locks."""
        # Create an active lock (held by this process)
        with FileLock(self.lock_path, operation="test_refuse_active"):
            # Verify lock exists
            self.assertTrue(self.lock_path.exists(), "Lock file should exist")

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

            # Verify lock still exists (not broken)
            self.assertTrue(
                self.lock_path.exists(),
                "Lock file should still exist (not broken)",
            )

    @pytest.mark.serial  # Can't run in parallel
    def test_age_based_fallback_for_foreign_locks(self):
        """Locks without metadata should use age-based heuristic (30 minutes)."""
        # Create a lock file without metadata (simulate foreign lock)
        self.lock_path.touch()

        # Set mtime to 31 minutes ago (stale by age heuristic)
        import os

        old_time = time.time() - (31 * 60)  # 31 minutes ago
        os.utime(self.lock_path, (old_time, old_time))

        # Verify lock is detected as stale (age-based fallback)
        self.assertTrue(
            is_lock_stale(self.lock_path),
            "Lock older than 30 minutes without metadata should be stale",
        )

        # Verify can be broken
        result = break_stale_lock(self.lock_path)
        self.assertTrue(result, "Should successfully break stale lock without metadata")

    @pytest.mark.serial  # Can't run in parallel
    def test_age_based_fallback_for_recent_foreign_locks(self):
        """Recent locks without metadata should NOT be considered stale."""
        # Create a recent lock file without metadata
        self.lock_path.touch()

        # Verify lock is NOT stale (too recent, < 30 minutes)
        self.assertFalse(
            is_lock_stale(self.lock_path),
            "Recent lock without metadata should NOT be stale",
        )

        # Verify break_stale_lock refuses to break it
        result = break_stale_lock(self.lock_path)
        self.assertFalse(result, "Should refuse to break recent lock without metadata")


if __name__ == "__main__":
    unittest.main()
