"""Basic unittest for ci.util.running_process.RunningProcess.

This test executes a trivial Python command via `uv run python -c` and verifies:
- The process exits successfully (return code 0)
- The streamed output contains the expected line
"""

import time
import unittest
from pathlib import Path

import pytest
from running_process import RunningProcess
from running_process.process_output_reader import EndOfStream


@pytest.mark.serial
class TestRunningProcess(unittest.TestCase):
    def test_sanity(self: "TestRunningProcess") -> None:
        """Run a trivial command and validate output streaming and exit code.

        Uses `uv run python -c "print('hello')"` to ensure we respect the
        repository rule that all Python execution goes through `uv run python`.
        """

        command: list[str] = [
            "uv",
            "run",
            "python",
            "-c",
            "print('hello')",
        ]

        rp: RunningProcess = RunningProcess(
            command=command,
            cwd=Path(".").absolute(),
            check=False,
            auto_run=True,
            timeout=30,
            on_complete=None,
            output_formatter=None,
        )

        captured_lines: list[str] = []

        while True:
            out: str | EndOfStream | None = rp.get_next_line_non_blocking()
            if isinstance(out, EndOfStream):
                break
            if isinstance(out, str):
                captured_lines.append(out)
            else:
                time.sleep(0.01)

        rc: int = rp.wait()
        self.assertEqual(rc, 0)

        combined: str = "\n".join(captured_lines).strip()
        self.assertIn("hello", combined)

    def test_line_iter_basic(self: "TestRunningProcess") -> None:
        """Validate context-managed line iteration yields only strings and completes."""

        command: list[str] = [
            "uv",
            "run",
            "python",
            "-c",
            "print('a'); print('b'); print('c')",
        ]

        rp: RunningProcess = RunningProcess(
            command=command,
            cwd=Path(".").absolute(),
            check=False,
            auto_run=True,
            timeout=10,
            on_complete=None,
            output_formatter=None,
        )

        iter_lines: list[str] = []
        with rp.line_iter(timeout=60) as it:
            for ln in it:
                # Should always be a string, never None
                self.assertIsInstance(ln, str)
                iter_lines.append(ln)

        # Process should have finished; ensure exit success
        rc: int = rp.wait()
        self.assertEqual(rc, 0)
        self.assertEqual(iter_lines, ["a", "b", "c"])


class _UpperFormatter:
    """Simple OutputFormatter that records begin/end calls and uppercases lines."""

    def __init__(self) -> None:
        self.begin_called: bool = False
        self.end_called: bool = False

    def begin(self) -> None:
        self.begin_called = True

    def transform(self, line: str) -> str:
        return line.upper()

    def end(self) -> None:
        self.end_called = True


@pytest.mark.serial
class TestRunningProcessAdditional(unittest.TestCase):
    def test_timeout_and_kill(self: "TestRunningProcessAdditional") -> None:
        """Process exceeding timeout should be killed and raise TimeoutError."""

        command: list[str] = [
            "uv",
            "run",
            "python",
            "-c",
            "import time; time.sleep(999)",
        ]

        rp: RunningProcess = RunningProcess(
            command=command,
            cwd=Path(".").absolute(),
            check=False,
            auto_run=True,
            timeout=1,
            on_complete=None,
            output_formatter=None,
        )

        # Do not block on output; wait should time out quickly
        with self.assertRaises(TimeoutError):
            _ = rp.wait()

        # After timeout, process should be finished
        self.assertTrue(rp.finished)

        # EndOfStream should be delivered shortly after
        end_seen: bool = False
        deadline: float = time.time() + 2.0
        while time.time() < deadline:
            nxt: str | EndOfStream | None = rp.get_next_line_non_blocking()
            if isinstance(nxt, EndOfStream):
                end_seen = True
                break
            time.sleep(0.01)
        self.assertTrue(end_seen)

    def test_output_formatter(self: "TestRunningProcessAdditional") -> None:
        """Output formatter hooks are invoked and transform is applied; blanks ignored."""

        formatter = _UpperFormatter()

        command: list[str] = [
            "uv",
            "run",
            "python",
            "-c",
            "print(); print('hello'); print('world')",
        ]

        rp: RunningProcess = RunningProcess(
            command=command,
            cwd=Path(".").absolute(),
            check=False,
            auto_run=True,
            timeout=30,  # Increased from 10 to match test_sanity; uv startup can be slow in CI
            on_complete=None,
            output_formatter=formatter,
        )

        # Drain output (optional; accumulated_output records lines regardless)
        while True:
            line: str | EndOfStream | None = rp.get_next_line_non_blocking()
            if isinstance(line, EndOfStream):
                break
            if line is None:
                time.sleep(0.005)
                continue

        rc: int = rp.wait()
        self.assertEqual(rc, 0)

        # Verify formatter begin/end were called
        self.assertTrue(formatter.begin_called)
        self.assertTrue(formatter.end_called)

        # Verify transformed, non-empty lines only
        output_text: str = rp.stdout.strip()
        # Should contain HELLO and WORLD, but not an empty line
        self.assertIn("HELLO", output_text)
        self.assertIn("WORLD", output_text)
        # Ensure no blank-only lines exist in accumulated output
        for ln in output_text.split("\n"):
            self.assertTrue(len(ln.strip()) > 0)


if __name__ == "__main__":
    unittest.main()
