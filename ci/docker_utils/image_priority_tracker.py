from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


#!/usr/bin/env python3
"""
Docker Image Priority Tracker

Tracks Docker image build timestamps and prioritizes images that:
1. Have never been built (new images)
2. Haven't been updated in the longest time (stale images)

This enables smart scheduling where we build the most important images
on each 24-hour cycle, ensuring all images stay reasonably up-to-date
without overwhelming the build system.

Usage:
    # Check which images need building (prioritized)
    python ci/docker_utils/image_priority_tracker.py --check --limit 5

    # Record successful build
    python ci/docker_utils/image_priority_tracker.py --record esp-32s3

    # Get JSON output for GitHub Actions
    python ci/docker_utils/image_priority_tracker.py --check --limit 5 --json
"""

import argparse
import json
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any

from ci.docker_utils.build_platforms import DOCKER_PLATFORMS, get_docker_image_name


def get_cache_file() -> Path:
    """Get path to the build timestamp cache file."""
    # Store in ci/docker directory as .build_timestamps.json
    return Path(__file__).parent / ".build_timestamps.json"


def load_build_timestamps() -> dict[str, str]:
    """
    Load build timestamp cache from disk.

    Returns:
        Dictionary mapping platform name to ISO timestamp string
    """
    cache_file = get_cache_file()

    if not cache_file.exists():
        return {}

    try:
        with open(cache_file, "r") as f:
            data = json.load(f)
            return data.get("timestamps", {})
    except (json.JSONDecodeError, IOError) as e:
        print(f"Warning: Failed to load cache file: {e}", file=sys.stderr)
        return {}


def save_build_timestamps(timestamps: dict[str, str]) -> None:
    """
    Save build timestamp cache to disk.

    Args:
        timestamps: Dictionary mapping platform name to ISO timestamp string
    """
    cache_file = get_cache_file()

    data = {
        "version": 1,
        "timestamps": timestamps,
        "last_updated": datetime.now(timezone.utc).isoformat(),
    }

    try:
        with open(cache_file, "w") as f:
            json.dump(data, f, indent=2)
    except IOError as e:
        print(f"Error: Failed to save cache file: {e}", file=sys.stderr)
        sys.exit(1)


def get_dockerhub_image_age(platform: str) -> float | None:
    """
    Query Docker Hub API to get the last update time of an image.

    Args:
        platform: Platform name (e.g., "esp-32s3", "avr")

    Returns:
        Age in days since last update, or None if image doesn't exist or query fails
    """
    image_name = get_docker_image_name(platform)

    # Extract repository and tag from image name
    # Format: niteris/fastled-compiler-{platform}:latest
    repo = image_name.split(":")[0] if ":" in image_name else image_name
    tag = "latest"

    # Query Docker Hub API
    # API endpoint: https://hub.docker.com/v2/repositories/{namespace}/{repo}/tags/{tag}
    namespace, repo_name = repo.split("/", 1)
    url = f"https://hub.docker.com/v2/repositories/{namespace}/{repo_name}/tags/{tag}"

    try:
        import urllib.request

        with urllib.request.urlopen(url, timeout=10) as response:
            data = json.loads(response.read().decode())

            # Parse last_updated timestamp
            last_updated_str = data.get("last_updated")
            if not last_updated_str:
                return None

            # Parse ISO 8601 timestamp
            last_updated = datetime.fromisoformat(
                last_updated_str.replace("Z", "+00:00")
            )
            now = datetime.now(timezone.utc)
            age = (now - last_updated).total_seconds() / 86400  # Convert to days

            return age

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(
            f"Warning: Failed to query Docker Hub for {platform}: {e}",
            file=sys.stderr,
        )
        return None


