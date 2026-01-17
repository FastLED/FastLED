#!/usr/bin/env python3
"""PlatformIO Process Tracking and Cleanup Module.

This module provides a central registry for tracking all PIO subprocess instances
and ensures they are properly terminated on program exit.

Key features:
- Track all subprocess.Popen instances that run PIO commands
- atexit handler to kill outstanding processes on normal exit
- signal handlers for SIGTERM/SIGINT to clean up on interrupts
- Thread-safe operations
"""

import atexit
import logging
import os
import signal
import subprocess
import threading
from typing import Any

import psutil

from ci.util.global_interrupt_handler import (  # noqa: F401
    handle_keyboard_interrupt_properly,
)


# Configure module logger
logger = logging.getLogger(__name__)


class PioProcessRegistry:
    """Thread-safe registry for tracking PIO subprocess instances."""

    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._processes: dict[int, subprocess.Popen[Any]] = {}  # pid -> Popen
        self._atexit_registered = False
        self._signal_handlers_installed = False

    def register(self, proc: subprocess.Popen[Any]) -> None:
        """Register a PIO subprocess for tracking.

        Args:
            proc: subprocess.Popen instance to track
        """
        pid = proc.pid
        if pid is None:
            return

        with self._lock:
            self._processes[pid] = proc
            self._ensure_cleanup_handlers()
            logger.debug(f"Registered PIO process: pid={pid}")

    def unregister(self, proc: subprocess.Popen[Any]) -> None:
        """Unregister a PIO subprocess (called when process completes normally).

        Args:
            proc: subprocess.Popen instance to unregister
        """
        pid = proc.pid
        if pid is None:
            return

        with self._lock:
            if pid in self._processes:
                del self._processes[pid]
                logger.debug(f"Unregistered PIO process: pid={pid}")

    def unregister_by_pid(self, pid: int) -> None:
        """Unregister a process by its PID.

        Args:
            pid: Process ID to unregister
        """
        with self._lock:
            if pid in self._processes:
                del self._processes[pid]
                logger.debug(f"Unregistered PIO process by pid: {pid}")

    def _ensure_cleanup_handlers(self) -> None:
        """Ensure atexit and signal handlers are registered (once)."""
        if not self._atexit_registered:
            atexit.register(self._cleanup_all_processes)
            self._atexit_registered = True
            logger.debug("Registered atexit handler for PIO process cleanup")

        if not self._signal_handlers_installed:
            self._install_signal_handlers()
            self._signal_handlers_installed = True

    def _install_signal_handlers(self) -> None:
        """Install signal handlers for graceful cleanup on SIGTERM/SIGINT."""
        # Save original handlers
        self._original_sigterm = signal.getsignal(signal.SIGTERM)
        self._original_sigint = signal.getsignal(signal.SIGINT)

        def signal_handler(signum: int, frame: Any) -> None:
            """Handle SIGTERM/SIGINT by cleaning up processes first."""
            logger.info(f"Received signal {signum}, cleaning up PIO processes...")
            self._cleanup_all_processes()

            # Call original handler if it exists
            original = (
                self._original_sigterm
                if signum == signal.SIGTERM
                else self._original_sigint
            )
            if callable(original):
                original(signum, frame)
            elif original == signal.SIG_DFL:
                # Re-raise to trigger default behavior
                signal.signal(signum, signal.SIG_DFL)
                os.kill(os.getpid(), signum)

        try:
            signal.signal(signal.SIGTERM, signal_handler)
            signal.signal(signal.SIGINT, signal_handler)
            logger.debug("Installed SIGTERM/SIGINT handlers for PIO process cleanup")
        except ValueError:
            # Signal handlers can only be set in main thread
            logger.debug("Cannot install signal handlers (not in main thread)")

    def _cleanup_all_processes(self) -> None:
        """Kill all tracked PIO processes and their children."""
        with self._lock:
            if not self._processes:
                return

            logger.info(
                f"Cleaning up {len(self._processes)} outstanding PIO processes..."
            )
            pids_to_cleanup = list(self._processes.keys())

        for pid in pids_to_cleanup:
            self._kill_process_tree(pid)

        with self._lock:
            self._processes.clear()

    def _kill_process_tree(self, pid: int) -> int:
        """Kill a process and all its children.

        Args:
            pid: Root process ID to kill

        Returns:
            Number of processes killed
        """
        killed_count = 0

        try:
            parent = psutil.Process(pid)
        except psutil.NoSuchProcess:
            logger.debug(f"Process {pid} already terminated")
            return 0
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise

        # Get all children recursively
        try:
            children = parent.children(recursive=True)
        except psutil.NoSuchProcess:
            children = []
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise

        # Build list of all processes to kill (children + parent)
        processes_to_kill: list[psutil.Process] = []
        for child in reversed(children):  # Kill children first
            try:
                processes_to_kill.append(child)
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception:
                pass

        processes_to_kill.append(parent)

        # Terminate all processes
        for proc in processes_to_kill:
            try:
                proc.terminate()
                killed_count += 1
                logger.debug(f"Terminated process {proc.pid} ({proc.name()})")
            except psutil.NoSuchProcess:
                pass
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                logger.warning(f"Failed to terminate process {proc.pid}: {e}")

        # Wait for graceful termination
        _gone, alive = psutil.wait_procs(processes_to_kill, timeout=3)
        del _gone  # unused

        # Force kill stragglers
        for proc in alive:
            try:
                proc.kill()
                logger.warning(f"Force killed process {proc.pid}")
            except psutil.NoSuchProcess:
                pass
            except KeyboardInterrupt:
                handle_keyboard_interrupt_properly()
                raise
            except Exception as e:
                logger.warning(f"Failed to force kill process {proc.pid}: {e}")

        return killed_count

    def get_active_pids(self) -> list[int]:
        """Get list of all tracked process PIDs.

        Returns:
            List of PIDs currently being tracked
        """
        with self._lock:
            return list(self._processes.keys())

    def get_active_count(self) -> int:
        """Get count of tracked processes.

        Returns:
            Number of processes currently being tracked
        """
        with self._lock:
            return len(self._processes)


