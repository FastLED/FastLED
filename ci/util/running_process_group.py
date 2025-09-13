#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false, reportUnknownVariableType=false, reportUnknownArgumentType=false
"""RunningProcessGroup - Unified process execution management for FastLED CI."""

import os
import subprocess
import sys
import threading
import time
import traceback
from dataclasses import dataclass
from datetime import datetime, timedelta
from enum import Enum
from typing import Dict, List, Optional, cast

from ci.util.running_process import RunningProcess
from ci.util.test_exceptions import (
    TestExecutionFailedException,
    TestFailureInfo,
    TestTimeoutException,
)
from ci.util.test_runner import (
    _GLOBAL_TIMEOUT,
    MAX_FAILURES_BEFORE_ABORT,
    ProcessOutputHandler,
    ProcessStuckMonitor,
    ProcessTiming,
    StuckProcessSignal,
    _extract_test_name,
    _get_friendly_test_name,
    _handle_stuck_processes,
    extract_error_snippet,
)


class ExecutionMode(Enum):
    """Execution mode for process groups."""

    PARALLEL = "parallel"
    SEQUENTIAL = "sequential"
    SEQUENTIAL_WITH_DEPENDENCIES = "sequential_with_dependencies"


@dataclass
class ProcessStatus:
    """Real-time status information for a running process."""

    name: str
    is_alive: bool
    is_completed: bool
    start_time: datetime
    running_duration: timedelta
    last_output_line: Optional[str] = None
    return_value: Optional[int] = None

    @property
    def running_time_seconds(self) -> float:
        """Get running duration in seconds for display."""
        return self.running_duration.total_seconds()


@dataclass
class GroupStatus:
    """Status information for all processes in a group."""

    group_name: str
    processes: List[ProcessStatus]
    total_processes: int
    completed_processes: int
    failed_processes: int

    @property
    def completion_percentage(self) -> float:
        """Percentage of processes completed."""
        if self.total_processes == 0:
            return 100.0
        return (self.completed_processes / self.total_processes) * 100.0


@dataclass
class ProcessExecutionConfig:
    """Configuration for how a process group should execute."""

    execution_mode: ExecutionMode = ExecutionMode.PARALLEL
    timeout_seconds: Optional[int] = None
    max_failures_before_abort: int = MAX_FAILURES_BEFORE_ABORT
    verbose: bool = False
    enable_stuck_detection: bool = True
    stuck_timeout_seconds: int = _GLOBAL_TIMEOUT
    # Real-time display options
    display_type: str = "auto"  # "auto", "rich", "textual", "ascii"
    live_updates: bool = True
    update_interval: float = 0.1


