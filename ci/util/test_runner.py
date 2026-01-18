from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


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
from queue import Queue
from typing import Callable, Optional, Protocol, cast

from running_process import RunningProcess
from typeguard import typechecked

from ci.util.color_output import print_cache_hit
from ci.util.output_formatter import TimestampFormatter
from ci.util.sccache_config import show_sccache_stats
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
from ci.util.timestamp_print import ts_print


_IS_GITHUB_ACTIONS = os.getenv("GITHUB_ACTIONS") == "true"
_TIMEOUT = 600 if _IS_GITHUB_ACTIONS else 240
_GLOBAL_TIMEOUT = (
    600 if _IS_GITHUB_ACTIONS else 600
)  # Increased to 600s for local uno compilation

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
    skipped: bool = False


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
        ts_print(f"  {msg}")

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
    timeout: Optional[int] = None
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
        self.current_command: Optional[str] = None
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
    if enable_stack_trace:
        test_cmd.append("--stack-trace")

    return RunningProcess(
        test_cmd,
        timeout=_TIMEOUT,  # 2 minutes timeout
        auto_run=True,
        output_formatter=TimestampFormatter(),
    )


def create_examples_test_process(
    args: TestArgs, enable_stack_trace: bool
) -> RunningProcess:
    """Create an examples test process using Meson build system"""
    # Use Meson-based example compilation instead of Python compiler
    cmd = ["uv", "run", "python", "-u", "ci/util/meson_example_runner.py"]

    # Add example names if specified
    if args.examples is not None:
        cmd.extend(args.examples)

    # Map command-line arguments to meson_example_runner.py
    if args.clean:
        cmd.append("--clean")
    if args.no_pch:
        cmd.append("--no-pch")  # Ignored by Meson (PCH always enabled)
    if args.no_parallel:
        cmd.append("--no-parallel")
    if args.verbose:
        cmd.append("--verbose")
    if args.debug:
        cmd.append("--debug")
    if args.build_mode:
        cmd.extend(["--build-mode", args.build_mode])

    # Auto-enable full mode for examples to include execution
    if args.full or args.examples is not None:
        cmd.append("--full")

    # Use longer timeout for no-parallel mode since sequential compilation takes much longer
    timeout = (
        1800 if args.no_parallel else 600
    )  # 30 minutes for sequential, 10 minutes for parallel

    return RunningProcess(cmd, auto_run=False, timeout=timeout)


