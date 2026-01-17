#!/usr/bin/env python3
"""PlatformIO command runner with Git Bash compatibility.

This module provides a wrapper around subprocess execution that automatically
handles Windows Git Bash environment issues by running commands via cmd.exe
when necessary.

For background on the Git Bash compatibility issue, see:
    ci/util/windows_cmd_runner.py
"""

import atexit
import subprocess
from pathlib import Path
from typing import Any

from running_process import RunningProcess

from ci.compiler.build_utils import get_pio_execution_env
from ci.util.build_process_client import (
    register_build_process,
    unregister_build_process,
)
from ci.util.pio_process_killer import (
    TrackedPopen,
    register_pio_process,
    unregister_pio_process,
)
from ci.util.windows_cmd_runner import format_cmd_for_shell, should_use_cmd_runner


def create_pio_process(
    cmd: list[str],
    cwd: Path | str,
    output_formatter: Any | None = None,
    auto_run: bool = True,
) -> RunningProcess:
    """Create a RunningProcess for PlatformIO commands with Git Bash compatibility.

    On Windows Git Bash: Runs command via cmd.exe with clean environment
    On other platforms: Runs command directly

    Args:
        cmd: Command to execute as list of strings
        cwd: Working directory for command execution
        output_formatter: Optional formatter for output lines
        auto_run: If True, start the process immediately

    Returns:
        RunningProcess instance configured for the current environment
    """
    env = get_pio_execution_env()

    # Convert cwd to Path if it's a string (RunningProcess expects Path | None)
    cwd_path = Path(cwd) if isinstance(cwd, str) else cwd

    if should_use_cmd_runner():
        # Windows Git Bash: Convert to shell command for cmd.exe
        cmd_str = format_cmd_for_shell(cmd)
        proc = RunningProcess(
            cmd_str,
            cwd=cwd_path,
            auto_run=auto_run,
            output_formatter=output_formatter,
            env=env,
            shell=True,  # Use shell=True for cmd.exe execution
        )
    else:
        # Linux/Mac or native Windows shell: Direct execution
        proc = RunningProcess(
            cmd,
            cwd=cwd_path,
            auto_run=auto_run,
            output_formatter=output_formatter,
            env=env,
        )

    # Register process for orphan cleanup by daemon and for atexit killing
    if auto_run and hasattr(proc, "pid") and proc.pid:  # type: ignore[reportUnknownMemberType]
        register_build_process(
            root_pid=proc.pid,  # type: ignore[reportUnknownMemberType, reportUnknownArgumentType]
            project_dir=str(cwd),
        )
        # Unregister on normal exit
        atexit.register(unregister_build_process)

        # Also register with process killer for atexit cleanup
        if hasattr(proc, "proc") and proc.proc is not None:  # type: ignore[reportUnknownMemberType]
            register_pio_process(proc.proc)  # type: ignore[reportUnknownMemberType, reportUnknownArgumentType]
            atexit.register(lambda p=proc.proc: unregister_pio_process(p))  # type: ignore[reportUnknownMemberType, reportUnknownArgumentType, reportUnknownLambdaType]

    return proc


def run_pio_command(
    cmd: list[str],
    cwd: Path | str | None = None,
    capture_output: bool = False,
    timeout: int | None = None,
) -> subprocess.CompletedProcess[str]:
    """Run a PlatformIO command with Git Bash compatibility.

    This is a simpler alternative to create_pio_process() for commands that
    don't need streaming output. The process is automatically tracked and will
    be killed on program exit if still running.

    Args:
        cmd: Command to execute as list of strings
        cwd: Working directory for command execution
        capture_output: If True, capture stdout and stderr
        timeout: Optional timeout in seconds

    Returns:
        subprocess.CompletedProcess result
    """
    env = get_pio_execution_env()
    stdout_pipe = subprocess.PIPE if capture_output else None
    stderr_pipe = subprocess.PIPE if capture_output else None
    cwd_str = str(cwd) if cwd else None

    if should_use_cmd_runner():
        # Windows Git Bash: Run via cmd.exe with tracking
        cmd_str = format_cmd_for_shell(cmd)
        with TrackedPopen(
            cmd_str,
            shell=True,
            env=env,
            cwd=cwd_str,
            stdout=stdout_pipe,
            stderr=stderr_pipe,
            text=True,
        ) as tracked:
            try:
                stdout, stderr = tracked.communicate(timeout=timeout)
            except subprocess.TimeoutExpired:
                tracked.kill()
                stdout, stderr = tracked.communicate()
                raise subprocess.TimeoutExpired(
                    cmd, timeout or 0, output=stdout, stderr=stderr
                )
            retcode = tracked.returncode
        return subprocess.CompletedProcess(cmd, retcode or 0, stdout, stderr)
    else:
        # Linux/Mac or native Windows shell: Direct execution with tracking
        with TrackedPopen(
            cmd,
            env=env,
            cwd=cwd_str,
            stdout=stdout_pipe,
            stderr=stderr_pipe,
            text=True,
        ) as tracked:
            try:
                stdout, stderr = tracked.communicate(timeout=timeout)
            except subprocess.TimeoutExpired:
                tracked.kill()
                stdout, stderr = tracked.communicate()
                raise subprocess.TimeoutExpired(
                    cmd, timeout or 0, output=stdout, stderr=stderr
                )
            retcode = tracked.returncode
        return subprocess.CompletedProcess(cmd, retcode or 0, stdout, stderr)
