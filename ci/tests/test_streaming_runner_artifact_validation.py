"""Tests for streaming-runner artifact validation (FastLED #3011).

`validate_test_artifact` is the gate that catches the false-pass /
false-fail mode where Ninja's "Linking <X>" announcement submits a
test to the runner, the link then fails (e.g. zccache daemon crash
under the parallel-link storm), and the runner loads either a
missing DLL (clean build) or a stale one from a previous build.
"""

import time
import unittest
from pathlib import Path
from tempfile import TemporaryDirectory

from ci.meson.streaming import TestResult  # noqa: E402
from ci.meson.streaming_runner import validate_test_artifact  # noqa: E402


class TestValidateTestArtifact(unittest.TestCase):
    """`validate_test_artifact` returns None on success, TestResult on failure."""

    def test_fresh_dll_returns_none(self) -> None:
        """A DLL whose mtime is newer than build start is accepted."""
        with TemporaryDirectory() as tmp:
            dll = Path(tmp) / "fl_audio.dll"
            dll.write_bytes(b"fake")
            build_start = dll.stat().st_mtime - 10.0  # 10 s before write
            result = validate_test_artifact(dll, build_start)
            self.assertIsNone(result)

    def test_missing_dll_returns_failure(self) -> None:
        """A nonexistent path produces a TestResult(success=False)."""
        with TemporaryDirectory() as tmp:
            dll = Path(tmp) / "fl_audio.dll"  # never created
            build_start = time.time()
            result = validate_test_artifact(dll, build_start)
            self.assertIsNotNone(result)
            assert isinstance(result, TestResult)
            self.assertFalse(result.success)
            self.assertIn("Test artifact missing", result.output)
            self.assertIn(str(dll), result.output)
            self.assertIn("#3011", result.output)

    def test_stale_dll_returns_failure(self) -> None:
        """A DLL with mtime older than build_start_time fails validation."""
        with TemporaryDirectory() as tmp:
            dll = Path(tmp) / "fl_audio.dll"
            dll.write_bytes(b"old")
            dll_mtime = dll.stat().st_mtime
            build_start = dll_mtime + 100.0  # 100 s AFTER the file was written
            result = validate_test_artifact(dll, build_start)
            self.assertIsNotNone(result)
            assert isinstance(result, TestResult)
            self.assertFalse(result.success)
            self.assertIn("Stale test artifact", result.output)
            self.assertIn(str(dll), result.output)
            self.assertIn("#3011", result.output)

    def test_exactly_at_build_start_is_accepted(self) -> None:
        """A DLL with mtime == build_start (no slop) passes the check.

        The validator uses ``<`` not ``<=``, so artifacts written at
        the exact tick the build started are treated as fresh. This
        matches reality on filesystems with coarse mtime resolution
        (Windows FAT32 is 2 s; ext3 was 1 s).
        """
        with TemporaryDirectory() as tmp:
            dll = Path(tmp) / "fl_audio.dll"
            dll.write_bytes(b"fresh")
            build_start = dll.stat().st_mtime  # exactly equal
            result = validate_test_artifact(dll, build_start)
            self.assertIsNone(result)

    def test_failure_messages_are_actionable(self) -> None:
        """The failure messages include both the path and a remediation hint."""
        with TemporaryDirectory() as tmp:
            dll = Path(tmp) / "fl_audio_reactive.dll"
            build_start = time.time()

            # Missing case
            missing = validate_test_artifact(dll, build_start)
            assert isinstance(missing, TestResult)
            self.assertIn("re-run", missing.output.lower())

            # Stale case
            dll.write_bytes(b"old")
            stale = validate_test_artifact(dll, dll.stat().st_mtime + 5.0)
            assert isinstance(stale, TestResult)
            self.assertIn("zccache stop", stale.output)


if __name__ == "__main__":
    unittest.main()