# Global singleton instance
_registry = PioProcessRegistry()


def register_pio_process(proc: subprocess.Popen[Any]) -> None:
    """Register a PIO subprocess for tracking and cleanup on exit.

    Args:
        proc: subprocess.Popen instance to track
    """
    _registry.register(proc)


def unregister_pio_process(proc: subprocess.Popen[Any]) -> None:
    """Unregister a PIO subprocess (call when process completes normally).

    Args:
        proc: subprocess.Popen instance to unregister
    """
    _registry.unregister(proc)


def unregister_pio_process_by_pid(pid: int) -> None:
    """Unregister a process by its PID.

    Args:
        pid: Process ID to unregister
    """
    _registry.unregister_by_pid(pid)


def get_active_pio_processes() -> list[int]:
    """Get list of all tracked PIO process PIDs.

    Returns:
        List of PIDs currently being tracked
    """
    return _registry.get_active_pids()


def kill_all_pio_processes() -> None:
    """Kill all tracked PIO processes immediately."""
    _registry._cleanup_all_processes()


class TrackedPopen:
    """Context manager wrapper for subprocess.Popen with automatic tracking.

    Usage:
        with TrackedPopen(["pio", "run"], cwd=build_dir) as proc:
            stdout, stderr = proc.communicate()
            # Process automatically unregistered on exit

    Or without context manager:
        proc = TrackedPopen(["pio", "run"], cwd=build_dir)
        proc.wait()
        proc.unregister()  # Must call manually when not using context manager
    """

    def __init__(
        self,
        args: list[str] | str,
        cwd: str | None = None,
        env: dict[str, str] | None = None,
        shell: bool = False,
        stdout: int | None = None,
        stderr: int | None = None,
        stdin: int | None = None,
        text: bool = False,
        **kwargs: Any,
    ) -> None:
        """Create and register a tracked subprocess.

        Args:
            args: Command to execute
            cwd: Working directory
            env: Environment variables
            shell: Use shell execution
            stdout: stdout handling (e.g., subprocess.PIPE)
            stderr: stderr handling
            stdin: stdin handling
            text: Text mode for I/O
            **kwargs: Additional arguments for subprocess.Popen
        """
        self._proc = subprocess.Popen(
            args,
            cwd=cwd,
            env=env,
            shell=shell,
            stdout=stdout,
            stderr=stderr,
            stdin=stdin,
            text=text,
            **kwargs,
        )
        register_pio_process(self._proc)

    @property
    def proc(self) -> subprocess.Popen[Any]:
        """Get the underlying Popen instance."""
        return self._proc

    @property
    def pid(self) -> int | None:
        """Get the process ID."""
        return self._proc.pid

    @property
    def returncode(self) -> int | None:
        """Get the return code (None if still running)."""
        return self._proc.returncode

    def wait(self, timeout: float | None = None) -> int:
        """Wait for process to complete.

        Args:
            timeout: Maximum time to wait in seconds

        Returns:
            Process return code
        """
        return self._proc.wait(timeout=timeout)

    def communicate(
        self, input: bytes | str | None = None, timeout: float | None = None
    ) -> tuple[Any, Any]:
        """Interact with process.

        Args:
            input: Data to send to stdin
            timeout: Maximum time to wait in seconds

        Returns:
            Tuple of (stdout, stderr)
        """
        return self._proc.communicate(input=input, timeout=timeout)  # type: ignore[arg-type]

    def poll(self) -> int | None:
        """Check if process has terminated.

        Returns:
            Return code if terminated, None otherwise
        """
        return self._proc.poll()

    def terminate(self) -> None:
        """Terminate the process."""
        self._proc.terminate()

    def kill(self) -> None:
        """Kill the process."""
        self._proc.kill()

    def unregister(self) -> None:
        """Unregister the process from tracking."""
        unregister_pio_process(self._proc)

    def __enter__(self) -> "TrackedPopen":
        return self

    def __exit__(self, _exc_type: Any, _exc_val: Any, _exc_tb: Any) -> None:
        # Wait for process to complete if still running
        if self._proc.poll() is None:
            try:
                self._proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._proc.kill()
                self._proc.wait()
        # Unregister from tracking
        self.unregister()


