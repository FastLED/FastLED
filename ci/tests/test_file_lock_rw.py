#!/usr/bin/env python3
"""
Fast tests for file_lock_rw module - focusing on lock breaking.

These tests validate:
- Lock acquisition and release
- Stale lock detection (dead process PIDs)
- Lock breaking functionality
- Metadata read/write
"""

import json
import os
import tempfile
import unittest
from pathlib import Path

from ci.util.file_lock_rw import (
    FileLock,
    _read_lock_metadata,
    _remove_lock_metadata,
    _write_lock_metadata,
    break_stale_lock,
    is_lock_stale,
    is_process_alive,
    read_lock,
    write_lock,
)


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


class TestLockMetadata(unittest.TestCase):
    """Test lock metadata operations"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.lock_path = Path(self.temp_dir) / "test.lock"

    def tearDown(self) -> None:
        # Cleanup
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_write_read_metadata(self) -> None:
        """Write and read lock metadata"""
        _write_lock_metadata(self.lock_path, operation="test_op")
        metadata = _read_lock_metadata(self.lock_path)

        self.assertIsNotNone(metadata)
        assert metadata is not None  # Type narrowing
        self.assertEqual(metadata["pid"], os.getpid())
        self.assertEqual(metadata["operation"], "test_op")
        self.assertIn("timestamp", metadata)
        self.assertIn("hostname", metadata)

    def test_read_nonexistent_metadata(self) -> None:
        """Reading nonexistent metadata should return None"""
        metadata = _read_lock_metadata(self.lock_path)
        self.assertIsNone(metadata)

    def test_remove_metadata(self) -> None:
        """Remove lock metadata"""
        _write_lock_metadata(self.lock_path, operation="test")
        _remove_lock_metadata(self.lock_path)

        metadata = _read_lock_metadata(self.lock_path)
        self.assertIsNone(metadata)


class TestStaleLockDetection(unittest.TestCase):
    """Test stale lock detection"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.lock_path = Path(self.temp_dir) / "test.lock"

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_nonexistent_lock_not_stale(self) -> None:
        """Nonexistent lock should not be considered stale"""
        self.assertFalse(is_lock_stale(self.lock_path))

    def test_active_lock_not_stale(self) -> None:
        """Lock held by current process should not be stale"""
        # Create lock file and metadata with current PID
        self.lock_path.touch()
        _write_lock_metadata(self.lock_path, operation="test")

        self.assertFalse(is_lock_stale(self.lock_path))

    def test_dead_pid_is_stale(self) -> None:
        """Lock with dead PID should be detected as stale"""
        # Create lock file
        self.lock_path.touch()

        # Write metadata with fake dead PID
        metadata_file = self.lock_path.with_suffix(self.lock_path.suffix + ".pid")
        fake_metadata = {
            "pid": 999999,  # Very unlikely to exist
            "timestamp": "2024-01-01T00:00:00",
            "operation": "test",
            "hostname": "localhost",
        }
        with open(metadata_file, "w") as f:
            json.dump(fake_metadata, f)

        self.assertTrue(is_lock_stale(self.lock_path))


class TestBreakStaleLock(unittest.TestCase):
    """Test lock breaking functionality"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.lock_path = Path(self.temp_dir) / "test.lock"

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_break_stale_lock_success(self) -> None:
        """Successfully break a stale lock"""
        # Create stale lock
        self.lock_path.touch()
        metadata_file = self.lock_path.with_suffix(self.lock_path.suffix + ".pid")
        fake_metadata = {
            "pid": 999999,
            "timestamp": "2024-01-01T00:00:00",
            "operation": "test",
            "hostname": "localhost",
        }
        with open(metadata_file, "w") as f:
            json.dump(fake_metadata, f)

        # Break the lock
        result = break_stale_lock(self.lock_path)

        self.assertTrue(result)
        self.assertFalse(self.lock_path.exists())
        self.assertFalse(metadata_file.exists())

    def test_break_active_lock_fails(self) -> None:
        """Cannot break an active lock"""
        # Create active lock with current PID
        self.lock_path.touch()
        _write_lock_metadata(self.lock_path, operation="test")

        # Try to break it
        result = break_stale_lock(self.lock_path)

        self.assertFalse(result)
        self.assertTrue(self.lock_path.exists())

    def test_break_nonexistent_lock(self) -> None:
        """Breaking nonexistent lock should return False"""
        result = break_stale_lock(self.lock_path)
        self.assertFalse(result)


class TestFileLockContext(unittest.TestCase):
    """Test FileLock context manager"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.lock_path = Path(self.temp_dir) / "test.lock"

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_lock_acquire_release(self) -> None:
        """Lock should be acquired and released"""
        lock = FileLock(self.lock_path, timeout=1.0, operation="test")

        # Lock should not exist initially
        self.assertFalse(self.lock_path.exists())

        with lock:
            # Lock should exist while held
            self.assertTrue(self.lock_path.exists())
            metadata = _read_lock_metadata(self.lock_path)
            self.assertIsNotNone(metadata)

        # Metadata should be cleaned up after release
        metadata = _read_lock_metadata(self.lock_path)
        self.assertIsNone(metadata)

    def test_write_lock_helper(self) -> None:
        """Test write_lock helper function"""
        test_file = Path(self.temp_dir) / "data.txt"
        test_file.touch()

        with write_lock(test_file, timeout=1.0) as lock:
            self.assertIsNotNone(lock)
            # Lock file should be in same directory with dot prefix
            lock_file = test_file.parent / f".{test_file.name}.lock"
            self.assertTrue(lock_file.exists())

    def test_read_lock_helper(self) -> None:
        """Test read_lock helper function"""
        test_file = Path(self.temp_dir) / "data.txt"
        test_file.touch()

        with read_lock(test_file, timeout=1.0) as lock:
            self.assertIsNotNone(lock)
            # Lock file should be in same directory with dot prefix
            lock_file = test_file.parent / f".{test_file.name}.lock"
            self.assertTrue(lock_file.exists())


class TestLockBreakingRecovery(unittest.TestCase):
    """Test automatic stale lock recovery during acquisition"""

    def setUp(self) -> None:
        self.temp_dir = tempfile.mkdtemp()
        self.lock_path = Path(self.temp_dir) / "test.lock"

    def tearDown(self) -> None:
        import shutil

        shutil.rmtree(self.temp_dir, ignore_errors=True)

    def test_stale_lock_auto_broken_on_timeout(self) -> None:
        """Stale lock should be automatically broken on timeout"""
        # Create a stale lock
        self.lock_path.touch()
        metadata_file = self.lock_path.with_suffix(self.lock_path.suffix + ".pid")
        fake_metadata = {
            "pid": 999999,
            "timestamp": "2024-01-01T00:00:00",
            "operation": "test",
            "hostname": "localhost",
        }
        with open(metadata_file, "w") as f:
            json.dump(fake_metadata, f)

        # Try to acquire lock - should break stale lock and succeed
        lock = FileLock(self.lock_path, timeout=0.1, operation="test")

        with lock:
            # Should successfully acquire
            self.assertTrue(self.lock_path.exists())
            metadata = _read_lock_metadata(self.lock_path)
            self.assertIsNotNone(metadata)
            assert metadata is not None
            # Should have current PID now
            self.assertEqual(metadata["pid"], os.getpid())


if __name__ == "__main__":
    unittest.main()
