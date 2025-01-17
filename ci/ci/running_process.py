import subprocess
import threading
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
    ):
        """
        Initialize the RunningProcess instance. Note that stderr is merged into stdout!!

        Args:
            command (str): The command to execute.
            cwd (Path | None): The working directory to execute the command in.
            check (bool): If True, raise an exception if the command returns a non-zero exit code.
            auto_run (bool): If True, automatically run the command when the instance is created.
            echo (bool): If True, print the output of the command to the console in real-time.
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
        self.reader_thread: threading.Thread | None = None
        self.shutdown: threading.Event = threading.Event()
        if auto_run:
            self.run()

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
            text=True,  # Automatically decode bytes to str
        )

        def output_reader():
            try:
                assert self.proc.stdout is not None
                for line in iter(self.proc.stdout.readline, ""):
                    if self.shutdown.is_set():
                        break
                    line = line.rstrip()
                    if self.echo:
                        print(line)  # Print to console in real time
                    self.buffer.append(line)
            finally:
                if self.proc.stdout:
                    self.proc.stdout.close()

        # Start output reader thread
        self.reader_thread = threading.Thread(target=output_reader, daemon=True)
        self.reader_thread.start()

    def wait(self) -> int:
        """
        Wait for the process to complete.

        Raises:
            ValueError: If the process hasn't been started.
        """
        if self.proc is None:
            raise ValueError("Process is not running.")
        rtn = self.proc.wait()
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
