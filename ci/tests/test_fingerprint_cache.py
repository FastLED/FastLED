"""
Comprehensive test suite for fingerprint cache functionality.

Tests cover:
- Core functionality (cache hits, misses, content changes)
- Edge cases (touched files, copied files, corruption)
- Performance requirements
- Integration scenarios
"""

import json
import os
import shutil
import tempfile
import time
from pathlib import Path
from typing import List, Optional
from unittest import TestCase

# Import the fingerprint cache module
from ci.ci.fingerprint_cache import CacheEntry, FingerprintCache, has_changed


class TestFingerprintCache(TestCase):
    """Test suite for fingerprint cache functionality."""

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

    def create_test_file(
        self, path: Path, content: str, modtime: Optional[float] = None
    ) -> Path:
        """Create a test file with specific content and optional modification time."""
        with open(path, "w") as f:
            f.write(content)

        if modtime:
            # Set specific modification time
            os.utime(path, (modtime, modtime))

        return path

    def touch_file(self, path: Path) -> float:
        """Touch a file to update its modification time without changing content."""
        # Wait a small amount to ensure modtime changes
        time.sleep(0.01)
        path.touch()
        return os.path.getmtime(path)

    def modify_file_content(self, path: Path, new_content: str) -> float:
        """Modify file content and return new modification time."""
        time.sleep(0.01)  # Ensure modtime changes
        with open(path, "w") as f:
            f.write(new_content)
        return os.path.getmtime(path)

    def test_cache_hit_modtime_unchanged(self) -> None:
        """Test that identical modtime returns False immediately."""
        cache_file = self.cache_dir / "test_cache.json"
        cache = FingerprintCache(cache_file)

        # Create test file
        test_file = self.temp_dir / "test.txt"
        self.create_test_file(test_file, "original content")
        original_modtime = os.path.getmtime(test_file)

        # Same modtime should return False (no change detected)
        result1 = cache.has_changed(test_file, original_modtime)
        self.assertFalse(result1, "Same modtime should return False")

        # Verify fast performance (cache hit)
        start_time = time.time()
        result2 = cache.has_changed(test_file, original_modtime)
        elapsed = time.time() - start_time

        self.assertFalse(result2, "Same modtime should return False")
        self.assertLess(
            elapsed, 0.002, f"Cache hit should be <2ms, took {elapsed * 1000:.2f}ms"
        )

    def test_cache_hit_with_existing_cache(self) -> None:
        """Test cache hit when file exists in cache with current modtime."""
        cache_file = self.cache_dir / "test_cache.json"
        cache = FingerprintCache(cache_file)

        # Create and cache a file
        test_file = self.temp_dir / "cached.txt"
        self.create_test_file(test_file, "cached content")
        current_modtime = os.path.getmtime(test_file)

        # Prime the cache
        cache.has_changed(
            test_file, current_modtime - 1
        )  # Different modtime to force caching

        # Test cache hit
        result = cache.has_changed(test_file, current_modtime)
        self.assertFalse(
            result, "File with current modtime in cache should return False"
        )

    def test_modtime_changed_content_same(self) -> None:
        """Critical test: modtime changes but content identical should return False."""
        cache_file = self.cache_dir / "test_cache.json"
        cache = FingerprintCache(cache_file)

        # Create test file
        test_file = self.temp_dir / "touched.txt"
        original_content = "unchanged content"
        self.create_test_file(test_file, original_content)
        original_modtime = os.path.getmtime(test_file)

        # Prime cache with original state
        cache.has_changed(test_file, original_modtime - 1)  # Force caching

        # Touch file (change modtime but not content)
        new_modtime = self.touch_file(test_file)
        self.assertNotEqual(
            new_modtime, original_modtime, "Touch should change modtime"
        )

        # Verify content is still the same
        with open(test_file, "r") as f:
            current_content = f.read()
        self.assertEqual(
            current_content, original_content, "Content should be unchanged"
        )

        # Test the critical behavior: should return False despite modtime change
        result = cache.has_changed(test_file, original_modtime)
        self.assertFalse(
            result, "File with changed modtime but same content should return False"
        )

    def test_file_copy_different_timestamp(self) -> None:
        """Test same file with different timestamps but identical content."""
        cache_file = self.cache_dir / "test_cache.json"
        cache = FingerprintCache(cache_file)

        # Create original file
        test_file = self.temp_dir / "timestamp_test.txt"
        content = "file content for timestamp testing"
        self.create_test_file(test_file, content)
        original_modtime = os.path.getmtime(test_file)

        # Prime cache with original state
        cache.has_changed(test_file, original_modtime - 1)

        # Touch file to change timestamp but keep content same
        time.sleep(0.01)
        new_modtime = time.time()
        os.utime(test_file, (new_modtime, new_modtime))

        # Verify content is still the same
        with open(test_file, "r") as f:
            current_content = f.read()
        self.assertEqual(current_content, content, "Content should be unchanged")

        # Test: different modtime, same content should return False
        result = cache.has_changed(test_file, original_modtime)
        self.assertFalse(
            result, "File with different timestamp but same content should return False"
        )

    def test_content_actually_changed(self) -> None:
        """Test that actual content changes are detected."""
        cache_file = self.cache_dir / "test_cache.json"
        cache = FingerprintCache(cache_file)

        # Create test file
        test_file = self.temp_dir / "modified.txt"
        original_content = "original content"
        self.create_test_file(test_file, original_content)
        original_modtime = os.path.getmtime(test_file)

        # Prime cache
        cache.has_changed(test_file, original_modtime - 1)

        # Actually modify content
        new_content = "modified content"
        new_modtime = self.modify_file_content(test_file, new_content)

        # Should detect change
        result = cache.has_changed(test_file, original_modtime)
        self.assertTrue(result, "File with changed content should return True")

    def test_incremental_changes(self) -> None:
        """Test detecting changes at different points in time."""
        cache_file = self.cache_dir / "test_cache.json"
        cache = FingerprintCache(cache_file)

        test_file = self.temp_dir / "incremental.txt"

        # Version 1 - baseline
        content_v1 = "version 1"
        self.create_test_file(test_file, content_v1)
        modtime_v1 = os.path.getmtime(test_file)

        # Test v1 against earlier time (should be changed)
        result = cache.has_changed(test_file, modtime_v1 - 1)
        self.assertTrue(result, "File should be detected as changed from earlier time")

        # Test v1 against same time (should be unchanged)
        result = cache.has_changed(test_file, modtime_v1)
        self.assertFalse(result, "File should be unchanged from same time")

        # Version 2 - modify content
        content_v2 = "version 2"
        modtime_v2 = self.modify_file_content(test_file, content_v2)

        # Test v2 against v1 time (should be changed)
        result = cache.has_changed(test_file, modtime_v1)
        self.assertTrue(result, "File should be detected as changed from v1 time")

        # Test v2 against v2 time (should be unchanged)
        result = cache.has_changed(test_file, modtime_v2)
        self.assertFalse(result, "File should be unchanged from v2 time")

    def test_cache_persistence(self) -> None:
        """Test that cache persists across FingerprintCache instances."""
        cache_file = self.cache_dir / "persistent.json"

        # First cache instance
        cache1 = FingerprintCache(cache_file)
        test_file = self.temp_dir / "persistent.txt"
        self.create_test_file(test_file, "persistent content")
        modtime = os.path.getmtime(test_file)

        # Prime cache
        cache1.has_changed(test_file, modtime - 1)

        # Second cache instance (reload from disk)
        cache2 = FingerprintCache(cache_file)

        # Should use cached data
        result = cache2.has_changed(test_file, modtime)
        self.assertFalse(result, "New cache instance should load existing cache")

    def test_cache_corruption_recovery(self) -> None:
        """Test graceful handling of corrupted cache files."""
        cache_file = self.cache_dir / "corrupted.json"

        # Create corrupted cache file
        with open(cache_file, "w") as f:
            f.write("{ invalid json content")

        # Should handle corruption gracefully
        cache = FingerprintCache(cache_file)
        test_file = self.temp_dir / "recovery.txt"
        self.create_test_file(test_file, "recovery content")
        modtime = os.path.getmtime(test_file)

        # Should work despite corrupted cache
        result = cache.has_changed(test_file, modtime - 1)
        self.assertTrue(result, "Should work with corrupted cache")

    def test_file_not_found_error(self) -> None:
        """Test FileNotFoundError for non-existent files."""
        cache_file = self.cache_dir / "test_cache.json"
        cache = FingerprintCache(cache_file)

        non_existent_file = self.temp_dir / "does_not_exist.txt"

        with self.assertRaises(FileNotFoundError):
            cache.has_changed(non_existent_file, time.time())

    def test_performance_cache_hit(self) -> None:
        """Benchmark cache hit performance."""
        cache_file = self.cache_dir / "perf.json"
        cache = FingerprintCache(cache_file)

        # Create test files
        test_files: List[Path] = []
        for i in range(10):  # Reduced for faster test execution
            test_file = self.temp_dir / f"perf_{i}.txt"
            self.create_test_file(test_file, f"content {i}")
            test_files.append(test_file)

        # Measure cache hit performance
        start_time = time.time()
        for test_file in test_files:
            modtime = os.path.getmtime(test_file)
            cache.has_changed(test_file, modtime)  # Cache hit
        elapsed = time.time() - start_time

        avg_time = elapsed / len(test_files) * 1000  # ms per file
        self.assertLess(
            avg_time, 1.0, f"Cache hit average {avg_time:.2f}ms should be <1.0ms"
        )

    def test_performance_large_files(self) -> None:
        """Test performance with moderately large files."""
        cache_file = self.cache_dir / "large.json"
        cache = FingerprintCache(cache_file)

        # Create moderately large test file (100KB)
        large_file = self.temp_dir / "large.txt"
        large_content = "x" * (100 * 1024)
        self.create_test_file(large_file, large_content)
        modtime = os.path.getmtime(large_file)

        # Measure MD5 computation time
        start_time = time.time()
        result = cache.has_changed(large_file, modtime - 1)  # Force MD5 computation
        elapsed = time.time() - start_time

        self.assertTrue(result, "Large file should be detected as changed")
        self.assertLess(
            elapsed,
            0.5,
            f"Large file processing {elapsed * 1000:.2f}ms should be <500ms",
        )

    def test_convenience_function(self) -> None:
        """Test the convenience has_changed function."""
        cache_file = self.cache_dir / "convenience.json"
        test_file = self.temp_dir / "convenience.txt"
        self.create_test_file(test_file, "convenience test")
        modtime = os.path.getmtime(test_file)

        # Test convenience function
        result = has_changed(test_file, modtime, cache_file)
        self.assertFalse(result, "Convenience function should work correctly")

    def test_cache_stats(self) -> None:
        """Test cache statistics functionality."""
        cache_file = self.cache_dir / "stats.json"
        cache = FingerprintCache(cache_file)

        # Initially empty cache
        stats = cache.get_cache_stats()
        self.assertEqual(stats["total_entries"], 0)
        self.assertFalse(stats["cache_file_exists"])

        # Add some entries
        test_file = self.temp_dir / "stats.txt"
        self.create_test_file(test_file, "stats content")
        cache.has_changed(test_file, time.time() - 1)

        # Check stats after entries
        stats = cache.get_cache_stats()
        self.assertEqual(stats["total_entries"], 1)
        self.assertTrue(stats["cache_file_exists"])
        self.assertGreater(stats["cache_file_size_bytes"], 0)

    def test_clear_cache(self) -> None:
        """Test cache clearing functionality."""
        cache_file = self.cache_dir / "clear.json"
        cache = FingerprintCache(cache_file)

        # Add an entry
        test_file = self.temp_dir / "clear.txt"
        self.create_test_file(test_file, "clear content")
        cache.has_changed(test_file, time.time() - 1)

        # Verify cache has entry
        self.assertEqual(len(cache.cache), 1)
        self.assertTrue(cache_file.exists())

        # Clear cache
        cache.clear_cache()

        # Verify cache is cleared
        self.assertEqual(len(cache.cache), 0)
        self.assertFalse(cache_file.exists())

    def test_deleted_file_detection(self) -> None:
        """Test detection of files that were cached but have been deleted."""
        cache_file = self.cache_dir / "deleted_test.json"
        cache = FingerprintCache(cache_file)

        # Create test files and add them to cache
        test_files = [
            self.temp_dir / "file1.txt",
            self.temp_dir / "file2.txt",
            self.temp_dir / "subdir" / "file3.txt",
        ]

        # Ensure subdir exists
        (self.temp_dir / "subdir").mkdir(exist_ok=True)

        # Create files and add to cache
        baseline_time = time.time() - 3600
        for i, test_file in enumerate(test_files):
            self.create_test_file(test_file, f"content {i}")
            cache.has_changed(test_file, baseline_time)  # Add to cache

        # Verify all files are cached
        cached_files = cache.get_cached_files()
        self.assertEqual(len(cached_files), 3, "All files should be cached")

        # Delete one file
        test_files[1].unlink()

        # Check for deleted files
        deleted_files = cache.check_for_deleted_files()
        self.assertEqual(len(deleted_files), 1, "Should detect one deleted file")
        self.assertEqual(
            deleted_files[0], test_files[1], "Should detect correct deleted file"
        )

        # Test with base_path filtering
        deleted_files_filtered = cache.check_for_deleted_files(self.temp_dir)
        self.assertEqual(
            len(deleted_files_filtered),
            1,
            "Should detect deleted file with base_path filter",
        )

        # Test with unrelated base_path
        unrelated_dir = self.test_dir / "unrelated"
        unrelated_dir.mkdir()
        deleted_files_unrelated = cache.check_for_deleted_files(unrelated_dir)
        self.assertEqual(
            len(deleted_files_unrelated), 0, "Should not detect files outside base_path"
        )

    def test_remove_deleted_files(self) -> None:
        """Test removal of deleted file entries from cache."""
        cache_file = self.cache_dir / "remove_test.json"
        cache = FingerprintCache(cache_file)

        # Create and cache test files
        test_files = [
            self.temp_dir / "keep.txt",
            self.temp_dir / "delete.txt",
        ]

        baseline_time = time.time() - 3600
        for i, test_file in enumerate(test_files):
            self.create_test_file(test_file, f"content {i}")
            cache.has_changed(test_file, baseline_time)

        # Verify both files are cached
        self.assertEqual(len(cache.cache), 2, "Both files should be cached")

        # Delete one file
        test_files[1].unlink()
        deleted_files = [test_files[1]]

        # Remove deleted file from cache
        cache.remove_deleted_files(deleted_files)

        # Verify cache was updated
        self.assertEqual(len(cache.cache), 1, "Cache should have one entry remaining")
        cached_files = cache.get_cached_files()
        self.assertEqual(
            cached_files[0], test_files[0], "Only non-deleted file should remain"
        )

        # Verify cache file was saved
        cache2 = FingerprintCache(cache_file)
        self.assertEqual(len(cache2.cache), 1, "Persisted cache should have one entry")

    def test_get_cached_files(self) -> None:
        """Test retrieval of cached file list."""
        cache_file = self.cache_dir / "cached_files_test.json"
        cache = FingerprintCache(cache_file)

        # Initially empty
        cached_files = cache.get_cached_files()
        self.assertEqual(len(cached_files), 0, "New cache should be empty")

        # Add some files
        test_files = [
            self.temp_dir / "test1.cpp",
            self.temp_dir / "test2.h",
        ]

        baseline_time = time.time() - 3600
        for i, test_file in enumerate(test_files):
            self.create_test_file(test_file, f"content {i}")
            cache.has_changed(test_file, baseline_time)

        # Check cached files
        cached_files = cache.get_cached_files()
        self.assertEqual(len(cached_files), 2, "Should return all cached files")

        # Verify files are returned as Path objects
        for cached_file in cached_files:
            self.assertIsInstance(cached_file, Path, "Should return Path objects")
            self.assertIn(cached_file, test_files, "Should return correct files")

    def test_build_system_workflow(self) -> None:
        """Test complete build system workflow."""
        cache_file = self.cache_dir / "build.json"
        cache = FingerprintCache(cache_file)

        # Simulate source files
        source_files = [
            self.temp_dir / "main.cpp",
            self.temp_dir / "utils.cpp",
            self.temp_dir / "config.h",
        ]

        # Create initial files
        for i, src_file in enumerate(source_files):
            self.create_test_file(src_file, f"source content {i}")

        # First build - all files should be "changed" (new)
        changed_files: List[Path] = []
        baseline_time = time.time() - 3600  # 1 hour ago

        for src_file in source_files:
            if cache.has_changed(src_file, baseline_time):
                changed_files.append(src_file)

        self.assertEqual(
            len(changed_files), 3, "All new files should be detected as changed"
        )

        # Second build - no changes
        current_modtimes = [os.path.getmtime(f) for f in source_files]
        changed_files: List[Path] = []

        for src_file, modtime in zip(source_files, current_modtimes):
            if cache.has_changed(src_file, modtime):
                changed_files.append(src_file)

        self.assertEqual(
            len(changed_files), 0, "Unchanged files should not be detected as changed"
        )

        # Third build - modify one file
        self.modify_file_content(source_files[0], "modified main.cpp")
        changed_files: List[Path] = []

        for src_file, modtime in zip(source_files, current_modtimes):
            if cache.has_changed(src_file, modtime):
                changed_files.append(src_file)

        self.assertEqual(len(changed_files), 1, "Only modified file should be detected")
        self.assertEqual(
            changed_files[0], source_files[0], "Should detect the correct modified file"
        )


def run_all_tests() -> bool:
    """Execute all test scenarios."""
    import unittest

    # Create test suite
    suite = unittest.TestLoader().loadTestsFromTestCase(TestFingerprintCache)

    # Run tests
    runner = unittest.TextTestRunner(verbosity=2)
    result = runner.run(suite)

    if result.wasSuccessful():
        print("✅ All fingerprint cache tests passed!")
        return True
    else:
        print("❌ Some tests failed!")
        return False


if __name__ == "__main__":
    run_all_tests()
