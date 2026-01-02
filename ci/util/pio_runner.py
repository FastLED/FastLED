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

    if should_use_cmd_runner():
        # Windows Git Bash: Convert to shell command for cmd.exe
        cmd_str = format_cmd_for_shell(cmd)
        proc = RunningProcess(
            cmd_str,
            cwd=cwd,
            auto_run=auto_run,
            output_formatter=output_formatter,
            env=env,
            shell=True,  # Use shell=True for cmd.exe execution
        )
    else:
        # Linux/Mac or native Windows shell: Direct execution
        proc = RunningProcess(
            cmd,
            cwd=cwd,
            auto_run=auto_run,
            output_formatter=output_formatter,
            env=env,
        )

    # Register process for orphan cleanup by daemon
    if auto_run and hasattr(proc, "pid") and proc.pid:
        register_build_process(
            root_pid=proc.pid,
            project_dir=str(cwd),
        )
        # Unregister on normal exit
        atexit.register(unregister_build_process)

    return proc


def run_pio_command(
    cmd: list[str],
    cwd: Path | str | None = None,
    capture_output: bool = False,
    timeout: int | None = None,
) -> subprocess.CompletedProcess:
    """Run a PlatformIO command with Git Bash compatibility.

    This is a simpler alternative to create_pio_process() for commands that
    don't need streaming output.

    Args:
        cmd: Command to execute as list of strings
        cwd: Working directory for command execution
        capture_output: If True, capture stdout and stderr
        timeout: Optional timeout in seconds

    Returns:
        subprocess.CompletedProcess result
    """
    env = get_pio_execution_env()

    if should_use_cmd_runner():
        # Windows Git Bash: Run via cmd.exe
        cmd_str = format_cmd_for_shell(cmd)
        return subprocess.run(
            cmd_str,
            shell=True,
            env=env,
            cwd=cwd,
            capture_output=capture_output,
            timeout=timeout,
        )
    else:
        # Linux/Mac or native Windows shell: Direct execution
        return subprocess.run(
            cmd,
            env=env,
            cwd=cwd,
            capture_output=capture_output,
            timeout=timeout,
        )
