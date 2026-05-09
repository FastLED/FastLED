#!/usr/bin/env python3
"""
Tests for BuildLock alive-but-wedged lock stealing.

Companion to test_stale_lock_recovery.py — those tests cover dead-process
stale locks. This file covers the case where the holding PID is alive but
has held the lock past _max_hold_seconds() (wedged process not killed by
its own watchdog). Without the steal, every subsequent build blocks for
LOCK_TIMEOUT_S (120s) waiting for a process that will never release.

See https://github.com/FastLED/FastLED/issues/2430
"""

import os
import tempfile
import time
import unittest
from pathlib import Path

import pytest

from ci.util.build_lock import BuildLock
from ci.util.lock_database import LockDatabase


class TestBuildLockWedgedSteal(unittest.TestCase):
    """Verify alive-but-wedged locks are stolen past max-hold threshold."""

    def setUp(self) -> None:
        self.temp_dir = Path(tempfile.mkdtemp())
        self.db_path = self.temp_dir / "test_locks.db"
        # Tight threshold so we don't have to wait an hour.
        self._prior_max_hold = os.environ.get("FASTLED_LOCK_MAX_HOLD_S")
        os.environ["FASTLED_LOCK_MAX_HOLD_S"] = "1"

    def tearDown(self) -> None:
        import shutil

        if self._prior_max_hold is None:
            os.environ.pop("FASTLED_LOCK_MAX_HOLD_S", None)
        else:
            os.environ["FASTLED_LOCK_MAX_HOLD_S"] = self._prior_max_hold
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def _make_lock_using_test_db(self, name: str = "libfastled_build") -> BuildLock:
        """Build a BuildLock pointing at our temp test DB.

        BuildLock constructs its DB path from project_root in __init__, so we
        swap in a test DB after construction to keep the test hermetic.
        """
        lock = BuildLock(name)
        lock._db = LockDatabase(self.db_path)
        return lock

    @pytest.mark.serial
    def test_wedged_alive_lock_is_stolen(self) -> None:
        """Lock held by an alive process past max-hold should be stolen."""
        lock = self._make_lock_using_test_db()

        # Insert a lock held by THIS process (alive) with an old acquired_at.
        my_pid = os.getpid()
        old_time = time.time() - 10.0  # 10s ago, threshold = 1s
        conn = lock._db._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                "INSERT OR REPLACE INTO lock_holders "
                "(lock_name, owner_pid, lock_mode, operation, hostname, acquired_at) "
                "VALUES (?, ?, 'write', 'test_wedged', 'localhost', ?)",
                ("build:libfastled_build", my_pid, old_time),
            )
            conn.commit()
        finally:
            conn.close()

        # Sanity: holder is still in DB.
        holders = lock._db.get_lock_info("build:libfastled_build")
        self.assertEqual(len(holders), 1, "Test setup: holder should be present")
        self.assertEqual(holders[0]["owner_pid"], my_pid)

        # _check_stale_lock should detect wedged holder and steal it.
        stolen = lock._check_stale_lock()
        self.assertTrue(stolen, "Wedged lock past max-hold should be stolen")

        # Holder row should be gone.
        holders_after = lock._db.get_lock_info("build:libfastled_build")
        self.assertEqual(
            len(holders_after), 0, "Wedged holder should be removed from DB"
        )

    @pytest.mark.serial
    def test_alive_recent_lock_is_not_stolen(self) -> None:
        """Lock held by an alive process within max-hold should NOT be stolen."""
        os.environ["FASTLED_LOCK_MAX_HOLD_S"] = "3600"
        lock = self._make_lock_using_test_db()

        my_pid = os.getpid()
        recent_time = time.time() - 2.0  # 2s ago, well under 3600s
        conn = lock._db._get_connection()
        try:
            cursor = conn.cursor()
            cursor.execute(
                "INSERT OR REPLACE INTO lock_holders "
                "(lock_name, owner_pid, lock_mode, operation, hostname, acquired_at) "
                "VALUES (?, ?, 'write', 'test_recent', 'localhost', ?)",
                ("build:libfastled_build", my_pid, recent_time),
            )
            conn.commit()
        finally:
            conn.close()

        stolen = lock._check_stale_lock()
        self.assertFalse(stolen, "Recent live lock should NOT be stolen")

        holders = lock._db.get_lock_info("build:libfastled_build")
        self.assertEqual(len(holders), 1, "Recent live holder should remain in DB")


if __name__ == "__main__":
    unittest.main()
