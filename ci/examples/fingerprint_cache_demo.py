#!/usr/bin/env python3
"""
Demonstration of the Fingerprint Cache Feature

This script shows how to use the fingerprint cache for efficient file change detection
in build systems and development workflows.
"""

import os
import tempfile
import time
from pathlib import Path
from typing import List

from ci.ci.fingerprint_cache import FingerprintCache, has_changed


def demo_basic_usage() -> None:
    """Demonstrate basic fingerprint cache usage."""
    print("=== Basic Fingerprint Cache Demo ===")

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        cache_file = temp_path / "demo_cache.json"
        cache = FingerprintCache(cache_file)

        # Create a source file
        source_file = temp_path / "example.cpp"
        with open(source_file, "w") as f:
            f.write("#include <iostream>\nint main() { return 0; }")

        baseline_time = time.time() - 3600  # 1 hour ago
        current_modtime = os.path.getmtime(source_file)

        print(f"Source file: {source_file}")
        print(f"Baseline time: {baseline_time}")
        print(f"Current modtime: {current_modtime}")

        # First check - file is newer than baseline
        changed = cache.has_changed(source_file, baseline_time)
        print(f"First check (vs baseline): {changed} (expected: True)")

        # Second check - same modtime
        changed = cache.has_changed(source_file, current_modtime)
        print(f"Second check (same modtime): {changed} (expected: False)")

        # Touch file (change modtime but not content)
        time.sleep(0.01)
        source_file.touch()

        # Third check - modtime changed but content same
        changed = cache.has_changed(source_file, current_modtime)
        print(
            f"Third check (touched file): {changed} (expected: False - content unchanged)"
        )

        # Actually modify content
        time.sleep(0.01)
        with open(source_file, "w") as f:
            f.write(
                '#include <iostream>\nint main() { std::cout << "Hello"; return 0; }'
            )

        # Fourth check - content actually changed
        changed = cache.has_changed(source_file, current_modtime)
        print(f"Fourth check (content changed): {changed} (expected: True)")


def demo_build_system_workflow() -> None:
    """Demonstrate build system integration workflow."""
    print("\n=== Build System Workflow Demo ===")

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        cache_file = temp_path / "build_cache.json"
        cache = FingerprintCache(cache_file)

        # Simulate multiple source files
        source_files = [
            temp_path / "main.cpp",
            temp_path / "utils.cpp",
            temp_path / "config.h",
        ]

        # Create source files
        contents = [
            '#include "config.h"\n#include "utils.h"\nint main() { return 0; }',
            '#include "utils.h"\nvoid utility_function() {}',
            '#pragma once\n#define VERSION "1.0"',
        ]

        for src_file, content in zip(source_files, contents):
            with open(src_file, "w") as f:
                f.write(content)

        print(f"Created {len(source_files)} source files")

        # Simulate first build
        print("\n--- First Build (all files new) ---")
        last_build_time = time.time() - 3600  # 1 hour ago
        changed_files: List[str] = []

        for src_file in source_files:
            if cache.has_changed(src_file, last_build_time):
                changed_files.append(src_file.name)

        print(
            f"Files to rebuild: {changed_files} ({len(changed_files)}/{len(source_files)})"
        )

        # Update last build time
        current_modtimes = [os.path.getmtime(f) for f in source_files]

        # Simulate second build (no changes)
        print("\n--- Second Build (no changes) ---")
        changed_files: List[str] = []

        for src_file, modtime in zip(source_files, current_modtimes):
            if cache.has_changed(src_file, modtime):
                changed_files.append(src_file.name)

        print(
            f"Files to rebuild: {changed_files} ({len(changed_files)}/{len(source_files)})"
        )

        # Modify one file
        print("\n--- Third Build (one file modified) ---")
        time.sleep(0.01)
        with open(source_files[0], "a") as f:
            f.write("\n// Added comment")

        changed_files: List[str] = []
        for src_file, modtime in zip(source_files, current_modtimes):
            if cache.has_changed(src_file, modtime):
                changed_files.append(src_file.name)

        print(
            f"Files to rebuild: {changed_files} ({len(changed_files)}/{len(source_files)})"
        )


def demo_performance() -> None:
    """Demonstrate cache performance characteristics."""
    print("\n=== Performance Demo ===")

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        cache_file = temp_path / "perf_cache.json"
        cache = FingerprintCache(cache_file)

        # Create multiple test files
        num_files = 20
        test_files: List[Path] = []

        for i in range(num_files):
            test_file = temp_path / f"file_{i:03d}.cpp"
            with open(test_file, "w") as f:
                f.write(
                    f"// File {i}\n#include <iostream>\nint func_{i}() {{ return {i}; }}"
                )
            test_files.append(test_file)

        print(f"Created {num_files} test files")

        # Measure cache miss performance (first check)
        start_time = time.time()
        baseline_time = time.time() - 3600

        for test_file in test_files:
            cache.has_changed(test_file, baseline_time)

        miss_time = time.time() - start_time
        print(
            f"Cache miss time: {miss_time * 1000:.1f}ms ({miss_time * 1000 / num_files:.2f}ms per file)"
        )

        # Measure cache hit performance (second check with same modtime)
        modtimes = [os.path.getmtime(f) for f in test_files]

        start_time = time.time()
        for test_file, modtime in zip(test_files, modtimes):
            cache.has_changed(test_file, modtime)

        hit_time = time.time() - start_time
        print(
            f"Cache hit time: {hit_time * 1000:.1f}ms ({hit_time * 1000 / num_files:.2f}ms per file)"
        )
        print(f"Speedup: {miss_time / hit_time:.1f}x faster")

        # Show cache statistics
        stats = cache.get_cache_stats()
        print(f"Cache entries: {stats['total_entries']}")
        print(f"Cache file size: {stats['cache_file_size_bytes']} bytes")


def demo_convenience_function() -> None:
    """Demonstrate the convenience has_changed function."""
    print("\n=== Convenience Function Demo ===")

    with tempfile.TemporaryDirectory() as temp_dir:
        temp_path = Path(temp_dir)
        cache_file = temp_path / "convenience_cache.json"

        # Create test file
        test_file = temp_path / "test.h"
        with open(test_file, "w") as f:
            f.write("#pragma once\n#define API_VERSION 1")

        baseline_time = time.time() - 1800  # 30 minutes ago

        # Use convenience function
        changed = has_changed(test_file, baseline_time, cache_file)
        print(f"Convenience function result: {changed}")
        print(f"Cache file created: {cache_file.exists()}")


if __name__ == "__main__":
    print("Fingerprint Cache Feature Demonstration")
    print("=" * 50)

    demo_basic_usage()
    demo_build_system_workflow()
    demo_performance()
    demo_convenience_function()

    print("\n" + "=" * 50)
    print("Demo completed successfully!")
    print("\nKey Benefits:")
    print("- Fast modification time checks avoid expensive MD5 computation")
    print("- Content-based verification prevents false positives")
    print("- Persistent cache survives across tool invocations")
    print("- Significant speedup for incremental builds and testing")
