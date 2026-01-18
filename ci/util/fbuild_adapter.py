"""
fbuild Adapter Module for FastLED CI.

This module provides a clean interface to the fbuild build system,
wrapping the fbuild Daemon API for use in FastLED's validation
and debugging workflows.

The fbuild daemon provides faster builds by:
- Caching compiled objects across builds
- Managing toolchain installation automatically
- Running as a background process for quick operations

Key functions:
    - fbuild_build_and_upload: Build and upload firmware to device
    - fbuild_build_only: Build firmware without uploading
    - is_fbuild_available: Check if fbuild is available
    - get_fbuild_version: Get fbuild version string

For daemon management, use fbuild.Daemon directly:
    import fbuild

    fbuild.Daemon.ensure_running()
    fbuild.Daemon.stop()
    fbuild.Daemon.status()
"""

from pathlib import Path

from ci.util.global_interrupt_handler import notify_main_thread


def is_fbuild_available() -> bool:
    """Check if fbuild is available in the environment.

    Returns:
        True if fbuild can be imported, False otherwise.
    """
    try:
        import fbuild  # type: ignore[import-not-found]

        return fbuild.is_available()
    except ImportError:
        return False


def get_fbuild_version() -> str | None:
    """Get the installed fbuild version.

    Returns:
        Version string if available, None otherwise.
    """
    try:
        import fbuild  # type: ignore[import-not-found]

        return fbuild.__version__  # type: ignore[no-any-return]
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
        import fbuild  # type: ignore[import-not-found]
    except ImportError as e:
        return False, f"fbuild not available: {e}"

    try:
        # Ensure daemon is running
        if not fbuild.Daemon.ensure_running():
            return False, "Failed to start fbuild daemon"

        # Request deploy (build + upload, no monitor)
        success = fbuild.Daemon.deploy(
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
        import fbuild  # type: ignore[import-not-found]
    except ImportError as e:
        return False, f"fbuild not available: {e}"

    try:
        # Ensure daemon is running
        if not fbuild.Daemon.ensure_running():
            return False, "Failed to start fbuild daemon"

        # Request build only
        success = fbuild.Daemon.build(
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
