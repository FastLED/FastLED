#!/usr/bin/env python3
"""
Prune old FastLED PlatformIO Docker images.

This script removes old Docker images with outdated config hashes,
keeping only the most recent image for each platform/architecture combination.

Usage:
    # Dry run (show what would be deleted)
    uv run python ci/docker/prune_old_images.py

    # Actually delete old images
    uv run python ci/docker/prune_old_images.py --force

    # Remove images older than specific days
    uv run python ci/docker/prune_old_images.py --days 30 --force

    # Remove all FastLED PlatformIO images (dangerous!)
    uv run python ci/docker/prune_old_images.py --all --force
"""

import argparse
import json
import re
import subprocess
import sys
from datetime import datetime, timedelta
from typing import Any, Dict, List, Set, cast


def parse_arguments() -> argparse.Namespace:
    """Parse and validate command-line arguments."""
    parser = argparse.ArgumentParser(
        description="Prune old FastLED PlatformIO Docker images",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Dry run (show what would be deleted)
  %(prog)s

  # Actually delete old images
  %(prog)s --force

  # Remove images older than 30 days
  %(prog)s --days 30 --force

  # Remove ALL FastLED PlatformIO images (use with caution!)
  %(prog)s --all --force
        """,
    )

    parser.add_argument(
        "--force",
        action="store_true",
        help="Actually delete images (without this, only shows what would be deleted)",
    )

    parser.add_argument(
        "--days",
        type=int,
        metavar="N",
        help="Remove images older than N days (default: keep all recent images)",
    )

    parser.add_argument(
        "--all",
        action="store_true",
        help="Remove ALL FastLED PlatformIO images, not just old ones (dangerous!)",
    )

    parser.add_argument(
        "--platform",
        type=str,
        help="Only prune images for specific platform (e.g., 'uno', 'esp32s3')",
    )

    return parser.parse_args()


def get_fastled_images() -> List[Dict[str, Any]]:
    """
    Get list of FastLED PlatformIO Docker images.

    Returns:
        List of image dictionaries with 'name', 'tag', 'created', 'size' keys
    """
    try:
        result = subprocess.run(
            [
                "docker",
                "images",
                "--format",
                "{{.Repository}}:{{.Tag}}\t{{.CreatedAt}}\t{{.Size}}",
                "fastled-platformio-*",
            ],
            capture_output=True,
            text=True,
            check=True,
        )

        images: List[Dict[str, Any]] = []
        for line in result.stdout.strip().split("\n"):
            if not line:
                continue

            parts = line.split("\t")
            if len(parts) >= 3:
                name_tag = parts[0]
                created = parts[1]
                size = parts[2]

                # Split name and tag
                if ":" in name_tag:
                    name, tag = name_tag.rsplit(":", 1)
                else:
                    name = name_tag
                    tag = "latest"

                images.append(
                    {"name": name, "tag": tag, "created": created, "size": size}
                )

        return images

    except subprocess.CalledProcessError as e:
        print(f"Error: Failed to list Docker images: {e}", file=sys.stderr)
        return []
    except FileNotFoundError:
        print("Error: Docker is not installed or not in PATH", file=sys.stderr)
        return []


def parse_image_name(image_name: str) -> Dict[str, str]:
    """
    Parse FastLED PlatformIO image name to extract components.

    Expected format: fastled-platformio-{architecture}-{platform}-{hash}
    Or: fastled-platformio-{architecture}-{platform} (old format without hash)

    Args:
        image_name: Docker image name

    Returns:
        Dictionary with 'architecture', 'platform', 'hash' keys
    """
    # Pattern: fastled-platformio-{arch}-{platform}-{hash}
    # Or: fastled-platformio-{arch}-{platform}
    pattern = r"^fastled-platformio-([^-]+)-([^-]+)(?:-([a-f0-9]{8}))?$"
    match = re.match(pattern, image_name)

    if match:
        architecture = match.group(1)
        platform = match.group(2)
        config_hash = match.group(3) or "none"
        return {
            "architecture": architecture,
            "platform": platform,
            "hash": config_hash,
        }

    return {"architecture": "unknown", "platform": "unknown", "hash": "none"}


def group_images_by_platform(
    images: List[Dict[str, Any]],
) -> Dict[str, List[Dict[str, Any]]]:
    """
    Group images by platform/architecture combination.

    Args:
        images: List of image dictionaries

    Returns:
        Dictionary mapping platform key to list of images
    """
    grouped: Dict[str, List[Dict[str, Any]]] = {}

    for image in images:
        parsed = parse_image_name(image["name"])
        platform_key = f"{parsed['architecture']}-{parsed['platform']}"

        if platform_key not in grouped:
            grouped[platform_key] = []

        grouped[platform_key].append(
            cast(
                Dict[str, Any],
                {
                    **image,
                    "parsed": parsed,
                },
            )
        )

    return grouped


def parse_docker_timestamp(timestamp: str) -> datetime:
    """
    Parse Docker timestamp to datetime object.

    Docker uses format like "2024-01-15 10:30:45 -0800 PST"

    Args:
        timestamp: Docker timestamp string

    Returns:
        datetime object
    """
    try:
        # Try to parse Docker's timestamp format
        # Format: "2024-01-15 10:30:45 -0800 PST"
        parts = timestamp.split()
        if len(parts) >= 2:
            date_str = f"{parts[0]} {parts[1]}"
            return datetime.strptime(date_str, "%Y-%m-%d %H:%M:%S")
    except Exception:
        pass

    # Fallback: return current time if parsing fails
    return datetime.now()


def filter_images_by_age(
    images: List[Dict[str, Any]], days: int
) -> List[Dict[str, Any]]:
    """
    Filter images older than specified days.

    Args:
        images: List of image dictionaries
        days: Age threshold in days

    Returns:
        List of old images
    """
    cutoff_date = datetime.now() - timedelta(days=days)
    old_images: List[Dict[str, Any]] = []

    for image in images:
        created_date = parse_docker_timestamp(image["created"])
        if created_date < cutoff_date:
            old_images.append(image)

    return old_images


def delete_image(image_name: str, tag: str, force: bool) -> bool:
    """
    Delete a Docker image.

    Args:
        image_name: Image name
        tag: Image tag
        force: Actually delete (if False, just print)

    Returns:
        True if successful, False otherwise
    """
    full_name = f"{image_name}:{tag}"

    if not force:
        print(f"  [DRY RUN] Would delete: {full_name}")
        return True

    try:
        subprocess.run(
            ["docker", "rmi", full_name],
            capture_output=True,
            text=True,
            check=True,
        )
        print(f"  Deleted: {full_name}")
        return True
    except subprocess.CalledProcessError as e:
        print(f"  Error deleting {full_name}: {e}", file=sys.stderr)
        return False


def main() -> int:
    """Main entry point for the script."""
    args = parse_arguments()

    # Get all FastLED images
    print("Scanning for FastLED PlatformIO Docker images...")
    images = get_fastled_images()

    if not images:
        print("No FastLED PlatformIO images found.")
        return 0

    print(f"Found {len(images)} FastLED PlatformIO images\n")

    # Filter by platform if specified
    if args.platform:
        images = [
            img
            for img in images
            if args.platform in img["name"]
            or args.platform in parse_image_name(img["name"])["platform"]
        ]
        print(f"Filtered to {len(images)} images for platform: {args.platform}\n")

    # Determine what to delete
    images_to_delete: List[Dict[str, Any]] = []

    if args.all:
        # Delete ALL images (dangerous!)
        images_to_delete = images
        print(
            "WARNING: --all flag specified. This will delete ALL FastLED PlatformIO images!"
        )
        print()

    elif args.days:
        # Delete images older than specified days
        images_to_delete = filter_images_by_age(images, args.days)
        print(f"Found {len(images_to_delete)} images older than {args.days} days")
        print()

    else:
        # Default: Keep newest image for each platform, delete others with different hashes
        grouped = group_images_by_platform(images)

        for platform_key, platform_images in grouped.items():
            # Sort by creation date (newest first)
            platform_images.sort(
                key=lambda x: parse_docker_timestamp(x["created"]), reverse=True
            )

            # Keep the newest, mark older ones for deletion
            if len(platform_images) > 1:
                newest = platform_images[0]
                older = platform_images[1:]

                print(f"Platform: {platform_key}")
                print(
                    f"  Keeping newest: {newest['name']}:{newest['tag']} (hash: {newest['parsed']['hash']})"
                )

                for old_img in older:
                    print(
                        f"  Marking for deletion: {old_img['name']}:{old_img['tag']} (hash: {old_img['parsed']['hash']})"
                    )
                    images_to_delete.append(old_img)

                print()

    # Summary
    if not images_to_delete:
        print("No images to delete.")
        return 0

    print(f"Total images to delete: {len(images_to_delete)}")
    print()

    # Delete or show dry run
    if not args.force:
        print("DRY RUN - No images will be deleted. Use --force to actually delete.")
        print()

    deleted_count = 0
    for image in images_to_delete:
        if delete_image(image["name"], image["tag"], args.force):
            deleted_count += 1

    print()
    if args.force:
        print(f"Successfully deleted {deleted_count} images")
    else:
        print(f"Would delete {deleted_count} images (use --force to actually delete)")

    return 0


if __name__ == "__main__":
    sys.exit(main())
