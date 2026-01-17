#!/usr/bin/env python3

import argparse
import os
import subprocess
import sys
import time
from pathlib import Path

from ci.compiler.native_fingerprint import NativeCompilationCache
from ci.util.pio_runner import run_pio_command


def parse_args():
    parser = argparse.ArgumentParser(
        description="Compile native host examples with fingerprint caching"
    )
    parser.add_argument(
        "--force",
        action="store_true",
        help="Force rebuild even if fingerprint cache indicates no changes",
    )
    parser.add_argument(
        "--cache-stats", action="store_true", help="Show cache statistics and exit"
    )
    parser.add_argument(
        "--clear-cache", action="store_true", help="Clear fingerprint cache and exit"
    )
    parser.add_argument(
        "--example", help="Specific example to build (for targeted fingerprinting)"
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Enable verbose output"
    )
    return parser.parse_args()


def main():
    args = parse_args()

    # Initialize fingerprint cache
    cache = NativeCompilationCache()

    # Handle cache management commands
    if args.cache_stats:
        stats = cache.get_cache_stats()
        print("Native Compilation Cache Statistics:")
        print(f"  File cache entries: {stats['file_cache_entries']}")
        print(f"  Build cache entries: {stats['build_cache_entries']}")
        print(f"  Cache directory: {stats['cache_dir']}")
        print(f"  Build cache size: {stats['build_cache_size_bytes']} bytes")
        return 0

    if args.clear_cache:
        cache.clear_cache()
        print("Native compilation cache cleared")
        return 0

    # Change to the directory of the script
    script_dir = Path(__file__).parent
    os.chdir(str(script_dir))

    # Determine build identifier
    example_path: Path | None = None
    if args.example:
        example_path = Path.cwd().parent / "examples" / args.example
        build_id = f"example_{args.example}"
    else:
        build_id = "default"

    # Check if rebuild is needed (unless forced)
    if not args.force:
        # Make sure we're in the project root before checking fingerprints
        project_root = script_dir.parent
        original_cwd = os.getcwd()
        os.chdir(str(project_root))

        try:
            start_time = time.time()
            needs_rebuild = cache.has_build_changed(example_path, build_id)
            fingerprint_time = time.time() - start_time

            if args.verbose:
                print(f"Fingerprint check completed in {fingerprint_time:.3f}s")

            if not needs_rebuild:
                print("✓ Native build cache is up-to-date (no changes detected)")
                if args.verbose:
                    fingerprint = cache.get_build_fingerprint(example_path)
                    print(f"  Build fingerprint: {fingerprint.combined_hash[:16]}...")
                return 0

            if args.verbose:
                print("⚡ Changes detected, proceeding with native compilation")
        finally:
            os.chdir(original_cwd)

    # Change to the 'native' directory and run 'pio run'
    native_dir = script_dir / "native"
    os.chdir(str(native_dir))

    start_time = time.time()

    try:
        if args.verbose:
            print("Running PlatformIO native compilation...")
        # Use run_pio_command for proper process tracking and atexit cleanup
        result = run_pio_command(["pio", "run"], cwd=native_dir)

        compile_time = time.time() - start_time

        if result.returncode != 0:
            print(
                f"✗ Native compilation failed after {compile_time:.2f}s (exit code {result.returncode})"
            )
            cache.invalidate_build(build_id)
            return result.returncode

        if args.verbose:
            print(f"✓ Native compilation completed in {compile_time:.2f}s")

        # Update cache only after successful compilation
        cache.mark_build_success(example_path, build_id)

        return result.returncode

    except subprocess.CalledProcessError as e:
        compile_time = time.time() - start_time
        print(
            f"✗ Native compilation failed after {compile_time:.2f}s (exit code {e.returncode})"
        )
        # Invalidate cache on failure
        cache.invalidate_build(build_id)
        return e.returncode


if __name__ == "__main__":
    sys.exit(main())
