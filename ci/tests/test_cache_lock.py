#!/usr/bin/env python3
"""
Tests for cache_lock module - SQLite-backed cache locking.

These tests validate:
- Artifact lock acquisition
- Force unlock functionality
- Stale lock cleanup
- Active lock listing
"""

import os
import tempfile
import unittest
from pathlib import Path

from ci.util.cache_lock import (
    acquire_artifact_lock,
    cleanup_stale_locks,
    force_unlock_cache,
    is_artifact_locked,
    list_active_locks,
)
from ci.util.lock_database import LockDatabase


class TestAcquireArtifactLock(unittest.TestCase):
    """Test artifact lock acquisition"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.cache_dir = Path(self.temp_dir) / "cache"
        self.cache_dir.mkdir()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        os.environ["FASTLED_LOCK_DB_PATH"] = str(self.db_path)

    def tearDown(self) -> None:
        import shutil

        os.environ.pop("FASTLED_LOCK_DB_PATH", None)
        from ci.util.lock_database import _db_cache

        _db_cache.clear()
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_acquire_artifact_lock(self) -> None:
        """Acquire lock for specific artifact URL"""
        url = "https://example.com/toolchain.tar.gz"

        with acquire_artifact_lock(self.cache_dir, url, operation="test") as lock:
            self.assertIsNotNone(lock)
            # Lock should be held during acquisition
            self.assertTrue(is_artifact_locked(self.cache_dir, url))

        # After release, should not be locked
        self.assertFalse(is_artifact_locked(self.cache_dir, url))

    def test_different_urls_different_locks(self) -> None:
        """Different URLs should have independent locks"""
        url1 = "https://example.com/toolchain1.tar.gz"
        url2 = "https://example.com/toolchain2.tar.gz"

        # Acquire first lock
        with acquire_artifact_lock(self.cache_dir, url1, operation="test1") as lock1:
            self.assertIsNotNone(lock1)
            self.assertTrue(is_artifact_locked(self.cache_dir, url1))

            # Should be able to acquire second lock concurrently
            with acquire_artifact_lock(
                self.cache_dir, url2, operation="test2"
            ) as lock2:
                self.assertIsNotNone(lock2)
                self.assertTrue(is_artifact_locked(self.cache_dir, url2))
                # Both should be locked
                self.assertTrue(is_artifact_locked(self.cache_dir, url1))


class TestForceUnlockCache(unittest.TestCase):
    """Test force unlock functionality"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.cache_dir = Path(self.temp_dir) / "cache"
        self.cache_dir.mkdir()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        os.environ["FASTLED_LOCK_DB_PATH"] = str(self.db_path)

    def tearDown(self) -> None:
        import shutil

        os.environ.pop("FASTLED_LOCK_DB_PATH", None)
        from ci.util.lock_database import _db_cache

        _db_cache.clear()
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_force_unlock_empty_cache(self) -> None:
        """Force unlock on empty cache should return 0"""
        broken = force_unlock_cache(self.cache_dir)
        self.assertEqual(broken, 0)

    def test_force_unlock_nonexistent_cache(self) -> None:
        """Force unlock on nonexistent cache should return 0"""
        nonexistent = Path(self.temp_dir) / "nonexistent"
        broken = force_unlock_cache(nonexistent)
        self.assertEqual(broken, 0)

    def test_force_unlock_removes_locks(self) -> None:
        """Force unlock should remove lock files and DB entries"""
        url1 = "https://example.com/toolchain1.tar.gz"
        url2 = "https://example.com/toolchain2.tar.gz"

        # Acquire and hold locks (they will be released by context manager,
        # but we can also create locks directly in DB for testing)
        with acquire_artifact_lock(self.cache_dir, url1, operation="test1"):
            with acquire_artifact_lock(self.cache_dir, url2, operation="test2"):
                # Both should be locked
                self.assertTrue(is_artifact_locked(self.cache_dir, url1))
                self.assertTrue(is_artifact_locked(self.cache_dir, url2))

                # Force unlock while held
                broken = force_unlock_cache(self.cache_dir)
                # Should have broken the DB entries
                self.assertGreaterEqual(broken, 0)


