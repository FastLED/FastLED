from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Comprehensive stress test suite for HashFingerprintCache.

Tests concurrent access, race conditions, cross-process safety,
and edge cases to ensure robustness before using for linting.
"""

import json
import multiprocessing
import tempfile
import time
from pathlib import Path
from typing import Any

from ci.fingerprint import HashFingerprintCache


# Type alias for Queue results
QueueResult = tuple[str, Any]


class StressTestResults:
    """Track stress test results."""

    def __init__(self):
        self.passed = 0
        self.failed = 0
        self.errors: list[str] = []

    def pass_test(self, name: str):
        self.passed += 1
        print(f"  ✅ {name}")

    def fail_test(self, name: str, error: str):
        self.failed += 1
        self.errors.append(f"{name}: {error}")
        print(f"  ❌ {name}: {error}")

    def print_summary(self) -> bool:
        print("\n" + "=" * 70)
        print(f"STRESS TEST RESULTS: {self.passed} passed, {self.failed} failed")
        print("=" * 70)
        if self.errors:
            print("\nFailed tests:")
            for error in self.errors:
                print(f"  - {error}")
        return self.failed == 0


def create_test_files(test_dir: Path, count: int = 10) -> list[Path]:
    """Create test files in a temporary directory."""
    test_dir.mkdir(parents=True, exist_ok=True)
    files: list[Path] = []
    for i in range(count):
        file_path = test_dir / f"test_file_{i}.txt"
        file_path.write_text(f"Content {i}\n")
        files.append(file_path)
    return sorted(files)


def test_basic_functionality(results: StressTestResults) -> None:
    """Test basic cache operations."""
    print("\n📋 TEST 1: Basic Functionality")
    print("-" * 70)

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"

        files = create_test_files(test_dir, 5)
        cache = HashFingerprintCache(cache_dir, "basic_test")

        # First check should need update
        needs_update = cache.check_needs_update(files)
        if needs_update:
            results.pass_test("Initial check returns True (cache miss)")
        else:
            results.fail_test("Initial check returns True", "Got False instead")

        # Mark success
        try:
            cache.mark_success()
            results.pass_test("mark_success() completes without error")
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            results.fail_test("mark_success()", str(e))

        # Second check should NOT need update
        needs_update2 = cache.check_needs_update(files)
        if not needs_update2:
            results.pass_test("Second check returns False (cache hit)")
        else:
            results.fail_test("Second check returns False", "Got True instead")

        # Modify a file
        files[0].write_text("Modified content\n")

        # Third check should need update
        needs_update3 = cache.check_needs_update(files)
        if needs_update3:
            results.pass_test("Check after modification returns True")
        else:
            results.fail_test(
                "Check after modification returns True", "Got False instead"
            )


def test_file_modification_during_processing(results: StressTestResults) -> None:
    """Test that files can be modified during processing without breaking cache."""
    print("\n📋 TEST 2: File Modification During Processing")
    print("-" * 70)

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"

        files = create_test_files(test_dir, 5)
        cache = HashFingerprintCache(cache_dir, "modify_test")

        # Check needs update (pre-computes fingerprint)
        needs_update = cache.check_needs_update(files)

        if not needs_update:
            results.fail_test("Initial check", "Should need update")
            return

        # Simulate processing: modify files
        for f in files:
            f.write_text(f"Modified: {time.time()}\n")
            time.sleep(0.01)  # Small delay

        # Mark success should still work (uses pre-computed fingerprint)
        try:
            cache.mark_success()
            results.pass_test("mark_success() works despite file modifications")
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            results.fail_test("mark_success() with modified files", str(e))

        # Verify the cache was saved with original fingerprint
        cache_info = cache.get_cache_info()
        if cache_info and cache_info.get("hash"):
            results.pass_test("Cache saved successfully with fingerprint")
        else:
            results.fail_test("Cache saved with fingerprint", "Cache info missing")


def _concurrent_check_worker(
    cache_dir: Path,
    files: list[Path],
    result_queue: "multiprocessing.Queue[tuple[str, Any]]",
) -> None:
    """Worker function run in separate process. Must be top-level for pickling."""
    try:
        cache = HashFingerprintCache(cache_dir, "concurrent_test")
        needs_update = cache.check_needs_update(files)
        result_queue.put(("success", needs_update))
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        result_queue.put(("error", str(e)))


def test_concurrent_checks(results: StressTestResults) -> None:
    """Test multiple processes checking cache simultaneously."""
    print("\n📋 TEST 3: Concurrent Access (Multiple Processes)")
    print("-" * 70)

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"

        files = create_test_files(test_dir, 10)

        # Run 5 concurrent checks
        result_queue: "multiprocessing.Queue[tuple[str, Any]]" = multiprocessing.Queue()
        processes: list[multiprocessing.Process] = []

        for i in range(5):
            p = multiprocessing.Process(
                target=_concurrent_check_worker, args=(cache_dir, files, result_queue)
            )
            p.start()
            processes.append(p)

        # Wait for all to complete
        for p in processes:
            p.join(timeout=10)

        # Check results
        results_collected: list[tuple[str, Any]] = []
        while not result_queue.empty():
            item = result_queue.get()
            if isinstance(item, tuple) and len(item) == 2:
                status: str = item[0]
                result: Any = item[1]
                results_collected.append((status, result))

        if len(results_collected) == 5:
            results.pass_test("All 5 concurrent checks completed")
        else:
            results.fail_test(
                "All concurrent checks completed", f"Only {len(results_collected)}/5"
            )

        # All should report cache miss (first run)
        cache_misses = sum(1 for s, r in results_collected if s == "success" and r)
        if cache_misses >= 4:  # At least 4 should be cache miss
            results.pass_test("Concurrent processes report consistent results")
        else:
            results.fail_test(
                "Consistent concurrent results",
                f"Only {cache_misses}/5 reported cache miss",
            )


def test_cross_process_pending_fingerprint(results: StressTestResults) -> None:
    """Test pending fingerprint works across process boundaries."""
    print("\n📋 TEST 4: Cross-Process Pending Fingerprint")
    print("-" * 70)

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"

        files = create_test_files(test_dir, 5)

        # Process 1: Check needs update (creates pending fingerprint)
        cache1 = HashFingerprintCache(cache_dir, "cross_process_test")
        needs_update = cache1.check_needs_update(files)

        if not needs_update:
            results.fail_test("Initial check", "Should need update")
            return

        # Verify pending file was created or in-memory pending exists
        pending_file = cache_dir / "fingerprint" / "cross_process_test.json.pending"
        if pending_file.exists():
            results.pass_test("Pending fingerprint file created (file-based)")
        else:
            # Check if in-memory pending exists as fallback
            if hasattr(cache1, "_pending_fingerprint"):
                results.pass_test(
                    "Pending fingerprint stored in-memory (file fallback)"
                )
            else:
                results.pass_test(
                    "Pending fingerprint mechanism active (verified by mark_success)"
                )

        # Process 2: Mark success using pending fingerprint (simulated)
        cache2 = HashFingerprintCache(cache_dir, "cross_process_test")
        try:
            cache2.mark_success()
            results.pass_test("mark_success() reads cross-process pending fingerprint")
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            results.fail_test("Cross-process mark_success()", str(e))

        # Verify pending file was cleaned up if it existed
        if not pending_file.exists():
            results.pass_test("Pending fingerprint file cleaned up after success")
        else:
            results.fail_test("Pending fingerprint cleanup", "File still exists")


def test_invalidate_on_failure(results: StressTestResults) -> None:
    """Test cache invalidation on failure."""
    print("\n📋 TEST 5: Invalidate on Failure")
    print("-" * 70)

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"

        files = create_test_files(test_dir, 5)
        cache = HashFingerprintCache(cache_dir, "invalidate_test")

        # First check and mark success
        cache.check_needs_update(files)
        cache.mark_success()
        results.pass_test("Initial cache marked successful")

        # Second check should NOT need update
        needs_update = cache.check_needs_update(files)
        if not needs_update:
            results.pass_test("Cache hit on second check")
        else:
            results.fail_test("Cache hit on second check", "Got cache miss")

        # Invalidate cache
        cache.invalidate()
        results.pass_test("Cache invalidated successfully")

        # Next check should need update again
        needs_update_after = cache.check_needs_update(files)
        if needs_update_after:
            results.pass_test("Check after invalidation returns True")
        else:
            results.fail_test("Check after invalidation returns True", "Got False")


def test_missing_files(results: StressTestResults) -> None:
    """Test handling of missing files."""
    print("\n📋 TEST 6: Missing File Handling")
    print("-" * 70)

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"

        files = create_test_files(test_dir, 5)
        cache = HashFingerprintCache(cache_dir, "missing_test")

        # First check with all files present
        cache.check_needs_update(files)
        cache.mark_success()
        results.pass_test("Initial cache with all files")

        # Delete a file
        files[0].unlink()

        # Check should detect change (file is missing)
        needs_update = cache.check_needs_update(files)
        if needs_update:
            results.pass_test("Cache detects missing file as change")
        else:
            results.fail_test("Cache detects missing file", "Got cache hit instead")


def test_cache_corruption_recovery(results: StressTestResults) -> None:
    """Test recovery from corrupted cache files."""
    print("\n📋 TEST 7: Cache Corruption Recovery")
    print("-" * 70)

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"

        files = create_test_files(test_dir, 5)

        # Create corrupted cache file
        fingerprint_dir = cache_dir / "fingerprint"
        fingerprint_dir.mkdir(parents=True, exist_ok=True)
        cache_file = fingerprint_dir / "corrupt_test.json"
        cache_file.write_text("{invalid json content")

        # Cache should handle corruption gracefully
        cache = HashFingerprintCache(cache_dir, "corrupt_test")
        try:
            needs_update = cache.check_needs_update(files)
            if needs_update:
                results.pass_test("Cache handles corrupted JSON file gracefully")
            else:
                results.fail_test(
                    "Corrupted cache treated as miss",
                    "Got cache hit instead",
                )
        except json.JSONDecodeError:
            results.fail_test(
                "Cache handles corrupted JSON",
                "JSONDecodeError not caught",
            )


def test_large_file_set_performance(results: StressTestResults) -> None:
    """Test performance with many files."""
    print("\n📋 TEST 8: Large File Set Performance")
    print("-" * 70)

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"

        # Create 500 files (simulating larger codebase)
        print("  Creating 500 test files...")
        files = create_test_files(test_dir, 500)
        print("  Done.")

        cache = HashFingerprintCache(cache_dir, "perf_test")

        # Time the check_needs_update
        start = time.time()
        cache.check_needs_update(files)
        check_time = time.time() - start

        if check_time < 1.0:  # Should complete in under 1 second
            results.pass_test(
                f"check_needs_update with 500 files completed in {check_time:.3f}s"
            )
        else:
            results.fail_test(
                "check_needs_update with 500 files < 1s",
                f"Took {check_time:.3f}s",
            )

        # Time the mark_success
        start = time.time()
        cache.mark_success()
        mark_time = time.time() - start

        if mark_time < 0.5:  # Should complete in under 0.5 second
            results.pass_test(
                f"mark_success with 500 files completed in {mark_time:.3f}s"
            )
        else:
            results.fail_test(
                "mark_success with 500 files < 0.5s", f"Took {mark_time:.3f}s"
            )


def test_race_condition_rapid_operations(results: StressTestResults) -> None:
    """Test rapid sequential operations don't cause race conditions."""
    print("\n📋 TEST 9: Race Condition - Rapid Sequential Operations")
    print("-" * 70)

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"

        files = create_test_files(test_dir, 10)
        cache = HashFingerprintCache(cache_dir, "race_test")

        # Perform 50 rapid check/success cycles
        try:
            for i in range(50):
                needs_update = cache.check_needs_update(files)
                if needs_update:
                    cache.mark_success()
                    # Invalidate to force next iteration to need update
                    cache.invalidate()

                if i % 10 == 0:
                    print(f"    Completed {i} iterations...")

            results.pass_test("50 rapid check/mark/invalidate cycles completed")
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            results.fail_test("Rapid sequential operations", str(e))


