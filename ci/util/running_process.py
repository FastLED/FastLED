# pyright: reportUnknownMemberType=false, reportMissingParameterType=false
import _thread
import os
import queue
import re
import subprocess
import sys
import tempfile
import threading
import time
import traceback
import urllib.parse
import warnings
from pathlib import Path
from queue import Queue
from typing import Any, Callable, Match, Union


class EndOfStream(Exception):
    """
    Exception raised when the end of the stream is reached.
    """

    pass


from ci.util.output_formatter import NullOutputFormatter, OutputFormatter


def normalize_error_warning_paths(line: str) -> str:
    r"""
    Normalize compiler output paths to use consistent separators.

    CONSTRAINT: Only process lines containing "error:" or "warning:"
    (Ignore -I include paths and other compiler flags)

    Fixes:
    - Mixed path separators (/ and \) in error/warning messages
    - URL encoding (%3A -> :) in error/warning messages
    - Relative path issues with .. in error/warning messages

    Args:
        line (str): Raw compiler output line

    Returns:
        str: Normalized line with fixed paths (only if it contains error/warning)
    """
    # Only process error and warning lines - ignore build command lines
    if not any(keyword in line.lower() for keyword in ["error:", "warning:", "note:"]):
        return line

    try:
        # Step 1: Decode URL encoding (e.g., %3A -> :)
        decoded_line = urllib.parse.unquote(line)

        # Step 2: Fix mixed path separators
        normalized_line = fix_path_separators(decoded_line)

        # Step 3: Resolve relative paths where beneficial
        resolved_line = resolve_relative_paths(normalized_line)

        return resolved_line

    except Exception:
        # If normalization fails for any reason, return original line
        # This ensures we never break the build output
        return line


def fix_path_separators(text: str) -> str:
    """
    Convert paths to use native OS separators consistently.

    This function identifies path-like strings and normalizes their separators
    while being careful not to modify non-path content.
    """
    # Pattern to match path-like strings (drive letters, directories, files)
    # Matches: C:\path\to\file, /path/to/file, .\relative\path, ../relative/path
    path_pattern = (
        r'(?:[A-Za-z]:[\\\/]|[.]{0,2}[\\\/])[^\s:;"\'<>|*?]*[\\\/][^\s:;"\'<>|*?]*'
    )

    def normalize_path_match(match: Match[str]) -> str:
        path_str: str = match.group(0)
        try:
            # Use pathlib to normalize the path separators for the current OS
            normalized: str = str(
                Path(path_str).as_posix() if os.name != "nt" else Path(path_str)
            )
            return normalized
        except (ValueError, OSError):
            # If pathlib can't handle it, do simple separator replacement
            if os.name == "nt":
                return path_str.replace("/", "\\")
            else:
                return path_str.replace("\\", "/")

    return re.sub(path_pattern, normalize_path_match, text)


def resolve_relative_paths(text: str) -> str:
    """
    Resolve .. and other relative path components where beneficial.

    This is conservative and only resolves paths that clearly benefit from it.
    """
    # Pattern to match paths with .. that can be resolved
    # Example: /some/path/../other -> /some/other
    relative_pattern = r'([A-Za-z]:[\\\/](?:[^\\\/\s:;"\'<>|*?]+[\\\/])*)[^\\\/\s:;"\'<>|*?]+[\\\/]\.\.[\\\/]([^\\\/\s:;"\'<>|*?]+(?:[\\\/][^\\\/\s:;"\'<>|*?]+)*)'

    def resolve_path_match(match: Match[str]) -> str:
        try:
            full_path = match.group(0)
            # Use pathlib to resolve the path
            resolved = str(Path(full_path).resolve())
            return resolved
        except (ValueError, OSError):
            # If resolution fails, return original
            return match.group(0)

    return re.sub(relative_pattern, resolve_path_match, text)


# Console UTF-8 configuration is now handled globally in ci/__init__.py