class RunningProcessGroup:
    """Manages execution of a group of RunningProcess instances."""

    def __init__(
        self,
        processes: Optional[List[RunningProcess]] = None,
        config: Optional[ProcessExecutionConfig] = None,
        name: str = "ProcessGroup",
    ):
        """Initialize a new process group.

        Args:
            processes: List of RunningProcess instances to manage
            config: Configuration for execution behavior
            name: Name for this process group (used for logging)
        """
        self.processes = processes or []
        self.config = config or ProcessExecutionConfig()
        self.name = name
        self._dependencies: Dict[RunningProcess, List[RunningProcess]] = {}

        # Status tracking
        self._process_start_times: Dict[RunningProcess, datetime] = {}
        self._process_last_output: Dict[RunningProcess, str] = {}
        self._status_monitoring_active: bool = False

    def add_process(self, process: RunningProcess) -> None:
        """Add a process to the group.

        Args:
            process: RunningProcess instance to add
        """
        if process not in self.processes:
            self.processes.append(process)

    def add_dependency(
        self, process: RunningProcess, depends_on: RunningProcess
    ) -> None:
        """Add a dependency relationship between processes.

        Args:
            process: The dependent process
            depends_on: The process that must complete first
        """
        if process not in self.processes:
            self.add_process(process)
        if depends_on not in self.processes:
            self.add_process(depends_on)

        if process not in self._dependencies:
            self._dependencies[process] = []
        self._dependencies[process].append(depends_on)

    def add_sequential_chain(self, processes: List[RunningProcess]) -> None:
        """Add processes that must run in sequence.

        Args:
            processes: List of processes to run sequentially
        """
        for process in processes:
            self.add_process(process)

        # Create dependency chain
        for i in range(1, len(processes)):
            self.add_dependency(processes[i], processes[i - 1])

    def run(self) -> List[ProcessTiming]:
        """Execute all processes according to the configuration.

        Returns:
            List of ProcessTiming objects with execution results
        """
        if not self.processes:
            return []

        print(f"Running process group: {self.name}")

        if self.config.execution_mode == ExecutionMode.PARALLEL:
            return self._run_parallel()
        elif self.config.execution_mode == ExecutionMode.SEQUENTIAL:
            return self._run_sequential()
        elif self.config.execution_mode == ExecutionMode.SEQUENTIAL_WITH_DEPENDENCIES:
            return self._run_with_dependencies()
        else:
            raise ValueError(f"Unknown execution mode: {self.config.execution_mode}")

    def _run_parallel(self) -> List[ProcessTiming]:
        """Execute processes in parallel (based on test_runner._run_processes_parallel)."""
        if not self.processes:
            return []

        # Create a shared output handler for formatting
        output_handler = ProcessOutputHandler(verbose=self.config.verbose)

        # Configure Windows console for UTF-8 output if needed
        if os.name == "nt":  # Windows
            if hasattr(sys.stdout, "reconfigure"):
                sys.stdout.reconfigure(encoding="utf-8", errors="replace")  # type: ignore
            if hasattr(sys.stderr, "reconfigure"):
                sys.stderr.reconfigure(encoding="utf-8", errors="replace")  # type: ignore

        # Track start times and enable status monitoring
        self._status_monitoring_active = True
        for proc in self.processes:
            self._track_process_start(proc)

        # Start processes that aren't already running
        for proc in self.processes:
            cmd_str = proc.get_command_str()
            if proc.proc is None:  # Only start if not already running
                proc.run()
                print(f"Started: {cmd_str}")
            else:
                print(f"Process already running: {cmd_str}")

        # Monitor all processes for output and completion
        active_processes = self.processes.copy()
        start_time = time.time()

        runner_timeouts: list[int] = [
            p.timeout for p in self.processes if p.timeout is not None
        ]
        global_timeout: int | None = self.config.timeout_seconds
        if global_timeout is None and runner_timeouts:
            global_timeout = max(runner_timeouts) + 60  # Add 1 minute buffer

        # Track last activity time for each process to detect stuck processes
        last_activity_time = {proc: time.time() for proc in active_processes}
        stuck_process_timeout = self.config.stuck_timeout_seconds

        # Track failed processes for proper error reporting
        failed_processes: list[str] = []  # Processes killed due to timeout/stuck
        exit_failed_processes: list[
            tuple[RunningProcess, int]
        ] = []  # Processes that failed with non-zero exit code

        # Track completed processes for timing summary
        completed_timings: List[ProcessTiming] = []

        # Create thread-based stuck process monitor if enabled
        stuck_monitor = None
        if self.config.enable_stuck_detection:
            stuck_monitor = ProcessStuckMonitor(stuck_process_timeout)

        try:
            # Start monitoring threads for each process
            if stuck_monitor:
                for proc in active_processes:
                    stuck_monitor.start_monitoring(proc)

            def time_expired() -> bool:
                if global_timeout is None:
                    return False
                return time.time() - start_time > global_timeout

            while active_processes:
                # Check global timeout
                if time_expired():
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
                if stuck_monitor:
                    stuck_signals = stuck_monitor.check_for_stuck_processes()
                    if stuck_signals:
                        self._handle_stuck_processes(
                            stuck_signals,
                            active_processes,
                            failed_processes,
                            stuck_monitor,
                        )

                        # Early abort if failure threshold reached via stuck processes
                        if (
                            len(exit_failed_processes) + len(failed_processes)
                        ) >= self.config.max_failures_before_abort:
                            print(
                                f"\nExceeded failure threshold ({self.config.max_failures_before_abort}). Aborting remaining tests."
                            )
                            # Kill any remaining active processes
                            for p in active_processes:
                                p.kill()
                            # Build detailed failures
                            failures = self._build_failure_list(
                                exit_failed_processes, failed_processes
                            )
                            raise TestExecutionFailedException(
                                "Exceeded failure threshold", failures
                            )

                # Process each active test individually
                any_activity = self._process_active_tests(
                    active_processes,
                    exit_failed_processes,
                    failed_processes,
                    completed_timings,
                    stuck_monitor,
                )

                # Brief sleep to avoid spinning if no activity
                if not any_activity:
                    time.sleep(0.01)

        finally:
            # Clean up monitoring
            if stuck_monitor:
                for proc in self.processes:
                    stuck_monitor.stop_monitoring(proc)
            self._status_monitoring_active = False

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

        # Check for failed processes (killed due to timeout/stuck)
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

        return completed_timings

    def get_status(self) -> GroupStatus:
        """Get current status of all processes in the group."""
        process_statuses = []
        completed = 0
        failed = 0

        for process in self.processes:
            start_time = self._process_start_times.get(process)
            if start_time is None:
                # Process hasn't started yet, use current time
                start_time = datetime.now()
                running_duration = timedelta(0)
            else:
                running_duration = datetime.now() - start_time

            # Get last output line from process
            last_output = self._get_last_output_line(process)

            # Get process name - use command friendly name if available
            process_name = getattr(process, "name", None)
            if not process_name:
                if hasattr(process, "command"):
                    process_name = _get_friendly_test_name(process.command)
                else:
                    process_name = f"Process-{id(process)}"

            status = ProcessStatus(
                name=process_name,
                is_alive=not process.finished,
                is_completed=process.finished,
                start_time=start_time,
                running_duration=running_duration,
                last_output_line=last_output,
                return_value=process.returncode,
            )

            process_statuses.append(status)

            if status.is_completed:
                completed += 1
                if status.return_value != 0:
                    failed += 1

        return GroupStatus(
            group_name=self.name,
            processes=process_statuses,
            total_processes=len(self.processes),
            completed_processes=completed,
            failed_processes=failed,
        )

    def _get_last_output_line(self, process: RunningProcess) -> Optional[str]:
        """Extract the last line of output from a process."""
        # Try to get from cached last output first
        cached = self._process_last_output.get(process)
        if cached:
            return cached

        # Fall back to accumulated output
        if process.accumulated_output:
            last_line = process.accumulated_output[-1].strip()
            self._process_last_output[process] = last_line
            return last_line

        return None

    def _track_process_start(self, process: RunningProcess) -> None:
        """Record when a process starts for timing calculations."""
        self._process_start_times[process] = datetime.now()

    def _update_process_output(self, process: RunningProcess) -> None:
        """Update cached last output line for a process."""
        if process.accumulated_output:
            last_line = process.accumulated_output[-1].strip()
            self._process_last_output[process] = last_line

    def _handle_stuck_processes(
        self,
        stuck_signals: list[StuckProcessSignal],
        active_processes: list[RunningProcess],
        failed_processes: list[str],
        stuck_monitor: ProcessStuckMonitor,
    ) -> None:
        """Handle processes that are detected as stuck."""
        _handle_stuck_processes(
            stuck_signals, active_processes, failed_processes, stuck_monitor
        )

    def _build_failure_list(
        self,
        exit_failed_processes: list[tuple[RunningProcess, int]],
        failed_processes: list[str],
    ) -> list[TestFailureInfo]:
        """Build a list of TestFailureInfo objects from failed processes."""
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
            if isinstance(cmd, list):
                cmd_str = subprocess.list2cmdline(cmd)
            else:
                cmd_str = str(cmd)
            failures.append(
                TestFailureInfo(
                    test_name=_extract_test_name(cmd_str),
                    command=cmd_str,
                    return_code=1,
                    output="Process was killed due to timeout/stuck detection",
                    error_type="killed_process",
                )
            )

        return failures

    def _process_active_tests(
        self,
        active_processes: list[RunningProcess],
        exit_failed_processes: list[tuple[RunningProcess, int]],
        failed_processes: list[str],
        completed_timings: List[ProcessTiming],
        stuck_monitor: Optional[ProcessStuckMonitor],
    ) -> bool:
        """Process active tests, return True if any activity occurred."""
        any_activity = False

        # Iterate backwards to safely remove processes from the list
        for i in range(len(active_processes) - 1, -1, -1):
            proc = active_processes[i]

            if self.config.verbose:
                with proc.line_iter(timeout=60) as line_iter:
                    for line in line_iter:
                        print(line)
                        any_activity = True

            # Check if process has finished
            if proc.finished:
                any_activity = True
                # Get the exit code to check for failure
                exit_code = proc.wait()

                # Process completed, remove from active list
                active_processes.remove(proc)
                # Stop monitoring this process
                if stuck_monitor:
                    stuck_monitor.stop_monitoring(proc)

                # Collect timing data
                if proc.duration is not None:
                    timing = ProcessTiming(
                        name=_get_friendly_test_name(proc.command),
                        duration=proc.duration,
                        command=str(proc.command),
                    )
                    completed_timings.append(timing)

                # Check for non-zero exit code (failure)
                if exit_code != 0:
                    print(f"Process failed with exit code {exit_code}: {proc.command}")
                    exit_failed_processes.append((proc, exit_code))
                    # Early abort if we reached the failure threshold
                    if (
                        len(exit_failed_processes) + len(failed_processes)
                    ) >= self.config.max_failures_before_abort:
                        print(
                            f"\nExceeded failure threshold ({self.config.max_failures_before_abort}). Aborting remaining tests."
                        )
                        # Kill remaining active processes
                        for p in active_processes:
                            if p is not proc:
                                p.kill()
                        # Prepare failures with snippets
                        failures = self._build_failure_list(
                            exit_failed_processes, failed_processes
                        )
                        raise TestExecutionFailedException(
                            "Exceeded failure threshold", failures
                        )

        return any_activity

    def _run_sequential(self) -> List[ProcessTiming]:
        """Execute processes in sequence."""
        completed_timings: List[ProcessTiming] = []

        # Enable status monitoring
        self._status_monitoring_active = True
        try:
            for process in self.processes:
                print(f"Running: {process.get_command_str()}")

                # Track process start time
                self._track_process_start(process)

                # Start the process if not already running
                if process.proc is None:
                    process.run()

                try:
                    exit_code = process.wait()

                    # Collect timing data
                    if process.duration is not None:
                        timing = ProcessTiming(
                            name=_get_friendly_test_name(process.command),
                            duration=process.duration,
                            command=str(process.command),
                        )
                        completed_timings.append(timing)

                    # Check for failure
                    if exit_code != 0:
                        error_snippet = extract_error_snippet(
                            process.accumulated_output
                        )
                        failure = TestFailureInfo(
                            test_name=_extract_test_name(process.command),
                            command=str(process.command),
                            return_code=exit_code,
                            output=error_snippet,
                            error_type="exit_failure",
                        )
                        raise TestExecutionFailedException(
                            f"Process failed with exit code {exit_code}", [failure]
                        )

                except Exception as e:
                    print(f"Process failed: {process.get_command_str()} - {e}")
                    raise
        finally:
            self._status_monitoring_active = False

        return completed_timings

    def _run_with_dependencies(self) -> List[ProcessTiming]:
        """Execute processes respecting dependency order."""
        completed_timings: List[ProcessTiming] = []
        completed_processes: set[RunningProcess] = set()
        remaining_processes = self.processes.copy()

        # Enable status monitoring
        self._status_monitoring_active = True
        try:
            while remaining_processes:
                # Find processes that can run (all dependencies completed)
                ready_processes = []
                for process in remaining_processes:
                    dependencies = self._dependencies.get(process, [])
                    if all(dep in completed_processes for dep in dependencies):
                        ready_processes.append(process)

                if not ready_processes:
                    # No processes can run - circular dependency or missing process
                    remaining_names = [
                        _get_friendly_test_name(p.command) for p in remaining_processes
                    ]
                    raise RuntimeError(
                        f"Circular dependency or missing dependency detected for: {remaining_names}"
                    )

                # Run the first ready process
                process = ready_processes[0]
                print(f"Running: {process.get_command_str()}")

                # Track process start time
                self._track_process_start(process)

                # Start the process if not already running
                if process.proc is None:
                    process.run()

                try:
                    exit_code = process.wait()

                    # Collect timing data
                    if process.duration is not None:
                        timing = ProcessTiming(
                            name=_get_friendly_test_name(process.command),
                            duration=process.duration,
                            command=str(process.command),
                        )
                        completed_timings.append(timing)

                    # Mark as completed
                    completed_processes.add(process)
                    remaining_processes.remove(process)

                    # Check for failure
                    if exit_code != 0:
                        error_snippet = extract_error_snippet(
                            process.accumulated_output
                        )
                        failure = TestFailureInfo(
                            test_name=_extract_test_name(process.command),
                            command=str(process.command),
                            return_code=exit_code,
                            output=error_snippet,
                            error_type="exit_failure",
                        )
                        raise TestExecutionFailedException(
                            f"Process failed with exit code {exit_code}", [failure]
                        )

                except Exception as e:
                    print(f"Process failed: {process.get_command_str()} - {e}")
                    raise
        finally:
            self._status_monitoring_active = False

        return completed_timings
