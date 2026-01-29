"""Safe file I/O operations for Meson build system."""

import os
import sys
from pathlib import Path
from typing import Optional

from ci.util.timestamp_print import ts_print as _ts_print


def atomic_write_text(path: Path, content: str, encoding: str = "utf-8") -> None:
    """
    Atomically write text to a file using temp file + rename pattern.

    This ensures the file is never left in a corrupted/partial state if the
    process crashes mid-write. The rename operation is atomic on POSIX systems
    and nearly atomic on Windows (uses ReplaceFile API via Path.replace()).

    Args:
        path: Path to the file to write
        content: Text content to write
        encoding: Text encoding (default: utf-8)

    Raises:
        OSError/IOError: If the write fails and cleanup cannot complete
    """
    # Ensure parent directory exists
    path.parent.mkdir(parents=True, exist_ok=True)

    # Write to temp file first, then atomic rename
    temp_path = path.with_suffix(path.suffix + ".tmp")
    try:
        with open(temp_path, "w", encoding=encoding) as f:
            f.write(content)
            f.flush()
            # Ensure data is written to disk before rename
            os.fsync(f.fileno())
        # Atomic rename - either completes fully or not at all
        temp_path.replace(path)
    except (OSError, IOError):
        # Clean up temp file on error
        try:
            temp_path.unlink()
        except (OSError, IOError):
            pass
        raise


def write_if_different(path: Path, content: str, mode: Optional[int] = None) -> bool:
    """
    Write file only if content differs from existing file.

    This prevents unnecessary file modifications that would trigger Meson
    regeneration. If the file doesn't exist or content has changed, writes
    the new content atomically.

    Args:
        path: Path to file to write
        content: Content to write
        mode: Optional file permissions (Unix only)

    Returns:
        True if file was written (created or modified), False if unchanged
    """
    try:
        if path.exists():
            existing_content = path.read_text(encoding="utf-8")
            if existing_content == content:
                return False  # Content unchanged, skip write

        # Content changed or file doesn't exist - write it atomically
        atomic_write_text(path, content)

        # Set permissions if specified (Unix only)
        if mode is not None and not sys.platform.startswith("win"):
            path.chmod(mode)

        return True
    except (OSError, IOError) as e:
        # On error, try to write anyway - caller will handle failures
        _ts_print(f"[MESON] Warning: Error checking file {path}: {e}")
        atomic_write_text(path, content)
        if mode is not None and not sys.platform.startswith("win"):
            path.chmod(mode)
        return True
