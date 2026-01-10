#!/usr/bin/env python3
"""
Fast tests for cache_lock module.

These tests validate:
- Artifact lock acquisition
- Force unlock functionality
- Stale lock cleanup
- Active lock listing
"""

import json
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
from ci.util.file_lock_rw import _write_lock_metadata


class TestAcquireArtifactLock(unittest.TestCase):
    """Test artifact lock acquisition"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.cache_dir = Path(self.temp_dir) / "cache"
        self.cache_dir.mkdir()

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_acquire_artifact_lock(self) -> None:
        """Acquire lock for specific artifact URL"""
        url = "https://example.com/toolchain.tar.gz"

        with acquire_artifact_lock(self.cache_dir, url, operation="test") as lock:
            self.assertIsNotNone(lock)
            # Lock file should exist during acquisition
            self.assertTrue(is_artifact_locked(self.cache_dir, url))

        # Note: Lock file may persist but metadata should be removed
        # Without metadata, is_artifact_locked conservatively returns True
        # This is expected behavior - stale detection requires metadata

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

    def tearDown(self) -> None:
        import shutil

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
        """Force unlock should remove all lock files"""
        # Create some lock files
        artifact1 = self.cache_dir / "artifact1"
        artifact1.mkdir()
        lock1 = artifact1 / ".download.lock"
        lock1.touch()
        _write_lock_metadata(lock1, operation="test1")

        artifact2 = self.cache_dir / "artifact2"
        artifact2.mkdir()
        lock2 = artifact2 / ".download.lock"
        lock2.touch()
        _write_lock_metadata(lock2, operation="test2")

        # Force unlock
        broken = force_unlock_cache(self.cache_dir)

        # Should have broken both locks
        self.assertEqual(broken, 2)
        self.assertFalse(lock1.exists())
        self.assertFalse(lock2.exists())


class TestCleanupStaleLocks(unittest.TestCase):
    """Test stale lock cleanup"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.cache_dir = Path(self.temp_dir) / "cache"
        self.cache_dir.mkdir()

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_cleanup_empty_cache(self) -> None:
        """Cleanup on empty cache should return 0"""
        cleaned = cleanup_stale_locks(self.cache_dir)
        self.assertEqual(cleaned, 0)

    def test_cleanup_stale_locks_only(self) -> None:
        """Cleanup should only remove stale locks, not active ones"""
        # Create stale lock (dead PID)
        artifact1 = self.cache_dir / "artifact1"
        artifact1.mkdir()
        stale_lock = artifact1 / ".download.lock"
        stale_lock.touch()
        metadata_file = stale_lock.with_suffix(stale_lock.suffix + ".pid")
        with open(metadata_file, "w") as f:
            json.dump(
                {
                    "pid": 999999,
                    "timestamp": "2024-01-01T00:00:00",
                    "operation": "test",
                    "hostname": "localhost",
                },
                f,
            )

        # Create active lock (current PID)
        artifact2 = self.cache_dir / "artifact2"
        artifact2.mkdir()
        active_lock = artifact2 / ".download.lock"
        active_lock.touch()
        _write_lock_metadata(active_lock, operation="active")

        # Cleanup
        cleaned = cleanup_stale_locks(self.cache_dir)

        # Should have cleaned only the stale lock
        self.assertEqual(cleaned, 1)
        self.assertFalse(stale_lock.exists())
        self.assertTrue(active_lock.exists())


class TestListActiveLocks(unittest.TestCase):
    """Test listing active locks"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.cache_dir = Path(self.temp_dir) / "cache"
        self.cache_dir.mkdir()

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_list_empty_cache(self) -> None:
        """List on empty cache should return empty list"""
        locks = list_active_locks(self.cache_dir)
        self.assertEqual(len(locks), 0)

    def test_list_active_and_stale_locks(self) -> None:
        """List should return both active and stale locks with correct status"""
        # Create stale lock
        artifact1 = self.cache_dir / "artifact1"
        artifact1.mkdir()
        stale_lock = artifact1 / ".download.lock"
        stale_lock.touch()
        metadata_file = stale_lock.with_suffix(stale_lock.suffix + ".pid")
        with open(metadata_file, "w") as f:
            json.dump(
                {
                    "pid": 999999,
                    "timestamp": "2024-01-01T00:00:00",
                    "operation": "stale_op",
                    "hostname": "localhost",
                },
                f,
            )

        # Create active lock
        artifact2 = self.cache_dir / "artifact2"
        artifact2.mkdir()
        active_lock = artifact2 / ".download.lock"
        active_lock.touch()
        _write_lock_metadata(active_lock, operation="active_op")

        # List locks
        locks = list_active_locks(self.cache_dir)

        # Should have 2 locks
        self.assertEqual(len(locks), 2)

        # Find each lock
        stale_info = next((l for l in locks if "artifact1" in str(l.path)), None)
        active_info = next((l for l in locks if "artifact2" in str(l.path)), None)

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

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_nonexistent_artifact_not_locked(self) -> None:
        """Nonexistent artifact should not be locked"""
        url = "https://example.com/toolchain.tar.gz"
        self.assertFalse(is_artifact_locked(self.cache_dir, url))

    def test_stale_lock_not_considered_locked(self) -> None:
        """Stale lock should not be considered active"""
        url = "https://example.com/toolchain.tar.gz"

        # Acquire and release lock to create artifact dir
        with acquire_artifact_lock(self.cache_dir, url):
            pass

        # Manually create stale lock
        from ci.util.url_utils import sanitize_url_for_path

        cache_key = str(sanitize_url_for_path(url))
        artifact_dir = self.cache_dir / cache_key
        lock_file = artifact_dir / ".download.lock"
        lock_file.touch()

        metadata_file = lock_file.with_suffix(lock_file.suffix + ".pid")
        with open(metadata_file, "w") as f:
            json.dump(
                {
                    "pid": 999999,
                    "timestamp": "2024-01-01T00:00:00",
                    "operation": "test",
                    "hostname": "localhost",
                },
                f,
            )

        # Should not be considered locked (stale)
        self.assertFalse(is_artifact_locked(self.cache_dir, url))


if __name__ == "__main__":
    unittest.main()
