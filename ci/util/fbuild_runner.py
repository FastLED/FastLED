#!/usr/bin/env python3
"""fbuild wrapper for FastLED build operations.

This module provides a wrapper around fbuild commands for use by the FastLED
CI system. It integrates with the fbuild DaemonConnection for build and deploy operations.

Usage:
    from ci.util.fbuild_runner import (
        run_fbuild_compile,
        run_fbuild_upload,
        run_fbuild_deploy,
    )

    # Compile using fbuild
    success = run_fbuild_compile(build_dir, environment="esp32s3", verbose=True)

    # Upload using fbuild
    success = run_fbuild_upload(build_dir, environment="esp32s3", port="COM3")

    # Combined build+deploy (enables firmware ledger skip)
    success = run_fbuild_deploy(build_dir, environment="esp32s3", upload_port="COM3")
"""

from __future__ import annotations

import io
import shutil
import sys
import time
from dataclasses import dataclass
from pathlib import Path
from typing import IO, Any, cast

from fbuild import Daemon, connect_daemon


@dataclass
class FbuildCommandResult:
    """Result for an fbuild CLI invocation."""

    success: bool
    output: str
    returncode: int | None = None


def get_fbuild_executable() -> str | None:
    """Resolve the ``fbuild`` executable for the active Python environment.

    Prefer the sibling script next to the current interpreter so ``uv run``
    reliably uses the version locked in this repo's virtualenv instead of an
    older global ``fbuild`` earlier on ``PATH``.
    """
    script_dir = Path(sys.executable).resolve().parent
    candidates = [
        script_dir / "fbuild",
        script_dir / "fbuild.exe",
    ]
    for candidate in candidates:
        if candidate.exists():
            return str(candidate)
    return shutil.which("fbuild")


def ensure_fbuild_daemon() -> None:
    """Ensure the fbuild daemon is running."""
    Daemon.ensure_running()


def _get_output(quiet: bool, log_file: IO[str] | None) -> IO[str]:
    """Return the appropriate output stream."""
    if quiet and log_file is not None:
        return log_file
    if quiet:
        return io.StringIO()  # discard
    return sys.stdout


def run_fbuild_compile(
    build_dir: Path,
    environment: str | None = None,
    verbose: bool = False,
    clean: bool = False,
    timeout: float = 1800,
    quiet: bool = False,
    log_file: IO[str] | None = None,
) -> FbuildCommandResult:
    """Compile the project using fbuild CLI subprocess.

    Uses ``fbuild build`` as a subprocess so that Ctrl+C terminates it
    immediately (the Rust binary handles SIGINT natively).

    Args:
        build_dir: Project directory containing platformio.ini
        environment: Build environment (None = auto-detect)
        verbose: Enable verbose output
        clean: Perform clean build
        timeout: Maximum build time in seconds (default: 30 minutes)
        quiet: Suppress verbose output (emit single summary line)
        log_file: File to redirect verbose output to in quiet mode

    Returns:
        Result containing success flag, return code, and captured output
    """
    import subprocess

    from running_process import RunningProcess

    if environment is None:
        raise ValueError("environment must be specified for fbuild compilation")

    out = _get_output(quiet, log_file)
    print("=" * 60, file=out)
    print("COMPILING (fbuild)", file=out)
    print("=" * 60, file=out)

    fbuild_exe = get_fbuild_executable()
    if fbuild_exe is None:
        message = "BUILD FAIL fbuild not found on PATH"
        print(message, file=out)
        return FbuildCommandResult(success=False, output=message)

    cmd: list[str] = [
        fbuild_exe,
        str(build_dir),
        "build",
        "-e",
        environment,
    ]
    if verbose:
        cmd.append("-v")
    if clean:
        cmd.append("-c")

    print(f"Running: {subprocess.list2cmdline(cmd)}", file=out)
    print(file=out)

    t0 = time.monotonic()
    try:
        process = RunningProcess(
            cmd,
            timeout=int(timeout),
            auto_run=False,
            capture=True,
        )
        process.start()
        returncode = cast(
            int, process.wait(echo=bool(not quiet or log_file is not None))
        )
        output = str(process.stdout)
        success = returncode == 0
    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt

        print("\nKeyboardInterrupt: Stopping compilation")
        handle_keyboard_interrupt(ki)
        raise
    except subprocess.TimeoutExpired:
        elapsed = time.monotonic() - t0
        message = f"BUILD FAIL timeout after {elapsed:.1f}s"
        print(message, file=out)
        return FbuildCommandResult(success=False, output=message)
    except Exception as e:
        message = f"BUILD FAIL {e}"
        print(message, file=out)
        return FbuildCommandResult(success=False, output=message)

    elapsed = time.monotonic() - t0
    if quiet:
        status = "ok" if success else "FAIL"
        print(f"BUILD {status} {elapsed:.1f}s")
    else:
        if success:
            print(f"\n✅ Compilation succeeded (fbuild) [{elapsed:.1f}s]\n")
        else:
            print(f"\n❌ Compilation failed (fbuild) [{elapsed:.1f}s]\n")

    return FbuildCommandResult(
        success=success,
        output=output,
        returncode=returncode,
    )


