#!/usr/bin/env python3
"""
Unit tests for LockDatabase - SQLite-backed reader-writer lock engine.

Tests validate:
- Basic write lock acquire/release
- Read lock shared access
- Write lock exclusivity
- Re-entrancy (same PID)
- Read-to-write upgrade
- Stale lock detection and breaking
- Force break
- Concurrent thread access
"""

import os
import tempfile
import threading
import unittest
from pathlib import Path

from ci.util.lock_database import LockDatabase


class TestLockDatabaseBasic(unittest.TestCase):
    """Basic lock acquisition and release tests."""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_db_created_on_init(self) -> None:
        """DB file and schema created automatically."""
        self.assertTrue(self.db_path.exists())

    def test_acquire_and_release(self) -> None:
        """Basic write lock acquire/release cycle."""
        pid = os.getpid()
        acquired = self.db.try_acquire("test_lock", pid, "localhost", "test")
        self.assertTrue(acquired)
        self.assertTrue(self.db.is_held("test_lock"))

        released = self.db.release("test_lock", pid)
        self.assertTrue(released)
        self.assertFalse(self.db.is_held("test_lock"))

    def test_release_unheld_lock(self) -> None:
        """Releasing a lock not held returns False."""
        released = self.db.release("nonexistent", os.getpid())
        self.assertFalse(released)

    def test_list_all_locks(self) -> None:
        """Returns all lock records with correct metadata."""
        pid = os.getpid()
        self.db.try_acquire("lock_a", pid, "host1", "op_a")
        self.db.try_acquire("lock_b", pid, "host1", "op_b")

        all_locks = self.db.list_all_locks()
        self.assertEqual(len(all_locks), 2)
        names = {lock["lock_name"] for lock in all_locks}
        self.assertEqual(names, {"lock_a", "lock_b"})


class TestReadLockShared(unittest.TestCase):
    """Multiple PIDs can hold read locks simultaneously."""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_read_lock_shared(self) -> None:
        """Multiple PIDs can hold read locks simultaneously."""
        pid1 = os.getpid()
        # Use a fake PID for the second reader (simulated)
        pid2 = pid1 + 99999

        acquired1 = self.db.try_acquire(
            "shared_lock", pid1, "localhost", "reader1", mode="read"
        )
        self.assertTrue(acquired1)

        acquired2 = self.db.try_acquire(
            "shared_lock", pid2, "localhost", "reader2", mode="read"
        )
        self.assertTrue(acquired2)

        # Both should be held
        holders = self.db.get_lock_info("shared_lock")
        self.assertEqual(len(holders), 2)


class TestWriteLockExclusive(unittest.TestCase):
    """Write lock blocks other PIDs."""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_write_lock_exclusive(self) -> None:
        """Write lock blocks other PIDs."""
        pid1 = os.getpid()
        pid2 = pid1 + 99999

        acquired1 = self.db.try_acquire(
            "excl_lock", pid1, "localhost", "writer1", mode="write"
        )
        self.assertTrue(acquired1)

        # Second PID should be blocked
        acquired2 = self.db.try_acquire(
            "excl_lock", pid2, "localhost", "writer2", mode="write"
        )
        self.assertFalse(acquired2)

    def test_write_blocks_read(self) -> None:
        """Read lock fails while another PID holds write."""
        pid1 = os.getpid()
        pid2 = pid1 + 99999

        self.db.try_acquire("wr_lock", pid1, "localhost", "writer", mode="write")

        # Read should be blocked by other writer
        acquired = self.db.try_acquire(
            "wr_lock", pid2, "localhost", "reader", mode="read"
        )
        self.assertFalse(acquired)


class TestReentrancy(unittest.TestCase):
    """Same PID re-acquiring succeeds."""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_reentrant_same_pid_read(self) -> None:
        """Same PID re-acquiring read lock succeeds."""
        pid = os.getpid()
        self.db.try_acquire("reentrant", pid, "localhost", "op1", mode="read")
        acquired = self.db.try_acquire(
            "reentrant", pid, "localhost", "op2", mode="read"
        )
        self.assertTrue(acquired)

    def test_reentrant_same_pid_write(self) -> None:
        """Same PID re-acquiring write lock succeeds."""
        pid = os.getpid()
        self.db.try_acquire("reentrant", pid, "localhost", "op1", mode="write")
        acquired = self.db.try_acquire(
            "reentrant", pid, "localhost", "op2", mode="write"
        )
        self.assertTrue(acquired)


