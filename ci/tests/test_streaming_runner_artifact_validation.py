"""Tests for streaming-runner artifact validation (FastLED #3011).

`validate_test_artifact` is the defense-in-depth gate for missing or stale
artifacts. Execution waits for the full build to finish (FastLED #3642), and
the validator catches anomalous cache/tool output before a runner loads it.
"""

import concurrent.futures
import json
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
    registered_test_artifacts,
    stream_compile_and_run_tests,
    stream_compile_only,
)
from ci.meson.streaming import (
    TestResult as StreamingTestResult,
)
from ci.meson.streaming_runner import (  # noqa: E402
    _test_process_timeout_seconds,
    _wait_for_test_process,
    validate_test_artifact,
)


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

    def test_registered_cached_artifact_accepts_old_mtime_but_not_missing(self) -> None:
        """A successful build permits unchanged artifacts, retaining existence checks."""
        with TemporaryDirectory() as tmp:
            artifact = Path(tmp) / "color_profile_tiny_layout"
            artifact.write_bytes(b"cached probe")
            build_start = artifact.stat().st_mtime + 100
            self.assertIsNone(
                validate_test_artifact(artifact, build_start, require_fresh=False)
            )
            self.assertIsNotNone(validate_test_artifact(artifact, build_start))
            self.assertIsNotNone(
                validate_test_artifact(
                    Path(tmp) / "missing", build_start, require_fresh=False
                )
            )

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

    def test_runner_timeout_preserves_captured_output(self) -> None:
        """Watchdog phase markers survive the outer Python timeout."""
        process = MagicMock()
        process.wait.side_effect = TimeoutError("runner stalled")
        process.stdout = "[FASTLED RUNNER] phase: dynamic-load\n"

        result = _wait_for_test_process(process)

        self.assertFalse(result.success)
        self.assertIn("runner stalled", result.output)
        self.assertIn("phase: dynamic-load", result.output)

    def test_macos_runner_timeout_is_bounded(self) -> None:
        """A pre-main Apple stall cannot consume a ten-minute worker slot."""
        with patch("ci.meson.streaming_runner.sys.platform", "darwin"):
            self.assertEqual(90, _test_process_timeout_seconds())

        with patch("ci.meson.streaming_runner.sys.platform", "linux"):
            self.assertEqual(600, _test_process_timeout_seconds())

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

    def test_complete_inventory_runs_unchanged_standalone_probe(self) -> None:
        """Full execution includes registered probes absent from Ninja output."""
        with TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            linked = build_dir / "tests" / "changed.so"
            cached = build_dir / "tests" / "color_profile_tiny_layout"
            gate = build_dir / "ci" / "meson" / "compile-tests"
            info = build_dir / "meson-info"
            info.mkdir()
            (info / "intro-tests.json").write_text(
                json.dumps(
                    [
                        {
                            "cmd": [
                                "python",
                                "test_wrapper.py",
                                str(build_dir / "tests" / "runner"),
                                str(linked),
                                "20",
                            ]
                        },
                        {"cmd": [str(cached)]},
                        {"cmd": [str(gate)]},
                    ]
                ),
                encoding="utf-8",
            )
            observed: list[Path] = []

            def callback(path: Path) -> StreamingTestResult:
                observed.append(path)
                self.assertEqual(
                    frozenset([cached, gate]),
                    getattr(callback, "_cached_test_artifacts"),
                )
                return StreamingTestResult(success=True)

            with (
                patch(
                    "ci.meson.streaming.stream_compile_only",
                    return_value=CompileOnlyResult(
                        success=True, compiled_tests=[linked]
                    ),
                ),
                patch(
                    "ci.meson.streaming.concurrent.futures.ThreadPoolExecutor",
                    side_effect=_ImmediateExecutor,
                ),
            ):
                result = stream_compile_and_run_tests(
                    build_dir, callback, target="all-with-examples"
                )
            self.assertTrue(result.success)
            self.assertEqual([linked, cached, gate], observed)
            self.assertEqual(3, result.num_passed)
            self.assertEqual(0, result.num_passed_examples)

    def test_registered_inventory_preserves_alias_and_targeted_scope(self) -> None:
        """Unit-only and example-only builds cannot execute unbuilt populations."""
        with TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            unit = build_dir / "tests" / "color_profile_tiny_layout"
            example = build_dir / "examples" / "Blink.so"
            info = build_dir / "meson-info"
            info.mkdir()
            (info / "intro-tests.json").write_text(
                json.dumps(
                    [
                        {"cmd": [str(unit)]},
                        {"cmd": ["python", "test_wrapper.py", str(example)]},
                    ]
                ),
                encoding="utf-8",
            )
            self.assertEqual([unit], registered_test_artifacts(build_dir, "all_tests"))
            self.assertEqual(
                [example], registered_test_artifacts(build_dir, "examples-host")
            )
            self.assertEqual(
                [], registered_test_artifacts(build_dir, "tests/specific.so")
            )
            self.assertEqual([], registered_test_artifacts(build_dir, None))

    def test_full_inventory_excludes_announced_and_cached_suite_artifacts(self) -> None:
        """Suite exclusions apply even when Ninja announces the excluded gate."""
        with TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            unit = build_dir / "tests" / "unit.so"
            gate = build_dir / "ci" / "meson" / "compile-tests"
            example = build_dir / "examples" / "Blink.so"
            info = build_dir / "meson-info"
            info.mkdir()
            (info / "intro-tests.json").write_text(
                json.dumps(
                    [
                        {"cmd": ["runner", str(unit)], "suite": ["fastled"]},
                        {"cmd": [str(gate)], "suite": ["fastled:compile-tests"]},
                        {
                            "cmd": ["example_runner", str(example)],
                            "suite": ["fastled:examples"],
                        },
                    ]
                ),
                encoding="utf-8",
            )
            for exclusion in ("compile-tests", "fastled:compile-tests"):
                with self.subTest(exclusion=exclusion):
                    observed: list[Path] = []

                    def callback(path: Path) -> StreamingTestResult:
                        observed.append(path)
                        return StreamingTestResult(success=True)

                    with (
                        patch(
                            "ci.meson.streaming.stream_compile_only",
                            return_value=CompileOnlyResult(
                                success=True, compiled_tests=[gate]
                            ),
                        ),
                        patch(
                            "ci.meson.streaming.concurrent.futures.ThreadPoolExecutor",
                            side_effect=_ImmediateExecutor,
                        ),
                    ):
                        result = stream_compile_and_run_tests(
                            build_dir,
                            callback,
                            target="all_tests",
                            exclude_suites=[exclusion],
                        )
                    self.assertTrue(result.success)
                    self.assertEqual([unit], observed)
                    self.assertEqual(1, result.num_passed)

    def test_default_target_retains_only_selected_link_artifacts(self) -> None:
        """The default build cannot add registered build-by-default:false probes."""
        with TemporaryDirectory() as tmp:
            build_dir = Path(tmp)
            linked = build_dir / "tests" / "selected.so"
            probe = build_dir / "tests" / "color_profile_tiny_layout"
            info = build_dir / "meson-info"
            info.mkdir()
            (info / "intro-tests.json").write_text(
                json.dumps([{"cmd": [str(probe)]}]), encoding="utf-8"
            )
            observed: list[Path] = []

            def callback(path: Path) -> StreamingTestResult:
                observed.append(path)
                return StreamingTestResult(success=True)

            with (
                patch(
                    "ci.meson.streaming.stream_compile_only",
                    return_value=CompileOnlyResult(
                        success=True, compiled_tests=[linked]
                    ),
                ),
                patch(
                    "ci.meson.streaming.concurrent.futures.ThreadPoolExecutor",
                    side_effect=_ImmediateExecutor,
                ),
            ):
                result = stream_compile_and_run_tests(build_dir, callback)
            self.assertTrue(result.success)
            self.assertEqual([linked], observed)

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
