"""Tests for streaming-runner artifact validation (FastLED #3011).

`validate_test_artifact` is the defense-in-depth gate for missing or stale
artifacts. Execution waits for the full build to finish (FastLED #3642), and
the validator catches anomalous cache/tool output before a runner loads it.
"""

import concurrent.futures
import os
import threading
import time
import unittest
from contextlib import nullcontext
from pathlib import Path
from tempfile import TemporaryDirectory
from typing import Any, Callable, TypeVar
from unittest.mock import MagicMock, patch

from ci.meson.streaming import (  # noqa: E402
    CompileOnlyResult,
    _wait_for_dll_touch,
    stream_compile_and_run_tests,
    stream_compile_only,
)
from ci.meson.streaming import (
    TestResult as StreamingTestResult,
)
from ci.meson.streaming_runner import validate_test_artifact  # noqa: E402


_T = TypeVar("_T")


class _ImmediateExecutor:
    """Deterministic executor for testing compile/run phase ordering."""

    def __init__(self, max_workers: int) -> None:
        self.max_workers = max_workers

    def submit(
        self, fn: Callable[..., _T], /, *args: Any, **kwargs: Any
    ) -> concurrent.futures.Future[_T]:
        future: concurrent.futures.Future[_T] = concurrent.futures.Future()
        future.set_result(fn(*args, **kwargs))
        return future

    def shutdown(self, wait: bool = True, *, cancel_futures: bool = False) -> None:
        pass


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
            assert isinstance(result, StreamingTestResult)
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
            assert isinstance(result, StreamingTestResult)
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
            assert isinstance(missing, StreamingTestResult)
            self.assertIn("re-run", missing.output.lower())

            # Stale case
            dll.write_bytes(b"old")
            stale = validate_test_artifact(dll, dll.stat().st_mtime + 5.0)
            assert isinstance(stale, StreamingTestResult)
            self.assertIn("zccache stop", stale.output)


class TestStreamingExecutionCoordination(unittest.TestCase):
    """Streaming execution starts only after a successful compile phase."""

    def test_dll_touch_wait_is_bounded(self) -> None:
        """A stalled optimizer reports failure instead of waiting forever."""
        done = threading.Event()

        self.assertFalse(_wait_for_dll_touch(done, timeout=0.0))
        done.set()
        self.assertTrue(_wait_for_dll_touch(done, timeout=0.0))

    def test_dll_touch_timeout_is_reported_in_compile_output(self) -> None:
        """Timeout diagnostics propagate to callers and persisted failure logs."""
        process = MagicMock()
        process.line_iter.return_value = nullcontext(iter(()))
        process.wait.return_value = 0
        process.stdout = "ninja completed"

        with (
            TemporaryDirectory() as tmp,
            patch("ci.meson.streaming.get_meson_executable", return_value="meson"),
            patch("ci.meson.streaming.kill_stale_runner_processes", return_value=0),
            patch("ci.meson.streaming.RunningProcess", return_value=process),
            patch("ci.meson.streaming._wait_for_dll_touch", return_value=False),
        ):
            result = stream_compile_only(Path(tmp) / "build", compile_timeout=1)

        self.assertFalse(result.success)
        self.assertIn(
            "DLL mtime optimization timed out after 1.0s", result.compile_output
        )

    def test_test_starts_after_compile_completes(self) -> None:
        """A pre-link status callback must not start a test process."""
        test_path = Path("build/tests/example.dylib")
        compile_finished = False
        observed_compile_states: list[bool] = []

        def fake_compile_only(*args: object, **kwargs: object) -> CompileOnlyResult:
            # The pre-fix implementation supplied this removed kwarg. Keep the
            # compatibility hook so this regression remains RED on old code.
            callback = kwargs.get("on_test_compiled")
            if callable(callback):
                callback(test_path)

            nonlocal compile_finished
            compile_finished = True
            return CompileOnlyResult(success=True, compiled_tests=[test_path])

        def test_callback(path: Path) -> StreamingTestResult:
            self.assertEqual(test_path, path)
            observed_compile_states.append(compile_finished)
            return StreamingTestResult(success=True)

        with (
            patch(
                "ci.meson.streaming.stream_compile_only", side_effect=fake_compile_only
            ),
            patch(
                "ci.meson.streaming.concurrent.futures.ThreadPoolExecutor",
                side_effect=_ImmediateExecutor,
            ),
        ):
            result = stream_compile_and_run_tests(Path("build"), test_callback)

        self.assertTrue(result.success)
        self.assertEqual([True], observed_compile_states)

    def test_failed_compile_runs_no_tests(self) -> None:
        """Artifacts mentioned before a failed link must never execute."""
        test_path = Path("build/tests/stale.dylib")
        executed_paths: list[Path] = []

        def fake_compile_only(*args: object, **kwargs: object) -> CompileOnlyResult:
            # Exercise the pre-fix early-execution path when run against it.
            callback = kwargs.get("on_test_compiled")
            if callable(callback):
                callback(test_path)
            return CompileOnlyResult(success=False, compiled_tests=[test_path])

        def test_callback(path: Path) -> StreamingTestResult:
            executed_paths.append(path)
            return StreamingTestResult(success=True)

        with (
            patch(
                "ci.meson.streaming.stream_compile_only", side_effect=fake_compile_only
            ),
            patch(
                "ci.meson.streaming.concurrent.futures.ThreadPoolExecutor",
                side_effect=_ImmediateExecutor,
            ),
        ):
            result = stream_compile_and_run_tests(Path("build"), test_callback)

        self.assertFalse(result.success)
        self.assertEqual([], executed_paths)

    def test_no_parallel_uses_one_worker(self) -> None:
        """NO_PARALLEL must constrain the streaming executor to one worker."""
        worker_counts: list[int] = []

        def capture_executor(
            max_workers: int,
        ) -> _ImmediateExecutor:
            worker_counts.append(max_workers)
            return _ImmediateExecutor(max_workers=max_workers)

        with (
            patch.dict(os.environ, {"NO_PARALLEL": "1"}),
            patch("ci.util.cpu_count.os.cpu_count", return_value=3),
            patch(
                "ci.meson.streaming.stream_compile_only",
                return_value=CompileOnlyResult(
                    success=True,
                    compiled_tests=[Path("build/tests/example.dylib")],
                ),
            ),
            patch(
                "ci.meson.streaming.concurrent.futures.ThreadPoolExecutor",
                side_effect=capture_executor,
            ),
        ):
            result = stream_compile_and_run_tests(
                Path("build"), lambda _: StreamingTestResult(success=True)
            )

        self.assertTrue(result.success)
        self.assertEqual([1], worker_counts)


if __name__ == "__main__":
    unittest.main()
