# pyright: reportUnknownMemberType=false, reportMissingParameterType=false
import os
import queue
import re
import subprocess
import sys
import tempfile
import threading
import time
import urllib.parse
from pathlib import Path
from queue import Queue
from typing import Any


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

    def normalize_path_match(match):
        path_str = match.group(0)
        try:
            # Use pathlib to normalize the path separators for the current OS
            normalized = str(
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

    def resolve_path_match(match):
        try:
            full_path = match.group(0)
            # Use pathlib to resolve the path
            resolved = str(Path(full_path).resolve())
            return resolved
        except (ValueError, OSError):
            # If resolution fails, return original
            return match.group(0)

    return re.sub(relative_pattern, resolve_path_match, text)


# Configure console for UTF-8 output on Windows
if os.name == "nt":  # Windows
    # Try to set console to UTF-8 mode
    try:
        # Set stdout and stderr to UTF-8 encoding
        # Note: reconfigure() was added in Python 3.7
        if hasattr(sys.stdout, "reconfigure") and callable(
            getattr(sys.stdout, "reconfigure", None)
        ):
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[attr-defined]
        if hasattr(sys.stderr, "reconfigure") and callable(
            getattr(sys.stderr, "reconfigure", None)
        ):
            sys.stderr.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[attr-defined]
    except (AttributeError, OSError):
        # Fallback for older Python versions or if reconfigure fails
        pass


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
        timeout: int = 60,  # sixty seconds
        enable_stack_trace: bool = True,  # Enable stack trace dumping on timeout
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
        """
        if isinstance(command, list):
            command = subprocess.list2cmdline(command)
        self.command = command
        self.cwd = str(cwd) if cwd is not None else None
        self.output_queue: Queue[str | None] = Queue()
        self.proc: subprocess.Popen[Any] | None = None
        self.check = check
        self.auto_run = auto_run
        self.timeout = timeout
        self.enable_stack_trace = enable_stack_trace
        self.reader_thread: threading.Thread | None = None
        self.shutdown: threading.Event = threading.Event()
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

    def run(self) -> None:
        """
        Execute the command and stream its output to the queue.

        Raises:
            subprocess.CalledProcessError: If the command returns a non-zero exit code.
        """
        self._start_time = time.time()  # Track when the process started
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

        def output_reader():
            """Continuously pump stdout to prevent subprocess from blocking on full pipe buffer"""
            try:
                assert self.proc is not None
                assert self.proc.stdout is not None

                # Continuously read lines to keep the stdout pipe drained
                # This prevents the subprocess from blocking when its output buffer fills
                try:
                    for line in iter(self.proc.stdout.readline, ""):
                        if self.shutdown.is_set():
                            break

                        # Strip whitespace and queue non-empty lines
                        line_stripped = line.rstrip()
                        if line_stripped:  # Only queue non-empty lines
                            self.output_queue.put(line_stripped)
                except (ValueError, OSError) as e:
                    # Handle "I/O operation on closed file" and similar errors
                    # This can happen if the process terminates while we're reading
                    if "closed file" in str(e) or "Bad file descriptor" in str(e):
                        # Normal shutdown - process stdout was closed
                        pass
                    else:
                        # Unexpected error, log it but don't crash
                        print(f"Warning: Output reader encountered error: {e}")

            finally:
                # Clean shutdown: close stream and signal end
                if self.proc and self.proc.stdout and not self.proc.stdout.closed:
                    try:
                        self.proc.stdout.close()
                    except (ValueError, OSError):
                        # Already closed, ignore
                        pass
                self.output_queue.put(None)  # End-of-stream marker

        # Start output reader thread
        self.reader_thread = threading.Thread(target=output_reader, daemon=True)
        self.reader_thread.start()

    def get_next_line(self, timeout: float | None = None) -> str | None:
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
        # Check if process has finished and queue is empty
        if self.proc is not None and self.proc.poll() is not None:
            try:
                # Try to get any remaining output
                line = self.output_queue.get_nowait()
                if line is None:
                    # End of output marker
                    return None
                return line
            except queue.Empty:
                return None

        # Process still running or queue not empty
        try:
            line = self.output_queue.get(timeout=timeout)
            if line is None and self.proc is not None and self.proc.poll() is not None:
                # Process finished and we got end marker
                return None
            return line
        except queue.Empty:
            if (
                self.proc is not None
                and time.time() - getattr(self, "_start_time", 0) > self.timeout
            ):
                raise TimeoutError(f"Process timed out after {self.timeout} seconds")
            raise

    def poll(self) -> int | None:
        """
        Check the return code of the process.
        """
        return self.proc.poll()

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
        while self.proc.poll() is None:
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

        # Wait for reader thread to finish and cleanup
        if self.reader_thread is not None:
            self.reader_thread.join(
                timeout=0.05
            )  # 50ms should be plenty for thread cleanup
            if self.reader_thread.is_alive():
                # Reader thread didn't finish, force shutdown
                self.shutdown.set()
                self.reader_thread.join(timeout=0.05)  # 50ms for forced shutdown

        # Drain any remaining output
        while True:
            try:
                line = self.output_queue.get_nowait()
                if line is None:  # End of output marker
                    break
            except queue.Empty:
                break

        return rtn

    def kill(self) -> None:
        """
        Immediately terminate the process with SIGKILL and all child processes.

        Raises:
            ValueError: If the process hasn't been started.
        """
        if self.proc is None:
            return

        # Signal reader thread to stop
        self.shutdown.set()

        # Kill the entire process tree (parent + all children)
        # This prevents orphaned clang++ processes from hanging the system
        try:
            from ci.ci.test_env import kill_process_tree

            kill_process_tree(self.proc.pid)
        except Exception as e:
            # Fallback to simple kill if tree kill fails
            print(f"Warning: Failed to kill process tree for {self.proc.pid}: {e}")
            try:
                self.proc.kill()
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
    def stdout(self) -> str:
        """
        Get the complete stdout output of the process.

        Returns:
            str: The complete stdout output as a string.
        """
        # Wait for process to complete
        self.wait()

        # Collect all lines from the queue
        lines: list[str] = []
        while True:
            try:
                line = self.output_queue.get_nowait()
                if line is None:  # End of output marker
                    break
                lines.append(line)
            except queue.Empty:
                break

        return "\n".join(lines)
