#!/usr/bin/env python3
import subprocess
import sys
from pathlib import Path
from typing import Any, List

from ci.util.running_process import RunningProcess
from ci.util.test_types import TestArgs


def build_cpp_test_command(args: TestArgs) -> str:
    """Build the C++ test command based on arguments"""
    cmd_list = ["uv", "run", "python", "-m", "ci.compiler.cpp_test_run"]

    if args.clang:
        cmd_list.append("--clang")

    if args.test:
        cmd_list.append("--test")
        cmd_list.append(args.test)
    if args.clean:
        cmd_list.append("--clean")
    if args.verbose:
        cmd_list.append("--verbose")  # Always pass verbose flag when enabled
    if args.show_compile:
        cmd_list.append("--show-compile")  # Pass show-compile flag
    if args.show_link:
        cmd_list.append("--show-link")  # Pass show-link flag
    if args.check:
        cmd_list.append("--check")

    if args.no_unity:
        cmd_list.append("--no-unity")

    return subprocess.list2cmdline(cmd_list)


def make_pio_check_cmd() -> List[str]:
    """Create the PlatformIO check command"""
    return [
        "pio",
        "check",
        "--skip-packages",
        "--src-filters=+<src/>",
        "--severity=medium",
        "--fail-on-defect=high",
        "--flags",
        "--inline-suppr --enable=all --std=c++17",
    ]


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
    ]
    return RunningProcess(cmd, auto_run=True, enable_stack_trace=enable_stack_trace)


def run_command(cmd: List[str], **kwargs: Any) -> None:
    """Run a command and handle errors"""
    try:
        subprocess.run(cmd, check=True, **kwargs)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)
