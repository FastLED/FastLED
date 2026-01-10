#!/usr/bin/env python3
import subprocess
import sys
from typing import Any

from running_process import RunningProcess

from ci.util.test_types import TestArgs


def build_cpp_test_command(args: TestArgs) -> str:
    """Build the C++ test command based on arguments"""
    # Use test.py with appropriate flags for C++ testing via Meson
    cmd_list = ["uv", "run", "python", "test.py"]

    # Always run C++ tests (unit tests)
    cmd_list.append("--unit")

    if args.clang:
        # Note: Meson uses Zig's bundled Clang, so --clang flag still applies
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

    return subprocess.list2cmdline(cmd_list)


def make_pio_check_cmd() -> list[str]:
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
        "--local",
    ]
    return RunningProcess(cmd, auto_run=True)


def run_command(cmd: list[str], **kwargs: Any) -> None:
    """Run a command and handle errors"""
    try:
        subprocess.run(cmd, check=True, **kwargs)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)