class ProcessOutputReader:
    """Dedicated reader that drains a process's stdout and enqueues lines.

    This keeps the stdout pipe drained to prevent blocking and forwards
    transformed, non-empty lines to the provided output queue. It also invokes
    lifecycle callbacks for timing/unregister behaviors.
    """

    def __init__(
        self,
        proc: subprocess.Popen[Any],
        shutdown: threading.Event,
        output_formatter: OutputFormatter | None,
        on_output: Callable[[str | EndOfStream], None],
        on_end: Callable[[], None],
    ) -> None:
        output_formatter = output_formatter or NullOutputFormatter()
        self._proc = proc
        self._shutdown = shutdown
        self._output_formatter = output_formatter
        self._on_output = on_output
        self._on_end = on_end
        self.last_stdout_ts: float | None = None

    def run(self) -> None:
        """Continuously read stdout lines and forward them until EOF or shutdown."""
        try:
            assert self._proc.stdout is not None

            try:
                for line in self._proc.stdout:
                    self.last_stdout_ts = time.time()
                    if self._shutdown.is_set():
                        break

                    line_stripped = line.rstrip()
                    if not line_stripped:
                        continue

                    transformed_line = self._output_formatter.transform(line_stripped)

                    self._on_output(transformed_line)

            except (ValueError, OSError) as e:
                # Normal shutdown scenarios include closed file descriptors.
                if "closed file" in str(e) or "Bad file descriptor" in str(e):
                    warnings.warn(f"Output reader encountered closed file: {e}")
                    pass
                else:
                    print(f"Warning: Output reader encountered error: {e}")
            finally:
                # Signal end-of-stream to consumers
                self._on_output(EndOfStream())
        finally:
            # Cleanup stream and invoke completion callback
            if self._proc.stdout and not self._proc.stdout.closed:
                try:
                    self._proc.stdout.close()
                except (ValueError, OSError) as err:
                    warnings.warn(f"Output reader encountered error: {err}")
                    pass

            # Notify parent for timing/unregistration
            try:
                self._on_end()
            finally:
                # Ensure a final end-of-stream marker is present
                self._on_output(EndOfStream())


