#!/usr/bin/env python3
"""Streaming execution path for ``run_meson_build_and_test``.

Extracted from ``ci/meson/runner.py``. Runs ``stream_compile_and_run_tests``
which compiles + executes tests in parallel — as soon as a test finishes
linking it's launched while other tests are still compiling.

This module owns the streaming-specific concerns: a test-callback closure
that captures per-test output (so failures can be summarized at the end),
zombie-process tracking, the proactive ``all-with-examples`` target check,
stale-build recovery retry, and the no-tests-ran fallback.
"""

import os
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Optional

from running_process import RunningProcess

from ci.meson.build_config import setup_meson_build
from ci.meson.build_optimizer import BuildOptimizer
from ci.meson.build_timer import BuildTimer
from ci.meson.compile import is_stale_build_error
from ci.meson.output import print_error, print_info, print_success
from ci.meson.runner_helpers import (
    _apply_compile_sub_phases,
    _apply_phase_timing,
    _extract_compile_failures,
    _recover_stale_build,
    _write_failure_log,
    _write_testlog_failures,
)
from ci.meson.streaming import TestResult, stream_compile_and_run_tests
from ci.meson.test_execution import MesonTestResult, run_meson_test
from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.output_formatter import TimestampFormatter
from ci.util.timestamp_print import ts_print as _ts_print


def validate_test_artifact(
    test_path: Path, build_start_time: float
) -> Optional[TestResult]:
    """Validate a test/example binary before running it.

    Returns ``None`` if the artifact is fresh and ready to run. Returns
    a populated ``TestResult`` (``success=False``) if the artifact is
    missing or stale, so the caller can surface the failure directly
    instead of silently running broken/old code.

    Why this matters (FastLED #3011): the streaming compiler submits
    tests to the runner as soon as Ninja prints ``[N/M] Linking <X>``
    — but that line is Ninja's PRE-link announcement, NOT a success
    signal. If the link then FAILS (zccache daemon crash under the
    parallel-link storm, ld.lld permission error, etc.) ``test_path``
    is either missing entirely (clean build) or stale from a previous
    build sitting in the same build dir. Running it produces a
    false pass or false fail; refusing to run it surfaces the link
    failure where it belongs.
    """
    if not test_path.exists():
        return TestResult(
            success=False,
            output=(
                f"[MESON] ❌ Test artifact missing: {test_path}\n"
                f"  Link likely failed during the parallel-link storm "
                f"(zccache daemon crash, ld.lld error, etc.) — see "
                f"FastLED #3011. Refusing to run a missing DLL.\n"
                f"  Resolve the underlying link failure and re-run."
            ),
        )
    try:
        dll_mtime = test_path.stat().st_mtime
    except OSError:
        # stat() failure is non-fatal — let the subprocess loader
        # surface whatever it actually sees on disk.
        return None
    if dll_mtime < build_start_time:
        return TestResult(
            success=False,
            output=(
                f"[MESON] ❌ Stale test artifact: {test_path}\n"
                f"  DLL mtime ({dll_mtime:.2f}) predates this build's "
                f"start ({build_start_time:.2f}). The current link "
                f"likely failed and left a previous build's DLL behind "
                f"— running it would produce misleading results (see "
                f"FastLED #3011).\n"
                f"  Resolve the underlying link failure (`zccache stop` "
                f"+ retry usually clears the daemon-crash variant) and "
                f"re-run."
            ),
        )
    return None


@dataclass
class StreamingContext:
    """Parameters needed by the streaming execution path."""

    source_dir: Path
    build_dir: Path
    use_debug: bool
    check: bool
    build_mode: str
    verbose: bool
    exclude_suites: Optional[list[str]]
    test_file_filter: Optional[str]
    log_failures: Optional[Path]
    start_time: float
    build_timer: BuildTimer
    build_optimizer: Optional[BuildOptimizer]


def _make_streaming_env(source_dir: Path, build_dir: Path) -> dict[str, str]:
    """Build the environment dict for streamed test subprocesses.

    Adds the fastled shared lib directory + clang toolchain runtime DLLs to
    PATH (Windows) or LD_LIBRARY_PATH (POSIX) so DLLs load without MSYS2.
    """
    streaming_env = os.environ.copy()
    fastled_lib_dir = str(build_dir / "ci" / "meson" / "native")
    if os.name == "nt":
        from clang_tool_chain import get_runtime_dll_paths

        extra_dirs = os.pathsep.join([fastled_lib_dir] + get_runtime_dll_paths())
        streaming_env["PATH"] = extra_dirs + os.pathsep + streaming_env.get("PATH", "")
    else:
        streaming_env["LD_LIBRARY_PATH"] = (
            fastled_lib_dir + os.pathsep + streaming_env.get("LD_LIBRARY_PATH", "")
        )
    # source_dir is intentionally unused — only the build_dir matters for
    # DLL lookup, but the parameter is kept for symmetry with the rest of
    # the streaming context.
    _ = source_dir
    return streaming_env


