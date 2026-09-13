#!/usr/bin/env python3
import sys
from typing import Any

from running_process import CalledProcessError, RunningProcess
from running_process.command_render import list2cmdline

from ci.util.test_types import TestArgs


def build_cpp_test_command(args: TestArgs) -> str:
    """Build the C++ test command based on arguments"""
    # Use test.py with appropriate flags for C++ testing via Meson
    cmd_list = ["uv", "run", "python", "test.py"]

    # Always run C++ tests (unit tests)
    cmd_list.append("--unit")

    if args.clang:
        # Note: Meson uses clang-tool-chain's Clang, so --clang flag still applies
        cmd_list.append("--clang")

    if args.test:
        # Pass specific test name
        cmd_list.append(args.test)

    if args.clean:
        cmd_list.append("--clean")

    if args.verbose:
        cmd_list.append("--verbose")

    if args.show_compile:
        cmd_list.append("--show-compile")

    if args.show_link:
        cmd_list.append("--show-link")

    if args.check:
        cmd_list.append("--check")

    return list2cmdline(cmd_list)


def make_compile_uno_test_process(enable_stack_trace: bool = True) -> RunningProcess:
    """Create a process to compile the uno tests"""
    cmd = [
        "uv",
        "run",
        "python",
        "-m",
        "ci.ci-compile",
        "uno",
        "--examples",
        "Blink",
        "--no-interactive",
        "--local",
    ]
    return RunningProcess(cmd, auto_run=True)


def run_command(cmd: list[str], **kwargs: Any) -> None:
    """Run a command and handle errors"""
    try:
        RunningProcess.run(cmd, check=True, **kwargs)
    except CalledProcessError as e:
        sys.exit(e.returncode)