class RunningProcess:
    """
    A class to manage and stream output from a running subprocess.

    This class provides functionality to execute shell commands, stream their output
    in real-time via a queue, and control the subprocess execution.
    """

    def __init__(
        self,
        command: str | list[str],
        cwd: Path | None = None,
        check: bool = False,
        auto_run: bool = True,
        timeout: int = 120,  # 2 minutes.
        enable_stack_trace: bool = False,  # Enable stack trace dumping on timeout
        on_complete: Callable[[], None]
        | None = None,  # Callback to execute when process completes
        output_formatter: OutputFormatter | None = None,
    ):
        """
        Initialize the RunningProcess instance. Note that stderr is merged into stdout!!

        Args:
            command (str): The command to execute.
            cwd (Path | None): The working directory to execute the command in.
            check (bool): If True, raise an exception if the command returns a non-zero exit code.
            auto_run (bool): If True, automatically run the command when the instance is created.
            timeout (int): Timeout in seconds for process execution. Default 30 seconds.
            enable_stack_trace (bool): If True, dump stack trace when process times out.
            on_complete (Callable[[], None] | None): Callback function to execute when process completes.
            output_formatter (OutputFormatter | None): Optional formatter for processing output lines.
        """
        if isinstance(command, list):
            command = subprocess.list2cmdline(command)
        self.command = command
        self.cwd = str(cwd) if cwd is not None else None
        self.output_queue: Queue[str | EndOfStream] = Queue()
        self.accumulated_output: list[str] = []  # Store all output for later retrieval
        self.proc: subprocess.Popen[Any] | None = None
        self.check = check
        # Force auto_run to False if NO_PARALLEL is set
        self.auto_run = False if os.environ.get("NO_PARALLEL") else auto_run
        self.timeout = timeout
        self.enable_stack_trace = enable_stack_trace
        self.on_complete = on_complete
        # Always keep a non-None formatter
        self.output_formatter = (
            output_formatter if output_formatter is not None else NullOutputFormatter()
        )
        self.reader_thread: threading.Thread | None = None
        self.watcher_thread: threading.Thread | None = None
        self.shutdown: threading.Event = threading.Event()
        self._start_time: float | None = None
        self._end_time: float | None = None
        self._stream_finished = False
        self._time_last_stdout_line: float | None = None
        self._termination_notified: bool = False
        if auto_run:
            self.run()

    def _dump_stack_trace(self) -> str:
        """
        Dump stack trace of the running process using GDB.

        Returns:
            str: GDB output containing stack trace information.
        """
        if self.proc is None:
            return "No process to dump stack trace for."

        try:
            # Get the process ID
            pid = self.proc.pid

            # Create GDB script for attaching to running process
            with tempfile.NamedTemporaryFile(mode="w+", delete=False) as gdb_script:
                gdb_script.write("set pagination off\n")
                gdb_script.write(f"attach {pid}\n")
                gdb_script.write("bt full\n")
                gdb_script.write("info registers\n")
                gdb_script.write("x/16i $pc\n")
                gdb_script.write("thread apply all bt full\n")
                gdb_script.write("detach\n")
                gdb_script.write("quit\n")

            # Run GDB to get stack trace
            gdb_command = f"gdb -batch -x {gdb_script.name}"
            gdb_process = subprocess.Popen(
                gdb_command,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,
                shell=True,
                text=True,
            )

            gdb_output, _ = gdb_process.communicate(
                timeout=30
            )  # 30 second timeout for GDB

            # Clean up GDB script
            os.unlink(gdb_script.name)

            return gdb_output

        except Exception as e:
            return f"Failed to dump stack trace: {e}"

    def time_last_stdout_line(self) -> float | None:
        return self._time_last_stdout_line

    def run(self) -> None:
        """
        Execute the command and stream its output to the queue.

        Raises:
            subprocess.CalledProcessError: If the command returns a non-zero exit code.
        """
        assert self.proc is None
        self.proc = subprocess.Popen(
            self.command,
            shell=True,
            cwd=self.cwd,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # Merge stderr into stdout
            text=True,  # Use text mode
            encoding="utf-8",  # Explicitly use UTF-8
            errors="replace",  # Replace invalid chars instead of failing
        )

        # Track start time after process is successfully created
        # This excludes process creation overhead from timing measurements
        self._start_time = time.time()

        # Register with global process manager
        try:
            from ci.util.running_process_manager import (
                RunningProcessManagerSingleton,
            )

            RunningProcessManagerSingleton.register(self)
        except Exception as e:
            warnings.warn(f"RunningProcessManager.register failed: {e}")

        # Start the output formatter if provided
        # Begin formatter lifecycle
        self.output_formatter.begin()

        # Prepare output reader helper
        assert self.proc is not None

        def _on_reader_end() -> None:
            # Set end time when stdout pumper finishes; captures completion time of useful output
            if self._end_time is None:
                self._end_time = time.time()
            # Unregister when stdout is fully drained
            try:
                self._notify_terminated()
            except Exception as e:
                warnings.warn(f"RunningProcess termination notify (drain) failed: {e}")

        def _on_output(item: str | EndOfStream) -> None:
            # Forward to queue and capture text lines for accumulated output
            if isinstance(item, EndOfStream):
                self.output_queue.put(item)
            else:
                self.output_queue.put(item)
                self.accumulated_output.append(item)

        reader = ProcessOutputReader(
            proc=self.proc,
            shutdown=self.shutdown,
            output_formatter=self.output_formatter,
            on_output=_on_output,
            on_end=_on_reader_end,
        )

        # Start output reader thread
        self.reader_thread = threading.Thread(target=reader.run, daemon=True)
        self.reader_thread.start()

        # Start watcher thread that polls for process termination every 100ms
        def _watch_termination() -> None:
            thread_id = threading.current_thread().ident
            thread_name = threading.current_thread().name
            try:
                while not self.shutdown.is_set():
                    # Use unified poll() so unregistration happens in one place
                    rc = self.poll()
                    if rc is not None:
                        break
                    time.sleep(0.1)
            except KeyboardInterrupt:
                print(f"Thread {thread_id} ({thread_name}) caught KeyboardInterrupt")
                print(f"Stack trace for thread {thread_id}:")
                traceback.print_exc()
                _thread.interrupt_main()
                raise
            except Exception as e:
                # Surface unexpected errors and keep behavior consistent
                print(f"Watcher thread error in {thread_name}: {e}")
                traceback.print_exc()

        watcher_name = "RPWatcher"
        try:
            if self.proc is not None and self.proc.pid is not None:
                watcher_name = f"RPWatcher-{self.proc.pid}"
        except Exception:
            pass
        self.watcher_thread = threading.Thread(
            target=_watch_termination, name=watcher_name, daemon=True
        )
        self.watcher_thread.start()

    def get_next_line(self, timeout: float | None = None) -> str | EndOfStream:
        """
        Get the next line of output from the process.

        Args:
            timeout: How long to wait for the next line in seconds.
                    None means wait forever, 0 means don't wait.

        Returns:
            The next line of output, or None if no more output is available
            or the timeout was reached.

        Raises:
            queue.Empty: If timeout is 0 or specified and no line is available
            TimeoutError: If the process times out
        """
        if self._stream_finished:
            return EndOfStream()

        assert self.proc is not None

        expired_time = time.time() + timeout if timeout is not None else None

        while True:
            if self._stream_finished:
                return EndOfStream()

            if expired_time is not None:
                if time.time() > expired_time:
                    raise TimeoutError(f"Timeout after {timeout} seconds")

            if self.output_queue.empty():
                if timeout != 0:
                    time.sleep(0.01)
                    continue
            try:
                line = self.output_queue.get(timeout=0.1)
                if isinstance(line, EndOfStream):
                    self._stream_finished = True
                    return EndOfStream()
                return line
            except queue.Empty:
                if self.finished:
                    self._stream_finished = True
                    return EndOfStream()
                continue

    def get_next_line_non_blocking(self) -> str | None | EndOfStream:
        """
        Get the next line of output from the process.

        Returns:
            str: Next line of output if available
            None: No output available right now (should continue polling)
            EndOfStream: Process has finished, no more output will be available
        """
        if self._stream_finished:
            return EndOfStream()

        try:
            out: str | EndOfStream = self.get_next_line(timeout=0)
            if isinstance(out, EndOfStream):
                return EndOfStream()
            return out
        except queue.Empty:
            return None
        except TimeoutError:
            return None

    def poll(self) -> int | None:
        """
        Check the return code of the process.
        """
        if self.proc is None:
            return None
        rc = self.proc.poll()
        if rc is not None:
            # Ensure unregistration only happens once
            try:
                self._notify_terminated()
            except Exception as e:
                warnings.warn(f"RunningProcess termination notify (poll) failed: {e}")
        return rc

    @property
    def finished(self) -> bool:
        return self.poll() is not None

    def wait(self) -> int:
        """
        Wait for the process to complete with timeout protection.

        Raises:
            ValueError: If the process hasn't been started.
            TimeoutError: If the process takes longer than the timeout.
        """
        if self.proc is None:
            raise ValueError("Process is not running.")

        # Use a timeout to prevent hanging
        start_time = time.time()
        while self.poll() is None:
            if time.time() - start_time > self.timeout:
                # Process is taking too long, dump stack trace if enabled
                if self.enable_stack_trace:
                    print(
                        f"\nProcess timeout after {self.timeout} seconds, dumping stack trace..."
                    )
                    print(f"Command: {self.command}")
                    print(f"Process ID: {self.proc.pid}")

                    try:
                        stack_trace = self._dump_stack_trace()
                        print("\n" + "=" * 80)
                        print("STACK TRACE DUMP (GDB Output)")
                        print("=" * 80)
                        print(stack_trace)
                        print("=" * 80)
                    except Exception as e:
                        print(f"Failed to dump stack trace: {e}")

                # Kill the process
                print(f"Killing timed out process: {self.command}")
                self.kill()
                raise TimeoutError(
                    f"Process timed out after {self.timeout} seconds: {self.command}"
                )
            time.sleep(0.01)  # Check every 10ms

        # Process has completed, get return code
        assert self.proc is not None  # For type checker
        rtn = self.proc.returncode
        assert rtn is not None  # Process has completed, so returncode exists

        is_keyboard_interrupt = (rtn == -11) or (rtn == 3221225786)
        if is_keyboard_interrupt:
            import _thread

            print("Keyboard interrupt detected, interrupting main thread")
            _thread.interrupt_main()
            return 1

        # Record end time only if not already set by output reader
        # The output reader sets end time when stdout pumper finishes, which is more accurate
        if self._end_time is None:
            self._end_time = time.time()

        # Wait for reader thread to finish and cleanup
        if self.reader_thread is not None:
            self.reader_thread.join(
                timeout=0.05
            )  # 50ms should be plenty for thread cleanup
            if self.reader_thread.is_alive():
                # Reader thread didn't finish, force shutdown
                self.shutdown.set()
                self.reader_thread.join(timeout=0.05)  # 50ms for forced shutdown

        # Execute completion callback if provided
        if self.on_complete is not None:
            try:
                self.on_complete()
            except Exception as e:
                print(f"Warning: on_complete callback failed: {e}")

        # End the output formatter if provided
        if self.output_formatter:
            self.output_formatter.end()

        # Unregister from global process manager on normal completion
        try:
            self._notify_terminated()
        except Exception as e:
            warnings.warn(f"RunningProcess termination notify (wait) failed: {e}")

        return rtn

    def kill(self) -> None:
        """
        Immediately terminate the process with SIGKILL and all child processes.

        Raises:
            ValueError: If the process hasn't been started.
        """
        if self.proc is None:
            return

        # Record end time when killed (only if not already set by output reader)
        if self._end_time is None:
            self._end_time = time.time()

        # Signal reader thread to stop
        self.shutdown.set()

        # Kill the entire process tree (parent + all children)
        # This prevents orphaned clang++ processes from hanging the system
        try:
            from ci.util.test_env import kill_process_tree

            kill_process_tree(self.proc.pid)
        except Exception as e:
            # Fallback to simple kill if tree kill fails
            print(f"Warning: Failed to kill process tree for {self.proc.pid}: {e}")
            try:
                self.proc.kill()
            except KeyboardInterrupt:
                print(f"Keyboard interrupt detected, interrupting main thread")
                _thread.interrupt_main()
                self.proc.kill()
                raise
            except Exception:
                pass  # Process might already be dead

        # Wait for reader thread to finish
        if self.reader_thread is not None:
            self.reader_thread.join(timeout=0.05)  # 50ms should be plenty for cleanup

        # Drain any remaining output
        while True:
            try:
                line = self.output_queue.get_nowait()
                if line is None:  # End of output marker
                    break
            except queue.Empty:
                break

        # Ensure unregistration even on forced kill
        try:
            from ci.util.running_process_manager import (
                RunningProcessManagerSingleton,
            )

            RunningProcessManagerSingleton.unregister(self)
        except Exception as e:
            warnings.warn(f"RunningProcessManager.unregister (kill) failed: {e}")

    def _notify_terminated(self) -> None:
        """Idempotent notification that the process has terminated.

        Ensures unregister is called only once across multiple termination paths
        (poll, wait, stdout drain, watcher thread) and records end time when
        available.
        """
        if self._termination_notified:
            return
        self._termination_notified = True

        # Record end time only if not already set
        if self._end_time is None:
            self._end_time = time.time()

        try:
            from ci.util.running_process_manager import RunningProcessManagerSingleton

            RunningProcessManagerSingleton.unregister(self)
        except Exception as e:
            warnings.warn(f"RunningProcessManager.unregister notify failed: {e}")

    def terminate(self) -> None:
        """
        Gracefully terminate the process with SIGTERM.

        Raises:
            ValueError: If the process hasn't been started.
        """
        if self.proc is None:
            raise ValueError("Process is not running.")
        self.shutdown.set()
        self.proc.terminate()

    @property
    def returncode(self) -> int | None:
        if self.proc is None:
            return None
        return self.proc.returncode

    @property
    def start_time(self) -> float | None:
        """Get the process start time"""
        return self._start_time

    @property
    def end_time(self) -> float | None:
        """Get the process end time"""
        return self._end_time

    @property
    def duration(self) -> float | None:
        """Get the process duration in seconds, or None if not completed"""
        if self._start_time is None or self._end_time is None:
            return None
        return self._end_time - self._start_time

    @property
    def stdout(self) -> str:
        """
        Get the complete stdout output of the process.

        Returns:
            str: The complete stdout output as a string.
        """
        # Return accumulated output (available even if process is still running)
        return "\n".join(self.accumulated_output)


