# pyright: reportUnknownMemberType=false, reportMissingParameterType=false
import _thread
import os
import queue
import shlex
import subprocess
import tempfile
import threading
import time
import traceback
import warnings
from pathlib import Path
from queue import Queue
from typing import Any, Callable, ContextManager, Iterator


class EndOfStream(Exception):
    """Sentinel used to indicate end-of-stream from the reader."""


from ci.util.output_formatter import NullOutputFormatter, OutputFormatter


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
        self._eos_emitted: bool = False

    def _emit_eos_once(self) -> None:
        """Ensure EndOfStream is only forwarded a single time."""
        if not self._eos_emitted:
            self._eos_emitted = True
            self._on_output(EndOfStream())

    def run(self) -> None:
        """Continuously read stdout lines and forward them until EOF or shutdown."""
        try:
            # Begin formatter lifecycle within the reader context
            try:
                self._output_formatter.begin()
            except Exception as e:
                warnings.warn(f"Output formatter begin() failed: {e}")

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

            except KeyboardInterrupt:
                # Per project rules, handle interrupts in threads explicitly
                thread_id = threading.current_thread().ident
                thread_name = threading.current_thread().name
                print(f"Thread {thread_id} ({thread_name}) caught KeyboardInterrupt")
                print(f"Stack trace for thread {thread_id}:")
                traceback.print_exc()
                # Try to ensure child process is terminated promptly
                try:
                    self._proc.kill()
                except Exception:
                    pass
                # Propagate to main thread and re-raise
                _thread.interrupt_main()
                # EOF
                self._emit_eos_once()
                raise

            except (ValueError, OSError) as e:
                # Normal shutdown scenarios include closed file descriptors.
                if "closed file" in str(e) or "Bad file descriptor" in str(e):
                    warnings.warn(f"Output reader encountered closed file: {e}")
                    pass
                else:
                    print(f"Warning: Output reader encountered error: {e}")
            finally:
                # Signal end-of-stream to consumers exactly once
                self._emit_eos_once()
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
                # End formatter lifecycle within the reader context
                try:
                    self._output_formatter.end()
                except Exception as e:
                    warnings.warn(f"Output formatter end() failed: {e}")


class ProcessWatcher:
    """Background watcher that polls a process until it terminates."""

    def __init__(self, running_process: "RunningProcess") -> None:
        self._rp = running_process
        self._thread: threading.Thread | None = None

    def start(self) -> None:
        name: str = "RPWatcher"
        try:
            if self._rp.proc is not None and self._rp.proc.pid is not None:
                name = f"RPWatcher-{self._rp.proc.pid}"
        except Exception:
            pass

        self._thread = threading.Thread(target=self._run, name=name, daemon=True)
        self._thread.start()

    def _run(self) -> None:
        thread_id = threading.current_thread().ident
        thread_name = threading.current_thread().name
        try:
            while not self._rp.shutdown.is_set():
                # Enforce per-process timeout independently of wait()
                if (
                    self._rp.timeout is not None
                    and self._rp.start_time is not None
                    and (time.time() - self._rp.start_time) > self._rp.timeout
                ):
                    print(
                        f"Process timeout after {self._rp.timeout} seconds (watcher), killing: {self._rp.command}"
                    )
                    if self._rp.enable_stack_trace:
                        try:
                            print("\n" + "=" * 80)
                            print("STACK TRACE DUMP (GDB Output)")
                            print("=" * 80)
                            print(self._rp._dump_stack_trace())
                            print("=" * 80)
                        except Exception as e:
                            print(f"Watcher stack trace dump failed: {e}")
                    self._rp.kill()
                    break

                rc: int | None = self._rp.poll()
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

    @property
    def thread(self) -> threading.Thread | None:
        return self._thread


