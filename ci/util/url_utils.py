#!/usr/bin/env python3
"""
URL utility functions for PlatformIO and other tools.

This module provides utilities for working with URLs, particularly for
sanitizing URLs to create filesystem-safe path names.
"""

import hashlib
from pathlib import Path


def sanitize_url_for_path(url: str) -> Path:
    """
    Sanitize URL to create a valid filesystem path name.
    Simply strips protocol and replaces invalid filesystem characters.

    Args:
        url: URL to sanitize

    Returns:
        Sanitized Path suitable for use as a filesystem path component

    Example:
        >>> sanitize_url_for_path("https://github.com/owner/repo/releases/download/v1.0.0/file.zip")
        Path('github_com_owner_repo_releases_download_v1_0_0_file_zip')
    """
    # Remove protocol (http://, https://, ftp://, etc.)
    if "://" in url:
        url = url.split("://", 1)[1]

    # Replace invalid filesystem characters with underscores
    # Invalid characters: / \ : * ? " < > |
    invalid_chars = ["/", "\\", ":", "*", "?", '"', "<", ">", "|"]
    result = url
    for char in invalid_chars:
        result = result.replace(char, "_")

    # Replace dots with underscores to avoid issues with file extensions
    result = result.replace(".", "_")

    # Replace spaces and dashes with underscores for consistency
    result = result.replace(" ", "_").replace("-", "_")

    # Remove any remaining non-alphanumeric characters except underscores
    result = "".join(c if c.isalnum() or c == "_" else "_" for c in result)

    # Remove multiple consecutive underscores
    while "__" in result:
        result = result.replace("__", "_")

    # Remove leading/trailing underscores
    result = result.strip("_")

    # Ensure reasonable length (max 100 chars)
    if len(result) > 100:
        # Keep first 70 chars and add a short hash
        result = result[:70] + "_" + hashlib.sha256(url.encode()).hexdigest()[:8]

    return Path(result)
