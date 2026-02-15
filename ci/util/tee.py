"""
StreamTee utility for capturing subprocess output to both file and console.

This module provides a thread-safe way to capture compilation output to log files
while simultaneously displaying it to the console. Inspired by the OutputCollector
pattern in serial_monitor.py.
"""

import threading
from pathlib import Path
from typing import Optional


class StreamTee:
    """
    Thread-safe output capture to both file and console.

    Captures subprocess output (stdout + stderr merged) to a file while
    optionally echoing to console. Designed for capturing compilation errors.

    Example:
        tee = StreamTee(Path("build.log"), echo=True)
        tee.write_line("Compiling main.cpp...")
        tee.write_line("Error: undefined reference to `foo`")
        tee.write_footer(1)  # Exit code 1
        tee.close()
    """

    def __init__(self, file_path: Path, echo: bool = True):
        """
        Initialize StreamTee.

        Args:
            file_path: Path to write captured output
            echo: Whether to echo lines to console (default True)
        """
        self._file_path = file_path
        self._echo = echo
        self._lock = threading.Lock()

        # Ensure parent directory exists
        file_path.parent.mkdir(parents=True, exist_ok=True)

        # Open file with buffered I/O for performance
        self._file_handle = open(
            file_path,
            "w",
            encoding="utf-8",
            buffering=8192,  # 8KB buffer
        )

    def write_line(self, line: str) -> None:
        """
        Write a line to both file and (optionally) console.

        Args:
            line: Line to write (newline will be appended)
        """
        with self._lock:
            # Write to file
            self._file_handle.write(line + "\n")

            # Echo to console if enabled
            if self._echo:
                print(line)

    def write_footer(self, exit_code: int) -> None:
        """
        Write standardized footer with exit code.

        Format:
            +++++++
            exit code N

        Args:
            exit_code: Process exit code
        """
        with self._lock:
            self._file_handle.write("+++++++\n")
            self._file_handle.write(f"exit code {exit_code}\n")

    def close(self) -> None:
        """Flush and close the file handle."""
        with self._lock:
            self._file_handle.flush()
            self._file_handle.close()

    @property
    def file_path(self) -> Path:
        """Get the path to the log file."""
        return self._file_path
