import subprocess
from pathlib import Path


class RunningProcess:
    def __init__(
        self,
        command: str,
        cwd: Path | None = None,
        check: bool = False,
        auto_run: bool = True,
        echo: bool = True,
    ):
        """
        Initialize the RunningProcess instance.

        Args:
            command (str): The command to execute.
            cwd (Path | None): The working directory to execute the command in.
        """
        self.command = command
        self.cwd = str(cwd) if cwd is not None else None
        self.buffer = []
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
        """
        if self.proc is None:
            raise ValueError("Process is not running.")
        self.proc.wait()

    def kill(self) -> None:
        """
        Terminate the process.
        """
        if self.proc is None:
            raise ValueError("Process is not running.")
        self.proc.kill()

    def terminate(self) -> None:
        """
        Terminate the process.
        """
        if self.proc is None:
            raise ValueError("Process is not running.")
        self.proc.terminate()

    @property
    def returncode(self) -> int:
        if self.proc is None:
            raise ValueError("Process is not running.")
        return self.proc.returncode

    @property
    def stdout(self) -> str:
        return "\n".join(self.buffer) if self.returncode is not None else None
