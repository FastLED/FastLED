#!/usr/bin/env python3
"""fbuild wrapper for FastLED build operations.

This module provides a wrapper around fbuild commands for use by the FastLED
CI system. It integrates with the fbuild DaemonConnection for build and deploy operations.

Usage:
    from ci.util.fbuild_runner import (
        run_fbuild_compile,
        run_fbuild_upload,
        ensure_fbuild_packages,
    )

    # Compile using fbuild
    success = run_fbuild_compile(build_dir, environment="esp32s3", verbose=True)

    # Upload using fbuild
    success = run_fbuild_upload(build_dir, environment="esp32s3", port="COM3")
"""

from pathlib import Path
from typing import Any

from fbuild import connect_daemon
from fbuild.daemon import (
    ensure_daemon_running,
    get_daemon_status,
    stop_daemon,
)
from fbuild.daemon.client import is_daemon_running


def ensure_fbuild_daemon() -> bool:
    """Ensure the fbuild daemon is running.

    Returns:
        True if daemon is running or was started successfully, False otherwise
    """
    return ensure_daemon_running()


def run_fbuild_compile(
    build_dir: Path,
    environment: str | None = None,
    verbose: bool = False,
    clean: bool = False,
    timeout: float = 1800,
) -> bool:
    """Compile the project using fbuild.

    Args:
        build_dir: Project directory containing platformio.ini
        environment: Build environment (None = auto-detect)
        verbose: Enable verbose output
        clean: Perform clean build
        timeout: Maximum build time in seconds (default: 30 minutes)

    Returns:
        True if compilation succeeded, False otherwise
    """
    print("=" * 60)
    print("COMPILING (fbuild)")
    print("=" * 60)

    try:
        # Determine environment if not specified
        env_name = environment
        if env_name is None:
            from fbuild.cli_utils import EnvironmentDetector

            env_name = EnvironmentDetector.detect_environment(build_dir, None)

        # Use the new DaemonConnection context manager API
        with connect_daemon(build_dir, env_name) as conn:
            success = conn.build(
                clean=clean,
                verbose=verbose,
                timeout=timeout,
            )

        if success:
            print("\n✅ Compilation succeeded (fbuild)\n")
        else:
            print("\n❌ Compilation failed (fbuild)\n")

        return success

    except KeyboardInterrupt:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly

        print("\nKeyboardInterrupt: Stopping compilation")
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"\n❌ Compilation error: {e}\n")
        return False


def run_fbuild_upload(
    build_dir: Path,
    environment: str | None = None,
    upload_port: str | None = None,
    verbose: bool = False,
    timeout: float = 1800,
) -> bool:
    """Upload firmware using fbuild.

    This function uses fbuild deploy without monitoring. For upload+monitor,
    use the run_fbuild_deploy function instead.

    Args:
        build_dir: Project directory containing platformio.ini
        environment: Build environment (None = auto-detect)
        upload_port: Serial port (None = auto-detect)
        verbose: Enable verbose output
        timeout: Maximum deploy time in seconds (default: 30 minutes)

    Returns:
        True if upload succeeded, False otherwise
    """
    print("=" * 60)
    print("UPLOADING (fbuild)")
    print("=" * 60)

    try:
        # Determine environment if not specified
        env_name = environment
        if env_name is None:
            from fbuild.cli_utils import EnvironmentDetector

            env_name = EnvironmentDetector.detect_environment(build_dir, None)

        # Use the new DaemonConnection context manager API
        with connect_daemon(build_dir, env_name) as conn:
            success = conn.deploy(
                port=upload_port,
                clean=False,
                skip_build=True,  # Upload-only mode - firmware already compiled
                monitor_after=False,  # Don't monitor - FastLED has its own monitoring
                timeout=timeout,
            )

        if success:
            print("\n✅ Upload succeeded (fbuild)\n")
        else:
            print("\n❌ Upload failed (fbuild)\n")

        return success

    except KeyboardInterrupt:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly

        print("\nKeyboardInterrupt: Stopping upload")
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"\n❌ Upload error: {e}\n")
        return False


