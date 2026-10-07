"""Forced native runs must execute registered tests, including cached artifacts."""

import time
import unittest
from pathlib import Path
from unittest.mock import Mock, patch

from ci.meson.streaming import CompileOnlyResult, stream_compile_and_run_tests
from ci.meson.streaming_runner import StreamingContext, run_streaming_path
from ci.meson.test_execution import MesonTestResult


class TestForcedNativeExecution(unittest.TestCase):
    def test_force_runs_registered_suite_after_partial_rebuild(self) -> None:
        timer = Mock()
        timer.meson_setup_time = 0.0
        timer.ninja_maintenance_time = 0.0
        timer.compile_time = 0.0
        ctx = StreamingContext(
            source_dir=Path("unused-source"),
            build_dir=Path("unused-build"),
            use_debug=False,
            check=False,
            build_mode="quick",
            verbose=False,
            exclude_suites=["fastled:examples"],
            test_file_filter=None,
            log_failures=None,
            start_time=time.time(),
            build_timer=timer,
            build_optimizer=None,
            force=True,
        )
        compiled = CompileOnlyResult(
            success=True, compiled_tests=[Path("tests/changed.dll")]
        )
        full_result = MesonTestResult(
            success=True,
            duration=0.0,
            num_tests_run=320,
            num_tests_passed=320,
            num_tests_failed=0,
        )
        with (
            patch("ci.meson.streaming_runner._make_streaming_env", return_value={}),
            patch("ci.meson.streaming.stream_compile_only", return_value=compiled),
            patch(
                "ci.meson.streaming_runner.run_meson_test", return_value=full_result
            ) as run_all,
        ):
            result = run_streaming_path(ctx)
        self.assertEqual(result.num_tests_run, 320)
        run_all.assert_called_once_with(
            ctx.build_dir,
            test_name=None,
            verbose=False,
            exclude_suites=["fastled:examples"],
        )

    def test_force_defers_partial_build_to_complete_suite_execution(self) -> None:
        callback = Mock()
        # A mixed build has one changed artifact and many unchanged tests.
        compiled = CompileOnlyResult(
            success=True,
            compiled_tests=[Path("tests/changed.dll")],
            compile_output="one artifact rebuilt",
        )
        with patch("ci.meson.streaming.stream_compile_only", return_value=compiled):
            result = stream_compile_and_run_tests(
                Path("unused-build"), callback, defer_test_execution=True
            )
        self.assertTrue(result.success)
        self.assertEqual(result.num_passed, 0)
        self.assertEqual(result.compile_output, "one artifact rebuilt")
        callback.assert_not_called()

    def test_force_does_not_defer_compile_failures_as_success(self) -> None:
        callback = Mock()
        compiled = CompileOnlyResult(success=False, compile_output="compile failed")
        with patch("ci.meson.streaming.stream_compile_only", return_value=compiled):
            result = stream_compile_and_run_tests(
                Path("unused-build"), callback, defer_test_execution=True
            )
        self.assertFalse(result.success)
        callback.assert_not_called()
