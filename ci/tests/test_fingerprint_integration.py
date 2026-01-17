"""
Comprehensive integration and stress tests for all fingerprint cache types.

Tests cover:
- All three cache types (FingerprintCache, TwoLayerFingerprintCache, HashFingerprintCache)
- Integration scenarios (switching between cache types)
- Stress tests (rapid operations, large file sets, edge cases)
- Performance benchmarks
- Accuracy validation
"""

import tempfile
import time
from pathlib import Path

import pytest

from ci.fingerprint import (
    FingerprintCache,
    HashFingerprintCache,
    TwoLayerFingerprintCache,
)


def create_test_files(test_dir: Path, count: int = 10) -> list[Path]:
    """Create test files in a temporary directory."""
    test_dir.mkdir(parents=True, exist_ok=True)
    files: list[Path] = []
    for i in range(count):
        file_path = test_dir / f"test_file_{i}.txt"
        file_path.write_text(f"Content {i}\n")
        files.append(file_path)
    return sorted(files)


def test_all_caches_basic_functionality() -> None:
    """Test basic functionality across all three cache types."""
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"
        cache_dir.mkdir(exist_ok=True)

        files = create_test_files(test_dir, 5)

        # Test FingerprintCache (legacy)
        cache_file = cache_dir / "fingerprint_cache.json"
        fp_cache = FingerprintCache(cache_file)
        baseline = time.time() - 3600

        changed_files = [f for f in files if fp_cache.has_changed(f, baseline)]
        assert len(changed_files) == 5, (
            f"FingerprintCache: Expected 5 changed files, got {len(changed_files)}"
        )

        # Test TwoLayerFingerprintCache
        two_layer = TwoLayerFingerprintCache(cache_dir, "two_layer_test")
        needs_update = two_layer.check_needs_update(files)
        assert needs_update, (
            "TwoLayerFingerprintCache: Expected cache miss on first check"
        )

        two_layer.mark_success()
        needs_update = two_layer.check_needs_update(files)
        assert not needs_update, (
            "TwoLayerFingerprintCache: Expected cache hit on second check"
        )

        # Test HashFingerprintCache
        hash_cache = HashFingerprintCache(cache_dir, "hash_test")
        needs_update = hash_cache.check_needs_update(files)
        assert needs_update, "HashFingerprintCache: Expected cache miss on first check"

        hash_cache.mark_success()
        needs_update = hash_cache.check_needs_update(files)
        assert not needs_update, (
            "HashFingerprintCache: Expected cache hit on second check"
        )


def test_touch_optimization_comparison() -> None:
    """Compare touch optimization across TwoLayer and Hash caches."""
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"
        cache_dir.mkdir(exist_ok=True)

        files = create_test_files(test_dir, 3)

        # TwoLayerFingerprintCache
        two_layer = TwoLayerFingerprintCache(cache_dir, "touch_two_layer")
        two_layer.check_needs_update(files)
        two_layer.mark_success()

        # Touch files
        time.sleep(0.01)
        for f in files:
            f.touch()

        # Should NOT need update (touch optimization)
        needs_update = two_layer.check_needs_update(files)
        assert not needs_update, (
            "TwoLayerFingerprintCache: Touch optimization should not trigger update"
        )

        # HashFingerprintCache
        hash_cache = HashFingerprintCache(cache_dir, "touch_hash")
        hash_cache.check_needs_update(files)
        hash_cache.mark_success()

        # Touch files
        time.sleep(0.01)
        for f in files:
            f.touch()

        # WILL need update (uses mtime in hash)
        needs_update = hash_cache.check_needs_update(files)
        assert needs_update, "HashFingerprintCache: Should detect mtime change"


def test_performance_comparison() -> None:
    """Compare performance across all cache types."""
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"
        cache_dir.mkdir(exist_ok=True)

        # Create 100 files for performance testing
        files = create_test_files(test_dir, 100)

        # TwoLayerFingerprintCache performance
        two_layer = TwoLayerFingerprintCache(cache_dir, "perf_two_layer")
        start = time.time()
        two_layer.check_needs_update(files)
        two_layer_time = time.time() - start
        two_layer.mark_success()

        # Cache hit performance
        start = time.time()
        two_layer.check_needs_update(files)
        two_layer_hit_time = time.time() - start

        assert two_layer_time < 15.0, (
            f"TwoLayerFingerprintCache first check too slow: {two_layer_time:.3f}s"
        )
        assert two_layer_hit_time < 0.5, (
            f"TwoLayerFingerprintCache cache hit too slow: {two_layer_hit_time:.3f}s"
        )

        # HashFingerprintCache performance
        hash_cache = HashFingerprintCache(cache_dir, "perf_hash")
        start = time.time()
        hash_cache.check_needs_update(files)
        hash_time = time.time() - start
        hash_cache.mark_success()

        # Cache hit performance
        start = time.time()
        hash_cache.check_needs_update(files)
        hash_hit_time = time.time() - start

        assert hash_time < 0.5, (
            f"HashFingerprintCache first check too slow: {hash_time:.3f}s"
        )
        assert hash_hit_time < 0.5, (
            f"HashFingerprintCache cache hit too slow: {hash_hit_time:.3f}s"
        )


def test_accuracy_all_caches() -> None:
    """Test accuracy of change detection across all cache types."""
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"
        cache_dir.mkdir(exist_ok=True)

        files = create_test_files(test_dir, 3)

        # TwoLayerFingerprintCache accuracy
        two_layer = TwoLayerFingerprintCache(cache_dir, "accuracy_two_layer")
        two_layer.check_needs_update(files)
        two_layer.mark_success()

        # Modify one file
        files[1].write_text("Modified content\n")

        needs_update = two_layer.check_needs_update(files)
        assert needs_update, (
            "TwoLayerFingerprintCache: Should detect single file change"
        )

        # HashFingerprintCache accuracy
        hash_cache = HashFingerprintCache(cache_dir, "accuracy_hash")
        hash_cache.check_needs_update(files)
        hash_cache.mark_success()

        # Modify different file
        files[2].write_text("Different modified content\n")

        needs_update = hash_cache.check_needs_update(files)
        assert needs_update, "HashFingerprintCache: Should detect single file change"