def create_python_test_process(
    enable_stack_trace: bool, run_slow: bool = False
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

    # If running slow tests, add --runslow flag (handled by conftest.py)
    if run_slow:
        cmd.append("--runslow")

    cmd_str = subprocess.list2cmdline(cmd)

    return RunningProcess(
        cmd_str,
        auto_run=False,  # Don't auto-start - will be started in parallel later
        timeout=_TIMEOUT,  # 2 minute timeout for Python tests
        output_formatter=TimestampFormatter(),
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
    return RunningProcess(cmd, auto_run=False, output_formatter=TimestampFormatter())


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
        "--local",
    ]

    # Force local compilation to avoid Docker detection hangs in test environment
    # Docker auto-detection can timeout if Docker is installed but not running

    # Use 15 minute timeout for platform compilation (as per CLAUDE.md guidelines)
    # Docker compilation may take longer during initial rsync sync without producing output
    return RunningProcess(
        cmd, auto_run=False, timeout=900, output_formatter=TimestampFormatter()
    )


def get_cpp_test_processes(
    args: TestArgs, test_categories: TestCategories, enable_stack_trace: bool
) -> list[RunningProcess]:
    """Return all processes needed for C++ tests"""
    processes: list[RunningProcess] = []

    if test_categories.unit:
        processes.append(create_unit_test_process(args, enable_stack_trace))

    if test_categories.examples:
        processes.append(create_examples_test_process(args, enable_stack_trace))

    return processes


def get_python_test_processes(
    enable_stack_trace: bool, run_slow: bool = False
) -> list[RunningProcess]:
    """Return all processes needed for Python tests"""
    return [
        create_python_test_process(False, run_slow)
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


def _create_skipped_timing(test_name: str, command: str = "") -> ProcessTiming:
    """Create a ProcessTiming entry for a skipped test"""
    return ProcessTiming(name=test_name, duration=0.0, command=command, skipped=True)


def _get_friendly_test_name(command: str | list[str]) -> str:
    """Extract a user-friendly test name for display in summary table"""
    if isinstance(command, list):
        command = " ".join(command)

    # Simplify common command patterns to friendly names
    if "cpp_test_run" in command and "ci.run_tests" in command:
        return "unit_tests"
    elif "meson_example_runner.py" in command:
        # Extract specific example names from command (after the .py script)
        tokens = command.split()
        example_names: list[str] = []
        found_script = False
        for tok in tokens:
            # Find the script first
            if "meson_example_runner" in tok:
                found_script = True
                continue
            if not found_script:
                continue
            # Skip flags
            if tok.startswith("-"):
                continue
            # This is likely an example name (after the script)
            example_names.append(tok)
        if example_names:
            return ", ".join(example_names)
        return "examples"
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
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
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


def _format_timing_summary(process_timings: list[ProcessTiming]) -> str:
    """Format a summary of process execution times.

    For single tests: Returns compact inline format (e.g., "Blink: 3.27s")
    For multiple tests: Returns a table format
    """
    if not process_timings:
        return ""

    # Sort by skipped status first (non-skipped first), then by duration (longest first)
    sorted_timings = sorted(process_timings, key=lambda x: (x.skipped, -x.duration))

    # For single test, use compact inline format
    if len(sorted_timings) == 1:
        timing = sorted_timings[0]
        if timing.skipped:
            return f"{timing.name}: skipped"
        else:
            return f"✅ {timing.name}: {timing.duration:.2f}s"

    # For 2-3 tests, use compact comma-separated format
    if len(sorted_timings) <= 3:
        parts: list[str] = []
        for timing in sorted_timings:
            if timing.skipped:
                parts.append(f"{timing.name}: skipped")
            else:
                parts.append(f"{timing.name} ({timing.duration:.2f}s)")
        # Show success indicator if all passed
        prefix = "✅ " if not any(t.skipped for t in sorted_timings) else ""
        return f"{prefix}{', '.join(parts)}"

    # 4+ tests: use table format
    # Calculate column widths dynamically
    max_name_width = max(len(timing.name) for timing in sorted_timings)
    max_name_width = max(max_name_width, len("Test"))  # Use shorter header

    # Create header with compact formatting
    header = f"{'Test':<{max_name_width}} | Duration"
    separator = f"{'-' * max_name_width}-+-{'-' * 8}"

    # Create rows with compact formatting
    rows: list[str] = []
    for timing in sorted_timings:
        if timing.skipped:
            duration_str = "skipped"
        else:
            duration_str = f"{timing.duration:.2f}s"
        row = f"{timing.name:<{max_name_width}} | {duration_str}"
        rows.append(row)

    # Combine all parts (header before separator, no bottom border)
    table_lines = [
        "Results:",
        header,
        separator,
    ] + rows

    return "\n".join(table_lines)


def _handle_process_completion(
    proc_state: ProcessState,
    active_processes: list[RunningProcess],
    completed_timings: list[ProcessTiming],
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
            ts_print(f"\nCommand failed: {cmd} with return code {returncode}")
            ts_print("\033[91m###### ERROR ######\033[0m")
            ts_print(f"Test failed: {test_name}")

            # Capture the actual output from the failed process
            try:
                actual_output = proc.stdout
                if actual_output.strip():
                    ts_print("\n=== ACTUAL OUTPUT FROM FAILED PROCESS ===")
                    ts_print(actual_output)
                    ts_print("=== END OF OUTPUT ===")
                else:
                    actual_output = "No output captured from failed process"
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
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
        # Process completion is shown in the timing summary table, no separate message needed

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

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        test_name = _extract_test_name(cmd)
        ts_print(f"\nError waiting for process: {cmd}")
        ts_print(f"Error: {e}")
        ts_print("\033[91m###### ERROR ######\033[0m")
        ts_print(f"Test error: {test_name}")

        # Try to capture any available output
        try:
            actual_output = proc.stdout
            if actual_output.strip():
                ts_print("\n=== PROCESS OUTPUT BEFORE ERROR ===")
                ts_print(actual_output)
                ts_print("=== END OF OUTPUT ===")
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as output_error:
            ts_print(f"Could not capture process output: {output_error}")

        failures: list[TestFailureInfo] = []
        for p in active_processes:
            p.kill()
            # Try to capture output from this process too
            try:
                process_output = p.stdout if hasattr(p, "stdout") else str(e)
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
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
            ts_print(f"🛑 Thread {thread_id} ({thread_name}) caught KeyboardInterrupt")
            ts_print(f"📍 Stack trace for thread {thread_id}:")
            traceback.print_exc()
            _thread.interrupt_main()
            raise
        except Exception as e:
            ts_print(f"❌ Thread {thread_id} ({thread_name}) unexpected error: {e}")
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
            ts_print(
                f"\nProcess appears stuck (no output for {signal.timeout_duration}s): {proc.command}"
            )
            ts_print("Killing stuck process and its children...")
            proc.kill()  # This now kills the entire process tree

            # Track this as a failure
            failed_processes.append(subprocess.list2cmdline(proc.command))

            active_processes.remove(proc)
            monitor.stop_monitoring(proc)
            ts_print(f"Killed stuck process: {proc.command}")


def _run_processes_parallel(
    processes: list[RunningProcess], verbose: bool = False
) -> list[ProcessTiming]:
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
    ProcessOutputHandler(verbose=verbose)

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
            proc.start()
            ts_print(f"Started: {cmd_str}")
        else:
            ts_print(f"Process already running: {cmd_str}")

    # Monitor all processes for output and completion
    active_processes = processes.copy()
    start_time = time.time()

    runner_timeouts: list[int] = [p.timeout for p in processes if p.timeout is not None]
    global_timeout: Optional[int] = None
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
    completed_timings: list[ProcessTiming] = []

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
                ts_print(f"\nGlobal timeout reached after {global_timeout} seconds")
                ts_print("\033[91m###### ERROR ######\033[0m")
                ts_print("Tests failed due to global timeout")
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
                    ts_print(
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
                            ts_print(line)

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
                        ts_print(
                            f"Process failed with exit code {exit_code}: {proc.command}"
                        )
                        exit_failed_processes.append((proc, exit_code))
                        # Early abort if we reached the failure threshold
                        if (
                            len(exit_failed_processes) + len(failed_processes)
                        ) >= MAX_FAILURES_BEFORE_ABORT:
                            ts_print(
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
                    # Process completion is shown in the timing summary table, no separate message needed
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
            ts_print("\n\033[91m###### ERROR ######\033[0m")
            ts_print(
                f"Tests failed due to {len(exit_failed_processes)} process(es) with non-zero exit codes:"
            )
            for proc, exit_code in exit_failed_processes:
                ts_print(f"  - {proc.command} (exit code {exit_code})")
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
            ts_print("\n\033[91m###### ERROR ######\033[0m")
            ts_print(f"Tests failed due to {len(failed_processes)} killed process(es):")
            for cmd in failed_processes:
                ts_print(f"  - {cmd}")
            ts_print("Processes were killed due to timeout/stuck detection")
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

        ts_print("\nAll parallel tests completed successfully")
        return completed_timings

    finally:
        # Always shutdown stuck monitoring threads, even on exception
        stuck_monitor.shutdown()


def run_test_processes(
    processes: list[RunningProcess], parallel: bool = True, verbose: bool = False
) -> list[ProcessTiming]:
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
        ts_print("\n✓ No tests to run")
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
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
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
            ts_print(f"\n✓ Tests completed ({elapsed:.2f}s)")

        return timings

    except (TestExecutionFailedException, TestTimeoutException) as e:
        # Tests failed - print detailed info and re-raise for proper handling
        ts_print("\n" + "=" * 60)
        ts_print("FASTLED TEST RUNNER FAILURE DETAILS")
        ts_print("=" * 60)
        ts_print(e.get_detailed_failure_info())
        ts_print("=" * 60)
        raise
    except SystemExit as e:
        # Tests failed - extract command name from the error
        if e.code != 0:
            ts_print("\033[91m###### ERROR ######\033[0m")
            ts_print(f"Tests failed with exit code {e.code}")
        raise


def runner(
    args: TestArgs,
    src_code_change: bool = True,
    cpp_test_change: bool = True,
    examples_change: bool = True,
    python_test_change: bool = True,
) -> None:
    """
    Main test runner function that determines what to run and executes tests

    Args:
        args: Parsed command line arguments
        src_code_change: Whether source code has changed since last run
        cpp_test_change: Whether C++ test-related files (src/ or tests/) have changed
        examples_change: Whether example-related files have changed
        python_test_change: Whether Python test-related files have changed
    """
    # Debug logging - only shown in verbose mode to reduce UI clutter
    if args.verbose:
        # Show only active/non-default flags instead of full TestArgs dump
        active_flags: list[str] = []
        if args.test:
            active_flags.append(f"test={args.test}")
        if args.examples:
            # Format list as comma-separated values instead of Python list syntax
            active_flags.append(f"examples={','.join(args.examples)}")
        if args.debug:
            active_flags.append("debug")
        if args.clean:
            active_flags.append("clean")
        if args.full:
            active_flags.append("full")
        if args.no_parallel:
            active_flags.append("no-parallel")
        if args.build_mode:
            active_flags.append(f"build_mode={args.build_mode}")
        if args.no_fingerprint:
            active_flags.append("no-fingerprint")
        if args.no_pch:
            active_flags.append("no-pch")

        if active_flags:
            ts_print(f"Active flags: {', '.join(active_flags)}")

    # Clear sccache stats at start to show only metrics from this build
    from ci.util.sccache_config import clear_sccache_stats

    clear_sccache_stats()

    # Determine test categories first to check if we should use meson
    test_categories = determine_test_categories(args)

    # Always use Meson build system for unit tests (Python build system has been removed)
    # Only return early if unit tests are the ONLY thing running (unit_only mode)
    # Skip if fingerprint cache indicates no changes
    if test_categories.unit_only:
        if cpp_test_change:
            from ci.util.meson_runner import run_meson_build_and_test
            from ci.util.paths import PROJECT_ROOT

            build_dir = PROJECT_ROOT / ".build" / "meson"
            test_name = args.test if args.test else None

            result = run_meson_build_and_test(
                source_dir=PROJECT_ROOT,
                build_dir=build_dir,
                test_name=test_name,
                clean=args.clean,
                verbose=args.verbose,
                debug=args.debug,
                build_mode=args.build_mode,
                check=args.check,
            )

            # Print summary with timing
            # Note: The meson_runner already prints "✅ All tests passed (n/n in X.XXs)"
            # so we don't need a second boxed summary here
            if not result.success:
                sys.exit(1)
        else:
            # Fingerprint cache hit - skip unit tests
            print_cache_hit(
                "Fingerprint cache valid - skipping C++ unit tests (no changes detected in C++ test-related files)"
            )

        return

    # For mixed test modes (unit + examples, etc.), run unit tests via Meson but continue to other tests
    # Skip if fingerprint cache indicates no changes
    # Track Meson test timing for summary
    meson_test_timing: Optional[ProcessTiming] = None

    if test_categories.unit and not test_categories.unit_only:
        if cpp_test_change:
            from ci.util.meson_runner import run_meson_build_and_test
            from ci.util.paths import PROJECT_ROOT

            build_dir = PROJECT_ROOT / ".build" / "meson"
            test_name = args.test if args.test else None

            result = run_meson_build_and_test(
                source_dir=PROJECT_ROOT,
                build_dir=build_dir,
                test_name=test_name,
                clean=args.clean,
                verbose=args.verbose,
                debug=args.debug,
                build_mode=args.build_mode,
                check=args.check,
            )

            # Create timing entry for summary
            meson_test_timing = ProcessTiming(
                name=f"cpp_unit_tests ({result.num_tests_passed}/{result.num_tests_run} passed)",
                duration=result.duration,
                command="meson test",
                skipped=False,
            )

            if not result.success:
                sys.exit(1)
        else:
            # Fingerprint cache hit - skip unit tests
            print_cache_hit(
                "Fingerprint cache valid - skipping C++ unit tests (no changes detected in C++ test-related files)"
            )
            meson_test_timing = _create_skipped_timing("cpp_unit_tests")

        # Continue to run other tests (examples, Python, etc.) - don't return

    try:
        # Test categories already determined above (for meson check)
        # Handle stack trace flags: --stack-trace overrides --no-stack-trace, default is enabled
        if args.stack_trace:
            enable_stack_trace = True
        elif args.no_stack_trace:
            enable_stack_trace = False
        else:
            enable_stack_trace = True  # Default: enabled

        # Build up unified list of all processes to run
        processes: list[RunningProcess] = []
        skipped_timings: list[ProcessTiming] = []

        # Note: Unit tests are now exclusively handled by Meson (see lines 1412-1450)
        # This avoids the call to the removed ci.compiler.cpp_test_run module

        # Add integration tests if needed
        if test_categories.integration or test_categories.integration_only:
            processes.append(create_integration_test_process(args, enable_stack_trace))

        # Add example compilation test if source changed OR examples explicitly requested
        # This uses the restored host-based Clang compiler for fast compilation
        examples_explicitly_requested = (
            test_categories.examples or test_categories.examples_only
        )
        should_compile_examples = (
            src_code_change or examples_explicitly_requested
        ) and not test_categories.py_only
        if should_compile_examples:
            processes.append(create_examples_test_process(args, enable_stack_trace))

        # Add Python tests if needed and Python test files have changed
        if (test_categories.py or test_categories.py_only) and python_test_change:
            # Pass run_slow=True if we're running integration tests or any form of full tests
            run_slow = (
                test_categories.integration
                or test_categories.integration_only
                or args.full
            )

            processes.append(
                create_python_test_process(False, run_slow)
            )  # Disable stack trace for Python tests
        elif (test_categories.py or test_categories.py_only) and not python_test_change:
            print_cache_hit(
                "Fingerprint cache valid - skipping Python tests (no changes detected in Python test-related files)"
            )
            skipped_timings.append(_create_skipped_timing("python_tests"))

        # Examples are now compiled via uno compilation test (ci-compile.py)
        # The uno compilation test above already covers example compilation
        # So we just track skipped status if needed
        if (
            test_categories.examples or test_categories.examples_only
        ) and not examples_change:
            print_cache_hit(
                "Fingerprint cache valid - skipping example tests (no changes detected in example-related files)"
            )
            skipped_timings.append(_create_skipped_timing("uno_compilation"))

        # Determine if we'll run in parallel
        will_run_parallel = not bool(os.environ.get("NO_PARALLEL"))

        # Print summary of what we're about to run

        # Run processes (parallel unless NO_PARALLEL is set)
        timings = run_test_processes(
            processes,
            parallel=will_run_parallel,
            verbose=args.verbose,
        )

        # Display timing summary
        all_timings = timings + skipped_timings
        # Add Meson test timing if it was run
        if meson_test_timing:
            all_timings.insert(0, meson_test_timing)  # Put unit tests first
        if all_timings:
            # Show sccache statistics before test summary
            show_sccache_stats()
            summary = _format_timing_summary(all_timings)
            # Use print() instead of ts_print() to avoid orphan timestamps
            # before multi-line content (the summary starts with \n)
            print(summary)
    except (TestExecutionFailedException, TestTimeoutException) as e:
        # Print summary and exit with proper code
        ts_print("\n\033[91m###### ERROR ######\033[0m")
        ts_print(f"Tests failed with {len(e.failures)} failure(s)")

        # Exit with appropriate code
        if e.failures:
            # Use the return code from the first failure, or 1 if none available
            exit_code = (
                e.failures[0].return_code if e.failures[0].return_code != 0 else 1
            )
        else:
            exit_code = 1
        sys.exit(exit_code)
