#!/usr/bin/env python3
"""
Unit tests for link cache garbage collection system

Tests the LinkCacheGC class and related functionality to ensure
proper cache cleanup and policy enforcement.
"""

import tempfile
import time
from pathlib import Path
from typing import Generator, List

import pytest

from ci.compiler.link_cache_gc import (
    CacheEntry,
    CachePolicy,
    GCStats,
    LinkCacheGC,
    run_link_cache_gc,
)


@pytest.fixture
def temp_cache_dir() -> Generator[Path, None, None]:
    """Create a temporary cache directory for testing"""
    with tempfile.TemporaryDirectory() as tmpdir:
        yield Path(tmpdir)


@pytest.fixture
def sample_policy() -> CachePolicy:
    """Create a sample cache policy for testing"""
    return CachePolicy(
        max_versions_per_example=3,
        max_age_days=7,
        max_total_size_mb=10,  # 10MB for testing
        preserve_at_least_one=True,
    )


def create_test_cache_file(
    cache_dir: Path, example_name: str, cache_key: str, size_bytes: int = 1024
) -> Path:
    """Helper to create a test cache file"""
    filename = f"{example_name}_{cache_key}.exe"
    file_path = cache_dir / filename

    # Create file with specific size
    with open(file_path, "wb") as f:
        f.write(b"X" * size_bytes)

    return file_path


def set_file_mtime(file_path: Path, days_ago: int) -> None:
    """Helper to set a file's modification time to N days ago"""
    mtime = time.time() - (days_ago * 86400)
    file_path.touch()
    import os

    os.utime(file_path, (mtime, mtime))


class TestCacheEntry:
    """Test CacheEntry data class"""

    def test_cache_entry_creation(self, temp_cache_dir: Path) -> None:
        """Test creating a cache entry"""
        file_path = create_test_cache_file(
            temp_cache_dir, "TestExample", "1234567890abcdef"
        )

        stat_info = file_path.stat()
        entry = CacheEntry(
            path=file_path,
            example_name="TestExample",
            cache_key="1234567890abcdef",
            size_bytes=stat_info.st_size,
            mtime=stat_info.st_mtime,
        )

        assert entry.example_name == "TestExample"
        assert entry.cache_key == "1234567890abcdef"
        assert entry.size_bytes == 1024
        assert entry.age_days >= 0

    def test_age_calculation(self, temp_cache_dir: Path) -> None:
        """Test age calculation for cache entries"""
        file_path = create_test_cache_file(
            temp_cache_dir, "TestExample", "1234567890abcdef"
        )

        # Set file to 10 days old
        set_file_mtime(file_path, 10)

        stat_info = file_path.stat()
        entry = CacheEntry(
            path=file_path,
            example_name="TestExample",
            cache_key="1234567890abcdef",
            size_bytes=stat_info.st_size,
            mtime=stat_info.st_mtime,
        )

        assert entry.age_days >= 9.9  # Allow small timing variance
        assert entry.age_days <= 10.1


class TestCachePolicy:
    """Test CachePolicy configuration"""

    def test_default_policy(self) -> None:
        """Test default policy values"""
        policy = CachePolicy()

        assert policy.max_versions_per_example == 3
        assert policy.max_age_days == 7
        assert policy.max_total_size_mb == 1024
        assert policy.preserve_at_least_one is True

    def test_custom_policy(self) -> None:
        """Test custom policy values"""
        policy = CachePolicy(
            max_versions_per_example=5,
            max_age_days=14,
            max_total_size_mb=2048,
            preserve_at_least_one=False,
        )

        assert policy.max_versions_per_example == 5
        assert policy.max_age_days == 14
        assert policy.max_total_size_mb == 2048
        assert policy.preserve_at_least_one is False

    def test_from_toml(self) -> None:
        """Test loading policy from TOML config"""
        config = {
            "cache": {
                "max_versions_per_example": 5,
                "max_age_days": 14,
                "max_total_size_mb": 512,
                "preserve_at_least_one": False,
            }
        }

        policy = CachePolicy.from_toml(config)

        assert policy.max_versions_per_example == 5
        assert policy.max_age_days == 14
        assert policy.max_total_size_mb == 512
        assert policy.preserve_at_least_one is False

    def test_from_toml_with_defaults(self) -> None:
        """Test loading policy from TOML with missing fields"""
        config = {"cache": {"max_versions_per_example": 5}}

        policy = CachePolicy.from_toml(config)

        assert policy.max_versions_per_example == 5
        assert policy.max_age_days == 7  # Default
        assert policy.max_total_size_mb == 1024  # Default


