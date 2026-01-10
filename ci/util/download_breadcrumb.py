"""
Unified Download Breadcrumb System

Provides consistent breadcrumb tracking for all download operations in the CI system.
Breadcrumbs are JSON files that track download status, metadata, and completion.
Thread-safe with file locking to prevent concurrent write corruption.
"""

import json
import logging
from datetime import datetime
from pathlib import Path
from typing import Any, Optional

from ci.util.file_lock_rw import write_lock
from ci.util.url_utils import sanitize_url_for_path


logger = logging.getLogger(__name__)


def get_breadcrumb_path(cache_dir: Path, url: str, filename: str = "info.json") -> Path:
    """
    Get the breadcrumb file path for a given URL in the cache directory.

    Args:
        cache_dir: Base cache directory
        url: URL being downloaded (used to generate unique directory)
        filename: Name of the breadcrumb file (default: info.json)

    Returns:
        Path to the breadcrumb file
    """
    cache_key = str(sanitize_url_for_path(url))
    artifact_dir = cache_dir / cache_key
    return artifact_dir / filename


def read_breadcrumb(breadcrumb_path: Path) -> Optional[dict[str, Any]]:
    """
    Read breadcrumb data from a JSON file.

    Args:
        breadcrumb_path: Path to the breadcrumb file

    Returns:
        Dictionary with breadcrumb data, or None if file doesn't exist or is invalid
    """
    if not breadcrumb_path.exists():
        return None

    try:
        with open(breadcrumb_path, "r") as f:
            return json.load(f)
    except (json.JSONDecodeError, OSError) as e:
        logger.warning(f"Failed to read breadcrumb {breadcrumb_path}: {e}")
        return None


def write_breadcrumb(breadcrumb_path: Path, data: dict[str, Any]) -> bool:
    """
    Write breadcrumb data to a JSON file with file locking.

    Automatically adds/updates timestamp if not present in data.
    Thread-safe: Uses file locking with 5-second timeout and stale lock recovery.

    Args:
        breadcrumb_path: Path to the breadcrumb file
        data: Dictionary with breadcrumb data to write

    Returns:
        True if write succeeded, False otherwise
    """
    try:
        # Ensure parent directory exists
        breadcrumb_path.parent.mkdir(parents=True, exist_ok=True)

        # Add timestamp if not present
        if "timestamp" not in data:
            data["timestamp"] = datetime.now().isoformat()

        # Use file locking with 5-second timeout to prevent concurrent write corruption
        with write_lock(breadcrumb_path, timeout=5.0, operation="breadcrumb_write"):
            with open(breadcrumb_path, "w") as f:
                json.dump(data, f, indent=2)

        return True

    except TimeoutError as e:
        logger.error(f"Lock timeout writing breadcrumb {breadcrumb_path}: {e}")
        return False
    except OSError as e:
        logger.warning(f"Failed to write breadcrumb {breadcrumb_path}: {e}")
        return False


def is_download_complete(breadcrumb_path: Path) -> bool:
    """
    Check if a download is marked as complete in its breadcrumb.

    Args:
        breadcrumb_path: Path to the breadcrumb file

    Returns:
        True if download is complete, False otherwise
    """
    data = read_breadcrumb(breadcrumb_path)
    if data is None:
        return False

    status = data.get("status", "")
    return status == "complete"


def create_download_breadcrumb(
    cache_dir: Path,
    url: str,
    status: str = "complete",
    **metadata: Any,
) -> bool:
    """
    Create a breadcrumb for a completed download with metadata.

    Args:
        cache_dir: Base cache directory
        url: URL that was downloaded
        status: Download status (default: "complete")
        **metadata: Additional metadata to include in breadcrumb

    Returns:
        True if breadcrumb was created successfully
    """
    breadcrumb_path = get_breadcrumb_path(cache_dir, url)

    data = {
        "status": status,
        "url": url,
        "timestamp": datetime.now().isoformat(),
        **metadata,
    }

    return write_breadcrumb(breadcrumb_path, data)


def update_breadcrumb_status(
    breadcrumb_path: Path, status: str, **updates: Any
) -> bool:
    """
    Update the status and other fields in an existing breadcrumb.

    Args:
        breadcrumb_path: Path to the breadcrumb file
        status: New status value
        **updates: Additional fields to update

    Returns:
        True if update succeeded, False otherwise
    """
    data = read_breadcrumb(breadcrumb_path)
    if data is None:
        data = {}

    data["status"] = status
    data["updated_at"] = datetime.now().isoformat()
    data.update(updates)

    return write_breadcrumb(breadcrumb_path, data)


def get_cache_artifact_dir(cache_dir: Path, url: str) -> Path:
    """
    Get the directory where an artifact for a URL should be cached.

    Args:
        cache_dir: Base cache directory
        url: URL being cached

    Returns:
        Path to the artifact directory
    """
    cache_key = str(sanitize_url_for_path(url))
    return cache_dir / cache_key


def check_cached_download(
    cache_dir: Path, url: str, expected_filename: Optional[str] = None
) -> Optional[Path]:
    """
    Check if a URL has been downloaded and cached successfully.

    Args:
        cache_dir: Base cache directory
        url: URL to check
        expected_filename: If provided, also verify this file exists

    Returns:
        Path to the cached artifact directory if complete, None otherwise
    """
    artifact_dir = get_cache_artifact_dir(cache_dir, url)
    breadcrumb_path = get_breadcrumb_path(cache_dir, url)

    # Check if breadcrumb indicates completion
    if not is_download_complete(breadcrumb_path):
        return None

    # Check if artifact directory exists
    if not artifact_dir.exists():
        logger.warning(
            f"Breadcrumb exists but artifact directory missing: {artifact_dir}"
        )
        return None

    # If expected filename provided, verify it exists
    if expected_filename:
        expected_path = artifact_dir / expected_filename
        if not expected_path.exists():
            logger.warning(f"Expected file missing: {expected_path}")
            return None

    return artifact_dir