def prioritize_platforms(
    limit: int | None = None, use_dockerhub: bool = True
) -> list[dict[str, Any]]:
    """
    Prioritize platforms for building based on:
    1. Never built (highest priority)
    2. Longest time since last build (stale images)

    Args:
        limit: Maximum number of platforms to return (None = all)
        use_dockerhub: If True, also check Docker Hub for actual image ages

    Returns:
        List of platform dictionaries with priority information, sorted by priority
        Each dict contains: platform, last_build, age_days, priority_score
    """
    timestamps = load_build_timestamps()
    now = datetime.now(timezone.utc)

    platform_info: list[dict[str, Any]] = []

    for platform in DOCKER_PLATFORMS.keys():
        last_build_str = timestamps.get(platform)

        if last_build_str:
            # Parse ISO timestamp
            last_build = datetime.fromisoformat(last_build_str)
            age_days = (now - last_build).total_seconds() / 86400
            priority_score = age_days  # Older = higher priority
            status = "stale" if age_days > 7 else "recent"
        else:
            # Never built locally
            last_build = None
            age_days = float("inf")
            priority_score = float("inf")
            status = "never_built"

        # Optionally check Docker Hub for actual image age
        dockerhub_age = None
        if use_dockerhub:
            dockerhub_age = get_dockerhub_image_age(platform)
            if dockerhub_age is not None and last_build is None:
                # Image exists on Docker Hub but not in local cache
                # Use Docker Hub age as priority
                age_days = dockerhub_age
                priority_score = dockerhub_age
                status = "dockerhub_stale" if dockerhub_age > 7 else "dockerhub_recent"

        platform_info.append(
            {
                "platform": platform,
                "last_build": last_build_str,
                "age_days": age_days if age_days != float("inf") else None,
                "dockerhub_age_days": dockerhub_age,
                "priority_score": priority_score,
                "status": status,
                "image_name": get_docker_image_name(platform),
            }
        )

    # Sort by priority (highest priority first)
    # Never built (inf) comes first, then oldest
    platform_info.sort(key=lambda x: x["priority_score"], reverse=True)

    # Apply limit if specified
    if limit is not None:
        platform_info = platform_info[:limit]

    return platform_info


def record_build(platform: str) -> None:
    """
    Record a successful build for a platform.

    Args:
        platform: Platform name (e.g., "esp-32s3", "avr")
    """
    if platform not in DOCKER_PLATFORMS:
        print(f"Error: Unknown platform '{platform}'", file=sys.stderr)
        print(f"Valid platforms: {', '.join(DOCKER_PLATFORMS.keys())}", file=sys.stderr)
        sys.exit(1)

    timestamps = load_build_timestamps()
    timestamps[platform] = datetime.now(timezone.utc).isoformat()
    save_build_timestamps(timestamps)

    print(f"✓ Recorded build for platform: {platform}")


def format_age(age_days: float | None) -> str:
    """Format age in days to human-readable string."""
    if age_days is None:
        return "never"
    if age_days == float("inf"):
        return "never"
    if age_days < 1:
        return f"{int(age_days * 24)}h"
    if age_days < 7:
        return f"{int(age_days)}d"
    if age_days < 30:
        return f"{int(age_days / 7)}w"
    return f"{int(age_days / 30)}mo"


def main() -> int:
    """Main entry point."""
    parser = argparse.ArgumentParser(
        description="Track and prioritize Docker image builds",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    # Command selection
    command_group = parser.add_mutually_exclusive_group(required=True)
    command_group.add_argument(
        "--check",
        action="store_true",
        help="Check which platforms need building (prioritized)",
    )
    command_group.add_argument(
        "--record",
        type=str,
        metavar="PLATFORM",
        help="Record successful build for a platform",
    )
    command_group.add_argument(
        "--list-all",
        action="store_true",
        help="List all platforms and their build status",
    )

    # Options for --check
    parser.add_argument(
        "--limit",
        type=int,
        metavar="N",
        help="Limit number of platforms to return (for --check)",
    )
    parser.add_argument(
        "--json",
        action="store_true",
        help="Output as JSON (for GitHub Actions)",
    )
    parser.add_argument(
        "--no-dockerhub",
        action="store_true",
        help="Skip Docker Hub API queries (faster but less accurate)",
    )

    args = parser.parse_args()

    if args.check or args.list_all:
        use_dockerhub = not args.no_dockerhub
        limit = args.limit if args.check else None

        platforms = prioritize_platforms(limit=limit, use_dockerhub=use_dockerhub)

        if args.json:
            # Output JSON for GitHub Actions
            print(json.dumps(platforms, indent=2))
        else:
            # Human-readable output
            print("Platform Build Priority Report")
            print("=" * 80)
            print()

            if args.check:
                print(f"Top {len(platforms)} platforms to build (by priority):")
            else:
                print(f"All {len(platforms)} platforms:")
            print()

            # Print table header
            print(
                f"{'Platform':<20} {'Status':<15} {'Age':<10} {'Docker Hub':<12} {'Image Name'}"
            )
            print("-" * 80)

            for info in platforms:
                platform = info["platform"]
                status = info["status"]
                age = format_age(info["age_days"])
                dockerhub_age = format_age(info["dockerhub_age_days"])
                image = info["image_name"]

                print(
                    f"{platform:<20} {status:<15} {age:<10} {dockerhub_age:<12} {image}"
                )

            print()

            if args.check:
                print(
                    f"Recommendation: Build these {len(platforms)} platform(s) in the next cycle"
                )

    elif args.record:
        record_build(args.record)

    return 0


if __name__ == "__main__":
    try:
        sys.exit(main())
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
        print("\n\nOperation cancelled by user", file=sys.stderr)
        sys.exit(130)