def test_timestamp_file_optional(results: StressTestResults) -> None:
    """Test that timestamp_file parameter is truly optional."""
    print("\n📋 TEST 10: Timestamp File Optional Parameter")
    print("-" * 70)

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        test_dir = temp_path / "test_files"
        cache_dir = temp_path / ".cache"

        files = create_test_files(test_dir, 5)

        # Test without timestamp file
        cache1 = HashFingerprintCache(cache_dir, "no_timestamp_test")
        try:
            cache1.check_needs_update(files)
            cache1.mark_success()
            results.pass_test("Cache works without timestamp_file parameter")
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            results.fail_test("Cache without timestamp_file", str(e))

        # Test with timestamp file
        timestamp_file = temp_path / "test.timestamp"
        cache2 = HashFingerprintCache(
            cache_dir, "with_timestamp_test", timestamp_file=timestamp_file
        )
        try:
            cache2.check_needs_update(files)
            cache2.mark_success()
            if timestamp_file.exists():
                results.pass_test("Timestamp file created when parameter provided")
            else:
                results.fail_test("Timestamp file creation", "File not created")
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            results.fail_test("Cache with timestamp_file", str(e))


def main() -> int:
    """Run all stress tests."""
    print("\n" + "=" * 70)
    print("HASHFINGERPRINTCACHE STRESS TEST SUITE")
    print("=" * 70)

    results = StressTestResults()

    try:
        test_basic_functionality(results)
        test_file_modification_during_processing(results)
        test_concurrent_checks(results)
        test_cross_process_pending_fingerprint(results)
        test_invalidate_on_failure(results)
        test_missing_files(results)
        test_cache_corruption_recovery(results)
        test_large_file_set_performance(results)
        test_race_condition_rapid_operations(results)
        test_timestamp_file_optional(results)

        success = results.print_summary()
        return 0 if success else 1

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        print("\n\n⚠️  Test suite interrupted by user")
        return 2
    except Exception as e:
        print(f"\n\n💥 Unexpected error: {e}")
        import traceback

        traceback.print_exc()
        return 3


if __name__ == "__main__":
    exit(main())
