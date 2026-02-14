#!/usr/bin/env python3
"""
Tests for file_lock_rw module - SQLite-backed locking.

These tests validate:
- Lock acquisition and release
- Stale lock detection (dead process PIDs)
- Lock breaking functionality
- Metadata read (backward compat)
- Read vs write lock semantics
- Re-entrancy
"""

import os
import tempfile
import unittest
from pathlib import Path

from ci.util.file_lock_rw import (
    FileLock,
    _read_lock_metadata,
    break_stale_lock,
    is_lock_stale,
    is_process_alive,
    read_lock,
    write_lock,
)
from ci.util.lock_database import LockDatabase


class TestProcessAlive(unittest.TestCase):
    """Test process alive detection"""

    def test_current_process_alive(self) -> None:
        """Current process should be detected as alive"""
        self.assertTrue(is_process_alive(os.getpid()))

    def test_invalid_pid_dead(self) -> None:
        """Invalid PIDs should be detected as dead"""
        self.assertFalse(is_process_alive(0))
        self.assertFalse(is_process_alive(-1))
        self.assertFalse(is_process_alive(999999))  # Unlikely to exist


class TestStaleLockDetection(unittest.TestCase):
    """Test stale lock detection via database"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)
        # Set env var to use our test DB
        os.environ["FASTLED_LOCK_DB_PATH"] = str(self.db_path)

    def tearDown(self) -> None:
        import shutil

        os.environ.pop("FASTLED_LOCK_DB_PATH", None)
        # Clear the DB cache so next test gets a fresh DB
        from ci.util.lock_database import _db_cache

        _db_cache.clear()
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_nonexistent_lock_not_stale(self) -> None:
        """Nonexistent lock should not be considered stale"""
        lock_path = Path(self.temp_dir) / "test.lock"
        self.assertFalse(is_lock_stale(lock_path))

    def test_active_lock_not_stale(self) -> None:
        """Lock held by current process should not be stale"""
        lock_path = Path(self.temp_dir) / "test.lock"
        # Insert a lock record with current PID
        self.db.try_acquire(str(lock_path), os.getpid(), "localhost", "test")
        self.assertFalse(is_lock_stale(lock_path))

    def test_dead_pid_is_stale(self) -> None:
        """Lock with dead PID should be detected as stale"""
        lock_path = Path(self.temp_dir) / "test.lock"
        self.db.try_acquire(str(lock_path), 999999, "localhost", "test")
        self.assertTrue(is_lock_stale(lock_path))


class TestBreakStaleLock(unittest.TestCase):
    """Test lock breaking functionality"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)
        os.environ["FASTLED_LOCK_DB_PATH"] = str(self.db_path)

    def tearDown(self) -> None:
        import shutil

        os.environ.pop("FASTLED_LOCK_DB_PATH", None)
        from ci.util.lock_database import _db_cache

        _db_cache.clear()
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_break_stale_lock_success(self) -> None:
        """Successfully break a stale lock"""
        lock_path = Path(self.temp_dir) / "test.lock"
        self.db.try_acquire(str(lock_path), 999999, "localhost", "test")

        result = break_stale_lock(lock_path)
        self.assertTrue(result)
        self.assertFalse(self.db.is_held(str(lock_path)))

    def test_break_active_lock_fails(self) -> None:
        """Cannot break an active lock"""
        lock_path = Path(self.temp_dir) / "test.lock"
        self.db.try_acquire(str(lock_path), os.getpid(), "localhost", "test")

        result = break_stale_lock(lock_path)
        self.assertFalse(result)
        self.assertTrue(self.db.is_held(str(lock_path)))

    def test_break_nonexistent_lock(self) -> None:
        """Breaking nonexistent lock should return False"""
        lock_path = Path(self.temp_dir) / "test.lock"
        result = break_stale_lock(lock_path)
        self.assertFalse(result)


class TestFileLockContext(unittest.TestCase):
    """Test FileLock context manager"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        os.environ["FASTLED_LOCK_DB_PATH"] = str(self.db_path)

    def tearDown(self) -> None:
        import shutil

        os.environ.pop("FASTLED_LOCK_DB_PATH", None)
        from ci.util.lock_database import _db_cache

        _db_cache.clear()
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_lock_acquire_release(self) -> None:
        """Lock should be acquired and released"""
        lock_path = Path(self.temp_dir) / "test.lock"
        lock = FileLock(lock_path, timeout=1.0, operation="test")

        from ci.util.lock_database import get_lock_database

        db = get_lock_database(lock_path)

        with lock:
            # Lock should be held in DB
            self.assertTrue(db.is_held(str(lock_path)))
            metadata = _read_lock_metadata(lock_path)
            self.assertIsNotNone(metadata)

        # Lock should be released after exit
        self.assertFalse(db.is_held(str(lock_path)))

    def test_write_lock_helper(self) -> None:
        """Test write_lock helper function"""
        test_file = Path(self.temp_dir) / "data.txt"
        test_file.touch()

        with write_lock(test_file, timeout=1.0) as lock:
            self.assertIsNotNone(lock)

    def test_read_lock_helper(self) -> None:
        """Test read_lock helper function"""
        test_file = Path(self.temp_dir) / "data.txt"
        test_file.touch()

        with read_lock(test_file, timeout=1.0) as lock:
            self.assertIsNotNone(lock)

    def test_read_lock_allows_concurrent(self) -> None:
        """Read locks should allow concurrent access (same PID = re-entrant)."""
        test_file = Path(self.temp_dir) / "data.txt"
        test_file.touch()

        with read_lock(test_file, timeout=1.0):
            # Nested read lock should succeed (re-entrant for same PID)
            with read_lock(test_file, timeout=1.0) as inner_lock:
                self.assertIsNotNone(inner_lock)

    def test_reentrant_write_lock(self) -> None:
        """Same PID acquiring write lock twice should succeed (re-entrant)."""
        lock_path = Path(self.temp_dir) / "reentrant.lock"

        with FileLock(lock_path, timeout=1.0, operation="outer"):
            with FileLock(lock_path, timeout=1.0, operation="inner") as inner:
                self.assertIsNotNone(inner)


class TestLockBreakingRecovery(unittest.TestCase):
    """Test automatic stale lock recovery during acquisition"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)
        os.environ["FASTLED_LOCK_DB_PATH"] = str(self.db_path)

    def tearDown(self) -> None:
        import shutil

        os.environ.pop("FASTLED_LOCK_DB_PATH", None)
        from ci.util.lock_database import _db_cache

        _db_cache.clear()
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_stale_lock_auto_broken_on_timeout(self) -> None:
        """Stale lock should be automatically broken on timeout"""
        lock_path = Path(self.temp_dir) / "test.lock"

        # Create a stale lock in the DB (dead PID)
        self.db.try_acquire(str(lock_path), 999999, "localhost", "test")

        # Try to acquire lock - should break stale lock and succeed
        lock = FileLock(lock_path, timeout=2.0, operation="test")

        with lock:
            # Should successfully acquire
            self.assertTrue(self.db.is_held(str(lock_path)))
            metadata = _read_lock_metadata(lock_path)
            self.assertIsNotNone(metadata)
            assert metadata is not None
            # Should have current PID now
            self.assertEqual(metadata["pid"], os.getpid())


if __name__ == "__main__":
    unittest.main()
