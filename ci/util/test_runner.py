#!/usr/bin/env python3
"""
WARNING: sys.stdout.flush() causes blocking issues on Windows with QEMU/subprocess processes!
Use conditional flushing: `if os.name != 'nt': sys.stdout.flush()` to avoid Windows blocking
while maintaining real-time output visibility on Unix systems.
"""

import _thread
import os
import queue
import re
import subprocess
import sys
import threading
import time
import traceback
from dataclasses import dataclass
from pathlib import Path
from queue import Queue
from typing import Any, Callable, List, Optional, Pattern, Protocol, cast

from typeguard import typechecked

from ci.util.running_process import EndOfStream, RunningProcess
from ci.util.test_exceptions import (
    TestExecutionFailedException,
    TestFailureInfo,
    TestTimeoutException,
)
from ci.util.test_types import (
    TestArgs,
    TestCategories,
    TestResult,
    TestResultType,
    TestSuiteResult,
    determine_test_categories,
)


_IS_GITHUB_ACTIONS = os.getenv("GITHUB_ACTIONS") == "true"
_TIMEOUT = 600 if _IS_GITHUB_ACTIONS else 240
_GLOBAL_TIMEOUT = 600 if _IS_GITHUB_ACTIONS else 300

# Abort threshold for total failures across all processes (unit + examples)
MAX_FAILURES_BEFORE_ABORT = 3


def extract_error_snippet(accumulated_output: list[str], context_lines: int = 5) -> str:
    """
    Extract relevant error snippets from process output.

    Searches for lines containing "error" (case insensitive) and extracts
    a small context window around the first few error occurrences.

    Args:
        accumulated_output: List of output lines from the process
        context_lines: Number of lines to capture before/after each error line (default: 5)

    Returns:
        Formatted string containing error snippets with minimal context
    """
    if not accumulated_output:
        return "No output captured"

    error_snippets: list[str] = []
    error_pattern = re.compile(r"error", re.IGNORECASE)

    # Find all lines that contain "error" (case insensitive)
    error_line_indices: list[int] = []
    for i, line in enumerate(accumulated_output):
        if error_pattern.search(line):
            error_line_indices.append(i)

    if not error_line_indices:
        # No specific errors found, return last 10 lines which might contain useful info
        max_lines = min(10, len(accumulated_output))
        return (
            "No 'error' keyword found. Last "
            + str(max_lines)
            + " lines:\n"
            + "\n".join(accumulated_output[-max_lines:])
        )

    # Extract context around first 5 errors (increased from 2 for better visibility)
    max_errors_to_show = 5
    for i, error_idx in enumerate(error_line_indices[:max_errors_to_show]):
        # Calculate context window (5 lines before to 5 lines after the error)
        start_idx = max(0, error_idx - context_lines)
        end_idx = min(len(accumulated_output), error_idx + context_lines + 1)

        snippet_lines: list[str] = []

        for j in range(start_idx, end_idx):
            line_marker = "➤ " if j == error_idx else "  "  # Mark the actual error line
            snippet_lines.append(f"{line_marker}{accumulated_output[j]}")

        error_snippets.append("\n".join(snippet_lines))

    # Add summary if there are more errors - but show the actual error lines
    if len(error_line_indices) > max_errors_to_show:
        remaining_errors = error_line_indices[max_errors_to_show:]
        additional_error_lines: list[str] = []
        for error_idx in remaining_errors[:3]:  # Show up to 3 more error lines
            additional_error_lines.append(f"➤ {accumulated_output[error_idx]}")

        if additional_error_lines:
            error_snippets.append("Additional errors found:")
            error_snippets.append("\n".join(additional_error_lines))

        if len(remaining_errors) > 3:
            error_snippets.append(
                f"... and {len(remaining_errors) - 3} more error(s) found"
            )

    return "\n\n".join(error_snippets)


@dataclass
class ProcessTiming:
    """Information about a completed process execution for timing summary"""

    name: str
    duration: float
    command: str


@dataclass
class ProcessState:
    """Tracks the state of an individual process during parallel execution"""

    process: RunningProcess
    last_activity_time: float
    command: str