def run_fbuild_deploy(
    build_dir: Path,
    environment: str | None = None,
    upload_port: str | None = None,
    verbose: bool = False,
    clean: bool = False,
    monitor_after: bool = False,
    monitor_timeout: float | None = None,
    monitor_halt_on_error: str | None = None,
    monitor_halt_on_success: str | None = None,
    monitor_expect: str | None = None,
    timeout: float = 1800,
) -> bool:
    """Deploy firmware using fbuild with optional monitoring.

    This function uses fbuild deploy with optional built-in monitoring.
    For FastLED's advanced monitoring features (JSON-RPC, multiple patterns, etc.),
    use run_fbuild_upload + the existing run_monitor function instead.

    Args:
        build_dir: Project directory containing platformio.ini
        environment: Build environment (None = auto-detect)
        upload_port: Serial port (None = auto-detect)
        verbose: Enable verbose output
        clean: Perform clean build before deploy
        monitor_after: Start monitoring after deploy
        monitor_timeout: Monitoring timeout in seconds
        monitor_halt_on_error: Pattern that triggers error exit (regex)
        monitor_halt_on_success: Pattern that triggers success exit (regex)
        monitor_expect: Expected pattern to check at timeout/success (regex)
        timeout: Maximum deploy time in seconds (default: 30 minutes)

    Returns:
        True if deploy (and optional monitoring) succeeded, False otherwise
    """
    print("=" * 60)
    print("DEPLOYING (fbuild)")
    print("=" * 60)

    try:
        # Determine environment if not specified
        env_name = environment
        if env_name is None:
            from fbuild.cli_utils import EnvironmentDetector

            env_name = EnvironmentDetector.detect_environment(build_dir, None)

        # Use the new DaemonConnection context manager API
        with connect_daemon(build_dir, env_name) as conn:
            success = conn.deploy(
                port=upload_port,
                clean=clean,
                monitor_after=monitor_after,
                monitor_timeout=monitor_timeout,
                monitor_halt_on_error=monitor_halt_on_error,
                monitor_halt_on_success=monitor_halt_on_success,
                monitor_expect=monitor_expect,
                timeout=timeout,
            )

        if success:
            print("\n✅ Deploy succeeded (fbuild)\n")
        else:
            print("\n❌ Deploy failed (fbuild)\n")

        return success

    except KeyboardInterrupt:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly

        print("\nKeyboardInterrupt: Stopping deploy")
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"\n❌ Deploy error: {e}\n")
        return False


def run_fbuild_monitor(
    build_dir: Path,
    environment: str | None = None,
    port: str | None = None,
    baud_rate: int = 115200,
    timeout: float | None = None,
    halt_on_error: str | None = None,
    halt_on_success: str | None = None,
    expect: str | None = None,
) -> bool:
    """Monitor serial output using fbuild.

    Note: fbuild monitor is simpler than FastLED's run_monitor. For advanced
    features like multiple expect patterns, JSON-RPC, input-on-trigger, etc.,
    use the existing run_monitor function instead.

    Args:
        build_dir: Project directory
        environment: Build environment (None = auto-detect)
        port: Serial port (None = auto-detect)
        baud_rate: Serial baud rate (default: 115200)
        timeout: Monitoring timeout in seconds (None = no timeout)
        halt_on_error: Pattern that triggers error exit (regex)
        halt_on_success: Pattern that triggers success exit (regex)
        expect: Expected pattern to check at timeout/success (regex)

    Returns:
        True if monitoring succeeded, False otherwise
    """
    try:
        # Determine environment if not specified
        env_name = environment
        if env_name is None:
            from fbuild.cli_utils import EnvironmentDetector

            env_name = EnvironmentDetector.detect_environment(build_dir, None)

        # Use the new DaemonConnection context manager API
        with connect_daemon(build_dir, env_name) as conn:
            success = conn.monitor(
                port=port,
                baud_rate=baud_rate,
                halt_on_error=halt_on_error,
                halt_on_success=halt_on_success,
                expect=expect,
                timeout=timeout,
            )

        return success

    except KeyboardInterrupt:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly

        print("\nKeyboardInterrupt: Stopping monitor")
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"\n❌ Monitor error: {e}\n")
        return False


def stop_fbuild_daemon() -> bool:
    """Stop the fbuild daemon.

    Returns:
        True if daemon was stopped, False otherwise
    """
    return stop_daemon()


def get_fbuild_daemon_status() -> dict[str, Any]:
    """Get current fbuild daemon status.

    Returns:
        Dictionary with daemon status information
    """
    return get_daemon_status()


def is_fbuild_daemon_running() -> bool:
    """Check if fbuild daemon is running.

    Returns:
        True if daemon is running, False otherwise
    """
    return is_daemon_running()
