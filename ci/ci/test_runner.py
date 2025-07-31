#!/usr/bin/env python3
import os
import queue
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, List, Optional, Protocol, cast

from typeguard import typechecked

from ci.ci.running_process import RunningProcess
from ci.ci.test_types import TestArgs, TestCategories


@typechecked
@dataclass
class TestProcessConfig:
    """Configuration for a test process"""

    command: str | list[str]
    echo: bool = True
    auto_run: bool = False
    timeout: int | None = None
    enable_stack_trace: bool = True
    description: str = ""
    parallel_safe: bool = True  # Whether this process can run in parallel with others
    output_filter: Callable[[str], bool] | None = (
        None  # Filter function for output lines
    )


class ProcessOutputHandler:
    """Handles capturing and displaying process output"""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose

    def handle_output_line(self, line: str, process_name: str) -> None:
        """Process and display a single line of output"""
        # Apply filtering based on verbosity
        if self.verbose or self._should_always_display(line):
            print(f"[{process_name}] {line}")

    def _should_always_display(self, line: str) -> bool:
        """Determine if a line should always be displayed regardless of verbosity"""
        return any(
            marker in line
            for marker in [
                "FAILED",
                "ERROR",
                "Crash",
                "Running test:",
                "Test passed",
                "Test FAILED",
            ]
        )


class ReconfigurableIO(Protocol):
    def reconfigure(self, *, encoding: str, errors: str) -> None: ...


def create_namespace_check_process(enable_stack_trace: bool) -> RunningProcess:
    """Create a namespace check process without starting it"""
    return RunningProcess(
        "uv run python ci/tests/no_using_namespace_fl_in_headers.py",
        auto_run=False,
        enable_stack_trace=enable_stack_trace,
    )


def create_unit_test_process(
    args: TestArgs, enable_stack_trace: bool
) -> RunningProcess:
    """Create a unit test process without starting it"""
    from test import build_cpp_test_command

    cmd_str_cpp = build_cpp_test_command(args)
    # GCC builds are 5x slower due to poor unified compilation performance
    cpp_test_timeout = 900 if args.gcc else 300
    return RunningProcess(
        cmd_str_cpp,
        enable_stack_trace=enable_stack_trace,
        timeout=cpp_test_timeout,
        auto_run=False,
    )


def create_examples_test_process(
    args: TestArgs, enable_stack_trace: bool
) -> RunningProcess:
    """Create an examples test process without starting it"""
    cmd = ["uv", "run", "python", "-m", "ci.compiler.test_example_compilation"]
    if args.examples:
        cmd.extend(args.examples)
    if args.clean:
        cmd.append("--clean")
    if args.no_pch:
        cmd.append("--no-pch")
    if args.cache:
        cmd.append("--cache")
    if args.unity:
        cmd.append("--unity")
    if args.full and args.examples is not None:
        cmd.append("--full")
    return RunningProcess(cmd, auto_run=False, enable_stack_trace=enable_stack_trace)


def create_python_test_process(enable_stack_trace: bool) -> RunningProcess:
    """Create a Python test process without starting it"""
    return RunningProcess(
        "uv run pytest -s ci/tests -xvs --durations=0",
        auto_run=False,
        enable_stack_trace=enable_stack_trace,
    )


def create_integration_test_process(
    args: TestArgs, enable_stack_trace: bool
) -> RunningProcess:
    """Create an integration test process without starting it"""
    cmd = ["uv", "run", "pytest", "-s", "ci/test_integration", "-xvs", "--durations=0"]
    if args.examples is not None:
        # When --examples --full is specified, only run example-related integration tests
        cmd.extend(["-k", "TestFullProgramLinking"])
    if args.verbose:
        cmd.append("-v")
    return RunningProcess(cmd, auto_run=False, enable_stack_trace=enable_stack_trace)


def create_compile_uno_test_process(enable_stack_trace: bool = True) -> RunningProcess:
    """Create a process to compile the uno tests without starting it"""
    cmd = [
        "uv",
        "run",
        "ci/ci-compile.py",
        "uno",
        "--examples",
        "Blink",
        "--no-interactive",
    ]
    return RunningProcess(cmd, auto_run=False, enable_stack_trace=enable_stack_trace)