@typechecked
class TestOutputFormatter:
    """Formats test output in a consistent way"""

    def __init__(self, verbose: bool = False):
        self.verbose = verbose
        self.current_suite: Optional[TestSuiteResult] = None
        self._start_time = time.time()

    def start_suite(self, name: str) -> None:
        """Start a new test suite"""
        self.current_suite = TestSuiteResult(
            name=name, results=[], start_time=time.time()
        )

    def end_suite(self, passed: bool = True) -> None:
        """End the current test suite"""
        if self.current_suite:
            self.current_suite.end_time = time.time()
            self.current_suite.passed = passed

    def add_result(self, result: TestResult) -> None:
        """Add a test result to the current suite"""
        if self.current_suite:
            self.current_suite.results.append(result)
            self._format_result(result)

    def _format_result(self, result: TestResult) -> None:
        """Format and display a test result"""
        # Only show if verbose or important
        if not (self.verbose or self._should_display(result)):
            return

        # Format timestamp
        delta = result.timestamp - self._start_time

        # Format message based on type
        if result.type == TestResultType.SUCCESS:
            color = "\033[92m"  # Green
        elif result.type == TestResultType.ERROR:
            color = "\033[91m"  # Red
        elif result.type == TestResultType.WARNING:
            color = "\033[93m"  # Yellow
        else:
            color = "\033[0m"  # Reset

        # Build message - only include timestamp, not command name
        msg = f"{delta:.2f} "
        msg += f"{color}{result.message}\033[0m"

        # Print with indentation
        print(f"  {msg}")

    def _should_display(self, result: TestResult) -> bool:
        """Determine if a result should be displayed"""
        # Always show errors
        if result.type == TestResultType.ERROR:
            return True

        # Always show final success/completion messages
        if any(
            marker in result.message
            for marker in ["### SUCCESS", "### ERROR", "Test execution complete:"]
        ):
            return True

        # Show warnings in verbose mode
        if result.type == TestResultType.WARNING and self.verbose:
            return True

        # Show info/debug only in verbose mode
        return self.verbose and result.type in [
            TestResultType.INFO,
            TestResultType.DEBUG,
        ]


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
    """Handles capturing and displaying process output with structured results"""

    def __init__(self, verbose: bool = False):
        self.formatter = TestOutputFormatter(verbose=verbose)
        self.current_command: str | None = None
        self.header_printed = False

    def handle_output_line(self, line: str, process_name: str) -> None:
        """Process a line of output and convert it to structured test results"""
        # Start new test suite if command changes
        if process_name != self.current_command:
            if self.current_command:
                self.formatter.end_suite()
            self.formatter.start_suite(process_name)
            self.current_command = process_name
            self.header_printed = True
            self.formatter.add_result(
                TestResult(
                    type=TestResultType.INFO,
                    message=f"=== [{process_name}] ===",
                    test_name=process_name,
                )
            )

        # Convert line to appropriate test result
        result = self._parse_line_to_result(line, process_name)
        if result:
            self.formatter.add_result(result)

    def _parse_line_to_result(
        self, line: str, process_name: str
    ) -> Optional[TestResult]:
        """Parse a line of output into a structured test result"""
        # Skip empty lines
        if not line.strip():
            return None

        # Skip test output noise
        if any(
            noise in line
            for noise in [
                "doctest version is",
                'run with "--help"',
                "assertions:",
                "test cases:",
                "MESSAGE:",
                "TEST CASE:",
                "Test passed",
                "Test execution",
                "Test completed",
                "Running test:",
                "Process completed:",
                "Command completed:",
                "Command output:",
                "Exit code:",
                "All parallel tests",
                "JSON parsing failed",
                "readFrameAt failed",
                "ByteStreamMemory",
                "C:\\Users\\",
                "\\dev\\fastled\\",
                "\\tests\\",
                "\\src\\",
                "test line_simplification.exe",
                "test noise_hires.exe",
                "test mutex.exe",
                "test rbtree.exe",
                "test priority_queue.exe",
                "test json_roundtrip.exe",
                "test malloc_hooks.exe",
                "test rectangular_buffer.exe",
                "test ostream.exe",
                "test active_strip_data_json.exe",
                "test noise_range.exe",
                "test json.exe",
                "test point.exe",
                "test screenmap.exe",
                "test task.exe",
                "test queue.exe",
                "test promise.exe",
                "test shared_ptr.exe",
                "test screenmap_serialization.exe",
                "test strstream.exe",
                "test slice.exe",
                "test hsv_conversion_accuracy.exe",
                "test tile2x2.exe",
                "test tuple.exe",
                "test raster.exe",
                "test transform.exe",
                "test transition_ramp.exe",
                "test thread_local.exe",
                "test set_inlined.exe",
                "test vector.exe",
                "test strip_id_map.exe",
                "test weak_ptr.exe",
                "test variant.exe",
                "test type_traits.exe",
                "test splat.exe",
                "test hsv16.exe",
                "test ui_help.exe",
                "test unordered_set.exe",
                "test traverse_grid.exe",
                "test ui_title_bug.exe",
                "test videofx_wrapper.exe",
                "test ui.exe",
                "test video.exe",
                "test xypath.exe",
            ]
        ):
            return None

        # Success messages
        if "### SUCCESS" in line:
            return TestResult(
                type=TestResultType.SUCCESS, message=line, test_name=process_name
            )

        # Error messages
        if any(
            marker in line
            for marker in [
                "### ERROR",
                "FAILED",
                "ERROR",
                "Crash",
                "Test FAILED",
                "Compilation failed",
                "Build failed",
            ]
        ):
            return TestResult(
                type=TestResultType.ERROR, message=line, test_name=process_name
            )

        # Warning messages
        if any(marker in line.lower() for marker in ["warning:", "note:"]):
            return TestResult(
                type=TestResultType.WARNING, message=line, test_name=process_name
            )

        # Test completion messages
        if "Test execution complete:" in line:
            return TestResult(
                type=TestResultType.INFO, message=line, test_name=process_name
            )

        # Default to info level for other lines
        return TestResult(
            type=TestResultType.INFO, message=line, test_name=process_name
        )


