#!/usr/bin/env python3
"""
Test script for unified cache approach.
"""

from pathlib import Path

from ci.util.unified_test_cache import create_src_code_cache, get_src_code_files


def test_unified_cache():
    """Test the unified cache approach."""
    print("Testing Unified Safe Fingerprint Cache")
    print("=" * 50)

    # Create cache
    cache = create_src_code_cache()

    # Get files to monitor
    src_files = get_src_code_files()
    print(f"Monitoring {len(src_files)} source files")

    # First check - should need update (no cache)
    needs_update = cache.check_needs_update(src_files)
    print(f"First check needs update: {needs_update}")

    if needs_update:
        print("Simulating successful processing...")
        # Simulate successful work
        cache.mark_success()
        print("Marked as successful")

    # Second check - should not need update
    needs_update2 = cache.check_needs_update(src_files)
    print(f"Second check needs update: {needs_update2}")

    # Touch a file to invalidate cache
    if src_files:
        test_file = src_files[0]
        print(f"Touching {test_file} to invalidate cache...")
        test_file.touch()

        # Third check - should need update again
        needs_update3 = cache.check_needs_update(src_files)
        print(f"Third check (after touch) needs update: {needs_update3}")

    print("\nUnified cache test completed!")


if __name__ == "__main__":
    test_unified_cache()