class RunningProcessManager:
    """
    Thread-safe registry of currently running processes for watchdog diagnostics.
    """

    def __init__(self) -> None:
        self._lock = threading.RLock()
        self._processes: list[RunningProcess] = []

    def register(self, proc: RunningProcess) -> None:
        with self._lock:
            if proc not in self._processes:
                self._processes.append(proc)

    def unregister(self, proc: RunningProcess) -> None:
        with self._lock:
            try:
                self._processes.remove(proc)
            except ValueError:
                pass

    def list_active(self) -> list[RunningProcess]:
        with self._lock:
            return [p for p in self._processes if not p.finished]

    def dump_active(self) -> None:
        active = self.list_active()
        if not active:
            print("\nNO ACTIVE SUBPROCESSES DETECTED - MAIN PROCESS LIKELY HUNG")
            return

        print("\nSTUCK SUBPROCESS COMMANDS:")
        now = time.time()
        for idx, p in enumerate(active, 1):
            pid: int | None = None
            try:
                if p.proc is not None:
                    pid = p.proc.pid
            except Exception:
                pid = None

            start: float | None = p.start_time
            last_out: float | None = p.time_last_stdout_line()
            duration_str = f"{(now - start):.1f}s" if start is not None else "?"
            since_out_str = (
                f"{(now - last_out):.1f}s" if last_out is not None else "no-output"
            )

            print(
                f"  {idx}. cmd={p.command} pid={pid} duration={duration_str} last_output={since_out_str}"
            )


