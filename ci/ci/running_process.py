import subprocess
from pathlib import Path


class RunningProcess:
    """
    A class to manage and stream output from a running subprocess.

    This class provides functionality to execute shell commands, stream their output
    in real-time, and control the subprocess execution.
    """

    def __init__(
        self,
        command: str,
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
        self.command = command
        self.cwd = str(cwd) if cwd is not None else None
        self.buffer: list[str] = []
        self.proc: subprocess.Popen | None = None
        self.check = check
        self.auto_run = auto_run
        self.echo = echo
        if auto_run:
            self.run()

    def run(self) -> str:
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
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # Merge stderr into stdout
            text=True,  # Automatically decode bytes to str
        )

        try:
            # Stream output line by line
            assert self.proc.stdout is not None
            for line in iter(self.proc.stdout.readline, ""):
                line = line.rstrip()
                if self.echo:
                    print(line)  # Print to console in real time
                self.buffer.append(line)

            # Wait for the process to complete
            self.proc.wait()
        finally:
            if self.proc.stdout:
                self.proc.stdout.close()

        if self.proc.returncode != 0:
            if self.check:
                raise subprocess.CalledProcessError(
                    self.proc.returncode, self.command, output=self.stdout
                )

        return "\n".join(self.buffer)

    def wait(self) -> None:
        """
        Wait for the process to complete.

        Raises:
            ValueError: If the process hasn't been started.
        """
        if self.proc is None:
            raise ValueError("Process is not running.")
        self.proc.wait()

    def kill(self) -> None:
        """
        Immediately terminate the process with SIGKILL.

        Raises:
            ValueError: If the process hasn't been started.
        """
        if self.proc is None:
            raise ValueError("Process is not running.")
        self.proc.kill()

    def terminate(self) -> None:
        """
        Gracefully terminate the process with SIGTERM.

        Raises:
            ValueError: If the process hasn't been started.
        """
        if self.proc is None:
            raise ValueError("Process is not running.")
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