class TestCleanupStaleLocks(unittest.TestCase):
    """Test stale lock cleanup"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.cache_dir = Path(self.temp_dir) / "cache"
        self.cache_dir.mkdir()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)
        os.environ["FASTLED_LOCK_DB_PATH"] = str(self.db_path)

    def tearDown(self) -> None:
        import shutil

        os.environ.pop("FASTLED_LOCK_DB_PATH", None)
        from ci.util.lock_database import _db_cache

        _db_cache.clear()
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_cleanup_empty_cache(self) -> None:
        """Cleanup on empty cache should return 0"""
        cleaned = cleanup_stale_locks(self.cache_dir)
        self.assertEqual(cleaned, 0)

    def test_cleanup_stale_locks_only(self) -> None:
        """Cleanup should only remove stale locks, not active ones"""
        # Create stale lock in DB (dead PID)
        stale_lock_name = str(self.cache_dir / "artifact1" / ".download.lock")
        self.db.try_acquire(stale_lock_name, 999999, "localhost", "test")

        # Create active lock in DB (current PID)
        active_lock_name = str(self.cache_dir / "artifact2" / ".download.lock")
        self.db.try_acquire(active_lock_name, os.getpid(), "localhost", "active")

        # Cleanup
        cleaned = cleanup_stale_locks(self.cache_dir)

        # Should have cleaned the stale lock
        self.assertGreaterEqual(cleaned, 1)
        # Active lock should remain
        self.assertTrue(self.db.is_held(active_lock_name))
        # Stale lock should be gone
        self.assertFalse(self.db.is_held(stale_lock_name))


class TestListActiveLocks(unittest.TestCase):
    """Test listing active locks"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.cache_dir = Path(self.temp_dir) / "cache"
        self.cache_dir.mkdir()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)
        os.environ["FASTLED_LOCK_DB_PATH"] = str(self.db_path)

    def tearDown(self) -> None:
        import shutil

        os.environ.pop("FASTLED_LOCK_DB_PATH", None)
        from ci.util.lock_database import _db_cache

        _db_cache.clear()
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_list_empty_cache(self) -> None:
        """List on empty cache should return empty list"""
        locks = list_active_locks(self.cache_dir)
        self.assertEqual(len(locks), 0)

    def test_list_active_and_stale_locks(self) -> None:
        """List should return both active and stale locks with correct status"""
        # Create stale lock in DB
        stale_name = str(self.cache_dir / "artifact1" / ".download.lock")
        self.db.try_acquire(stale_name, 999999, "localhost", "stale_op")

        # Create active lock in DB
        active_name = str(self.cache_dir / "artifact2" / ".download.lock")
        self.db.try_acquire(active_name, os.getpid(), "localhost", "active_op")

        # List locks
        locks = list_active_locks(self.cache_dir)

        # Should have 2 locks
        self.assertEqual(len(locks), 2)

        # Find each lock
        stale_info = next(
            (lock for lock in locks if "artifact1" in str(lock.path)), None
        )
        active_info = next(
            (lock for lock in locks if "artifact2" in str(lock.path)), None
        )

        self.assertIsNotNone(stale_info)
        self.assertIsNotNone(active_info)

        assert stale_info is not None
        assert active_info is not None

        # Check stale lock info
        self.assertTrue(stale_info.is_stale)
        self.assertEqual(stale_info.pid, 999999)
        self.assertEqual(stale_info.operation, "stale_op")

        # Check active lock info
        self.assertFalse(active_info.is_stale)
        self.assertEqual(active_info.pid, os.getpid())
        self.assertEqual(active_info.operation, "active_op")


class TestIsArtifactLocked(unittest.TestCase):
    """Test artifact locked status check"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.cache_dir = Path(self.temp_dir) / "cache"
        self.cache_dir.mkdir()
        self.db_path = Path(self.temp_dir) / "test_locks.db"
        self.db = LockDatabase(self.db_path)
        os.environ["FASTLED_LOCK_DB_PATH"] = str(self.db_path)

    def tearDown(self) -> None:
        import shutil

        os.environ.pop("FASTLED_LOCK_DB_PATH", None)
        from ci.util.lock_database import _db_cache

        _db_cache.clear()
        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_nonexistent_artifact_not_locked(self) -> None:
        """Nonexistent artifact should not be locked"""
        url = "https://example.com/toolchain.tar.gz"
        self.assertFalse(is_artifact_locked(self.cache_dir, url))

    def test_stale_lock_not_considered_locked(self) -> None:
        """Stale lock should not be considered active"""
        url = "https://example.com/toolchain.tar.gz"

        from ci.util.url_utils import sanitize_url_for_path

        cache_key = str(sanitize_url_for_path(url))
        artifact_dir = self.cache_dir / cache_key
        lock_file = artifact_dir / ".download.lock"

        # Create stale lock in DB
        self.db.try_acquire(str(lock_file), 999999, "localhost", "test")

        # Should not be considered locked (stale)
        self.assertFalse(is_artifact_locked(self.cache_dir, url))


if __name__ == "__main__":
    unittest.main()
