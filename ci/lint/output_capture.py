"""Thread-safe output buffering for parallel lint stages."""

import sys
import threading
from io import StringIO
from typing import TextIO


class OutputCapture:
    """
    Captures stdout/stderr for a thread-safe lint stage.

    Used to buffer output during parallel execution and replay it later.
    """

    def __init__(self) -> None:
        self.buffer = StringIO()
        self._original_stdout: TextIO | None = None
        self._original_stderr: TextIO | None = None
        self._lock = threading.Lock()

    def __enter__(self) -> "OutputCapture":
        """Start capturing output."""
        with self._lock:
            self._original_stdout = sys.stdout
            self._original_stderr = sys.stderr
            # Note: We don't actually redirect here because parallel stages
            # write to temp files instead. This is here for future flexibility.
        return self

    def __exit__(self, exc_type, exc_val, exc_tb) -> None:  # type: ignore
        """Stop capturing output."""
        with self._lock:
            if self._original_stdout is not None:
                sys.stdout = self._original_stdout
            if self._original_stderr is not None:
                sys.stderr = self._original_stderr

    def write(self, text: str) -> None:
        """Write text to the buffer."""
        with self._lock:
            self.buffer.write(text)

    def getvalue(self) -> str:
        """Get the captured output."""
        with self._lock:
            return self.buffer.getvalue()

    def clear(self) -> None:
        """Clear the buffer."""
        with self._lock:
            self.buffer = StringIO()
