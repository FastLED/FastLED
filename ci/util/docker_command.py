#!/usr/bin/env python3
"""Docker command utilities - minimal module with no internal dependencies.

This module is separate from docker_helper to avoid circular imports.
It provides the basic functionality to find and execute the docker command.
"""

import os
import subprocess
import sys
from typing import Optional


# Cache for docker executable path
_docker_executable_cache: Optional[str] = None


def find_docker_executable() -> Optional[str]:
    """Find the docker executable on the system.

    Returns:
        Path to docker executable, or None if not found
    """
    global _docker_executable_cache

    if _docker_executable_cache is not None:
        return _docker_executable_cache

    # Check if docker command is available
    try:
        result = subprocess.run(
            ["docker", "--version"],
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        )
        if result.returncode == 0:
            _docker_executable_cache = "docker"
            return "docker"
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    # On Windows, try to find docker.exe in common locations
    if sys.platform == "win32":
        common_paths = [
            r"C:\Program Files\Docker\Docker\resources\bin\docker.exe",
            r"C:\Program Files\Docker\Docker\docker.exe",
        ]
        for path in common_paths:
            if os.path.exists(path):
                _docker_executable_cache = path
                return path

    return None


def get_docker_command() -> str:
    """Get the docker command to use (as a string).

    Returns:
        The path to the docker executable, or "docker" if not found (will fail later with clear error)
    """
    docker_exe = find_docker_executable()
    if sys.platform == "win32":
        return "docker"
    return docker_exe or "docker"


def is_docker_available() -> bool:
    """Check if Docker is available on the system.

    Returns:
        True if Docker is available and responsive, False otherwise
    """
    docker_cmd = get_docker_command()

    try:
        result = subprocess.run(
            [docker_cmd, "--version"],
            capture_output=True,
            text=True,
            timeout=5,
            check=False,
        )
        return result.returncode == 0
    except (FileNotFoundError, subprocess.TimeoutExpired):
        return False
