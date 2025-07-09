import os
import subprocess
import tempfile
import threading
import time
from pathlib import Path


class RunningProcess:
    """
    A class to manage and stream output from a running subprocess.

    This class provides functionality to execute shell commands, stream their output
    in real-time, and control the subprocess execution.
    """

    def __init__(
        self,
        command: str | list[str],
        cwd: Path | None = None,
        check: bool = False,
        auto_run: bool = True,
        echo: bool = True,
        timeout: int = 300,  # 5 minute default timeout
        enable_stack_trace: bool = True,  # Enable stack trace dumping on timeout
    ):
        """
        Initialize the RunningProcess instance. Note that stderr is merged into stdout!!

        Args:
            command (str): The command to execute.
            cwd (Path | None): The working directory to execute the command in.
            check (bool): If True, raise an exception if the command returns a non-zero exit code.
            auto_run (bool): If True, automatically run the command when the instance is created.
            echo (bool): If True, print the output of the command to the console in real-time.
            timeout (int): Timeout in seconds for process execution. Default 300 seconds (5 minutes).
            enable_stack_trace (bool): If True, dump stack trace when process times out.
        """
        if isinstance(command, list):
            command = subprocess.list2cmdline(command)
        self.command = command
        self.cwd = str(cwd) if cwd is not None else None
        self.buffer: list[str] = []
        self.proc: subprocess.Popen | None = None
        self.check = check
        self.auto_run = auto_run
        self.echo = echo
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
        Execute the command and stream its output in real-time.

        Returns:
            str: The full output of the command.

        Raises:
            subprocess.CalledProcessError: If the command returns a non-zero exit code.
        """

        self.proc = subprocess.Popen(
            self.command,
            shell=True,
            cwd=self.cwd,
            bufsize=256,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # Merge stderr into stdout
            text=False,  # Automatically decode bytes to str
        )

        def output_reader():
            try:
                assert self.proc is not None
                assert self.proc.stdout is not None
                line: bytes
                for line in iter(self.proc.stdout.readline, b""):
                    if self.shutdown.is_set():
                        break
                    linestr = line.decode("utf-8", errors="ignore")
                    linestr = linestr.rstrip()
                    if self.echo:
                        print(linestr)  # Print to console in real time
                    self.buffer.append(linestr)
            finally:
                if self.proc and self.proc.stdout:
                    self.proc.stdout.close()

        # Start output reader thread
        self.reader_thread = threading.Thread(target=output_reader, daemon=True)
        self.reader_thread.start()

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
            time.sleep(0.1)  # Check every 100ms

        rtn = self.proc.returncode
        assert self.reader_thread is not None
        self.reader_thread.join(timeout=1)
        return rtn

    def kill(self) -> None:
        """
        Immediately terminate the process with SIGKILL.

        Raises:
            ValueError: If the process hasn't been started.
        """
        if self.proc is None:
            return
        self.shutdown.set()
        self.proc.kill()

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
            str: The complete stdout output as a string, or None if process hasn't completed.
        """
        self.wait()
        return "\n".join(self.buffer)
