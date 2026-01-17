#!/usr/bin/env python3
"""
Version Lockfile System

Prevents unwanted automatic upgrades by pinning resolved package versions.
Similar to package-lock.json (npm), Cargo.lock (Rust), or poetry.lock (Python).

When a package with semver requirement (like ~1.23800.0) is first resolved,
the exact version is recorded. Subsequent builds use the pinned version
unless --upgrade flag is explicitly passed.
"""

import json
import logging
from datetime import datetime
from pathlib import Path
from typing import Any, Optional, cast

from ci.util.file_lock_rw import FileLock, write_lock


logger = logging.getLogger(__name__)


class VersionLockfile:
    """
    Manages version pinning for reproducible builds.

    Prevents automatic upgrades when semver requirements (~, ^) are used.
    """

    def __init__(self, lockfile_path: Path):
        """
        Initialize version lockfile.

        Args:
            lockfile_path: Path to the lockfile (e.g., platformio.lock)
        """
        self.lockfile_path = lockfile_path
        self._data: Optional[dict[str, Any]] = None

    def _load(self) -> dict[str, Any]:
        """Load lockfile data from disk."""
        if self._data is not None:
            return self._data

        if not self.lockfile_path.exists():
            self._data = {
                "version": "1.0",
                "created_at": datetime.now().isoformat(),
                "packages": {},
            }
            return self._data

        try:
            with open(self.lockfile_path, "r") as f:
                data = json.load(f)
                assert isinstance(data, dict), "Lockfile must contain a JSON object"
                self._data = cast(dict[str, Any], data)
                return self._data
        except (json.JSONDecodeError, OSError) as e:
            logger.warning(f"Failed to load lockfile {self.lockfile_path}: {e}")
            logger.warning("Creating new lockfile")
            self._data = {
                "version": "1.0",
                "created_at": datetime.now().isoformat(),
                "packages": {},
            }
            return self._data

    def _save(self) -> bool:
        """Save lockfile data to disk with locking."""
        if self._data is None:
            return False

        try:
            # Ensure parent directory exists
            self.lockfile_path.parent.mkdir(parents=True, exist_ok=True)

            # Update timestamp
            self._data["updated_at"] = datetime.now().isoformat()

            # Use write lock to prevent concurrent modifications
            lock: FileLock = write_lock(self.lockfile_path)
            with lock:
                with open(self.lockfile_path, "w") as f:
                    json.dump(self._data, f, indent=2, sort_keys=True)

            return True

        except OSError as e:
            logger.warning(f"Failed to save lockfile {self.lockfile_path}: {e}")
            return False

    def get_pinned_version(
        self, package_name: str, requirement: str, package_type: str = "tool"
    ) -> Optional[str]:
        """
        Get pinned version for a package requirement.

        Args:
            package_name: Name of the package (e.g., "toolchain-esp32ulp")
            requirement: Version requirement (e.g., "~1.23800.0")
            package_type: Type of package (e.g., "tool", "framework")

        Returns:
            Pinned version string if found, None otherwise
        """
        data = self._load()
        packages = data.get("packages", {})

        key = f"{package_type}/{package_name}"
        if key not in packages:
            return None

        entry = packages[key]
        if entry.get("requirement") != requirement:
            # Requirement changed, needs re-resolution
            return None

        return entry.get("resolved_version")

    def pin_version(
        self,
        package_name: str,
        requirement: str,
        resolved_version: str,
        package_type: str = "tool",
        metadata: Optional[dict[str, Any]] = None,
    ) -> bool:
        """
        Pin a resolved version for a package requirement.

        Args:
            package_name: Name of the package
            requirement: Original requirement (e.g., "~1.23800.0")
            resolved_version: Resolved version (e.g., "1.23800.240113")
            package_type: Type of package
            metadata: Optional additional metadata

        Returns:
            True if pinning succeeded
        """
        data = self._load()
        packages = data.get("packages", {})

        key = f"{package_type}/{package_name}"
        entry: dict[str, Any] = {
            "requirement": requirement,
            "resolved_version": resolved_version,
            "pinned_at": datetime.now().isoformat(),
        }

        if metadata:
            entry["metadata"] = metadata

        packages[key] = entry
        data["packages"] = packages

        return self._save()

    def clear_pins(self) -> bool:
        """
        Clear all pinned versions (for --upgrade).

        Returns:
            True if clearing succeeded
        """
        data = self._load()
        data["packages"] = {}
        return self._save()

    def upgrade_package(self, package_name: str, package_type: str = "tool") -> bool:
        """
        Remove pin for a specific package (for selective upgrade).

        Args:
            package_name: Name of the package to upgrade
            package_type: Type of package

        Returns:
            True if unpinning succeeded
        """
        data = self._load()
        packages = data.get("packages", {})

        key = f"{package_type}/{package_name}"
        if key in packages:
            del packages[key]
            data["packages"] = packages
            return self._save()

        return True

    def list_pins(self) -> dict[str, dict[str, Any]]:
        """
        List all pinned packages.

        Returns:
            Dictionary of package keys to their pin entries
        """
        data = self._load()
        return data.get("packages", {})


def get_default_lockfile_path(base_dir: Path) -> Path:
    """
    Get the default lockfile path for a project directory.

    Args:
        base_dir: Project base directory

    Returns:
        Path to platformio-lock.json
    """
    return base_dir / "platformio-lock.json"