class _RunningProcessLineIterator(ContextManager[Iterator[str]], Iterator[str]):
    """Context-managed iterator over a RunningProcess's output lines.

    Yields only strings (never None). Stops on EndOfStream or when a per-line
    timeout elapses.
    """

    def __init__(self, rp: "RunningProcess", timeout: float | None) -> None:
        self._rp = rp
        self._timeout = timeout

    # Context manager protocol
    def __enter__(self) -> "_RunningProcessLineIterator":
        return self

    def __exit__(
        self,
        exc_type: type[BaseException] | None,
        exc: BaseException | None,
        tb: Any | None,
    ) -> bool:
        # Do not suppress exceptions
        return False

    # Iterator protocol
    def __iter__(self) -> Iterator[str]:
        return self

    def __next__(self) -> str:
        next_item: str | EndOfStream = self._rp.get_next_line(timeout=self._timeout)

        if isinstance(next_item, EndOfStream):
            raise StopIteration

        # Must be a string by contract
        return next_item


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
        shell: bool | None = None,
        timeout: int | None = None,  # None means no global timeout
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
            timeout (int | None): Timeout in seconds for process execution. None disables the global timeout.
            enable_stack_trace (bool): If True, dump stack trace when process times out.
            on_complete (Callable[[], None] | None): Callback function to execute when process completes.
            output_formatter (OutputFormatter | None): Optional formatter for processing output lines.
            shell (bool | None): If None, infer from command type.
        """
        if shell is None:
            # Default: use shell only when given a string, or when a list includes shell metachars
            if isinstance(command, str):
                shell = True
            elif isinstance(command, list):
                shell_meta = {"&&", "||", "|", ";", ">", "<", "2>", "&"}
                shell = any(part in shell_meta for part in command)
            else:
                shell = False
        self.command = command
        self.shell: bool = shell
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
        self._time_last_stdout_line: float | None = None
        self._termination_notified: bool = False
        if auto_run:
            self.run()

    def get_command_str(self) -> str:
        return (
            subprocess.list2cmdline(self.command)
            if isinstance(self.command, list)
            else self.command
        )

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
        shell = self.shell
        popen_command: str | list[str]
        if shell and isinstance(self.command, list):
            # Convert list to a single shell string with proper quoting
            popen_command = subprocess.list2cmdline(self.command)
        else:
            popen_command = self.command

        self.proc = subprocess.Popen(
            popen_command,
            shell=shell,
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

        # Output formatter lifecycle is managed by ProcessOutputReader

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
                # Track time of last stdout line observed
                self._time_last_stdout_line = time.time()
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

        # Start watcher thread via helper class and expose thread for compatibility
        self._watcher = ProcessWatcher(self)
        self._watcher.start()
        self.watcher_thread = self._watcher.thread

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
        assert self.proc is not None

        # Fast non-blocking path: honor timeout==0 by peeking before raising
        if timeout == 0:
            # Peek EOS without consuming
            with self.output_queue.mutex:
                if len(self.output_queue.queue) > 0:
                    head = self.output_queue.queue[0]
                    if isinstance(head, EndOfStream):
                        return EndOfStream()
            # Try immediate get
            try:
                item_nb: str | EndOfStream = self.output_queue.get_nowait()
                if isinstance(item_nb, EndOfStream):
                    with self.output_queue.mutex:
                        self.output_queue.queue.appendleft(item_nb)
                    return EndOfStream()
                return item_nb
            except queue.Empty:
                if self.finished:
                    return EndOfStream()
                raise TimeoutError("Timeout after 0 seconds")

        expired_time = time.time() + timeout if timeout is not None else None

        while True:
            if expired_time is not None and time.time() > expired_time:
                raise TimeoutError(f"Timeout after {timeout} seconds")

            # Peek without popping if EndOfStream is at the front
            with self.output_queue.mutex:
                if len(self.output_queue.queue) > 0:
                    head = self.output_queue.queue[0]
                    if isinstance(head, EndOfStream):
                        return EndOfStream()

            # Nothing available yet; wait briefly in blocking mode
            if self.output_queue.empty():
                time.sleep(0.01)
                if self.finished and self.output_queue.empty():
                    return EndOfStream()
                continue

            try:
                # Safe to pop now; head is not EndOfStream
                item: str | EndOfStream = self.output_queue.get(timeout=0.1)
                if isinstance(item, EndOfStream):
                    # In rare race conditions, EndOfStream could appear after peek; do not consume
                    with self.output_queue.mutex:
                        self.output_queue.queue.appendleft(item)
                    return EndOfStream()
                return item
            except queue.Empty:
                if self.finished:
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
        # Peek without consuming EndOfStream
        try:
            line: str | EndOfStream = self.get_next_line(timeout=0)
            return line
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
            cmd_str = self.get_command_str()
            if self.timeout is not None and (time.time() - start_time) > self.timeout:
                # Process is taking too long, dump stack trace if enabled
                if self.enable_stack_trace:
                    print(
                        f"\nProcess timeout after {self.timeout} seconds, dumping stack trace..."
                    )

                    print(f"Command: {cmd_str}")
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
                print(f"Killing timed out process: {cmd_str}")
                self.kill()
                raise TimeoutError(
                    f"Process timed out after {self.timeout} seconds: {cmd_str}"
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

        # Output formatter end is handled by ProcessOutputReader

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
        except KeyboardInterrupt:
            print("Keyboard interrupt detected, interrupting main thread")
            _thread.interrupt_main()
            try:
                self.proc.kill()
            except (ProcessLookupError, PermissionError, OSError, ValueError) as e:
                print(f"Warning: Failed to kill process tree for {self.proc.pid}: {e}")
                pass
            raise
        except Exception as e:
            # Fallback to simple kill if tree kill fails
            print(f"Warning: Failed to kill process tree for {self.proc.pid}: {e}")
            try:
                self.proc.kill()
            except (ProcessLookupError, PermissionError, OSError, ValueError):
                pass  # Process might already be dead

        # Wait for reader thread to finish
        if self.reader_thread is not None:
            self.reader_thread.join(timeout=0.05)  # 50ms should be plenty for cleanup

        # # Drain any remaining output
        # while True:
        #     try:
        #         line = self.output_queue.get_nowait()
        #         if line is None:  # End of output marker
        #             break
        #     except queue.Empty:
        #         break

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

    def line_iter(self, timeout: float | None) -> _RunningProcessLineIterator:
        """Return a context-managed iterator over output lines.

        Args:
            timeout: Per-line timeout in seconds. None waits indefinitely for each line.

        Returns:
            A context-managed iterator yielding non-empty, transformed stdout lines.
        """
        return _RunningProcessLineIterator(self, timeout)


# NOTE: RunningProcessManager and its singleton live in ci/util/running_process_manager.py


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
