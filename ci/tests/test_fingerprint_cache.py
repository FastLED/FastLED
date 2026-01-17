"""
Comprehensive test suite for fingerprint cache functionality.

Tests cover:
- Core functionality (cache hits, misses, content changes)
- Special modes (modtime_only for PCH compilation)
- Edge cases (touched files, copied files, corruption, binary files)
- Concurrent access patterns
- Performance requirements (<0.5s total runtime)
- Integration scenarios and dataclass functionality
"""

import concurrent.futures
import json
import os
import shutil
import tempfile
import time
from pathlib import Path
from typing import Optional
from unittest import TestCase

# Import the fingerprint cache module
from ci.fingerprint.core import (
    CacheEntry,
    FingerprintCache,
    FingerprintCacheConfig,
)


def has_changed(src_path: Path, previous_modtime: float, cache_file: Path) -> bool:
    """Convenience wrapper for backward compatibility."""
    cache = FingerprintCache(cache_file)
    return cache.has_changed(src_path, previous_modtime)


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
            elapsed, 0.020, f"Cache hit should be <20ms, took {elapsed * 1000:.2f}ms"
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
        self.modify_file_content(test_file, new_content)

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
        test_files: list[Path] = []
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
            avg_time, 2.0, f"Cache hit average {avg_time:.2f}ms should be <2.0ms"
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
        changed_files: list[Path] = []
        baseline_time = time.time() - 3600  # 1 hour ago

        for src_file in source_files:
            if cache.has_changed(src_file, baseline_time):
                changed_files.append(src_file)

        self.assertEqual(
            len(changed_files), 3, "All new files should be detected as changed"
        )

        # Second build - no changes
        current_modtimes = [os.path.getmtime(f) for f in source_files]
        changed_files: list[Path] = []

        for src_file, modtime in zip(source_files, current_modtimes):
            if cache.has_changed(src_file, modtime):
                changed_files.append(src_file)

        self.assertEqual(
            len(changed_files), 0, "Unchanged files should not be detected as changed"
        )

        # Third build - modify one file
        self.modify_file_content(source_files[0], "modified main.cpp")
        changed_files: list[Path] = []

        for src_file, modtime in zip(source_files, current_modtimes):
            if cache.has_changed(src_file, modtime):
                changed_files.append(src_file)

        self.assertEqual(len(changed_files), 1, "Only modified file should be detected")
        self.assertEqual(
            changed_files[0], source_files[0], "Should detect the correct modified file"
        )

    def test_modtime_only_mode_basic(self) -> None:
        """Test basic functionality of modtime_only mode."""
        cache_file = self.cache_dir / "modtime_only.json"
        cache = FingerprintCache(cache_file, modtime_only=True)

        # Create test file
        test_file = self.temp_dir / "modtime_test.txt"
        content = "test content"
        self.create_test_file(test_file, content)
        original_modtime = os.path.getmtime(test_file)

        # Test 1: Same modtime should return False
        result = cache.has_changed(test_file, original_modtime)
        self.assertFalse(
            result, "Same modtime should return False in modtime_only mode"
        )

        # Test 2: Older reference time should return True
        result = cache.has_changed(test_file, original_modtime - 1)
        self.assertTrue(
            result, "Older reference time should return True in modtime_only mode"
        )

        # Test 3: Newer reference time should return False
        result = cache.has_changed(test_file, original_modtime + 1)
        self.assertFalse(
            result, "Newer reference time should return False in modtime_only mode"
        )

    def test_modtime_only_mode_touch_behavior(self) -> None:
        """Test that modtime_only mode treats touched files as changed."""
        cache_file = self.cache_dir / "modtime_only_touch.json"
        cache = FingerprintCache(cache_file, modtime_only=True)

        # Create test file
        test_file = self.temp_dir / "touch_test.txt"
        content = "unchanged content"
        self.create_test_file(test_file, content)
        original_modtime = os.path.getmtime(test_file)

        # Touch the file (changes modtime but not content)
        # Use manual timestamp setting to avoid sleep
        new_modtime = original_modtime + 1.0
        os.utime(test_file, (new_modtime, new_modtime))

        # Verify content is unchanged
        with open(test_file, "r") as f:
            current_content = f.read()
        self.assertEqual(current_content, content, "Content should be unchanged")

        # In modtime_only mode, this should return True (different from hash mode)
        result = cache.has_changed(test_file, original_modtime)
        self.assertTrue(
            result, "Touched file should be detected as changed in modtime_only mode"
        )

    def test_cache_entry_dataclass(self) -> None:
        """Test CacheEntry dataclass functionality."""
        entry = CacheEntry(modification_time=1234567890.0, md5_hash="abc123")

        self.assertEqual(entry.modification_time, 1234567890.0)
        self.assertEqual(entry.md5_hash, "abc123")

        # Test equality
        entry2 = CacheEntry(modification_time=1234567890.0, md5_hash="abc123")
        self.assertEqual(entry, entry2)

        # Test inequality
        entry3 = CacheEntry(modification_time=1234567890.0, md5_hash="xyz789")
        self.assertNotEqual(entry, entry3)

    def test_cache_config_dataclass(self) -> None:
        """Test FingerprintCacheConfig dataclass functionality."""
        config = FingerprintCacheConfig(
            cache_file=Path("test.json"),
            hash_algorithm="md5",
            ignore_patterns=["*.tmp", "*.log"],
            max_cache_size=1000,
        )

        self.assertEqual(config.cache_file, Path("test.json"))
        self.assertEqual(config.hash_algorithm, "md5")
        self.assertEqual(config.ignore_patterns, ["*.tmp", "*.log"])
        self.assertEqual(config.max_cache_size, 1000)

    def test_concurrent_access_same_file(self) -> None:
        """Test concurrent access to the same cache file."""
        cache_file = self.cache_dir / "concurrent.json"
        test_file = self.temp_dir / "concurrent_test.txt"
        self.create_test_file(test_file, "concurrent content")
        baseline_time = time.time() - 3600

        def check_file(thread_id: int) -> bool:
            cache = FingerprintCache(cache_file)
            return cache.has_changed(test_file, baseline_time)

        # Run concurrent checks with fewer threads for speed
        with concurrent.futures.ThreadPoolExecutor(max_workers=3) as executor:
            futures = [executor.submit(check_file, i) for i in range(3)]
            results = [
                future.result() for future in concurrent.futures.as_completed(futures)
            ]

        # The first check should return True, subsequent ones may return False due to caching
        # What's important is that at least one returned True and no exceptions occurred
        self.assertGreaterEqual(
            sum(results), 1, "At least one concurrent check should return True"
        )

        # Verify cache file exists and is valid
        self.assertTrue(
            cache_file.exists(), "Cache file should exist after concurrent access"
        )

        # Verify we can still use the cache
        cache = FingerprintCache(cache_file)
        modtime = os.path.getmtime(test_file)
        result = cache.has_changed(test_file, modtime)
        self.assertFalse(result, "Cache should still work after concurrent access")

    def test_cache_corruption_recovery_edge_cases(self) -> None:
        """Test recovery from various types of cache corruption."""
        cache_file = self.cache_dir / "corruption_test.json"
        test_file = self.temp_dir / "recovery.txt"
        self.create_test_file(test_file, "recovery content")
        baseline_time = time.time() - 3600

        # Test 1: Empty file
        with open(cache_file, "w") as f:
            f.write("")

        cache = FingerprintCache(cache_file)
        result = cache.has_changed(test_file, baseline_time)
        self.assertTrue(result, "Should work with empty cache file")

        # Test 2: Invalid JSON structure
        with open(cache_file, "w") as f:
            f.write('{"key": "value"')  # Missing closing brace

        cache = FingerprintCache(cache_file)
        result = cache.has_changed(test_file, baseline_time)
        self.assertTrue(result, "Should work with malformed JSON")

        # Test 3: Wrong data types
        with open(cache_file, "w") as f:
            json.dump(
                {
                    "test": {
                        "modification_time": "not_a_number",
                        "md5_hash": 12345,  # Should be string
                    }
                },
                f,
            )

        cache = FingerprintCache(cache_file)
        result = cache.has_changed(test_file, baseline_time)
        self.assertTrue(result, "Should work with wrong data types")

    def test_large_file_performance(self) -> None:
        """Test caching behavior with moderately large files."""
        cache_file = self.cache_dir / "large_file.json"
        cache = FingerprintCache(cache_file)

        # Create a test file (5KB) for reasonable speed
        large_file = self.temp_dir / "large.txt"
        large_content = "x" * (5 * 1024)  # 5KB
        self.create_test_file(large_file, large_content)
        baseline_time = time.time() - 3600

        # Measure performance
        start_time = time.time()
        result = cache.has_changed(large_file, baseline_time)
        elapsed = time.time() - start_time

        self.assertTrue(result, "Large file should be detected as changed")
        self.assertLess(
            elapsed,
            1.0,
            f"Large file processing took {elapsed:.2f}s, should be < 1.0s",
        )

        # Test cache hit performance
        modtime = os.path.getmtime(large_file)
        start_time = time.time()
        result = cache.has_changed(large_file, modtime)
        elapsed = time.time() - start_time

        self.assertFalse(result, "Large file cache hit should return False")
        self.assertLess(
            elapsed, 0.5, f"Cache hit took {elapsed:.3f}s, should be < 0.5s"
        )

    def test_binary_file_handling(self) -> None:
        """Test cache with binary files."""
        cache_file = self.cache_dir / "binary.json"
        cache = FingerprintCache(cache_file)

        # Create binary file
        binary_file = self.temp_dir / "test.bin"
        binary_data = bytes(range(256))  # 0-255
        with open(binary_file, "wb") as f:
            f.write(binary_data)

        baseline_time = time.time() - 3600

        # Test binary file caching
        result = cache.has_changed(binary_file, baseline_time)
        self.assertTrue(result, "Binary file should be detected as changed")

        # Test cache hit
        modtime = os.path.getmtime(binary_file)
        result = cache.has_changed(binary_file, modtime)
        self.assertFalse(result, "Binary file cache hit should return False")

        # Modify binary file
        with open(binary_file, "wb") as f:
            f.write(binary_data + b"\x00\x01")  # Add two bytes
        # Update modtime manually to avoid sleep
        os.utime(binary_file, (modtime + 1, modtime + 1))

        # Should detect change
        result = cache.has_changed(binary_file, modtime)
        self.assertTrue(result, "Modified binary file should be detected as changed")

    def test_directory_handling(self) -> None:
        """Test cache behavior with directories (expects IOError)."""
        cache_file = self.cache_dir / "directory.json"
        cache = FingerprintCache(cache_file)

        # Create a directory
        test_dir = self.temp_dir / "test_directory"
        test_dir.mkdir()
        baseline_time = time.time() - 3600

        # The fingerprint cache is designed for files only
        # Directories should raise IOError when it tries to compute MD5
        with self.assertRaises(IOError) as context:
            cache.has_changed(test_dir, baseline_time)

        # Verify the error message mentions the directory
        self.assertIn("Cannot read file", str(context.exception))

    def test_modtime_only_directory_handling(self) -> None:
        """Test directory handling in modtime_only mode."""
        cache_file = self.cache_dir / "directory_modtime_only.json"
        cache = FingerprintCache(cache_file, modtime_only=True)

        # Create a directory
        test_dir = self.temp_dir / "test_directory_modtime"
        test_dir.mkdir()

        # In modtime_only mode, directories should work fine since no hashing is attempted
        dir_modtime = os.path.getmtime(test_dir)

        # Same modtime should return False
        result = cache.has_changed(test_dir, dir_modtime)
        self.assertFalse(
            result,
            "Directory with same modtime should return False in modtime_only mode",
        )

        # Older reference time should return True
        result = cache.has_changed(test_dir, dir_modtime - 1.0)
        self.assertTrue(
            result,
            "Directory should be newer than older reference time in modtime_only mode",
        )

        # Future reference time should return False
        result = cache.has_changed(test_dir, dir_modtime + 1.0)
        self.assertFalse(
            result,
            "Directory should be older than future reference time in modtime_only mode",
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