class TestReadToWriteUpgrade(unittest.TestCase):
    """Read-to-write lock upgrade."""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_upgrade_read_to_write(self) -> None:
        """Same PID upgrades read->write when sole holder."""
        pid = os.getpid()
        self.db.try_acquire("upgrade_lock", pid, "localhost", "reader", mode="read")

        # Upgrade to write (sole holder)
        acquired = self.db.try_acquire(
            "upgrade_lock", pid, "localhost", "writer", mode="write"
        )
        self.assertTrue(acquired)

        # Verify it's now a write lock
        holders = self.db.get_lock_info("upgrade_lock")
        self.assertEqual(len(holders), 1)
        self.assertEqual(holders[0]["lock_mode"], "write")

    def test_upgrade_blocked_by_other_reader(self) -> None:
        """Read->write upgrade fails if other readers exist."""
        pid1 = os.getpid()
        pid2 = pid1 + 99999

        self.db.try_acquire("upgrade_lock2", pid1, "localhost", "r1", mode="read")
        self.db.try_acquire("upgrade_lock2", pid2, "localhost", "r2", mode="read")

        # pid1 upgrade should fail (pid2 is also reading)
        acquired = self.db.try_acquire(
            "upgrade_lock2", pid1, "localhost", "w1", mode="write"
        )
        self.assertFalse(acquired)


class TestStaleDetection(unittest.TestCase):
    """Stale lock detection and breaking."""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_stale_detection_dead_pid(self) -> None:
        """Lock with dead PID (999999) detected as stale."""
        dead_pid = 999999
        self.db.try_acquire("stale_lock", dead_pid, "localhost", "dead_op")
        self.assertTrue(self.db.is_lock_stale("stale_lock"))

    def test_break_stale_lock(self) -> None:
        """Stale lock successfully broken."""
        dead_pid = 999999
        self.db.try_acquire("stale_lock", dead_pid, "localhost", "dead_op")

        broken = self.db.break_stale_lock("stale_lock")
        self.assertTrue(broken)
        self.assertFalse(self.db.is_held("stale_lock"))

    def test_break_active_lock_fails(self) -> None:
        """Active lock (current PID) cannot be broken via break_stale_lock."""
        pid = os.getpid()
        self.db.try_acquire("active_lock", pid, "localhost", "active_op")

        broken = self.db.break_stale_lock("active_lock")
        self.assertFalse(broken)
        self.assertTrue(self.db.is_held("active_lock"))

    def test_cleanup_stale_locks(self) -> None:
        """Batch cleanup removes only dead-PID locks."""
        pid = os.getpid()
        dead_pid = 999999

        self.db.try_acquire("active", pid, "localhost", "active_op")
        self.db.try_acquire("stale1", dead_pid, "localhost", "dead_op1")
        self.db.try_acquire("stale2", dead_pid + 1, "localhost", "dead_op2")

        cleaned = self.db.cleanup_stale_locks()
        self.assertEqual(cleaned, 2)

        # Active lock should remain
        self.assertTrue(self.db.is_held("active"))
        self.assertFalse(self.db.is_held("stale1"))
        self.assertFalse(self.db.is_held("stale2"))


class TestForceBreak(unittest.TestCase):
    """Force break removes any lock."""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_force_break(self) -> None:
        """Force break removes any lock regardless of PID status."""
        pid = os.getpid()
        self.db.try_acquire("force_lock", pid, "localhost", "op")

        broken = self.db.force_break("force_lock")
        self.assertTrue(broken)
        self.assertFalse(self.db.is_held("force_lock"))


class TestConcurrentThreads(unittest.TestCase):
    """Two threads compete for lock."""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_concurrent_threads(self) -> None:
        """Two threads compete; both use same PID so re-entrancy allows both."""
        results: list[bool] = []

        def acquire_lock(thread_id: int) -> None:
            # Both threads share the same PID (os.getpid())
            # So re-entrancy means both succeed
            acquired = self.db.try_acquire(
                "thread_lock",
                os.getpid(),
                "localhost",
                f"thread_{thread_id}",
            )
            results.append(acquired)

        t1 = threading.Thread(target=acquire_lock, args=(1,))
        t2 = threading.Thread(target=acquire_lock, args=(2,))
        t1.start()
        t1.join()
        t2.start()
        t2.join()

        # Both threads have same PID, so re-entrancy means both succeed
        self.assertTrue(all(results))


if __name__ == "__main__":
    unittest.main()
