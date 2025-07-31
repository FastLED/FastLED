#!/usr/bin/env python3
import os
import queue
import re
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Callable, List, Optional, Pattern, Protocol, cast

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
    """Handles capturing and displaying process output with simple indentation"""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.current_command: str | None = None
        self.header_printed = False

    def handle_output_line(self, line: str, process_name: str) -> None:
        """Process and display a single line of output with proper formatting"""
        # Only process if verbose or should always display
        if not (self.verbose or self._should_always_display(line)):
            return

        # Print command header if this is a new command
        if process_name != self.current_command:
            print(f"\n=== [{process_name}] ===")
            self.current_command = process_name
            self.header_printed = True

        # Simply indent all lines under the command
        print(f"  {line}")

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
    from ci.ci.test_commands import build_cpp_test_command

    cmd_str_cpp = build_cpp_test_command(args)
    # Balanced timeout for debugging and completion (was 900/300 seconds)
    cpp_test_timeout = 120 if args.gcc else 60
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


def _extract_test_name(command: str | list[str]) -> str:
    """Extract a human-readable test name from a command"""
    if isinstance(command, list):
        command = " ".join(command)

    # Extract test name patterns
    if "--test " in command:
        # Extract specific test name after --test flag
        parts = command.split("--test ")
        if len(parts) > 1:
            test_name = parts[1].split()[0]
            return test_name
    elif ".exe" in command:
        # Extract from executable name
        for part in command.split():
            if part.endswith(".exe"):
                return part.replace(".exe", "")
    elif "python" in command and "-m " in command:
        # Extract module name
        parts = command.split("-m ")
        if len(parts) > 1:
            module = parts[1].split()[0]
            return module.replace("ci.compiler.", "").replace("ci.", "")
    elif "python" in command and command.endswith(".py"):
        # Extract script name
        for part in command.split():
            if part.endswith(".py"):
                return part.split("/")[-1].replace(".py", "")

    # Fallback to first meaningful part of command
    parts = command.split()
    for part in parts[1:]:  # Skip 'uv' or 'python'
        if not part.startswith("-") and "python" not in part:
            return part

    return "unknown_test"


