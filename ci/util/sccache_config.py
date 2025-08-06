#!/usr/bin/env python3
"""
SCCACHE Configuration for FastLED Builds

Provides sccache (distributed compiler cache) support for faster compilation.
SCCACHE is faster and more reliable than ccache, especially for CI/CD environments.
"""

import os
import platform
import shutil
import subprocess
from pathlib import Path
from typing import Any, Protocol


class PlatformIOEnv(Protocol):
    """Type hint for PlatformIO environment object."""

    def get(self, key: str) -> str | None:
        """Get environment variable value."""
        ...

    def Replace(self, **kwargs: Any) -> None:
        """Replace environment variables."""
        ...


def is_sccache_available() -> bool:
    """Check if sccache is available in the system."""
    try:
        subprocess.run(["sccache", "--version"], capture_output=True, check=True)
        return True
    except (subprocess.CalledProcessError, FileNotFoundError):
        return False


def get_sccache_path() -> str | None:
    """Get the full path to sccache executable."""
    # Use shutil.which for cross-platform executable finding
    sccache_path = shutil.which("sccache")
    if sccache_path:
        return sccache_path

    # Additional Windows-specific paths
    if platform.system() == "Windows":
        additional_paths = [
            "C:\\ProgramData\\chocolatey\\bin\\sccache.exe",
            os.path.expanduser("~\\scoop\\shims\\sccache.exe"),
            os.path.expanduser("~\\.cargo\\bin\\sccache.exe"),
        ]
        for path in additional_paths:
            if os.path.exists(path):
                return path

    return None


def configure_sccache(env: PlatformIOEnv) -> None:
    """Configure SCCACHE for the build environment."""
    if not is_sccache_available():
        print("SCCACHE is not available. Skipping SCCACHE configuration.")
        return

    sccache_path = get_sccache_path()
    if not sccache_path:
        print("Could not find SCCACHE executable. Skipping SCCACHE configuration.")
        return

    print(f"Found SCCACHE at: {sccache_path}")

    # Set up SCCACHE environment variables
    project_dir = env.get("PROJECT_DIR")
    if project_dir is None:
        project_dir = os.getcwd()

    # Use board-specific sccache directory if PIOENV (board environment) is available
    board_name = env.get("PIOENV")
    if board_name:
        sccache_dir = os.path.join(project_dir, ".sccache", board_name)
    else:
        sccache_dir = os.path.join(project_dir, ".sccache", "default")

    # Create sccache directory
    Path(sccache_dir).mkdir(parents=True, exist_ok=True)
    print(f"Using board-specific SCCACHE directory: {sccache_dir}")

    # Configure SCCACHE environment variables
    os.environ["SCCACHE_DIR"] = sccache_dir
    os.environ["SCCACHE_CACHE_SIZE"] = "2G"  # Larger cache for better hit rates

    # Optional: Configure distributed caching (Redis/Memcached) if available
    # This can be enabled by setting environment variables before build:
    # export SCCACHE_REDIS=redis://localhost:6379
    # export SCCACHE_MEMCACHED=localhost:11211

    # Configure compression for better storage efficiency
    if platform.system() != "Windows":
        # Only set on Unix-like systems where it's more reliable
        os.environ["SCCACHE_LOG"] = "info"

    # Get current compiler paths
    original_cc = env.get("CC")
    original_cxx = env.get("CXX")

    # Note: For PlatformIO, we don't need to manually wrap CC/CXX here
    # Instead, we'll use build_flags to wrap the compiler commands
    # This is handled in the Board configuration via extra_scripts

    print(f"SCCACHE configuration completed")
    print(f"Cache directory: {sccache_dir}")
    print(f"Cache size limit: 2G")

    # Show SCCACHE stats if available
    try:
        result = subprocess.run(
            [sccache_path, "--show-stats"], capture_output=True, text=True, check=False
        )
        if result.returncode == 0:
            print("SCCACHE Statistics:")
            print(result.stdout)
    except Exception:
        pass  # Don't fail build if stats aren't available


def get_sccache_wrapper_script_content() -> str:
    """Generate content for sccache wrapper script for PlatformIO extra_scripts."""
    return '''
# SCCACHE wrapper script for PlatformIO builds
# This script automatically wraps compiler commands with sccache

Import("env")

import os
import shutil
from pathlib import Path

def setup_sccache_wrapper():
    """Setup sccache wrapper for compiler commands."""
    
    # Check if sccache is available
    sccache_path = shutil.which("sccache")
    if not sccache_path:
        print("SCCACHE not found, compilation will proceed without caching")
        return
    
    print(f"Setting up SCCACHE wrapper: {sccache_path}")
    
    # Get current build environment
    project_dir = env.get("PROJECT_DIR", os.getcwd())
    board_name = env.get("PIOENV", "default")
    
    # Setup sccache directory
    sccache_dir = os.path.join(project_dir, ".sccache", board_name)
    Path(sccache_dir).mkdir(parents=True, exist_ok=True)
    
    # Configure sccache environment
    os.environ["SCCACHE_DIR"] = sccache_dir
    os.environ["SCCACHE_CACHE_SIZE"] = "2G"
    
    # Wrap compiler commands
    current_cc = env.get("CC", "gcc")
    current_cxx = env.get("CXX", "g++")
    
    # Only wrap if not already wrapped
    if "sccache" not in current_cc:
        env.Replace(
            CC=f'"{sccache_path}" {current_cc}',
            CXX=f'"{sccache_path}" {current_cxx}',
        )
        print(f"Wrapped CC with SCCACHE: {env.get('CC')}")
        print(f"Wrapped CXX with SCCACHE: {env.get('CXX')}")

# Setup sccache wrapper
setup_sccache_wrapper()
'''


# This script can be executed directly by PlatformIO as an extra_script
if __name__ == "__main__":
    # For direct execution by PlatformIO extra_scripts
    pass
