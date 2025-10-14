#!/usr/bin/env python3
"""
FastLED Build Script

Builds the Blink example across multiple platforms to verify compilation.
Uses local compilation by default for speed, with optional Docker support.
"""

import argparse
import os
import sys
from typing import List


def main() -> int:
    """Main function."""
    parser = argparse.ArgumentParser(
        description="Build FastLED Blink example across multiple platforms"
    )
    parser.add_argument(
        "--platforms",
        type=str,
        help="Comma-separated list of platforms to build (default: all supported)",
    )
    parser.add_argument(
        "--docker",
        action="store_true",
        help="Use Docker for compilation (slower but more reliable)",
    )
    parser.add_argument(
        "--build",
        action="store_true",
        help="Build Docker images if they don't exist (use with --docker)",
    )
    parser.add_argument(
        "--example",
        type=str,
        default="Blink",
        help="Example to compile (default: Blink)",
    )
    parser.add_argument(
        "-v", "--verbose", action="store_true", help="Enable verbose output"
    )

    args = parser.parse_args()

    # Default platforms to test
    default_platforms = ["uno", "esp32dev", "esp32s3", "esp32c3", "esp32p4", "teensy41"]

    if args.platforms:
        platforms = [p.strip() for p in args.platforms.split(",")]
    else:
        platforms = default_platforms

    # Build the base command
    docker_flags = ""
    if args.docker:
        docker_flags = " --docker"
        if args.build:
            docker_flags += " --build"
    else:
        docker_flags = " --local"

    verbose_flag = " --verbose" if args.verbose else ""

    failed_platforms: List[str] = []
    succeeded_platforms: List[str] = []

    print(f"Building {args.example} for {len(platforms)} platform(s)")
    print(f"Compilation mode: {'Docker' if args.docker else 'Local'}")
    print()

    for platform in platforms:
        command = f"bash compile {platform} {args.example}{docker_flags}{verbose_flag}"
        print(f"\n=== Building {platform} ===")
        if args.verbose:
            print(f"Command: {command}")
        print()

        result = os.system(command)

        if result != 0:
            print(f"❌ Failed: {platform} (exit code {result})")
            failed_platforms.append(platform)
        else:
            print(f"✅ Success: {platform}")
            succeeded_platforms.append(platform)

    # Print summary
    print("\n" + "=" * 60)
    print("BUILD SUMMARY")
    print("=" * 60)
    print(f"Total platforms: {len(platforms)}")
    print(f"Succeeded: {len(succeeded_platforms)}")
    print(f"Failed: {len(failed_platforms)}")

    if failed_platforms:
        print("\nFailed platforms:")
        for platform in failed_platforms:
            print(f"  ❌ {platform}")

    if succeeded_platforms:
        print("\nSucceeded platforms:")
        for platform in succeeded_platforms:
            print(f"  ✅ {platform}")

    return 1 if failed_platforms else 0


if __name__ == "__main__":
    sys.exit(main())