def run_fbuild_upload(
    build_dir: Path,
    environment: str | None = None,
    upload_port: str | None = None,
    verbose: bool = False,
    timeout: float = 1800,
    quiet: bool = False,
    log_file: IO[str] | None = None,
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
        quiet: Suppress verbose output (emit single summary line)
        log_file: File to redirect verbose output to in quiet mode

    Returns:
        True if upload succeeded, False otherwise
    """
    out = _get_output(quiet, log_file)
    print("=" * 60, file=out)
    print("UPLOADING (fbuild)", file=out)
    print("=" * 60, file=out)

    ensure_fbuild_daemon()

    t0 = time.monotonic()
    try:
        if environment is None:
            raise ValueError("environment must be specified for fbuild upload")

        with connect_daemon(str(build_dir), environment) as conn:
            success: bool = conn.deploy(
                port=upload_port,
                clean=False,
                skip_build=True,  # Upload-only mode - firmware already compiled
                monitor_after=False,  # Don't monitor - FastLED has its own monitoring
                timeout=timeout,
            )

        elapsed = time.monotonic() - t0
        if quiet:
            status = "ok" if success else "FAIL"
            print(f"FLASH {status} {elapsed:.1f}s")
        else:
            if success:
                print(f"\n✅ Upload succeeded (fbuild) [{elapsed:.1f}s]\n")
            else:
                print(f"\n❌ Upload failed (fbuild) [{elapsed:.1f}s]\n")

        return success

    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt

        print("\nKeyboardInterrupt: Stopping upload")
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        elapsed = time.monotonic() - t0
        print(f"FLASH FAIL {e}")
        return False


def run_fbuild_deploy(
    build_dir: Path,
    environment: str | None = None,
    upload_port: str | None = None,
    verbose: bool = False,
    clean: bool = False,
    monitor_after: bool = False,
    timeout: float = 1800,
    quiet: bool = False,
    log_file: IO[str] | None = None,
) -> bool:
    """Deploy firmware using fbuild with optional monitoring.

    Uses the daemon's combined build+deploy path which checks the firmware
    ledger — if source hash + build flags are unchanged, the entire
    build+flash is skipped (returns in ~1s).

    Args:
        build_dir: Project directory containing platformio.ini
        environment: Build environment (None = auto-detect)
        upload_port: Serial port (None = auto-detect)
        verbose: Enable verbose output
        clean: Perform clean build before deploy
        monitor_after: Start monitoring after deploy
        timeout: Maximum deploy time in seconds (default: 30 minutes)
        quiet: Suppress verbose output (emit single summary line)
        log_file: File to redirect verbose output to in quiet mode

    Returns:
        True if deploy (and optional monitoring) succeeded, False otherwise
    """
    out = _get_output(quiet, log_file)
    print("=" * 60, file=out)
    print("DEPLOYING (fbuild)", file=out)
    print("=" * 60, file=out)

    ensure_fbuild_daemon()

    t0 = time.monotonic()
    try:
        if environment is None:
            raise ValueError("environment must be specified for fbuild deploy")

        with connect_daemon(str(build_dir), environment) as conn:
            success: bool = conn.deploy(
                port=upload_port,
                clean=clean,
                monitor_after=monitor_after,
                timeout=timeout,
            )

        elapsed = time.monotonic() - t0
        if quiet:
            status = "ok" if success else "FAIL"
            print(f"BUILD+FLASH {status} {elapsed:.1f}s")
        else:
            if success:
                print(f"\n✅ Deploy succeeded (fbuild) [{elapsed:.1f}s]\n")
            else:
                print(f"\n❌ Deploy failed (fbuild) [{elapsed:.1f}s]\n")

        return success

    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt

        print("\nKeyboardInterrupt: Stopping deploy")
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        elapsed = time.monotonic() - t0
        print(f"BUILD+FLASH FAIL {e}")
        return False


def run_fbuild_monitor(
    build_dir: Path,
    environment: str | None = None,
    port: str | None = None,
    baud_rate: int = 115200,
    timeout: float | None = None,
) -> bool:
    """Monitor serial output using fbuild.

    Args:
        build_dir: Project directory
        environment: Build environment (None = auto-detect)
        port: Serial port (None = auto-detect)
        baud_rate: Serial baud rate (default: 115200)
        timeout: Monitoring timeout in seconds (None = no timeout)

    Returns:
        True if monitoring succeeded, False otherwise
    """
    ensure_fbuild_daemon()

    try:
        if environment is None:
            raise ValueError("environment must be specified for fbuild monitor")

        with connect_daemon(str(build_dir), environment) as conn:
            success: bool = conn.monitor(
                port=port,
                baud_rate=baud_rate,
                timeout=timeout,
            )

        return success

    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt

        print("\nKeyboardInterrupt: Stopping monitor")
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        print(f"\n❌ Monitor error: {e}\n")
        return False


def stop_fbuild_daemon() -> None:
    """Stop the fbuild daemon."""
    Daemon.stop()


def get_fbuild_daemon_status() -> dict[str, Any]:
    """Get current fbuild daemon status.

    Returns:
        Dictionary with daemon status information
    """
    return Daemon.status()  # type: ignore[return-value]