def _run_process_with_output(process: RunningProcess, verbose: bool = False) -> None:
    """
    Run a single process and handle its output
    Uses improved timeout handling to prevent hanging

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
    print(f"Started: {process.command}")

    # Print test execution status
    if isinstance(process.command, str) and process.command.endswith(".exe"):
        print(f"Running test: {process.command}")

    # Create single output handler instance to maintain state
    output_handler = ProcessOutputHandler(verbose=True)

    # Use event-driven processing with reasonable timeouts to prevent hanging
    while process.poll() is None:
        try:
            # Use consistent timeout for all output reading
            line = process.get_next_line(timeout=0.1)  # 100ms timeout
            if line is not None:
                # Use the shared output handler for proper formatting
                output_handler.handle_output_line(line, process_name)
                sys.stdout.flush()  # Ensure output is visible immediately

                # After getting one line, quickly drain any additional output
                # This maintains responsiveness while avoiding tight polling loops
                while True:  # Keep draining until no more output
                    try:
                        additional_line = process.get_next_line(
                            timeout=0.01
                        )  # 10ms for draining
                        if additional_line is None:
                            break  # End of stream
                        if additional_line.strip():  # Only print non-empty lines
                            output_handler.handle_output_line(
                                additional_line, process_name
                            )
                            sys.stdout.flush()
                    except queue.Empty:
                        break  # No more output available right now

        except queue.Empty:
            # No output available right now - this is normal and expected
            pass
        except TimeoutError:
            print(f"\nProcess timed out: {process.command}")
            process.kill()
            sys.exit(1)
        except Exception as e:
            print(f"\nUnexpected error processing output from {process.command}: {e}")
            process.kill()
            sys.exit(1)

    # Process has completed, check return code
    try:
        returncode = process.wait()
        if returncode != 0:
            # Extract test name from command for better error reporting
            test_name = _extract_test_name(process.command)
            print(f"Command failed: {process.command}")
            print(f"\033[91m###### ERROR ######\033[0m")
            print(f"Test failed: {test_name}")
            sys.exit(returncode)
        else:
            print(f"Process completed: {process.command}")
            if isinstance(process.command, str) and process.command.endswith(".exe"):
                print(f"Test {process.command} passed with return code {returncode}")
    except Exception as e:
        test_name = _extract_test_name(process.command)
        print(f"\nError waiting for process: {process.command}")
        print(f"Error: {e}")
        print(f"\033[91m###### ERROR ######\033[0m")
        print(f"Test error: {test_name}")
        process.kill()
        sys.exit(1)


def _run_processes_parallel(
    processes: list[RunningProcess], verbose: bool = False
) -> None:
    """
    Run multiple test processes in parallel and handle their output

    Args:
        processes: List of RunningProcess objects to execute
    """
    if not processes:
        return

    # Create a shared output handler for formatting
    output_handler = ProcessOutputHandler(verbose=True)

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
    start_time = time.time()
    global_timeout = max(p.timeout for p in processes) + 60  # Add 1 minute buffer

    # Track last activity time for each process to detect stuck processes
    last_activity_time = {proc: time.time() for proc in active_processes}
    stuck_process_timeout = 30  # 30 seconds without output indicates stuck process

    # Track failed processes for proper error reporting
    failed_processes: list[str] = []

    while active_processes:
        # Check global timeout
        if time.time() - start_time > global_timeout:
            print(f"\nGlobal timeout reached after {global_timeout} seconds")
            print("\033[91m###### ERROR ######\033[0m")
            print("Tests failed due to global timeout")
            for p in active_processes:
                failed_processes.append(
                    p.command
                )  # Track all active processes as failed
                p.kill()
            sys.exit(1)

        # Check for stuck processes (no output for 30 seconds)
        current_time = time.time()
        for proc in active_processes[:]:
            if current_time - last_activity_time[proc] > stuck_process_timeout:
                print(
                    f"\nProcess appears stuck (no output for {stuck_process_timeout}s): {proc.command}"
                )
                print("Killing stuck process and its children...")
                proc.kill()  # This now kills the entire process tree

                # Track this as a failure - CRITICAL FIX
                failed_processes.append(proc.command)

                active_processes.remove(proc)
                if proc in last_activity_time:
                    del last_activity_time[proc]
                print(f"Killed stuck process: {proc.command}")
                continue  # Skip to next process

        # Use event-driven processing instead of sleep-and-poll
        # Process all available output from all processes with reasonable timeout
        any_activity = False

        for proc in active_processes[:]:  # Copy list for safe modification
            cmd = proc.command
            try:
                # Use consistent timeout for all output reading
                try:
                    line = proc.get_next_line(timeout=0.1)  # 100ms timeout
                    if line is not None:
                        if line.strip():  # Only print non-empty lines
                            # Use the shared output handler for proper formatting
                            output_handler.handle_output_line(line, cmd)
                            sys.stdout.flush()  # Immediately flush output for real-time visibility
                        any_activity = True
                        last_activity_time[proc] = time.time()  # Update activity time

                        # After getting one line, quickly drain any additional output
                        # This maintains responsiveness while avoiding tight polling loops
                        while True:  # Keep draining until no more output
                            try:
                                additional_line = proc.get_next_line(
                                    timeout=0.01
                                )  # 10ms for draining
                                if additional_line is None:
                                    break  # End of stream
                                if (
                                    additional_line.strip()
                                ):  # Only print non-empty lines
                                    output_handler.handle_output_line(
                                        additional_line, cmd
                                    )
                                    sys.stdout.flush()
                                any_activity = True
                                last_activity_time[proc] = (
                                    time.time()
                                )  # Update activity time
                            except queue.Empty:
                                break  # No more output available right now

                except queue.Empty:
                    # No output available right now - this is normal and expected
                    # Continue to process completion check
                    pass

            except TimeoutError:
                print(f"\nProcess timed out: {cmd}")
                print("\033[91m###### ERROR ######\033[0m")
                print(f"Test failed due to timeout: {_extract_test_name(cmd)}")
                sys.stdout.flush()
                for p in active_processes:
                    failed_processes.append(p.command)  # Track all as failed
                    p.kill()
                sys.exit(1)
            except Exception as e:
                print(f"\nUnexpected error processing output from {cmd}: {e}")
                print("\033[91m###### ERROR ######\033[0m")
                print(f"Test failed due to unexpected error: {_extract_test_name(cmd)}")
                sys.stdout.flush()
                for p in active_processes:
                    failed_processes.append(p.command)  # Track all as failed
                    p.kill()
                sys.exit(1)

            # Check process completion status
            if proc.poll() is not None:
                try:
                    returncode = proc.wait()
                    if returncode != 0:
                        test_name = _extract_test_name(cmd)
                        print(f"\nCommand failed: {cmd} with return code {returncode}")
                        print(f"\033[91m###### ERROR ######\033[0m")
                        print(f"Test failed: {test_name}")
                        sys.stdout.flush()
                        for p in active_processes:
                            if p != proc:
                                p.kill()
                        sys.exit(returncode)
                    active_processes.remove(proc)
                    if proc in last_activity_time:
                        del last_activity_time[proc]  # Clean up tracking
                    any_activity = True
                    print(f"Process completed: {cmd}")
                    sys.stdout.flush()
                except Exception as e:
                    test_name = _extract_test_name(cmd)
                    print(f"\nError waiting for process: {cmd}")
                    print(f"Error: {e}")
                    print(f"\033[91m###### ERROR ######\033[0m")
                    print(f"Test error: {test_name}")
                    for p in active_processes:
                        failed_processes.append(p.command)  # Track all as failed
                        p.kill()
                    sys.exit(1)

        # Only sleep if no activity was detected, and use a shorter sleep
        # This prevents excessive CPU usage while maintaining responsiveness
        if not any_activity:
            time.sleep(0.01)  # 10ms sleep only when no activity

    # Check for failed processes - CRITICAL FIX
    if failed_processes:
        print(f"\n\033[91m###### ERROR ######\033[0m")
        print(f"Tests failed due to {len(failed_processes)} killed process(es):")
        for cmd in failed_processes:
            print(f"  - {cmd}")
        print("Processes were killed due to timeout/stuck detection")
        sys.exit(1)  # Exit with error code to indicate failure

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
        print("\033[92m###### SUCCESS ######\033[0m")
        print("No tests to run")
        return

    failed_tests: list[str] = []

    try:
        if parallel:
            # Run all processes in parallel with output interleaving
            _run_processes_parallel(processes, verbose=verbose)
        else:
            # Run processes sequentially with full output capture
            for process in processes:
                _run_process_with_output(process, verbose)

        # If we get here, all tests passed
        print("\033[92m###### SUCCESS ######\033[0m")
        print("All tests passed successfully!")

    except SystemExit as e:
        # Tests failed - extract command name from the error
        if e.code != 0:
            print("\033[91m###### ERROR ######\033[0m")
            print(f"Tests failed with exit code {e.code}")
        raise


def runner(args: TestArgs, src_code_change: bool = True) -> None:
    """
    Main test runner function that determines what to run and executes tests

    Args:
        args: Parsed command line arguments
        src_code_change: Whether source code has changed since last run
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
        processes = get_all_test_processes(
            args, test_categories, enable_stack_trace, src_code_change
        )
        parallel = True

    # Run the processes
    run_test_processes(processes, parallel=parallel, verbose=args.verbose)
