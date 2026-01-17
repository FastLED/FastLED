"""
Comprehensive test suite for TwoLayerFingerprintCache.

Tests cover:
- Core functionality (cache hits, misses, content changes)
- Two-layer detection (mtime + MD5)
- Smart mtime updates (touch optimization)
- Edge cases (missing files, corruption, binary files)
- Concurrent access patterns
- Performance requirements
- Cross-process safety
"""

import concurrent.futures
import json
import shutil
import tempfile
import time
from pathlib import Path
from unittest import TestCase

from ci.fingerprint import TwoLayerFingerprintCache


class TestTwoLayerFingerprintCache(TestCase):
    """Test suite for TwoLayerFingerprintCache functionality."""

    def setUp(self) -> None:
        """Set up test environment with temporary directory."""
        self.test_dir = Path(tempfile.mkdtemp())
        self.cache_dir = self.test_dir / "cache"
        self.cache_dir.mkdir(exist_ok=True)
        self.temp_dir = self.test_dir / "temp"
        self.temp_dir.mkdir(exist_ok=True)

    def tearDown(self) -> None:
        """Clean up test environment."""
        shutil.rmtree(self.test_dir, ignore_errors=True)

    def create_test_file(self, path: Path, content: str) -> Path:
        """Create a test file with specific content."""
        path.parent.mkdir(parents=True, exist_ok=True)
        with open(path, "w") as f:
            f.write(content)
        return path

    def modify_file_content(self, path: Path, new_content: str) -> None:
        """Modify file content."""
        time.sleep(0.01)  # Ensure modtime changes
        with open(path, "w") as f:
            f.write(new_content)

    def test_basic_cache_miss_hit(self) -> None:
        """Test basic cache miss and hit behavior."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "basic_test")

        files = [
            self.create_test_file(self.temp_dir / "file1.txt", "content 1"),
            self.create_test_file(self.temp_dir / "file2.txt", "content 2"),
        ]

        # First check should need update (cache miss)
        needs_update = cache.check_needs_update(files)
        self.assertTrue(needs_update, "First check should need update")

        # Mark success
        cache.mark_success()

        # Second check should NOT need update (cache hit)
        needs_update = cache.check_needs_update(files)
        self.assertFalse(needs_update, "Second check should be cache hit")

    def test_content_change_detection(self) -> None:
        """Test that content changes are detected."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "content_test")

        files = [
            self.create_test_file(self.temp_dir / "file1.txt", "original"),
        ]

        # Prime cache
        cache.check_needs_update(files)
        cache.mark_success()

        # Modify content
        self.modify_file_content(files[0], "modified")

        # Should detect change
        needs_update = cache.check_needs_update(files)
        self.assertTrue(needs_update, "Content change should be detected")

    def test_touch_optimization(self) -> None:
        """Test smart mtime update when file is touched but content unchanged."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "touch_test")

        files = [
            self.create_test_file(self.temp_dir / "file1.txt", "unchanged content"),
        ]

        # Prime cache
        cache.check_needs_update(files)
        cache.mark_success()

        # Touch file (change mtime but not content)
        time.sleep(0.01)
        files[0].touch()

        # First check after touch should NOT need update (smart mtime update)
        needs_update = cache.check_needs_update(files)
        self.assertFalse(
            needs_update, "Touched file with same content should NOT need update"
        )

        # Second check should also be fast (mtime was updated in cache)
        start = time.time()
        needs_update = cache.check_needs_update(files)
        elapsed = time.time() - start
        self.assertFalse(needs_update, "Second check should still be cache hit")
        self.assertLess(elapsed, 0.05, "Second check should be fast (no rehashing)")

    def test_missing_file_detection(self) -> None:
        """Test detection of missing files."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "missing_test")

        files = [
            self.create_test_file(self.temp_dir / "file1.txt", "content 1"),
            self.create_test_file(self.temp_dir / "file2.txt", "content 2"),
        ]

        # Prime cache
        cache.check_needs_update(files)
        cache.mark_success()

        # Delete a file
        files[0].unlink()

        # Should detect change
        needs_update = cache.check_needs_update(files)
        self.assertTrue(needs_update, "Missing file should be detected")

    def test_removed_file_from_monitored_set(self) -> None:
        """Test detection when a file is removed from the monitored set."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "removed_test")

        files = [
            self.create_test_file(self.temp_dir / "file1.txt", "content 1"),
            self.create_test_file(self.temp_dir / "file2.txt", "content 2"),
        ]

        # Prime cache with both files
        cache.check_needs_update(files)
        cache.mark_success()

        # Remove one file from monitored set (but don't delete from disk)
        files_subset = files[:1]

        # Should detect change (file removed from monitored set)
        needs_update = cache.check_needs_update(files_subset)
        self.assertTrue(
            needs_update, "File removed from monitored set should be detected"
        )

    def test_new_file_addition(self) -> None:
        """Test detection of new files added to monitored set."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "new_file_test")

        files = [
            self.create_test_file(self.temp_dir / "file1.txt", "content 1"),
        ]

        # Prime cache
        cache.check_needs_update(files)
        cache.mark_success()

        # Add new file
        new_file = self.create_test_file(self.temp_dir / "file2.txt", "content 2")
        files.append(new_file)

        # Should detect change
        needs_update = cache.check_needs_update(files)
        self.assertTrue(needs_update, "New file addition should be detected")

    def test_cache_persistence(self) -> None:
        """Test that cache persists across instances."""
        files = [
            self.create_test_file(self.temp_dir / "file1.txt", "content 1"),
        ]

        # First cache instance
        cache1 = TwoLayerFingerprintCache(self.cache_dir, "persist_test")
        cache1.check_needs_update(files)
        cache1.mark_success()

        # Second cache instance (reload from disk)
        cache2 = TwoLayerFingerprintCache(self.cache_dir, "persist_test")
        needs_update = cache2.check_needs_update(files)
        self.assertFalse(needs_update, "Cache should persist across instances")

    def test_invalidate(self) -> None:
        """Test cache invalidation."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "invalidate_test")

        files = [
            self.create_test_file(self.temp_dir / "file1.txt", "content 1"),
        ]

        # Prime cache
        cache.check_needs_update(files)
        cache.mark_success()

        # Invalidate
        cache.invalidate()

        # Should need update after invalidation
        needs_update = cache.check_needs_update(files)
        self.assertTrue(needs_update, "Cache should need update after invalidation")

    def test_cache_corruption_recovery(self) -> None:
        """Test recovery from corrupted cache files."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "corrupt_test")

        files = [
            self.create_test_file(self.temp_dir / "file1.txt", "content 1"),
        ]

        # Create corrupted cache file
        cache_file = self.cache_dir / "fingerprint" / "corrupt_test.json"
        cache_file.parent.mkdir(parents=True, exist_ok=True)
        cache_file.write_text("{invalid json")

        # Should handle corruption gracefully
        needs_update = cache.check_needs_update(files)
        self.assertTrue(needs_update, "Corrupted cache should be treated as miss")

    def test_binary_files(self) -> None:
        """Test cache with binary files."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "binary_test")

        binary_file = self.temp_dir / "test.bin"
        binary_data = bytes(range(256))
        with open(binary_file, "wb") as f:
            f.write(binary_data)

        files = [binary_file]

        # Prime cache
        cache.check_needs_update(files)
        cache.mark_success()

        # Should be cache hit
        needs_update = cache.check_needs_update(files)
        self.assertFalse(needs_update, "Binary file cache should hit")

        # Modify binary file
        with open(binary_file, "wb") as f:
            f.write(binary_data + b"\x00")
        time.sleep(0.01)

        # Should detect change
        needs_update = cache.check_needs_update(files)
        self.assertTrue(needs_update, "Binary file change should be detected")

    def test_large_file_set_performance(self) -> None:
        """Test performance with many files."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "perf_test")

        # Create 50 files (reduced for faster test execution)
        files: list[Path] = []
        for i in range(50):
            files.append(
                self.create_test_file(self.temp_dir / f"file_{i}.txt", f"content {i}")
            )

        # Time the check (first check computes MD5 for all files)
        start = time.time()
        needs_update = cache.check_needs_update(files)
        check_time = time.time() - start

        self.assertTrue(needs_update, "First check should need update")
        # Windows file I/O can be slower - allow up to 3 seconds for 50 files
        self.assertLess(
            check_time,
            3.0,
            f"Check with 50 files took {check_time:.3f}s, should be <3s",
        )

        # Mark success
        cache.mark_success()

        # Time cache hit (should be fast - just mtime comparison)
        start = time.time()
        needs_update = cache.check_needs_update(files)
        hit_time = time.time() - start

        self.assertFalse(needs_update, "Cache should hit")
        self.assertLess(
            hit_time,
            0.2,
            f"Cache hit with 50 files took {hit_time:.3f}s, should be <0.2s",
        )

    def test_concurrent_access(self) -> None:
        """Test concurrent access to the same cache."""
        files = [
            self.create_test_file(self.temp_dir / "file1.txt", "content 1"),
        ]

        def check_cache(thread_id: int) -> bool:
            cache = TwoLayerFingerprintCache(self.cache_dir, "concurrent_test")
            return cache.check_needs_update(files)

        # Run concurrent checks
        with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
            futures = [executor.submit(check_cache, i) for i in range(3)]
            results = [
                future.result() for future in concurrent.futures.as_completed(futures)
            ]

        # At least one should report cache miss
        self.assertGreaterEqual(
            sum(results), 1, "At least one check should need update"
        )

    def test_pending_fingerprint_cross_process(self) -> None:
        """Test pending fingerprint works across process boundaries."""
        files = [
            self.create_test_file(self.temp_dir / "file1.txt", "content 1"),
        ]

        # Instance 1: Check needs update
        cache1 = TwoLayerFingerprintCache(self.cache_dir, "cross_process_test")
        needs_update = cache1.check_needs_update(files)
        self.assertTrue(needs_update, "First check should need update")

        # Instance 2: Mark success (simulates different process)
        cache2 = TwoLayerFingerprintCache(self.cache_dir, "cross_process_test")
        cache2.mark_success()

        # Instance 3: Verify cache hit
        cache3 = TwoLayerFingerprintCache(self.cache_dir, "cross_process_test")
        needs_update = cache3.check_needs_update(files)
        self.assertFalse(
            needs_update, "Cache should hit after cross-process mark_success"
        )

    def test_mark_success_without_check_raises_error(self) -> None:
        """Test that mark_success without check_needs_update raises error."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "error_test")

        with self.assertRaises(RuntimeError):
            cache.mark_success()

    def test_get_cache_info(self) -> None:
        """Test cache info retrieval."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "info_test")

        files = [
            self.create_test_file(self.temp_dir / "file1.txt", "content 1"),
        ]

        # No cache initially
        info = cache.get_cache_info()
        self.assertIsNone(info, "No cache info should exist initially")

        # After caching
        cache.check_needs_update(files)
        cache.mark_success()

        info = cache.get_cache_info()
        self.assertIsNotNone(info, "Cache info should exist after caching")
        self.assertEqual(info["file_count"], 1, "File count should be 1")
        self.assertTrue(info["cache_exists"], "Cache file should exist")

    def test_empty_file_list(self) -> None:
        """Test behavior with empty file list."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "empty_test")

        # Empty list returns False (no files = no changes detected)
        # This is the expected behavior - empty list means nothing to monitor
        needs_update = cache.check_needs_update([])
        self.assertFalse(
            needs_update, "Empty list should return False (nothing to monitor)"
        )

        # Mark success
        cache.mark_success()

        # Second check should also be False
        needs_update = cache.check_needs_update([])
        self.assertFalse(needs_update, "Empty list should still return False")

    def test_multiple_modifications_sequence(self) -> None:
        """Test sequence of multiple modifications."""
        cache = TwoLayerFingerprintCache(self.cache_dir, "sequence_test")

        file1 = self.create_test_file(self.temp_dir / "file1.txt", "v1")

        # Initial cache
        cache.check_needs_update([file1])
        cache.mark_success()

        # Modify v1 -> v2
        self.modify_file_content(file1, "v2")
        needs_update = cache.check_needs_update([file1])
        self.assertTrue(needs_update, "v1->v2 change should be detected")
        cache.mark_success()

        # No change
        needs_update = cache.check_needs_update([file1])
        self.assertFalse(needs_update, "No change should be cache hit")

        # Modify v2 -> v3
        self.modify_file_content(file1, "v3")
        needs_update = cache.check_needs_update([file1])
        self.assertTrue(needs_update, "v2->v3 change should be detected")
        cache.mark_success()

        # No change
        needs_update = cache.check_needs_update([file1])
        self.assertFalse(needs_update, "Final state should be cache hit")


def run_all_tests() -> bool:
    """Execute all test scenarios."""
    import unittest

    # Create test suite
    suite = unittest.TestLoader().loadTestsFromTestCase(TestTwoLayerFingerprintCache)

    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    if result.wasSuccessful():
        print("\n✅ All TwoLayerFingerprintCache tests passed!")
        return True
    else:
        print("\n❌ Some tests failed!")
        return False


if __name__ == "__main__":
    import sys

    sys.exit(0 if run_all_tests() else 1)