def test_stress_rapid_operations() -> None:
    """Stress test with rapid cache operations."""
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"
        cache_dir.mkdir(exist_ok=True)

        files = create_test_files(test_dir, 10)

        # TwoLayerFingerprintCache stress
        two_layer = TwoLayerFingerprintCache(cache_dir, "stress_two_layer")
        for i in range(20):
            needs_update = two_layer.check_needs_update(files)
            if needs_update:
                two_layer.mark_success()
            two_layer.invalidate()

        # HashFingerprintCache stress
        hash_cache = HashFingerprintCache(cache_dir, "stress_hash")
        for i in range(20):
            needs_update = hash_cache.check_needs_update(files)
            if needs_update:
                hash_cache.mark_success()
            hash_cache.invalidate()


def test_stress_large_files() -> None:
    """Stress test with larger files."""
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"
        cache_dir.mkdir(exist_ok=True)

        # Create files with larger content (10KB each)
        files: list[Path] = []
        for i in range(5):
            file_path = test_dir / f"large_file_{i}.txt"
            file_path.parent.mkdir(parents=True, exist_ok=True)
            large_content = f"Line {i}\n" * 1000  # ~10KB
            file_path.write_text(large_content)
            files.append(file_path)

        # TwoLayerFingerprintCache with large files
        two_layer = TwoLayerFingerprintCache(cache_dir, "large_two_layer")
        start = time.time()
        needs_update = two_layer.check_needs_update(files)
        elapsed = time.time() - start

        assert needs_update, "TwoLayerFingerprintCache: Should detect new large files"
        assert elapsed < 2.0, (
            f"TwoLayerFingerprintCache: Large files took {elapsed:.3f}s (expected < 2.0s)"
        )

        # HashFingerprintCache with large files
        hash_cache = HashFingerprintCache(cache_dir, "large_hash")
        start = time.time()
        needs_update = hash_cache.check_needs_update(files)
        elapsed = time.time() - start

        assert needs_update, "HashFingerprintCache: Should detect new large files"
        assert elapsed < 1.0, (
            f"HashFingerprintCache: Large files took {elapsed:.3f}s (expected < 1.0s)"
        )


def test_edge_case_symlinks() -> None:
    """Test edge case with symlinks (if supported)."""
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"
        cache_dir.mkdir(exist_ok=True)

        # Create original files
        original = test_dir / "original.txt"
        original.parent.mkdir(parents=True, exist_ok=True)
        original.write_text("Original content")

        # Try to create symlink
        symlink = test_dir / "symlink.txt"
        try:
            symlink.symlink_to(original)
        except (OSError, NotImplementedError):
            pytest.skip("Symlinks not supported on this platform")

        # Test with TwoLayerFingerprintCache
        two_layer = TwoLayerFingerprintCache(cache_dir, "symlink_two_layer")
        needs_update = two_layer.check_needs_update([symlink])
        two_layer.mark_success()

        # Test with HashFingerprintCache
        hash_cache = HashFingerprintCache(cache_dir, "symlink_hash")
        needs_update = hash_cache.check_needs_update([symlink])
        hash_cache.mark_success()


def test_edge_case_unicode_filenames() -> None:
    """Test edge case with Unicode filenames."""
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"
        cache_dir.mkdir(exist_ok=True)

        # Create files with Unicode names
        unicode_files = [
            test_dir / "测试文件.txt",  # Chinese
            test_dir / "файл.txt",  # Russian
            test_dir / "αρχείο.txt",  # Greek
        ]

        for f in unicode_files:
            f.parent.mkdir(parents=True, exist_ok=True)
            f.write_text("Unicode filename test\n")

        # TwoLayerFingerprintCache with Unicode
        two_layer = TwoLayerFingerprintCache(cache_dir, "unicode_two_layer")
        needs_update = two_layer.check_needs_update(unicode_files)
        two_layer.mark_success()

        # HashFingerprintCache with Unicode
        hash_cache = HashFingerprintCache(cache_dir, "unicode_hash")
        needs_update = hash_cache.check_needs_update(unicode_files)
        hash_cache.mark_success()


def test_consistency_after_crash() -> None:
    """Test cache consistency after simulated crash (no mark_success)."""
    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"
        cache_dir.mkdir(exist_ok=True)

        files = create_test_files(test_dir, 3)

        # TwoLayerFingerprintCache - check but don't mark success (simulated crash)
        two_layer = TwoLayerFingerprintCache(cache_dir, "crash_two_layer")
        needs_update = two_layer.check_needs_update(files)
        # Intentionally skip mark_success()

        # Create new instance - should still need update
        two_layer2 = TwoLayerFingerprintCache(cache_dir, "crash_two_layer")
        needs_update = two_layer2.check_needs_update(files)
        assert needs_update, (
            "TwoLayerFingerprintCache: Should still need update after crash"
        )

        # HashFingerprintCache - check but don't mark success
        hash_cache = HashFingerprintCache(cache_dir, "crash_hash")
        needs_update = hash_cache.check_needs_update(files)
        # Intentionally skip mark_success()

        # Create new instance - should still need update
        hash_cache2 = HashFingerprintCache(cache_dir, "crash_hash")
        needs_update = hash_cache2.check_needs_update(files)
        assert needs_update, (
            "HashFingerprintCache: Should still need update after crash"
        )
