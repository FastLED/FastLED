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
import os
import re
import shutil
import sys
import time
from dataclasses import dataclass, field
from functools import lru_cache
from pathlib import Path
from typing import IO, Any, cast

from fbuild import Daemon, connect_daemon
from typeguard import typechecked

from ci.util.cpu_count import cpu_count


@dataclass
class FbuildCommandResult:
    """Result for an fbuild CLI invocation."""

    success: bool
    output: str
    returncode: int | None = None


@typechecked
@dataclass
class FbuildCompileManySketchResult:
    """Result for one sketch reported by ``fbuild compile-many``."""

    sketch_dir: Path
    success: bool
    stage: str
    build_time_secs: float
    log_path: Path | None
    message: str


@typechecked
@dataclass
class FbuildCompileManyResult(FbuildCommandResult):
    """Result for an ``fbuild compile-many`` invocation."""

    sketch_results: list[FbuildCompileManySketchResult] = field(default_factory=list)


_COMPILE_MANY_RESULT_RE = re.compile(
    r"^\s+\[(?P<stage>stage1|stage2)\]\s+"
    r"(?P<status>OK|FAIL)\s+"
    r"\((?P<secs>[0-9.]+)s\)\s+"
    r"(?P<sketch>.*?)\s{2}log="
    r"(?P<log>.*?)\s{2}(?P<message>.*)$"
)


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


def fbuild_subprocess_env() -> dict[str, str]:
    """Build an env dict with the venv's Scripts dir prepended to PATH.

    Why this exists (#2853): fbuild internally shells out to helper tools
    like ``zccache`` via plain PATH resolution. If the user has another
    ``zccache.exe`` earlier on the system PATH (e.g.
    ``C:\\tools\\python13\\Scripts\\zccache.exe`` from a globally
    installed ``clang-tool-chain``), it can race with the venv-installed
    one and produce cross-version daemon corruption — the symptom that
    motivated #2853 (5 GB daemon balloons, locked named pipes, "daemon
    started but not accepting connections after 10s").

    Building the env here ensures any tool fbuild's Rust binary resolves
    via PATH picks the venv copy first. This mirrors what
    ``get_fbuild_executable()`` already does for ``fbuild`` itself —
    extending the same hygiene to its dependencies.
    """
    env = os.environ.copy()
    venv_scripts = Path(sys.executable).resolve().parent
    if venv_scripts.is_dir():
        path_sep = os.pathsep
        existing_path = env.get("PATH", "")
        # Only prepend if not already first — avoids unbounded growth on
        # repeated invocations.
        existing_head = existing_path.split(path_sep, 1)[0] if existing_path else ""
        if existing_head.lower() != str(venv_scripts).lower():
            env["PATH"] = str(venv_scripts) + (
                path_sep + existing_path if existing_path else ""
            )
    return env


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


def _get_env_job_count(name: str) -> int | None:
    """Read a positive integer job override from the environment."""
    raw_value = os.environ.get(name)
    if raw_value is None:
        return None
    try:
        value = int(raw_value)
    except ValueError:
        print(f"Warning: ignoring invalid {name}={raw_value!r}; expected integer")
        return None
    if value < 1:
        print(f"Warning: ignoring invalid {name}={raw_value!r}; expected >= 1")
        return None
    return value


def default_fbuild_framework_jobs() -> int:
    """Default stage-1 parallelism for batched ``fbuild`` sketch builds."""
    return _get_env_job_count("FASTLED_FRAMEWORK_JOBS") or 1


def default_fbuild_sketch_jobs() -> int:
    """Default stage-2 parallelism for batched ``fbuild`` sketch builds."""
    env_override = _get_env_job_count("FASTLED_SKETCH_JOBS")
    if env_override is not None:
        return env_override
    cpus = cpu_count()
    if os.environ.get("GITHUB_ACTIONS") == "true":
        return min(cpus, 2)
    return cpus


def _fbuild_supports_subcommand(subcommand: str) -> bool:
    """Return whether the active fbuild executable exposes ``subcommand``.

    Probes with ``<fbuild> <subcommand> --help`` rather than ``<fbuild> help
    <subcommand>``: the latter parses ``help`` as the subcommand name and
    ``<subcommand>`` as a positional arg, which fails clap arg validation
    (exit 2) on any subcommand with required args like ``--board``. With
    ``--help`` clap intercepts and prints help with exit 0 regardless of
    other required args. The old probe silently disabled compile-many on
    every CI run and forced the legacy serial fallback path.
    """
    import subprocess

    fbuild_exe = get_fbuild_executable()
    if fbuild_exe is None:
        return False
    try:
        proc = subprocess.run(
            [fbuild_exe, subcommand, "--help"],
            capture_output=True,
            text=True,
            timeout=15,
            check=False,
        )
    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt

        handle_keyboard_interrupt(ki)
        raise
    except (FileNotFoundError, subprocess.SubprocessError, OSError):
        return False
    return proc.returncode == 0


