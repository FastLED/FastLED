from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


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

from ci.util.timestamp_print import ts_print


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


def clear_sccache_stats() -> None:
    """Clear sccache statistics to get clean metrics for current build."""
    if not is_sccache_available():
        return

    sccache_path = get_sccache_path()
    if not sccache_path:
        return

    try:
        subprocess.run(
            [sccache_path, "--zero-stats"],
            capture_output=True,
            check=False,
            timeout=5,
        )
    except subprocess.TimeoutExpired:
        pass  # Don't fail build if stats clearing times out
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception:
        pass  # Don't fail build if stats clearing fails


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


def show_sccache_stats() -> None:
    """Display sccache statistics if sccache is available."""
    if not is_sccache_available():
        return

    sccache_path = get_sccache_path()
    if not sccache_path:
        return

    try:
        result = subprocess.run(
            [sccache_path, "--show-stats"],
            capture_output=True,
            text=True,
            check=False,
            timeout=10,
        )
        if result.returncode == 0:
            # Parse and display key metrics only
            lines = result.stdout.strip().split("\n")
            key_metrics: dict[str, str] = {}

            # Extract only the most important metrics
            for line in lines:
                line_stripped = line.strip()
                if not line_stripped:
                    continue

                # Split on whitespace with max 1 split to get key and value
                if "Compile requests" in line and "executed" not in line.lower():
                    # Get last token which is the number
                    key_metrics["requests"] = line_stripped.split()[-1]
                elif "Cache hits (C/C++)" in line:
                    key_metrics["hits"] = line_stripped.split()[-1]
                elif "Cache misses (C/C++)" in line:
                    key_metrics["misses"] = line_stripped.split()[-1]
                elif "Cache hits rate (C/C++)" in line:
                    # Get the percentage (last 2 tokens: "32.88 %")
                    tokens = line_stripped.split()
                    key_metrics["hit_rate"] = f"{tokens[-2]} {tokens[-1]}"
                elif "Average compiler" in line:
                    # Get the time value (last 2 tokens: "5.438 s")
                    tokens = line_stripped.split()
                    key_metrics["avg_compile"] = f"{tokens[-2]} {tokens[-1]}"

            # Display compact summary only if there's meaningful data
            # Skip displaying SCCACHE stats if no compilation occurred (all N/A values)
            has_hits = key_metrics.get("hits") not in (None, "N/A", "0")
            has_misses = key_metrics.get("misses") not in (None, "N/A", "0")

            # Only show SCCACHE stats if there was actual compilation activity
            # Require actual hits/misses, not just requests count
            if has_hits or has_misses:
                ts_print("\nSCCACHE (this build):")
                if key_metrics:
                    ts_print(
                        f"  Requests: {key_metrics.get('requests', 'N/A')} | "
                        f"Hits: {key_metrics.get('hits', 'N/A')} | "
                        f"Misses: {key_metrics.get('misses', 'N/A')} | "
                        f"Hit Rate: {key_metrics.get('hit_rate', 'N/A')}"
                    )
                    if "avg_compile" in key_metrics:
                        ts_print(f"  Avg: {key_metrics['avg_compile']}")
                else:
                    # Fallback: show raw output if parsing fails
                    ts_print(result.stdout)
    except subprocess.TimeoutExpired:
        ts_print("Warning: sccache --show-stats timed out")
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        ts_print(f"Warning: Failed to retrieve sccache stats: {e}")


def configure_sccache(env: PlatformIOEnv) -> None:
    """Configure SCCACHE for the build environment."""
    if not is_sccache_available():
        ts_print("SCCACHE is not available. Skipping SCCACHE configuration.")
        return

    sccache_path = get_sccache_path()
    if not sccache_path:
        ts_print("Could not find SCCACHE executable. Skipping SCCACHE configuration.")
        return

    ts_print(f"Found SCCACHE at: {sccache_path}")

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
    ts_print(f"Using board-specific SCCACHE directory: {sccache_dir}")

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
    env.get("CC")
    env.get("CXX")

    # Note: For PlatformIO, we don't need to manually wrap CC/CXX here
    # Instead, we'll use build_flags to wrap the compiler commands
    # This is handled in the Board configuration via extra_scripts

    ts_print("SCCACHE configuration completed")
    ts_print(f"Cache directory: {sccache_dir}")
    ts_print("Cache size limit: 2G")

    # Show SCCACHE stats if available
    try:
        result = subprocess.run(
            [sccache_path, "--show-stats"], capture_output=True, text=True, check=False
        )
        if result.returncode == 0:
            ts_print("SCCACHE Statistics:")
            ts_print(result.stdout)
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
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
from ci.util.timestamp_print import ts_print

def setup_sccache_wrapper():
    """Setup sccache wrapper for compiler commands."""

    # Check if sccache is available
    sccache_path = shutil.which("sccache")
    if not sccache_path:
        ts_print("SCCACHE not found, compilation will proceed without caching")
        return

    ts_print(f"Setting up SCCACHE wrapper: {sccache_path}")

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
        ts_print(f"Wrapped CC with SCCACHE: {env.get('CC')}")
        ts_print(f"Wrapped CXX with SCCACHE: {env.get('CXX')}")

# Setup sccache wrapper
setup_sccache_wrapper()
'''


# This script can be executed directly by PlatformIO as an extra_script
if __name__ == "__main__":
    # For direct execution by PlatformIO extra_scripts
    pass