def _check_all_with_examples_target(ctx: StreamingContext) -> None:
    """Verify ``all-with-examples`` is in build.ninja; reconfigure if not.

    The marker file can desynchronize from the actual build configuration,
    so we reconfigure proactively to avoid a "target not found" error.
    """
    build_ninja = ctx.build_dir / "build.ninja"
    if not build_ninja.exists():
        return
    try:
        content = build_ninja.read_text(encoding="utf-8")
        if "all-with-examples" not in content:
            _ts_print("[MESON] ⚠️  Target 'all-with-examples' missing from build.ninja")
            _ts_print("[MESON] 🔄 Forcing reconfiguration with enable_examples=true...")
            setup_meson_build(
                ctx.source_dir,
                ctx.build_dir,
                reconfigure=True,
                debug=ctx.use_debug,
                check=ctx.check,
                build_mode=ctx.build_mode,
                verbose=ctx.verbose,
                enable_examples=True,
                enable_unit_tests=True,
            )
    except (OSError, IOError):
        pass


def _streaming_no_tests_fallback(
    ctx: StreamingContext,
    sr,
) -> MesonTestResult:
    """When streaming reported success but ran 0 tests, fall back to ``meson test``."""
    if ctx.verbose:
        print_info("[MESON] Build up-to-date, running existing tests...")
    ctx.build_timer.checkpoint("compile_done")

    result = run_meson_test(
        ctx.build_dir,
        test_name=None,
        verbose=ctx.verbose,
        exclude_suites=ctx.exclude_suites,
    )

    ctx.build_timer.checkpoint("test_execution_done")
    ctx.build_timer.calculate_phases()
    test_only_duration = result.duration
    result.duration = time.time() - ctx.start_time
    result.meson_setup_time = ctx.build_timer.meson_setup_time
    result.ninja_maintenance_time = ctx.build_timer.ninja_maintenance_time
    result.compile_time = ctx.build_timer.compile_time
    result.test_execution_time = test_only_duration

    _apply_compile_sub_phases(result, sr.compile_sub_phases)
    if not result.success and ctx.log_failures is not None:
        _write_testlog_failures(ctx.build_dir, ctx.log_failures)
    return result


def _handle_streaming_failure(
    ctx: StreamingContext,
    sr,
    failed_outputs: dict[str, str],
    failed_outputs_lock: threading.Lock,
    duration: float,
    num_tests_run: int,
) -> MesonTestResult:
    """Print failure details and write logs after the streaming path failed."""
    print_error(
        f"[MESON] ❌ Some tests failed ({sr.num_passed}/{num_tests_run} tests in {duration:.2f}s)"
    )
    # Snapshot under lock — workers may still be running after shutdown(wait=False).
    with failed_outputs_lock:
        failed_snapshot = dict(failed_outputs)

    if failed_snapshot:
        print_error(f"\n{'=' * 80}")
        print_error(f"[MESON] Test failure details ({len(failed_snapshot)} tests):")
        print_error(f"{'=' * 80}")
        for tname, output in failed_snapshot.items():
            print_error(f"\n--- {tname} ---")
            for line in output.splitlines()[-30:]:
                print_error(f"  {line}")
        print_error(f"{'=' * 80}")

    if ctx.log_failures is not None:
        if num_tests_run == 0:
            per_target = _extract_compile_failures(sr.compile_output)
            if per_target:
                for tname, output in per_target.items():
                    _write_failure_log(ctx.log_failures, tname, "compile", output)
            else:
                _write_failure_log(
                    ctx.log_failures,
                    "compile",
                    "compile",
                    sr.compile_output,
                )
        else:
            for tname, output in failed_snapshot.items():
                _write_failure_log(ctx.log_failures, tname, "run", output)

    return MesonTestResult(
        success=False,
        duration=duration,
        num_tests_run=num_tests_run,
        num_tests_passed=sr.num_passed,
        num_tests_failed=sr.num_failed,
        failed_test_names=sr.failed_names,
    )