# NOTE: Singleton moved to ci.util.running_process_manager for reuse


def subprocess_run(
    command: str | list[str],
    cwd: Path | None,
    check: bool,
    timeout: int,
    enable_stack_trace: bool,
) -> subprocess.CompletedProcess[str]:
    """Execute a command with stdout pumping and merged stderr, returning CompletedProcess.

    This emulates subprocess.run() behavior using RunningProcess as the backend:
    - Streams and drains stdout continuously to avoid pipe blocking
    - Merges stderr into stdout
    - Returns a standard subprocess.CompletedProcess with combined stdout
    - Raises subprocess.CalledProcessError when check is True and exit code != 0

    Args:
        command: Command to execute (string or list of arguments).
        cwd: Working directory to execute the command in. Must be provided explicitly.
        check: When True, raise CalledProcessError if the command exits non-zero.
        timeout: Maximum number of seconds to allow the process to run.
        enable_stack_trace: Enable stack trace dumping on timeout for diagnostics.

    Returns:
        subprocess.CompletedProcess[str]: Completed process with combined stdout and return code.
    """
    # Use RunningProcess for robust stdout pumping with merged stderr
    proc = RunningProcess(
        command=command,
        cwd=cwd,
        check=False,
        auto_run=True,
        timeout=timeout,
        enable_stack_trace=enable_stack_trace,
        on_complete=None,
        output_formatter=None,
    )

    try:
        return_code: int = proc.wait()
    except KeyboardInterrupt:
        # Propagate interrupt behavior consistent with subprocess.run
        raise
    except TimeoutError as e:
        # Align with subprocess.TimeoutExpired semantics by raising a CalledProcessError-like
        # error with available output. Using TimeoutError here is consistent with internal RP.
        completed_output: str = proc.stdout
        raise RuntimeError(
            f"CRITICAL: Process timed out after {timeout} seconds: {command}"
        ) from e

    combined_stdout: str = proc.stdout

    # Construct CompletedProcess (stderr is merged into stdout by design)
    completed = subprocess.CompletedProcess(
        args=command,
        returncode=return_code,
        stdout=combined_stdout,
        stderr=None,
    )

    if check and return_code != 0:
        # Raise the standard exception with captured output
        raise subprocess.CalledProcessError(
            returncode=return_code,
            cmd=command,
            output=combined_stdout,
            stderr=None,
        )

    return completed