class TestLinkCacheGC:
    """Test LinkCacheGC garbage collector"""

    def test_scan_empty_cache(
        self, temp_cache_dir: Path, sample_policy: CachePolicy
    ) -> None:
        """Test scanning an empty cache directory"""
        gc = LinkCacheGC(temp_cache_dir, sample_policy)
        entries_by_example = gc.scan_cache()

        assert len(entries_by_example) == 0

    def test_scan_cache_with_files(
        self, temp_cache_dir: Path, sample_policy: CachePolicy
    ) -> None:
        """Test scanning cache with multiple files"""
        # Create test files
        create_test_cache_file(temp_cache_dir, "Example1", "1234567890abcdef")
        create_test_cache_file(temp_cache_dir, "Example1", "abcdef1234567890")
        create_test_cache_file(temp_cache_dir, "Example2", "fedcba0987654321")

        gc = LinkCacheGC(temp_cache_dir, sample_policy)
        entries_by_example = gc.scan_cache()

        assert len(entries_by_example) == 2
        assert "Example1" in entries_by_example
        assert "Example2" in entries_by_example
        assert len(entries_by_example["Example1"]) == 2
        assert len(entries_by_example["Example2"]) == 1

    def test_version_limit_enforcement(self, temp_cache_dir: Path) -> None:
        """Test per-example version limit enforcement"""
        policy = CachePolicy(max_versions_per_example=2)

        # Create 4 versions of the same example
        for i in range(4):
            file_path = create_test_cache_file(
                temp_cache_dir, "Example1", f"000000000000000{i}"
            )
            # Set different mtimes (newer files first)
            set_file_mtime(file_path, 3 - i)

        gc = LinkCacheGC(temp_cache_dir, policy)
        entries_by_example = gc.scan_cache()
        to_remove, to_keep = gc.identify_removable_entries(entries_by_example)

        # Should keep 2 most recent, remove 2 oldest
        assert len(to_keep) == 2
        assert len(to_remove) == 2

    def test_age_limit_enforcement(self, temp_cache_dir: Path) -> None:
        """Test age-based cleanup"""
        policy = CachePolicy(max_age_days=7, max_versions_per_example=10)

        # Create files with different ages
        old_file = create_test_cache_file(
            temp_cache_dir, "Example1", "0000000000000000"
        )
        set_file_mtime(old_file, 10)  # 10 days old

        recent_file = create_test_cache_file(
            temp_cache_dir, "Example1", "1111111111111111"
        )
        set_file_mtime(recent_file, 3)  # 3 days old

        gc = LinkCacheGC(temp_cache_dir, policy)
        entries_by_example = gc.scan_cache()
        to_remove, to_keep = gc.identify_removable_entries(entries_by_example)

        # Old file should be removed, recent file kept
        assert len(to_keep) == 1
        assert len(to_remove) == 1
        assert to_keep[0].cache_key == "1111111111111111"

    def test_preserve_at_least_one(self, temp_cache_dir: Path) -> None:
        """Test that at least one version is preserved"""
        policy = CachePolicy(max_age_days=1, preserve_at_least_one=True)

        # Create single old file
        old_file = create_test_cache_file(
            temp_cache_dir, "Example1", "0000000000000000"
        )
        set_file_mtime(old_file, 10)

        gc = LinkCacheGC(temp_cache_dir, policy)
        entries_by_example = gc.scan_cache()
        to_remove, to_keep = gc.identify_removable_entries(entries_by_example)

        # Should preserve the file even though it's too old
        assert len(to_keep) == 1
        assert len(to_remove) == 0

    def test_size_limit_enforcement(self, temp_cache_dir: Path) -> None:
        """Test total size limit enforcement"""
        policy = CachePolicy(
            max_total_size_mb=1, max_versions_per_example=10
        )  # 1MB limit

        # Create files that exceed size limit (3 files x 512KB = 1.5MB > 1MB)
        for i in range(3):
            file_path = create_test_cache_file(
                temp_cache_dir, "Example1", f"000000000000000{i}", size_bytes=512 * 1024
            )
            set_file_mtime(file_path, 2 - i)

        gc = LinkCacheGC(temp_cache_dir, policy)
        entries_by_example = gc.scan_cache()
        to_remove, to_keep = gc.identify_removable_entries(entries_by_example)

        # Should remove files to get under size limit
        total_kept_size = sum(e.size_bytes for e in to_keep)
        assert total_kept_size <= policy.max_total_size_mb * 1024 * 1024

    def test_run_gc_dry_run(
        self, temp_cache_dir: Path, sample_policy: CachePolicy
    ) -> None:
        """Test dry run mode (no actual deletions)"""
        # Create test files
        for i in range(5):
            create_test_cache_file(temp_cache_dir, "Example1", f"000000000000000{i}")

        gc = LinkCacheGC(temp_cache_dir, sample_policy)
        stats = gc.run_gc(dry_run=True)

        # Files should still exist after dry run
        remaining_files = list(temp_cache_dir.glob("*.exe"))
        assert len(remaining_files) == 5
        assert stats.files_removed > 0  # Stats should show what WOULD be removed

    def test_run_gc_actual_deletion(self, temp_cache_dir: Path) -> None:
        """Test actual file deletion"""
        policy = CachePolicy(max_versions_per_example=2)

        # Create 4 versions
        for i in range(4):
            create_test_cache_file(temp_cache_dir, "Example1", f"000000000000000{i}")

        gc = LinkCacheGC(temp_cache_dir, policy)
        stats = gc.run_gc(dry_run=False)

        # Should have removed 2 files, kept 2
        remaining_files = list(temp_cache_dir.glob("*.exe"))
        assert len(remaining_files) == 2
        assert stats.files_removed == 2
        assert stats.files_kept == 2


