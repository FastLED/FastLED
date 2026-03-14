from ci.util.global_interrupt_handler import handle_keyboard_interrupt


#!/usr/bin/env python3
"""
ZCCACHE Configuration for FastLED Builds

Provides zccache (distributed compiler cache) support for faster compilation.
ZCCACHE is faster and more reliable than ccache, especially for CI/CD environments.
"""

import os
import platform
import re
import shutil
from pathlib import Path
from typing import Any, Protocol

from running_process import RunningProcess

from ci.util.timestamp_print import ts_print


class PlatformIOEnv(Protocol):
    """Type hint for PlatformIO environment object."""

    def get(self, key: str) -> str | None:
        """Get environment variable value."""
        ...

    def Replace(self, **kwargs: Any) -> None:
        """Replace environment variables."""
        ...


def get_zccache_wrapper_path() -> str | None:
    """Get the path to zccache binary.

    Uses shutil.which to find zccache in PATH.
    """
    return shutil.which("zccache")


def get_zccache_path() -> str | None:
    """DEPRECATED: Use get_zccache_wrapper_path() instead.

    Kept for API compatibility.
    """
    return get_zccache_wrapper_path()


def is_zccache_available() -> bool:
    """Check if zccache is available."""
    return get_zccache_wrapper_path() is not None


def clear_zccache_stats() -> None:
    """Clear zccache statistics (no-op).

    zccache 1.0.3 does not support --zero-stats.
    Stats are per-daemon-uptime and reset automatically on daemon restart.
    """
    pass


def show_zccache_stats() -> None:
    """Display zccache statistics if zccache is available."""
    if not is_zccache_available():
        return

    zccache_path = get_zccache_wrapper_path()
    if not zccache_path:
        return

    try:
        result = RunningProcess.run(
            [zccache_path, "status"],
            cwd=None,
            check=False,
            timeout=10,
            capture_output=True,
        )
        if result.returncode == 0:
            # Parse zccache status output format:
            #   Compilations:  N total (X cached, Y cold, Z non-cacheable)
            #   Hit rate:      XX.X%
            lines = result.stdout.strip().split("\n")
            key_metrics: dict[str, str] = {}

            for line in lines:
                line_stripped = line.strip()
                if not line_stripped:
                    continue

                if "Compilations:" in line:
                    # Parse: "Compilations:  N total (X cached, Y cold, Z non-cacheable)"
                    key_metrics["requests"] = line_stripped.split()[1]
                    # Extract cached count from parenthetical
                    cached_match = re.search(r"\((\d+)\s+cached", line_stripped)
                    cold_match = re.search(r"(\d+)\s+cold", line_stripped)
                    if cached_match:
                        key_metrics["hits"] = cached_match.group(1)
                    if cold_match:
                        key_metrics["misses"] = cold_match.group(1)
                elif "Hit rate:" in line:
                    # Parse: "Hit rate:      XX.X%" or "Hit rate:      n/a"
                    parts = line_stripped.split(":", 1)
                    if len(parts) > 1:
                        key_metrics["hit_rate"] = parts[1].strip()

            # Display compact summary only if there's meaningful data
            has_hits = key_metrics.get("hits") not in (None, "N/A", "n/a", "0")
            has_misses = key_metrics.get("misses") not in (None, "N/A", "n/a", "0")

            if has_hits or has_misses:
                ts_print("\nZCCACHE (this build):")
                if key_metrics:
                    ts_print(
                        f"  Requests: {key_metrics.get('requests', 'N/A')} | "
                        f"Hits: {key_metrics.get('hits', 'N/A')} | "
                        f"Misses: {key_metrics.get('misses', 'N/A')} | "
                        f"Hit Rate: {key_metrics.get('hit_rate', 'N/A')}"
                    )
                else:
                    ts_print(result.stdout)
    except RuntimeError as e:
        if "timeout" in str(e).lower():
            ts_print("Warning: zccache status timed out")
        else:
            ts_print(f"Warning: Failed to retrieve zccache stats: {e}")
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        ts_print(f"Warning: Failed to retrieve zccache stats: {e}")