@lru_cache(maxsize=1)
def fbuild_supports_ci() -> bool:
    """Return whether the active fbuild executable exposes ``ci``."""
    return _fbuild_supports_subcommand("ci")


@lru_cache(maxsize=1)
def fbuild_supports_compile_many() -> bool:
    """Return whether the active fbuild executable exposes ``compile-many``."""
    return _fbuild_supports_subcommand("compile-many")


# Parses size lines emitted into per-sketch fbuild logs. Matches both unit
# variants the AVR/ESP orchestrators emit:
#   "Flash: 2.75KB / 31.50KB (8.7%)"
#   "RAM:   223 bytes / 2.00KB (10.9%)"
# Returns the leading number+unit (e.g. "2.75KB", "223 bytes") so the caller
# can render it back without unit conversion.
_FLASH_SIZE_RE = re.compile(r"^\s*Flash:\s+([0-9.]+(?:\s*KB|\s*MB|\s*bytes))\s*/")
_RAM_SIZE_RE = re.compile(r"^\s*RAM:\s+([0-9.]+(?:\s*KB|\s*MB|\s*bytes))\s*/")


def _parse_size_info_from_log(log_path: Path | None) -> tuple[str | None, str | None]:
    """Pull the (flash, ram) size strings out of an fbuild per-sketch log.

    Returns (None, None) when the log is missing or doesn't contain the
    Flash/RAM lines (e.g. a stage-1 fingerprint cache hit prints a shorter
    log without size info). Callers should gracefully omit size in that case.
    """
    if log_path is None or not log_path.exists():
        return (None, None)
    try:
        content = log_path.read_text(encoding="utf-8", errors="ignore")
    except OSError:
        return (None, None)
    flash: str | None = None
    ram: str | None = None
    for line in content.splitlines():
        if flash is None:
            m = _FLASH_SIZE_RE.match(line)
            if m:
                flash = m.group(1).strip()
                continue
        if ram is None:
            m = _RAM_SIZE_RE.match(line)
            if m:
                ram = m.group(1).strip()
                continue
        if flash is not None and ram is not None:
            break
    return (flash, ram)


def _parse_compile_many_results(output: str) -> list[FbuildCompileManySketchResult]:
    """Extract per-sketch results from ``fbuild compile-many`` output."""
    results: list[FbuildCompileManySketchResult] = []
    for line in output.splitlines():
        match = _COMPILE_MANY_RESULT_RE.match(line)
        if match is None:
            continue
        log_path_raw = match.group("log")
        log_path = None if log_path_raw == "-" else Path(log_path_raw)
        results.append(
            FbuildCompileManySketchResult(
                sketch_dir=Path(match.group("sketch")),
                success=match.group("status") == "OK",
                stage=match.group("stage"),
                build_time_secs=float(match.group("secs")),
                log_path=log_path,
                message=match.group("message"),
            )
        )
    return results


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
            env=fbuild_subprocess_env(),
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