class ReconfigurableIO(Protocol):
    def reconfigure(self, *, encoding: str, errors: str) -> None: ...


def create_namespace_check_process(enable_stack_trace: bool) -> RunningProcess:
    """Create a namespace check process without starting it"""
    return RunningProcess(
        "uv run python ci/tests/no_using_namespace_fl_in_headers.py",
        shell=True,
        auto_run=False,  # Don't auto-start - will be started in parallel later
        enable_stack_trace=enable_stack_trace,
    )


def create_unit_test_process(
    args: TestArgs, enable_stack_trace: bool
) -> RunningProcess:
    """Create a unit test process without starting it"""
    # First compile the tests
    compile_cmd: list[str] = [
        "uv",
        "run",
        "python",
        "-m",
        "ci.compiler.cpp_test_run",
        "--compile-only",
    ]
    if args.test:
        compile_cmd.extend(["--test", args.test])
    if args.clean:
        compile_cmd.append("--clean")
    if args.verbose:
        compile_cmd.append("--verbose")
    if args.show_compile:
        compile_cmd.append("--show-compile")
    if args.show_link:
        compile_cmd.append("--show-link")
    if args.check:
        compile_cmd.append("--check")

    if args.clang:
        compile_cmd.append("--clang")
    if args.gcc:
        compile_cmd.append("--gcc")
    if args.no_unity:
        compile_cmd.append("--no-unity")
    if args.no_pch:
        compile_cmd.append("--no-pch")
    if args.debug:
        compile_cmd.append("--debug")

    # subprocess.run(compile_cmd, check=True)

    # Then run the tests using our new test runner
    test_cmd = ["uv", "run", "python", "-m", "ci.run_tests"]
    if args.test:
        test_cmd.extend(["--test", str(args.test)])
    if args.verbose:
        test_cmd.append("--verbose")

    both_cmds: list[str] = []
    both_cmds.extend(compile_cmd)
    both_cmds.extend(["&&"])
    both_cmds.extend(test_cmd)
    cmd_str = subprocess.list2cmdline(both_cmds)

    return RunningProcess(
        cmd_str,
        enable_stack_trace=enable_stack_trace,
        timeout=_TIMEOUT,  # 2 minutes timeout
        auto_run=True,
    )


def create_examples_test_process(
    args: TestArgs, enable_stack_trace: bool
) -> RunningProcess:
    """Create an examples test process without starting it"""
    cmd = ["uv", "run", "python", "-u", "ci/compiler/test_example_compilation.py"]
    if args.examples is not None:
        cmd.extend(args.examples)
    if args.clean:
        cmd.append("--clean")
    if args.no_pch:
        cmd.append("--no-pch")
    if args.unity:
        cmd.append("--unity")
    if args.full and args.examples is not None:
        cmd.append("--full")
    elif args.examples is not None:
        # Auto-enable full mode for examples to include execution
        cmd.append("--full")
    if args.no_parallel:
        cmd.append("--no-parallel")
    if args.verbose:
        cmd.append("--verbose")

    # Use longer timeout for no-parallel mode since sequential compilation takes much longer
    timeout = (
        1800 if args.no_parallel else 600
    )  # 30 minutes for sequential, 10 minutes for parallel

    cmd_str = subprocess.list2cmdline(cmd)

    return RunningProcess(
        cmd_str, auto_run=False, enable_stack_trace=enable_stack_trace, timeout=timeout
    )