def configure_zccache(env: PlatformIOEnv) -> None:
    """Configure ZCCACHE for the build environment."""
    if not is_zccache_available():
        ts_print("ZCCACHE is not available. Skipping ZCCACHE configuration.")
        return

    zccache_path = get_zccache_wrapper_path()
    if not zccache_path:
        ts_print("Could not find ZCCACHE executable. Skipping ZCCACHE configuration.")
        return

    ts_print(f"Found ZCCACHE at: {zccache_path}")

    # Set up ZCCACHE environment variables
    project_dir = env.get("PROJECT_DIR")
    if project_dir is None:
        project_dir = os.getcwd()

    # Use board-specific zccache directory if PIOENV (board environment) is available
    board_name = env.get("PIOENV")
    if board_name:
        zccache_dir = os.path.join(project_dir, ".zccache", board_name)
    else:
        zccache_dir = os.path.join(project_dir, ".zccache", "default")

    # Create zccache directory
    Path(zccache_dir).mkdir(parents=True, exist_ok=True)
    ts_print(f"Using board-specific ZCCACHE directory: {zccache_dir}")

    # Configure ZCCACHE environment variables
    os.environ["ZCCACHE_DIR"] = zccache_dir
    os.environ["ZCCACHE_CACHE_SIZE"] = "2G"  # Larger cache for better hit rates

    # Optional: Configure distributed caching (Redis/Memcached) if available
    # This can be enabled by setting environment variables before build:
    # export ZCCACHE_REDIS=redis://localhost:6379
    # export ZCCACHE_MEMCACHED=localhost:11211

    # Configure compression for better storage efficiency
    if platform.system() != "Windows":
        # Only set on Unix-like systems where it's more reliable
        os.environ["ZCCACHE_LOG"] = "info"

    # Get current compiler paths
    env.get("CC")
    env.get("CXX")

    # Note: For PlatformIO, we don't need to manually wrap CC/CXX here
    # Instead, we'll use build_flags to wrap the compiler commands
    # This is handled in the Board configuration via extra_scripts

    ts_print("ZCCACHE configuration completed")
    ts_print(f"Cache directory: {zccache_dir}")
    ts_print("Cache size limit: 2G")

    # Show ZCCACHE stats if available
    try:
        result = RunningProcess.run(
            [zccache_path, "status"],
            cwd=None,
            check=False,
            timeout=10,
            capture_output=True,
        )
        if result.returncode == 0:
            ts_print("ZCCACHE Statistics:")
            ts_print(result.stdout)
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        pass  # Don't fail build if stats aren't available


def get_zccache_wrapper_script_content() -> str:
    """Generate content for zccache wrapper script for PlatformIO extra_scripts."""
    return '''
# ZCCACHE wrapper script for PlatformIO builds
# This script automatically wraps compiler commands with zccache

Import("env")

import os
import shutil
from pathlib import Path
from ci.util.timestamp_print import ts_print

def setup_zccache_wrapper():
    """Setup zccache wrapper for compiler commands."""

    # Use zccache (guaranteed to be available)
    zccache_path = shutil.which("zccache")

    if not zccache_path:
        ts_print("zccache not found, compilation will proceed without caching")
        return

    ts_print(f"Setting up ZCCACHE wrapper: {zccache_path}")

    # Get current build environment
    project_dir = env.get("PROJECT_DIR", os.getcwd())
    board_name = env.get("PIOENV", "default")

    # Setup zccache directory
    zccache_dir = os.path.join(project_dir, ".zccache", board_name)
    Path(zccache_dir).mkdir(parents=True, exist_ok=True)

    # Configure zccache environment
    os.environ["ZCCACHE_DIR"] = zccache_dir
    os.environ["ZCCACHE_CACHE_SIZE"] = "2G"

    # Wrap compiler commands
    current_cc = env.get("CC", "gcc")
    current_cxx = env.get("CXX", "g++")

    # Only wrap if not already wrapped
    if "zccache" not in current_cc:
        env.Replace(
            CC=f\'"{zccache_path}" {current_cc}\',
            CXX=f\'"{zccache_path}" {current_cxx}\',
        )
        ts_print(f"Wrapped CC with ZCCACHE: {env.get(\'CC\')}")
        ts_print(f"Wrapped CXX with ZCCACHE: {env.get(\'CXX\')}")

# Setup zccache wrapper
setup_zccache_wrapper()
'''


# This script can be executed directly by PlatformIO as an extra_script
if __name__ == "__main__":
    # For direct execution by PlatformIO extra_scripts
    pass
