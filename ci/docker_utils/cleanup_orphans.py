#!/usr/bin/env python3
"""Clean up orphaned Docker containers.

This script cleans up containers from dead processes by:
1. Scanning the container tracking database
2. Identifying containers whose owner PIDs no longer exist
3. Stopping and removing orphaned containers
4. Cleaning up database records

Usage:
    uv run python -m ci.docker.cleanup_orphans
    uv run python -m ci.docker.cleanup_orphans --verbose
"""

import argparse
import sys

from ci.docker_utils.container_db import ContainerDatabase, cleanup_orphaned_containers


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Clean up orphaned Docker containers from dead processes"
    )
    parser.add_argument(
        "-v",
        "--verbose",
        action="store_true",
        help="Print verbose output",
    )
    args = parser.parse_args()

    if args.verbose:
        print("Scanning for orphaned containers...")
        print()

    db = ContainerDatabase()
    cleaned_count = cleanup_orphaned_containers(db)

    if cleaned_count > 0:
        print()
        print(f"✓ Cleaned up {cleaned_count} orphaned container(s)")
    else:
        if args.verbose:
            print("No orphaned containers found")

    return 0


if __name__ == "__main__":
    sys.exit(main())
