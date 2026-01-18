"""
fbuild Adapter Module for FastLED CI.

This module provides a clean interface to the fbuild build system,
wrapping the fbuild daemon client API for use in FastLED's validation
and debugging workflows.

The fbuild daemon provides faster builds by:
- Caching compiled objects across builds
- Managing toolchain installation automatically
- Running as a background process for quick operations

Key functions:
    - fbuild_build_and_upload: Build and upload firmware to device
    - is_fbuild_available: Check if fbuild is available
"""

import sys
from pathlib import Path
from typing import Any

from ci.util.global_interrupt_handler import notify_main_thread


def is_fbuild_available() -> bool:
    """Check if fbuild is available in the environment.

    Returns:
        True if fbuild can be imported, False otherwise.
    """
    try:
        import fbuild  # noqa: F401
        from fbuild.daemon import client as daemon_client  # noqa: F401

        return True
    except ImportError:
        return False


def get_fbuild_version() -> str | None:
    """Get the installed fbuild version.

    Returns:
        Version string if available, None otherwise.
    """
    try:
        from fbuild import __version__

        return __version__
    except ImportError:
        return None


def fbuild_build_and_upload(
    project_dir: Path,
    environment: str,
    port: str | None = None,
    clean_build: bool = False,
    timeout: float = 1800,
) -> tuple[bool, str]:
    """Build and upload firmware using fbuild.

    This function uses the fbuild daemon to compile and upload firmware
    to an embedded device. It does NOT start monitoring - the caller
    should use the existing run_monitor function for that.

    Uses request_deploy() from fbuild.daemon.client which handles:
    - Build compilation
    - Firmware upload to device
    - Progress monitoring through status file

    Args:
        project_dir: Path to project directory containing platformio.ini
        environment: Build environment name (e.g., 'esp32c6')
        port: Serial port for upload (auto-detect if None)
        clean_build: Whether to perform a clean build
        timeout: Maximum wait time in seconds (default: 30 minutes)

    Returns:
        Tuple of (success: bool, message: str)
    """
    try:
        from fbuild.daemon import client as daemon_client
    except ImportError as e:
        return False, f"fbuild not available: {e}"

    try:
        # Ensure daemon is running
        if not daemon_client.ensure_daemon_running():
            return False, "Failed to start fbuild daemon"

        # Request deploy (build + upload, no monitor)
        # The request_deploy function takes these parameters:
        # - project_dir: Path to project directory
        # - environment: Build environment
        # - port: Serial port (optional, auto-detect if None)
        # - clean_build: Whether to perform clean build
        # - monitor_after: Whether to start monitor after deploy
        # - monitor_timeout: Timeout for monitor (if monitor_after=True)
        # - monitor_halt_on_error: Pattern to halt on error
        # - monitor_halt_on_success: Pattern to halt on success
        # - monitor_expect: Expected pattern to check
        # - timeout: Maximum wait time in seconds
        success = daemon_client.request_deploy(
            project_dir=project_dir,
            environment=environment,
            port=port,
            clean_build=clean_build,
            monitor_after=False,  # We'll handle monitoring separately via run_monitor
            timeout=timeout,
        )

        if success:
            return True, "Build and upload completed successfully"
        else:
            return False, "Build or upload failed"

    except KeyboardInterrupt:
        notify_main_thread()
        raise
    except Exception as e:
        return False, f"fbuild error: {e}"


def fbuild_build_only(
    project_dir: Path,
    environment: str,
    clean_build: bool = False,
    verbose: bool = False,
    timeout: float = 1800,
) -> tuple[bool, str]:
    """Build firmware using fbuild without uploading.

    Uses request_build() from fbuild.daemon.client which handles:
    - Source compilation
    - Library dependency management
    - Object file caching

    Args:
        project_dir: Path to project directory containing platformio.ini
        environment: Build environment name (e.g., 'esp32c6')
        clean_build: Whether to perform a clean build
        verbose: Enable verbose build output
        timeout: Maximum wait time in seconds (default: 30 minutes)

    Returns:
        Tuple of (success: bool, message: str)
    """
    try:
        from fbuild.daemon import client as daemon_client
    except ImportError as e:
        return False, f"fbuild not available: {e}"

    try:
        # Ensure daemon is running
        if not daemon_client.ensure_daemon_running():
            return False, "Failed to start fbuild daemon"

        # Request build only
        # The request_build function takes these parameters:
        # - project_dir: Project directory
        # - environment: Build environment
        # - clean_build: Whether to perform clean build
        # - verbose: Enable verbose build output
        # - timeout: Maximum wait time in seconds
        success = daemon_client.request_build(
            project_dir=project_dir,
            environment=environment,
            clean_build=clean_build,
            verbose=verbose,
            timeout=timeout,
        )

        if success:
            return True, "Build completed successfully"
        else:
            return False, "Build failed"

    except KeyboardInterrupt:
        notify_main_thread()
        raise
    except Exception as e:
        return False, f"fbuild error: {e}"


def stop_fbuild_daemon() -> bool:
    """Stop the fbuild daemon.

    Returns:
        True if daemon was stopped, False otherwise.
    """
    try:
        from fbuild.daemon import client as daemon_client

        return daemon_client.stop_daemon()
    except ImportError:
        return False
    except KeyboardInterrupt:
        notify_main_thread()
        raise
    except Exception:
        return False


def get_fbuild_daemon_status() -> dict[str, Any]:
    """Get current fbuild daemon status.

    Returns:
        Dictionary with daemon status information.
    """
    try:
        from fbuild.daemon import client as daemon_client

        return daemon_client.get_daemon_status()
    except ImportError:
        return {"error": "fbuild not available", "running": False}
    except KeyboardInterrupt:
        notify_main_thread()
        raise
    except Exception as e:
        return {"error": str(e), "running": False}


def ensure_daemon_running() -> bool:
    """Ensure the fbuild daemon is running.

    Returns:
        True if daemon is running or was started successfully, False otherwise.
    """
    try:
        from fbuild.daemon import client as daemon_client

        return daemon_client.ensure_daemon_running()
    except ImportError:
        return False
    except KeyboardInterrupt:
        notify_main_thread()
        raise
    except Exception:
        return False


def main() -> int:
    """Test the fbuild adapter."""
    print("Testing fbuild adapter...")
    print()

    # Check availability
    available = is_fbuild_available()
    print(f"fbuild available: {available}")

    if available:
        version = get_fbuild_version()
        print(f"fbuild version: {version}")

        status = get_fbuild_daemon_status()
        print(f"Daemon status: {status}")

    return 0 if available else 1


if __name__ == "__main__":
    sys.exit(main())