def _run_fbuild_batch_command(
    command_name: str,
    board: str,
    sketch_project_dirs: list[Path],
    verbose: bool,
    timeout: float,
    quiet: bool,
    log_file: IO[str] | None,
) -> FbuildCompileManyResult:
    """Compile many sketches with one batched ``fbuild`` invocation."""
    import subprocess

    from running_process import RunningProcess

    if not sketch_project_dirs:
        raise ValueError("sketch_project_dirs must not be empty")

    out = _get_output(quiet, log_file)
    print("=" * 60, file=out)
    print(f"COMPILING MANY (fbuild {command_name})", file=out)
    print("=" * 60, file=out)

    fbuild_exe = get_fbuild_executable()
    if fbuild_exe is None:
        message = "BUILD FAIL fbuild not found on PATH"
        print(message, file=out)
        return FbuildCompileManyResult(success=False, output=message)

    framework_jobs = default_fbuild_framework_jobs()
    sketch_jobs = default_fbuild_sketch_jobs()
    cmd: list[str] = [
        fbuild_exe,
        command_name,
        "--board",
        board,
        "--framework-jobs",
        str(framework_jobs),
        "--sketch-jobs",
        str(sketch_jobs),
    ]
    if verbose:
        cmd.append("-v")
    cmd.extend(str(path) for path in sketch_project_dirs)

    print(f"Running: {subprocess.list2cmdline(cmd)}", file=out)
    print(file=out)

    t0 = time.monotonic()
    try:
        process = RunningProcess(
            cmd,
            timeout=int(timeout),
            auto_run=False,
            capture=True,
            env=fbuild_subprocess_env(),
        )
        process.start()
        returncode = cast(
            int, process.wait(echo=bool(not quiet or log_file is not None))
        )
        output = str(process.stdout)
        sketch_results = _parse_compile_many_results(output)
        success = returncode == 0
        expected_results = len(sketch_project_dirs)
        if success and len(sketch_results) != expected_results:
            message = (
                f"BUILD FAIL {command_name} returned 0 but parsed "
                f"{len(sketch_results)}/{expected_results} per-sketch results "
                "from stdout (output format drift?)"
            )
            print(message, file=out)
            return FbuildCompileManyResult(
                success=False,
                output=output,
                returncode=returncode,
            )
    except KeyboardInterrupt as ki:
        from ci.util.global_interrupt_handler import handle_keyboard_interrupt

        print(f"\nKeyboardInterrupt: Stopping {command_name}")
        handle_keyboard_interrupt(ki)
        raise
    except subprocess.TimeoutExpired:
        elapsed = time.monotonic() - t0
        message = f"BUILD FAIL timeout after {elapsed:.1f}s"
        print(message, file=out)
        return FbuildCompileManyResult(success=False, output=message)
    except Exception as e:
        message = f"BUILD FAIL {e}"
        print(message, file=out)
        return FbuildCompileManyResult(success=False, output=message)

    elapsed = time.monotonic() - t0
    if quiet:
        status = "ok" if success else "FAIL"
        print(f"BUILD {status} {elapsed:.1f}s")
    else:
        if success:
            print(f"\nCompilation succeeded (fbuild {command_name}) [{elapsed:.1f}s]\n")
        else:
            print(f"\nCompilation failed (fbuild {command_name}) [{elapsed:.1f}s]\n")

    return FbuildCompileManyResult(
        success=success,
        output=output,
        returncode=returncode,
        sketch_results=sketch_results,
    )


def run_fbuild_ci(
    board: str,
    sketch_project_dirs: list[Path],
    verbose: bool,
    timeout: float,
    quiet: bool,
    log_file: IO[str] | None,
) -> FbuildCompileManyResult:
    """Compile many sketches with one ``fbuild ci`` invocation."""
    return _run_fbuild_batch_command(
        "ci",
        board,
        sketch_project_dirs,
        verbose,
        timeout,
        quiet,
        log_file,
    )


def run_fbuild_compile_many(
    board: str,
    sketch_project_dirs: list[Path],
    verbose: bool,
    timeout: float,
    quiet: bool,
    log_file: IO[str] | None,
) -> FbuildCompileManyResult:
    """Compile many sketches with one ``fbuild compile-many`` invocation."""
    return _run_fbuild_batch_command(
        "compile-many",
        board,
        sketch_project_dirs,
        verbose,
        timeout,
        quiet,
        log_file,
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
    import subprocess

    out = _get_output(quiet, log_file)
    print("=" * 60, file=out)
    print("DEPLOYING (fbuild)", file=out)
    print("=" * 60, file=out)

    if environment is None:
        raise ValueError("environment must be specified for fbuild deploy")

    fbuild_exe = get_fbuild_executable()
    if fbuild_exe is None:
        print("BUILD+FLASH FAIL fbuild not found on PATH")
        return False

    cmd: list[str] = [
        fbuild_exe,
        str(build_dir),
        "deploy",
        "-e",
        environment,
    ]
    if upload_port:
        cmd.extend(["-p", upload_port])
    if verbose:
        cmd.append("-v")
    if clean:
        cmd.append("-c")
    if monitor_after:
        cmd.append("--monitor")

    env = os.environ.copy()
    force_restart_daemon = False
    if environment.lower() == "esp32p4":
        for name in ("FBUILD_USE_ESPFLASH_VERIFY", "FBUILD_USE_ESPFLASH_WRITE"):
            if not env.get(name):
                env[name] = "0"
                force_restart_daemon = True
    if force_restart_daemon:
        subprocess.run(
            [fbuild_exe, "daemon", "stop"],
            capture_output=True,
            text=True,
            env=env,
            check=False,
        )

    t0 = time.monotonic()
    try:
        print(f"Running: {subprocess.list2cmdline(cmd)}", file=out)
        print(file=out)
        proc = subprocess.run(
            cmd,
            timeout=int(timeout),
            env=env,
            text=True,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            check=False,
        )
        if proc.stdout:
            print(proc.stdout, file=out, end="")
        returncode = proc.returncode
        success = returncode == 0

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
    except subprocess.TimeoutExpired:
        elapsed = time.monotonic() - t0
        print(f"BUILD+FLASH FAIL timeout after {elapsed:.1f}s")
        return False
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