def get_cpp_test_processes(
    args: TestArgs, test_categories: TestCategories, enable_stack_trace: bool
) -> list[RunningProcess]:
    """Return all processes needed for C++ tests"""
    processes: list[RunningProcess] = []

    # Always include namespace check
    processes.append(create_namespace_check_process(enable_stack_trace))

    if test_categories.unit:
        processes.append(create_unit_test_process(args, enable_stack_trace))

    if test_categories.examples:
        processes.append(create_examples_test_process(args, enable_stack_trace))

    return processes


def get_python_test_processes(enable_stack_trace: bool) -> list[RunningProcess]:
    """Return all processes needed for Python tests"""
    return [create_python_test_process(enable_stack_trace)]


def get_integration_test_processes(
    args: TestArgs, enable_stack_trace: bool
) -> list[RunningProcess]:
    """Return all processes needed for integration tests"""
    return [create_integration_test_process(args, enable_stack_trace)]


def get_all_test_processes(
    args: TestArgs,
    test_categories: TestCategories,
    enable_stack_trace: bool,
    src_code_change: bool,
) -> list[RunningProcess]:
    """Return all processes needed for all tests"""
    processes: list[RunningProcess] = []

    # Always include namespace check
    processes.append(create_namespace_check_process(enable_stack_trace))

    # Add test processes based on categories
    if test_categories.unit:
        processes.append(create_unit_test_process(args, enable_stack_trace))
    if test_categories.examples:
        processes.append(create_examples_test_process(args, enable_stack_trace))
    if test_categories.py:
        processes.append(create_python_test_process(enable_stack_trace))
    if test_categories.integration:
        processes.append(create_integration_test_process(args, enable_stack_trace))

    # Add uno test process if source code changed
    if src_code_change:
        processes.append(create_compile_uno_test_process(enable_stack_trace))

    return processes


def _run_process_with_output(process: RunningProcess, verbose: bool = False) -> None:
    """
    Run a single process and handle its output

    Args:
        process: RunningProcess object to execute
        verbose: Whether to show all output
    """
    process.run()
    process_name = (
        process.command.split()[0]
        if isinstance(process.command, str)
        else process.command[0]
    )

    # Stream output in real-time while the process is running
    while process.poll() is None:
        try:
            line = process.get_next_line(timeout=0.1)
            if line is not None:
                # Always show important output or all output in verbose mode
                if verbose or any(
                    marker in line
                    for marker in [
                        "FAILED",
                        "ERROR",
                        "Crash",
                        "Running test:",
                        "Test passed",
                        "Test FAILED",
                    ]
                ):
                    print(f"[{process_name}] {line}")
        except queue.Empty:
            # No output available right now, continue waiting
            pass

    # Process has completed, check return code
    returncode = process.returncode
    if returncode != 0:
        print(f"Command failed: {process.command}")
        sys.exit(returncode)


def _run_processes_parallel(processes: list[RunningProcess]) -> None:
    """
    Run multiple test processes in parallel and handle their output

    Args:
        processes: List of RunningProcess objects to execute
    """
    if not processes:
        return

    # Configure Windows console for UTF-8 output if needed
    if os.name == "nt":  # Windows
        if hasattr(sys.stdout, "reconfigure"):
            cast(ReconfigurableIO, sys.stdout).reconfigure(
                encoding="utf-8", errors="replace"
            )
        if hasattr(sys.stderr, "reconfigure"):
            cast(ReconfigurableIO, sys.stderr).reconfigure(
                encoding="utf-8", errors="replace"
            )

    # Start all processes
    for proc in processes:
        proc.run()
        print(f"Started: {proc.command}")

    # Monitor all processes for output and completion
    active_processes = processes.copy()
    while active_processes:
        for proc in active_processes[:]:  # Copy list for safe modification
            # Check for new output
            cmd = proc.command
            try:
                while True:
                    line = proc.get_next_line(timeout=0.1)
                    if line is None:
                        break
                    # Print line - encoding handled by console configuration above
                    print(line)
            except queue.Empty:
                # No output available right now, continue checking other processes
                pass
            except TimeoutError:
                print(f"Process timed out: {cmd}")
                for p in active_processes:
                    if p != proc:
                        p.kill()
                sys.exit(1)

            if proc.poll() is not None:
                # Process has finished, call wait() to ensure proper cleanup
                try:
                    returncode = proc.wait()
                    if returncode != 0:
                        print(
                            f"\nCommand failed: {proc.command} with return code {returncode}"
                        )
                        # Kill all remaining processes
                        for p in active_processes:
                            if (
                                p != proc
                            ):  # Don't try to kill the already finished process
                                p.kill()
                        sys.exit(returncode)
                    active_processes.remove(proc)
                except TimeoutError:
                    print(f"\nProcess timed out: {proc.command}")
                    # Kill all processes on timeout
                    for p in active_processes:
                        p.kill()
                    sys.exit(1)
                except Exception as e:
                    print(f"\nError waiting for process: {proc.command}")
                    print(f"Error: {e}")
                    # Kill all processes on error
                    for p in active_processes:
                        p.kill()
                    sys.exit(1)

        # Small sleep to prevent busy waiting
        time.sleep(0.1)

    print("\nAll parallel tests completed successfully")


