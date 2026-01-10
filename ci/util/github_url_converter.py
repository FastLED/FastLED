"""
GitHub URL Converter for Toolchain Downloads

Converts GitHub repository URLs to downloadable zip URLs when possible.
Handles various GitHub URL patterns and provides warnings for unconvertible URLs.
"""

from typing import Optional
from urllib.parse import urlparse


def convert_github_url_to_zip(url: str) -> tuple[Optional[str], bool]:
    """
    Convert a GitHub URL to a downloadable zip URL if possible.

    Args:
        url: GitHub URL (can be git, https, or already a zip URL)

    Returns:
        Tuple of (converted_url, needs_manual_install):
        - converted_url: Zip URL if conversion succeeded, None otherwise
        - needs_manual_install: True if URL requires manual git installation

    Examples:
        >>> convert_github_url_to_zip("https://github.com/user/repo.git")
        (None, True)  # Cannot convert without version info

        >>> convert_github_url_to_zip("https://github.com/user/repo/archive/refs/tags/v1.0.0.zip")
        ("https://github.com/user/repo/archive/refs/tags/v1.0.0.zip", False)  # Already zip

        >>> convert_github_url_to_zip("https://github.com/user/repo.git#v1.0.0")
        ("https://github.com/user/repo/archive/refs/tags/v1.0.0.zip", False)  # Converted
    """
    if not url:
        return None, False

    # Already a zip URL - return as-is
    if url.endswith(".zip") or ".zip?" in url:
        return url, False

    # Parse the URL
    parsed = urlparse(url)

    # Only handle GitHub URLs
    if parsed.netloc not in ("github.com", "www.github.com"):
        return None, False

    # Extract repository path and ref/tag info
    # Pattern: https://github.com/owner/repo.git#ref
    # Pattern: https://github.com/owner/repo.git
    # Pattern: https://github.com/owner/repo (without .git)

    path = parsed.path.rstrip("/")
    fragment = parsed.fragment  # The part after #

    # Remove .git suffix if present
    if path.endswith(".git"):
        path = path[:-4]

    # Split path into components
    path_parts = [p for p in path.split("/") if p]
    if len(path_parts) < 2:
        # Not a valid owner/repo path
        return None, False

    owner = path_parts[0]
    repo = path_parts[1]

    # Check if there's a ref/tag in the fragment or path
    ref = None

    if fragment:
        # Fragment can be a branch, tag, or commit
        ref = fragment
    elif len(path_parts) > 2:
        # Check for archive/refs/tags/ or archive/refs/heads/ patterns
        # Example: /owner/repo/archive/refs/tags/v1.0.0
        if "archive" in path_parts and "refs" in path_parts:
            # Already in archive format, might just need .zip
            if path.endswith(".zip"):
                return url, False
            return f"{url}.zip", False

    # If we have a ref, convert to archive URL
    if ref:
        # Determine if it's a tag or branch
        # Common tag patterns: v1.0.0, 1.0.0, release-1.0.0
        # For simplicity, try tags first, then heads

        # Try as tag first (most common for releases)
        tag_url = f"https://github.com/{owner}/{repo}/archive/refs/tags/{ref}.zip"
        return tag_url, False

    # No version info - cannot safely convert to zip
    # This requires manual git installation
    return None, True


def is_git_url(url: str) -> bool:
    """Check if a URL is a git repository URL."""
    if not url:
        return False
    return url.endswith(".git") or url.startswith("git://")


def is_zip_url(url: str) -> bool:
    """Check if a URL points to a zip file."""
    if not url:
        return False
    return url.endswith(".zip") or ".zip?" in url


def format_manual_install_warning(package_name: str, url: str) -> str:
    """
    Format a warning message for packages that require manual installation.

    Returns warning in YELLOW color using ANSI escape codes.
    """
    YELLOW = "\033[93m"
    RESET = "\033[0m"
    return f"{YELLOW}⚠️  WARNING: Toolchain '{package_name}' requires manual git installation: {url}{RESET}"