def run_streaming_path(ctx: StreamingContext) -> MesonTestResult:
    """Execute the streaming compile + test path and return the aggregated result."""
    if ctx.verbose:
        _ts_print("[MESON] Using streaming execution (compile + test in parallel)")

    failed_test_outputs: dict[str, str] = {}
    streaming_env = _make_streaming_env(ctx.source_dir, ctx.build_dir)
    failed_outputs_lock = threading.Lock()

    active_procs: set[RunningProcess] = set()
    active_procs_lock = threading.Lock()

    def test_callback(test_path: Path) -> TestResult:
        """Run a single test executable and return TestResult.

        Output is always captured silently (echo=False) so that the caller
        can print it sequentially on the main thread, avoiding interleaved
        output when tests run in parallel.
        """
        try:
            # FastLED #3011 — refuse to run missing or stale artifacts.
            # The streaming compiler submits tests as soon as Ninja prints
            # "Linking <X>" (a PRE-link announcement, not success). If
            # the link then fails, what's at test_path is either nothing
            # or yesterday's DLL — running it produces false pass / fail.
            failure = validate_test_artifact(test_path, ctx.start_time)
            if failure is not None:
                return failure

            if test_path.suffix.lower() in (".dll", ".so", ".dylib"):
                runner_suffix = ".exe" if os.name == "nt" else ""
                if test_path.parent.name == "tests":
                    runner = ctx.build_dir / "tests" / f"runner{runner_suffix}"
                else:
                    runner = (
                        ctx.build_dir / "examples" / f"example_runner{runner_suffix}"
                    )
                if not runner.exists():
                    return TestResult(
                        success=False,
                        output=f"[MESON] ⚠️  Runner not found: {runner}",
                    )
                cmd = [str(runner), str(test_path)]
            else:
                cmd = [str(test_path)]

            env = streaming_env
            file_filter = getattr(test_callback, "_test_file_filter", None)
            if file_filter:
                env = dict(streaming_env)
                env["FL_TEST_FILE_FILTER"] = file_filter

            proc = RunningProcess(
                cmd,
                cwd=ctx.source_dir,
                timeout=600,
                auto_run=True,
                check=False,
                env=env,
                output_formatter=TimestampFormatter(),
            )

            with active_procs_lock:
                active_procs.add(proc)
            try:
                returncode = proc.wait(echo=False)
            finally:
                with active_procs_lock:
                    active_procs.discard(proc)
            captured = str(proc.stdout)

            if returncode != 0:
                with failed_outputs_lock:
                    failed_test_outputs[test_path.stem] = captured

            return TestResult(success=returncode == 0, output=captured)

        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            return TestResult(
                success=False,
                output="[MESON] Test interrupted by user",
            )
        except Exception as e:
            return TestResult(
                success=False,
                output=f"[MESON] Test execution error: {e}",
            )

    def _kill_active_procs() -> None:
        """Kill all running test subprocesses (called on halt)."""
        with active_procs_lock:
            for p in list(active_procs):
                try:
                    p.kill()
                except KeyboardInterrupt as ki:
                    handle_keyboard_interrupt(ki)
                except Exception:
                    pass

    setattr(test_callback, "kill_all", _kill_active_procs)

    # Debug builds with ASAN are significantly slower (esp. Windows CI).
    compile_timeout = 2700 if ctx.use_debug else 600

    include_examples = not (
        ctx.exclude_suites and "fastled:examples" in ctx.exclude_suites
    )
    compile_target = "all-with-examples" if include_examples else None

    if compile_target == "all-with-examples":
        _check_all_with_examples_target(ctx)

    sr = stream_compile_and_run_tests(
        build_dir=ctx.build_dir,
        test_callback=test_callback,
        target=compile_target,
        verbose=ctx.verbose,
        compile_timeout=compile_timeout,
        build_optimizer=ctx.build_optimizer,
        test_file_filter=ctx.test_file_filter,
        build_timer=ctx.build_timer,
    )

    # SELF-HEALING: stale-build recovery + retry once
    if not sr.success and is_stale_build_error(sr.compile_output):
        if _recover_stale_build(
            ctx.source_dir,
            ctx.build_dir,
            ctx.use_debug,
            ctx.check,
            ctx.build_mode,
            ctx.verbose,
            enable_examples=True,
        ):
            sr = stream_compile_and_run_tests(
                build_dir=ctx.build_dir,
                test_callback=test_callback,
                target=compile_target,
                verbose=ctx.verbose,
                compile_timeout=compile_timeout,
                build_optimizer=ctx.build_optimizer,
                test_file_filter=ctx.test_file_filter,
                build_timer=ctx.build_timer,
            )

    if sr.success and ctx.build_optimizer is not None:
        ctx.build_optimizer.save_fingerprints(ctx.build_dir)

    duration = time.time() - ctx.start_time
    num_tests_run = sr.num_passed + sr.num_failed

    if num_tests_run == 0 and sr.success:
        return _streaming_no_tests_fallback(ctx, sr)

    if not sr.success:
        return _handle_streaming_failure(
            ctx, sr, failed_test_outputs, failed_outputs_lock, duration, num_tests_run
        )

    _ts_print(ctx.build_timer.format_table())
    print_success(
        f"✅ All tests passed ({sr.num_passed}/{num_tests_run} in {duration:.2f}s)"
    )
    result = MesonTestResult(
        success=True,
        duration=duration,
        num_tests_run=num_tests_run,
        num_tests_passed=sr.num_passed,
        num_tests_failed=sr.num_failed,
    )
    result = _apply_phase_timing(result, ctx.build_timer)
    _apply_compile_sub_phases(result, sr.compile_sub_phases)
    return result