def run_pio_tracked(
    args: list[str],
    cwd: str | None = None,
    env: dict[str, str] | None = None,
    shell: bool = False,
    capture_output: bool = False,
    text: bool = True,
    check: bool = False,
    timeout: float | None = None,
) -> subprocess.CompletedProcess[Any]:
    """Run a PIO command with automatic process tracking.

    This is a drop-in replacement for subprocess.run() that tracks the process
    and ensures it's killed on program exit if still running.

    Args:
        args: Command to execute
        cwd: Working directory
        env: Environment variables
        shell: Use shell execution
        capture_output: Capture stdout/stderr
        text: Text mode for I/O
        check: Raise CalledProcessError on non-zero exit
        timeout: Maximum time to wait in seconds

    Returns:
        subprocess.CompletedProcess instance
    """
    stdout_pipe = subprocess.PIPE if capture_output else None
    stderr_pipe = subprocess.PIPE if capture_output else None

    with TrackedPopen(
        args,
        cwd=cwd,
        env=env,
        shell=shell,
        stdout=stdout_pipe,
        stderr=stderr_pipe,
        text=text,
    ) as tracked:
        try:
            stdout, stderr = tracked.communicate(timeout=timeout)
        except subprocess.TimeoutExpired:
            tracked.kill()
            stdout, stderr = tracked.communicate()
            raise subprocess.TimeoutExpired(
                args, timeout or 0.0, output=stdout, stderr=stderr
            )

        retcode = tracked.returncode

    if check and retcode:
        raise subprocess.CalledProcessError(retcode, args, output=stdout, stderr=stderr)

    return subprocess.CompletedProcess(args, retcode or 0, stdout, stderr)