def create_python_test_process(
    enable_stack_trace: bool, full_tests: bool = False
) -> RunningProcess:
    """Create a Python test process without starting it"""
    # Use list format for better environment handling
    cmd = [
        "uv",
        "run",
        "pytest",
        "-s",  # Don't capture stdout/stderr
        "-v",  # Verbose output
        "--tb=short",  # Shorter traceback format
        "--durations=0",  # Show all durations
        "ci/tests",  # Test directory
    ]

    # If not running full tests, exclude tests marked with @pytest.mark.full
    if not full_tests:
        cmd.extend(["-m", "not full"])

    cmd_str = subprocess.list2cmdline(cmd)

    return RunningProcess(
        cmd_str,
        auto_run=False,  # Don't auto-start - will be started in parallel later
        enable_stack_trace=False,  # Always disable stack traces for Python tests
        timeout=_TIMEOUT,  # 2 minute timeout for Python tests
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
        "python",
        "-m",
        "ci.ci-compile",
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


def get_python_test_processes(
    enable_stack_trace: bool, full_tests: bool = False
) -> list[RunningProcess]:
    """Return all processes needed for Python tests"""
    return [
        create_python_test_process(False, full_tests)
    ]  # Disable stack trace for Python tests


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
        processes.append(
            create_python_test_process(False)
        )  # Disable stack trace for Python tests
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


def _get_friendly_test_name(command: str | list[str]) -> str:
    """Extract a user-friendly test name for display in summary table"""
    if isinstance(command, list):
        command = " ".join(command)

    # Simplify common command patterns to friendly names
    if "cpp_test_run" in command and "ci.run_tests" in command:
        return "unit_tests"
    elif "test_example_compilation.py" in command:
        # Show script name plus example targets, e.g. "test_example_compilation.py Luminova"
        try:
            import os

            tokens = command.split()
            # Find the script token and collect following non-flag args as examples
            for i, tok in enumerate(tokens):
                normalized = tok.strip('"')
                if normalized.endswith("test_example_compilation.py"):
                    script_name = os.path.basename(normalized)
                    example_parts: list[str] = []
                    for t in tokens[i + 1 :]:
                        if t.startswith("-"):
                            break
                        example_parts.append(t.strip('"'))
                    if example_parts:
                        return f"{script_name} {' '.join(example_parts)}"
                    return script_name
        except Exception:
            # Fall back to generic extraction on any unexpected parsing issue
            pass
        return _extract_test_name(command)
    elif "pytest" in command and "ci/tests" in command:
        return "python_tests"
    elif "pytest" in command and "ci/test_integration" in command:
        return "integration_tests"
    elif "ci-compile" in command and "uno" in command:
        return "uno_compilation"
    else:
        # Fallback to the existing extraction logic
        return _extract_test_name(command)


def _format_timing_summary(process_timings: List[ProcessTiming]) -> str:
    """Format a summary table of process execution times"""
    if not process_timings:
        return ""

    # Sort by duration (longest first)
    sorted_timings = sorted(process_timings, key=lambda x: x.duration, reverse=True)

    # Calculate column widths
    max_name_width = max(len(timing.name) for timing in sorted_timings)
    max_name_width = max(max_name_width, len("Test Name"))  # Ensure header fits

    # Create header
    header = f"{'Test Name':<{max_name_width}} | {'Duration':>10}"
    separator = f"{'-' * max_name_width}-+-{'-' * 10}"

    # Create rows
    rows: list[str] = []
    for timing in sorted_timings:
        duration_str = f"{timing.duration:.2f}s"
        row = f"{timing.name:<{max_name_width}} | {duration_str:>10}"
        rows.append(row)

    # Combine all parts
    table_lines = (
        [
            "\nTest Execution Summary:",
            separator,
            header,
            separator,
        ]
        + rows
        + [separator]
    )

    return "\n".join(table_lines)


def _handle_process_completion(
    proc_state: ProcessState,
    active_processes: list[RunningProcess],
    completed_timings: List[ProcessTiming],
    last_activity_time: dict[RunningProcess, float],
) -> None:
    """
    Handle completion of a single test process

    Args:
        proc_state: State information for the completed process
        active_processes: List of currently active processes
        completed_timings: List to collect timing data
        last_activity_time: Dictionary tracking activity times

    Raises:
        TestExecutionFailedException: If process failed
    """
    proc = proc_state.process
    cmd = proc_state.command
    if isinstance(cmd, list):
        cmd = subprocess.list2cmdline(cmd)

    try:
        returncode = proc.wait()
        if returncode != 0:
            test_name = _extract_test_name(cmd)
            print(f"\nCommand failed: {cmd} with return code {returncode}")
            print(f"\033[91m###### ERROR ######\033[0m")
            print(f"Test failed: {test_name}")

            # Capture the actual output from the failed process
            try:
                actual_output = proc.stdout
                if actual_output.strip():
                    print(f"\n=== ACTUAL OUTPUT FROM FAILED PROCESS ===")
                    print(actual_output)
                    print(f"=== END OF OUTPUT ===")
                else:
                    actual_output = "No output captured from failed process"
            except Exception as e:
                actual_output = f"Error capturing output: {e}"

            # Flush output for real-time visibility (but avoid on Windows due to blocking issues)
            if os.name != "nt":  # Only flush on non-Windows systems
                sys.stdout.flush()
            for p in active_processes:
                if p != proc:
                    p.kill()
            failure = TestFailureInfo(
                test_name=test_name,
                command=str(cmd),
                return_code=returncode,
                output=actual_output,
                error_type="command_failure",
            )
            raise TestExecutionFailedException("Test command failed", [failure])

        active_processes.remove(proc)
        if proc in last_activity_time:
            del last_activity_time[proc]  # Clean up tracking
        print(f"Process completed: {cmd}")

        # Collect timing data for summary
        if proc.duration is not None:
            timing = ProcessTiming(
                name=_get_friendly_test_name(cmd),
                duration=proc.duration,
                command=str(cmd),
            )
            completed_timings.append(timing)

        # Flush output for real-time visibility (but avoid on Windows due to blocking issues)
        if os.name != "nt":  # Only flush on non-Windows systems
            sys.stdout.flush()

    except Exception as e:
        test_name = _extract_test_name(cmd)
        print(f"\nError waiting for process: {cmd}")
        print(f"Error: {e}")
        print(f"\033[91m###### ERROR ######\033[0m")
        print(f"Test error: {test_name}")

        # Try to capture any available output
        try:
            actual_output = proc.stdout
            if actual_output.strip():
                print(f"\n=== PROCESS OUTPUT BEFORE ERROR ===")
                print(actual_output)
                print(f"=== END OF OUTPUT ===")
        except Exception as output_error:
            print(f"Could not capture process output: {output_error}")

        failures: list[TestFailureInfo] = []
        for p in active_processes:
            p.kill()
            # Try to capture output from this process too
            try:
                process_output = p.stdout if hasattr(p, "stdout") else str(e)
            except Exception:
                process_output = str(e)

            failures.append(
                TestFailureInfo(
                    test_name=_extract_test_name(p.command),
                    command=str(p.command),
                    return_code=1,
                    output=process_output,
                    error_type="process_wait_error",
                )
            )
        raise TestExecutionFailedException("Error waiting for process", failures)


@dataclass
class StuckProcessSignal:
    """Signal from monitoring thread that a process is stuck"""

    process: RunningProcess
    timeout_duration: float


class ProcessStuckMonitor:
    """Manages individual monitoring threads for stuck process detection"""

    def __init__(self, stuck_process_timeout: float):
        self.stuck_process_timeout = stuck_process_timeout
        self.stuck_signals: Queue[StuckProcessSignal] = Queue()
        self.monitoring_threads: dict[RunningProcess, threading.Thread] = {}
        self.shutdown_event = threading.Event()

    def start_monitoring(self, process: RunningProcess) -> None:
        """Start a monitoring thread for the given process"""
        if process in self.monitoring_threads:
            return  # Already monitoring this process

        monitor_thread = threading.Thread(
            target=self._monitor_process,
            args=(process,),
            name=f"StuckMonitor-{_extract_test_name(process.command)}",
            daemon=True,
        )
        self.monitoring_threads[process] = monitor_thread
        monitor_thread.start()

    def stop_monitoring(self, process: RunningProcess) -> None:
        """Stop monitoring a specific process"""
        if process in self.monitoring_threads:
            del self.monitoring_threads[process]

    def shutdown(self) -> None:
        """Shutdown all monitoring threads"""
        self.shutdown_event.set()
        self.monitoring_threads.clear()

    def check_for_stuck_processes(self) -> list[StuckProcessSignal]:
        """Check for any stuck process signals from monitoring threads"""
        stuck_processes: list[StuckProcessSignal] = []
        try:
            while True:
                signal = self.stuck_signals.get_nowait()
                stuck_processes.append(signal)
        except queue.Empty:
            pass
        return stuck_processes

    def _monitor_process(self, process: RunningProcess) -> None:
        """Monitor a single process for being stuck (runs in separate thread)"""
        thread_id = threading.current_thread().ident
        thread_name = threading.current_thread().name

        try:
            last_activity_time = time.time()

            while not self.shutdown_event.is_set():
                if process.finished:
                    # Process completed normally, stop monitoring
                    return

                # Check if we have recent stdout activity
                stdout_time = process.time_last_stdout_line()
                if stdout_time is not None:
                    last_activity_time = stdout_time

                # Check if process is stuck
                current_time = time.time()
                if current_time - last_activity_time > self.stuck_process_timeout:
                    # Process is stuck, signal the main thread
                    signal = StuckProcessSignal(
                        process=process, timeout_duration=self.stuck_process_timeout
                    )
                    self.stuck_signals.put(signal)
                    return

                # Sleep briefly before next check
                time.sleep(1.0)  # Check every second

        except KeyboardInterrupt:
            print(f"🛑 Thread {thread_id} ({thread_name}) caught KeyboardInterrupt")
            print(f"📍 Stack trace for thread {thread_id}:")
            traceback.print_exc()
            _thread.interrupt_main()
            raise
        except Exception as e:
            print(f"❌ Thread {thread_id} ({thread_name}) unexpected error: {e}")
            traceback.print_exc()
            _thread.interrupt_main()
            raise


def _handle_stuck_processes(
    stuck_signals: list[StuckProcessSignal],
    active_processes: list[RunningProcess],
    failed_processes: list[str],
    monitor: ProcessStuckMonitor,
) -> None:
    """
    Handle stuck processes reported by monitoring threads

    Args:
        stuck_signals: List of stuck process signals from monitoring threads
        active_processes: List of currently active processes (modified in place)
        failed_processes: List to track failed process commands
        monitor: The process stuck monitor instance
    """
    for signal in stuck_signals:
        proc = signal.process
        if proc in active_processes:
            print(
                f"\nProcess appears stuck (no output for {signal.timeout_duration}s): {proc.command}"
            )
            print("Killing stuck process and its children...")
            proc.kill()  # This now kills the entire process tree

            # Track this as a failure
            failed_processes.append(subprocess.list2cmdline(proc.command))

            active_processes.remove(proc)
            monitor.stop_monitoring(proc)
            print(f"Killed stuck process: {proc.command}")


def _run_processes_parallel(
    processes: list[RunningProcess], verbose: bool = False
) -> List[ProcessTiming]:
    """
    DEPRECATED: Use RunningProcessGroup instead.

    This function has been replaced by RunningProcessGroup.run()
    for better maintainability and consistency.

    Run multiple test processes in parallel and handle their output

    Args:
        processes: List of RunningProcess objects to execute

    Returns:
        List of ProcessTiming objects with execution times
    """
    if not processes:
        return []

    # Create a shared output handler for formatting
    output_handler = ProcessOutputHandler(verbose=verbose)

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

    # Start processes that aren't already running
    for proc in processes:
        cmd_str = proc.get_command_str()
        if proc.proc is None:  # Only start if not already running
            proc.run()
            print(f"Started: {cmd_str}")
        else:
            print(f"Process already running: {cmd_str}")

    # Monitor all processes for output and completion
    active_processes = processes.copy()
    start_time = time.time()

    runner_timeouts: list[int] = [p.timeout for p in processes if p.timeout is not None]
    global_timeout: int | None = None
    if runner_timeouts:
        global_timeout = max(runner_timeouts) + 60  # Add 1 minute buffer

    # Track last activity time for each process to detect stuck processes
    last_activity_time = {proc: time.time() for proc in active_processes}
    stuck_process_timeout = _GLOBAL_TIMEOUT

    # Track failed processes for proper error reporting
    failed_processes: list[str] = []  # Processes killed due to timeout/stuck
    exit_failed_processes: list[
        tuple[RunningProcess, int]
    ] = []  # Processes that failed with non-zero exit code

    # Track completed processes for timing summary
    completed_timings: List[ProcessTiming] = []

    # Create thread-based stuck process monitor
    stuck_monitor = ProcessStuckMonitor(stuck_process_timeout)

    try:
        # Start monitoring threads for each process
        for proc in active_processes:
            stuck_monitor.start_monitoring(proc)

        def time_expired() -> bool:
            if global_timeout is None:
                return False
            assert global_timeout is not None
            return time.time() - start_time > global_timeout

        while active_processes:
            # Check global timeout
            if time_expired():
                assert global_timeout is not None
                print(f"\nGlobal timeout reached after {global_timeout} seconds")
                print("\033[91m###### ERROR ######\033[0m")
                print("Tests failed due to global timeout")
                failures: list[TestFailureInfo] = []
                for p in active_processes:
                    failed_processes.append(
                        subprocess.list2cmdline(p.command)
                    )  # Track all active processes as failed
                    p.kill()
                    failures.append(
                        TestFailureInfo(
                            test_name=_extract_test_name(p.command),
                            command=str(p.command),
                            return_code=1,
                            output="Process killed due to global timeout",
                            error_type="global_timeout",
                        )
                    )
                raise TestTimeoutException("Global timeout reached", failures)

            # Check for stuck processes (using threaded monitoring)
            stuck_signals = stuck_monitor.check_for_stuck_processes()
            if stuck_signals:
                _handle_stuck_processes(
                    stuck_signals, active_processes, failed_processes, stuck_monitor
                )

                # Early abort if failure threshold reached via stuck processes
                if (
                    len(exit_failed_processes) + len(failed_processes)
                ) >= MAX_FAILURES_BEFORE_ABORT:
                    print(
                        f"\nExceeded failure threshold ({MAX_FAILURES_BEFORE_ABORT}). Aborting remaining tests."
                    )
                    # Kill any remaining active processes
                    for p in active_processes:
                        p.kill()
                    # Build detailed failures
                    failures: list[TestFailureInfo] = []
                    for proc, exit_code in exit_failed_processes:
                        error_snippet = extract_error_snippet(proc.accumulated_output)
                        failures.append(
                            TestFailureInfo(
                                test_name=_extract_test_name(proc.command),
                                command=str(proc.command),
                                return_code=exit_code,
                                output=error_snippet,
                                error_type="exit_failure",
                            )
                        )
                    for cmd in failed_processes:
                        cmd = subprocess.list2cmdline(cmd)
                        failures.append(
                            TestFailureInfo(
                                test_name=_extract_test_name(cmd),
                                command=str(cmd),
                                return_code=1,
                                output="Process was killed due to timeout/stuck detection",
                                error_type="killed_process",
                            )
                        )
                    raise TestExecutionFailedException(
                        "Exceeded failure threshold", failures
                    )

            # Process each active test individually
            # Iterate backwards to safely remove processes from the list
            any_activity = False
            for i in range(len(active_processes) - 1, -1, -1):
                proc = active_processes[i]

                if verbose:
                    with proc.line_iter(timeout=60) as line_iter:
                        for line in line_iter:
                            print(line)

                # Check if process has finished
                if proc.finished:
                    # Get the exit code to check for failure
                    exit_code = proc.wait()

                    # Process completed, remove from active list
                    active_processes.remove(proc)
                    # Stop monitoring this process
                    stuck_monitor.stop_monitoring(proc)

                    # Check for non-zero exit code (failure)
                    if exit_code != 0:
                        print(
                            f"Process failed with exit code {exit_code}: {proc.command}"
                        )
                        exit_failed_processes.append((proc, exit_code))
                        # Early abort if we reached the failure threshold
                        if (
                            len(exit_failed_processes) + len(failed_processes)
                        ) >= MAX_FAILURES_BEFORE_ABORT:
                            print(
                                f"\nExceeded failure threshold ({MAX_FAILURES_BEFORE_ABORT}). Aborting remaining tests."
                            )
                            # Kill remaining active processes
                            for p in active_processes:
                                if p is not proc:
                                    p.kill()
                            # Prepare failures with snippets
                            failures: list[TestFailureInfo] = []
                            for p, code in exit_failed_processes:
                                error_snippet = extract_error_snippet(
                                    p.accumulated_output
                                )
                                failures.append(
                                    TestFailureInfo(
                                        test_name=_extract_test_name(p.command),
                                        command=str(p.command),
                                        return_code=code,
                                        output=error_snippet,
                                        error_type="exit_failure",
                                    )
                                )
                            for cmd in failed_processes:
                                cmd_str = subprocess.list2cmdline(cmd)
                                failures.append(
                                    TestFailureInfo(
                                        test_name=_extract_test_name(cmd_str),
                                        command=cmd_str,
                                        return_code=1,
                                        output="Process was killed due to timeout/stuck detection",
                                        error_type="killed_process",
                                    )
                                )
                            raise TestExecutionFailedException(
                                "Exceeded failure threshold", failures
                            )
                        any_activity = True
                        continue

                    # Update timing information
                    # Calculate duration properly - if process duration is None, calculate it manually
                    if proc.duration is not None:
                        duration = proc.duration
                    elif proc.start_time is not None:
                        # Calculate duration from start time to now
                        duration = time.time() - proc.start_time
                    else:
                        duration = 0.0

                    timing = ProcessTiming(
                        name=_get_friendly_test_name(proc.command),
                        command=subprocess.list2cmdline(proc.command),
                        duration=duration,
                    )
                    completed_timings.append(timing)
                    print(f"Process completed: {proc.get_command_str()}")
                    any_activity = True
                    continue

                # Update last activity time if we have stdout activity
                stdout_time = proc.time_last_stdout_line()
                if stdout_time is not None:
                    last_activity_time[proc] = stdout_time
                    any_activity = True

            # Only sleep if no activity was detected, and use a shorter sleep
            # This prevents excessive CPU usage while maintaining responsiveness
            if not any_activity:
                time.sleep(0.01)  # 10ms sleep only when no activity

        # Check for processes that failed with non-zero exit codes
        if exit_failed_processes:
            print(f"\n\033[91m###### ERROR ######\033[0m")
            print(
                f"Tests failed due to {len(exit_failed_processes)} process(es) with non-zero exit codes:"
            )
            for proc, exit_code in exit_failed_processes:
                print(f"  - {proc.command} (exit code {exit_code})")
            failures: list[TestFailureInfo] = []
            for proc, exit_code in exit_failed_processes:
                # Extract error snippet from process output
                error_snippet = extract_error_snippet(proc.accumulated_output)

                failures.append(
                    TestFailureInfo(
                        test_name=_extract_test_name(proc.command),
                        command=str(proc.command),
                        return_code=exit_code,
                        output=error_snippet,
                        error_type="exit_failure",
                    )
                )
            raise TestExecutionFailedException("Tests failed", failures)

        # Check for failed processes - CRITICAL FIX
        if failed_processes:
            print(f"\n\033[91m###### ERROR ######\033[0m")
            print(f"Tests failed due to {len(failed_processes)} killed process(es):")
            for cmd in failed_processes:
                print(f"  - {cmd}")
            print("Processes were killed due to timeout/stuck detection")
            failures: list[TestFailureInfo] = []
            for cmd in failed_processes:
                failures.append(
                    TestFailureInfo(
                        test_name=_extract_test_name(cmd),
                        command=str(cmd),
                        return_code=1,
                        output="Process was killed due to timeout/stuck detection",
                        error_type="killed_process",
                    )
                )
            raise TestExecutionFailedException("Processes were killed", failures)

        print("\nAll parallel tests completed successfully")
        return completed_timings

    finally:
        # Always shutdown stuck monitoring threads, even on exception
        stuck_monitor.shutdown()


def run_test_processes(
    processes: list[RunningProcess], parallel: bool = True, verbose: bool = False
) -> List[ProcessTiming]:
    """
    Run multiple test processes using RunningProcessGroup

    Args:
        processes: List of RunningProcess objects to execute
        parallel: Whether to run processes in parallel or sequentially (ignored if NO_PARALLEL is set)
        verbose: Whether to show all output

    Returns:
        List of ProcessTiming objects with execution times
    """
    from ci.util.running_process_group import (
        ExecutionMode,
        ProcessExecutionConfig,
        RunningProcessGroup,
    )

    start_time = time.time()

    # Force sequential execution if NO_PARALLEL is set
    if os.environ.get("NO_PARALLEL"):
        parallel = False

    if not processes:
        print("\033[92m###### SUCCESS ######\033[0m")
        print("No tests to run")
        return []

    try:
        # Configure execution mode
        execution_mode = (
            ExecutionMode.PARALLEL if parallel else ExecutionMode.SEQUENTIAL
        )

        config = ProcessExecutionConfig(
            execution_mode=execution_mode,
            verbose=verbose,
            max_failures_before_abort=MAX_FAILURES_BEFORE_ABORT,
            enable_stuck_detection=True,
            stuck_timeout_seconds=_GLOBAL_TIMEOUT,
            live_updates=True,  # Enable real-time display
            display_type="auto",  # Auto-detect best display format
        )

        # Create and run process group
        group = RunningProcessGroup(
            processes=processes, config=config, name="TestProcesses"
        )

        # Start real-time display if we have processes and live updates enabled
        display_thread = None
        if len(processes) > 0 and config.live_updates and not verbose:
            # Only show live display if not in verbose mode (verbose already shows all output)
            try:
                from ci.util.process_status_display import display_process_status

                display_thread = display_process_status(
                    group,
                    display_type=config.display_type,
                    update_interval=config.update_interval,
                )
            except ImportError:
                pass  # Fall back to normal execution if display not available
            except Exception:
                pass  # Fall back to normal execution on any display error

        timings = group.run()

        # Stop display thread if it was started
        if display_thread:
            # Give it a moment to show final status
            time.sleep(0.5)

        # Success message for sequential execution
        if not parallel:
            elapsed = time.time() - start_time
            print(f"\033[92m### SUCCESS ({elapsed:.2f}s) ###\033[0m")

        return timings

    except (TestExecutionFailedException, TestTimeoutException) as e:
        # Tests failed - print detailed info and re-raise for proper handling
        print("\n" + "=" * 60)
        print("FASTLED TEST RUNNER FAILURE DETAILS")
        print("=" * 60)
        print(e.get_detailed_failure_info())
        print("=" * 60)
        raise
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
    print(f"[TEST_RUNNER] Starting runner function")
    print(f"[TEST_RUNNER] Args: {args}")
    print(f"[TEST_RUNNER] Source code changed: {src_code_change}")
    try:
        # Determine test categories
        test_categories = determine_test_categories(args)
        enable_stack_trace = not args.no_stack_trace

        # Build up unified list of all processes to run
        processes: list[RunningProcess] = []

        # Always start with namespace check
        processes.append(create_namespace_check_process(enable_stack_trace))

        # Add unit tests if needed
        if test_categories.unit or test_categories.unit_only:
            processes.append(create_unit_test_process(args, enable_stack_trace))

        # Add integration tests if needed
        if test_categories.integration or test_categories.integration_only:
            processes.append(create_integration_test_process(args, enable_stack_trace))

        # Add uno compilation test if source changed
        if src_code_change and not test_categories.py_only:
            processes.append(create_compile_uno_test_process(enable_stack_trace))

        # Add Python tests if needed
        if test_categories.py or test_categories.py_only:
            # Pass full_tests=True if we're running integration tests or any form of full tests
            full_tests = (
                test_categories.integration
                or test_categories.integration_only
                or args.full
            )

            processes.append(
                create_python_test_process(False, full_tests)
            )  # Disable stack trace for Python tests

        # Add example tests if needed
        if test_categories.examples or test_categories.examples_only:
            processes.append(create_examples_test_process(args, enable_stack_trace))

        # Determine if we'll run in parallel
        will_run_parallel = not bool(os.environ.get("NO_PARALLEL"))

        # Print summary of what we're about to run
        execution_mode = "in parallel" if will_run_parallel else "sequentially"

        # Run processes (parallel unless NO_PARALLEL is set)
        timings = run_test_processes(
            processes,
            parallel=will_run_parallel,
            verbose=args.verbose,
        )

        # Display timing summary
        if timings:
            summary = _format_timing_summary(timings)
            print(summary)
    except (TestExecutionFailedException, TestTimeoutException) as e:
        # Print summary and exit with proper code
        print(f"\n\033[91m###### ERROR ######\033[0m")
        print(f"Tests failed with {len(e.failures)} failure(s)")

        # Exit with appropriate code
        if e.failures:
            # Use the return code from the first failure, or 1 if none available
            exit_code = (
                e.failures[0].return_code if e.failures[0].return_code != 0 else 1
            )
        else:
            exit_code = 1
        sys.exit(exit_code)