class TestIntegration:
    """Integration tests for the GC system"""

    def test_run_link_cache_gc_convenience_function(
        self, temp_cache_dir: Path, sample_policy: CachePolicy
    ) -> None:
        """Test the convenience function"""
        # Create test files
        for i in range(5):
            create_test_cache_file(temp_cache_dir, "Example1", f"000000000000000{i}")

        stats = run_link_cache_gc(
            cache_dir=temp_cache_dir, policy=sample_policy, dry_run=False
        )

        assert isinstance(stats, GCStats)
        assert stats.files_removed > 0

    def test_multiple_examples(self, temp_cache_dir: Path) -> None:
        """Test GC with multiple examples"""
        policy = CachePolicy(max_versions_per_example=2)

        # Create files for multiple examples
        for example in ["Blink", "Blur", "XYMatrix"]:
            for i in range(4):
                create_test_cache_file(temp_cache_dir, example, f"000000000000000{i}")

        gc = LinkCacheGC(temp_cache_dir, policy)
        stats = gc.run_gc(dry_run=False)

        # Each example should have 2 versions kept, 2 removed
        remaining_files = list(temp_cache_dir.glob("*.exe"))
        assert len(remaining_files) == 6  # 3 examples * 2 versions
        assert stats.files_removed == 6  # 3 examples * 2 removed


if __name__ == "__main__":
    pytest.main([__file__, "-v"])