def run_test_processes(
    processes: list[RunningProcess], parallel: bool = True, verbose: bool = False
) -> None:
    """
    Run multiple test processes and handle their output

    Args:
        processes: List of RunningProcess objects to execute
        parallel: Whether to run processes in parallel or sequentially
        verbose: Whether to show all output
    """
    if not processes:
        return

    if parallel:
        # Run all processes in parallel with output interleaving
        _run_processes_parallel(processes)
    else:
        # Run processes sequentially with full output capture
        for process in processes:
            _run_process_with_output(process, verbose)


def runner(args: TestArgs) -> None:
    """
    Main test runner function that determines what to run and executes tests

    Args:
        args: Parsed command line arguments
    """
    # Determine test categories
    test_categories = TestCategories(
        unit=args.unit,
        examples=args.examples is not None,
        py=args.py,
        integration=args.full and args.examples is None,
        unit_only=args.unit
        and not args.examples
        and not args.py
        and not (args.full and args.examples is None),
        examples_only=args.examples is not None
        and not args.unit
        and not args.py
        and not (args.full and args.examples is None),
        py_only=args.py
        and not args.unit
        and not args.examples
        and not (args.full and args.examples is None),
        integration_only=(args.full and args.examples is None)
        and not args.unit
        and not args.examples
        and not args.py,
    )
    enable_stack_trace = not args.no_stack_trace

    # Get processes to run based on test categories
    processes: list[RunningProcess] = []
    parallel = False

    if test_categories.integration_only:
        processes = get_integration_test_processes(args, enable_stack_trace)
        parallel = False
    elif test_categories.unit_only:
        processes = get_cpp_test_processes(args, test_categories, enable_stack_trace)
        parallel = False
    elif test_categories.examples_only:
        processes = get_cpp_test_processes(args, test_categories, enable_stack_trace)
        parallel = False
    elif test_categories.py_only:
        processes = get_python_test_processes(enable_stack_trace)
        parallel = False
    elif (
        test_categories.unit
        and test_categories.examples
        and not test_categories.py
        and not test_categories.integration
    ):
        # C++ mode: unit + examples
        processes = get_cpp_test_processes(args, test_categories, enable_stack_trace)
        parallel = True
    elif (
        test_categories.unit
        and test_categories.py
        and not test_categories.examples
        and not test_categories.integration
    ):
        # Unit + Python
        processes = []
        processes.append(create_namespace_check_process(enable_stack_trace))
        processes.append(create_unit_test_process(args, enable_stack_trace))
        processes.append(create_python_test_process(enable_stack_trace))
        parallel = True
    elif (
        test_categories.examples
        and test_categories.py
        and not test_categories.unit
        and not test_categories.integration
    ):
        # Examples + Python
        processes = []
        processes.append(create_namespace_check_process(enable_stack_trace))
        processes.append(create_examples_test_process(args, enable_stack_trace))
        processes.append(create_python_test_process(enable_stack_trace))
        parallel = True
    else:
        # All tests or complex combinations
        src_code_change = (
            True  # This should be determined from fingerprint in the main script
        )
        processes = get_all_test_processes(
            args, test_categories, enable_stack_trace, src_code_change
        )
        parallel = True

    # Run the processes
    run_test_processes(processes, parallel=parallel, verbose=args.verbose)
