#!/usr/bin/env python3
"""Meson build system runner for FastLED tests.

This module is the top-level orchestrator. The execution paths and helpers
have been split into focused modules:

- ``ci.meson.runner_helpers``: timing aggregation, stale-build recovery,
  build-dir cleanup with locked-file handling, failure-log writers, and
  zccache session bootstrap.
- ``ci.meson.streaming_runner``: parallel compile + execute path.
- ``ci.meson.sequential_runner``: per-target compile + direct executable
  invocation path used for single-test runs.

This file owns the entry point ``run_meson_build_and_test`` and the CLI.
"""

import argparse
import glob
import os
import sys
import time
from pathlib import Path
from typing import Optional

from ci.meson.build_config import (
    perform_ninja_maintenance,
    setup_meson_build,
)
from ci.meson.build_optimizer import make_build_optimizer
from ci.meson.build_timer import BuildTimer
from ci.meson.compiler import check_meson_installed
from ci.meson.phase_tracker import PhaseTracker
from ci.meson.runner_helpers import _safe_rmtree, _start_zccache_session
from ci.meson.sequential_runner import (
    DirectTestContext,
    SequentialCompileContext,
    run_direct_test,
    run_sequential_compile,
)
from ci.meson.streaming_runner import StreamingContext, run_streaming_path
from ci.meson.test_execution import MesonTestResult, run_meson_test
from ci.util.build_lock import libfastled_build_lock
from ci.util.timestamp_print import ts_print as _ts_print


def _setup_sanitizer_env(source_dir: Path, verbose: bool) -> None:
    """Configure ASan/LSan options for debug builds via clang-tool-chain."""
    from clang_tool_chain import (
        prepare_sanitizer_environment,  # noqa: PLC0415 - lazy: ~60ms on non-debug
    )

    sanitizer_flags = ["-fsanitize=address"]
    sanitizer_env = prepare_sanitizer_environment(
        base_env=os.environ.copy(),
        compiler_flags=sanitizer_flags,
    )
    os.environ.update(sanitizer_env)

    lsan_suppressions = source_dir / "tests" / "lsan_suppressions.txt"
    if lsan_suppressions.exists():
        existing_lsan = os.environ.get("LSAN_OPTIONS", "")
        suppression_opt = f"suppressions={lsan_suppressions}"
        if existing_lsan:
            os.environ["LSAN_OPTIONS"] = f"{existing_lsan}:{suppression_opt}"
        else:
            os.environ["LSAN_OPTIONS"] = suppression_opt

    if verbose:
        symbolizer_path = sanitizer_env.get("ASAN_SYMBOLIZER_PATH")
        if symbolizer_path:
            _ts_print(f"[MESON] Set ASAN_SYMBOLIZER_PATH={symbolizer_path}")


def _resolve_test_name_candidates(
    build_dir: Path, test_name: str
) -> tuple[str, str, list[str]]:  # noqa: DCT002
    """Compute (primary, fallback, fuzzy) candidate names for a requested test.

    The primary is the most specific guess; the fallback is the next-best
    guess; the fuzzy list comes from globbing build_dir/tests/ for partial
    matches. Each is matched against meson target names later.
    """
    test_name_lower = test_name.lower()
    if test_name_lower.startswith("test_"):
        meson_test_name = test_name_lower
        fallback_test_name = test_name_lower[5:]
    else:
        meson_test_name = f"test_{test_name_lower}"
        fallback_test_name = test_name_lower

    fuzzy_candidates: list[str] = []
    tests_dir = build_dir / "tests"
    if tests_dir.exists():
        exe_pattern = f"*{test_name_lower}*.exe"
        matches = glob.glob(str(tests_dir / exe_pattern))
        for match_path in matches:
            exe_name = Path(match_path).stem
            if exe_name not in [meson_test_name, fallback_test_name]:
                fuzzy_candidates.append(exe_name)

    return meson_test_name, fallback_test_name, fuzzy_candidates


def run_meson_build_and_test(
    source_dir: Path,
    build_dir: Path,
    test_name: Optional[str] = None,
    clean: bool = False,
    verbose: bool = False,
    debug: bool = False,
    build_mode: Optional[str] = None,
    check: bool = False,
    exclude_suites: Optional[list[str]] = None,
    test_file_filter: Optional[str] = None,
    log_failures: Optional[Path] = None,
) -> MesonTestResult:
    """
    Complete Meson build and test workflow.

    Args:
        source_dir: Project root directory
        build_dir: Build output directory (will be modified to be mode-specific)
        test_name: Specific test to run (without test_ prefix, e.g., "json")
        clean: Clean build directory before setup
        verbose: Enable verbose output
        debug: Enable debug mode with full symbols and sanitizers (default: False)
        build_mode: Build mode override ("quick", "debug", "release"). If None, uses debug parameter.
        check: Enable IWYU static analysis (default: False)
        exclude_suites: Optional list of test suites to exclude (e.g., ['examples'])
        test_file_filter: Optional .hpp filename to filter test execution (e.g., "backbeat.hpp")
        log_failures: Optional directory to write per-test failure logs (<name>_compile.log, <name>_run.log)

    Returns:
        MesonTestResult with success status, duration, and test counts
    """
    start_time = time.time()
    build_timer = BuildTimer()
    build_timer.start()

    if build_mode is None:
        build_mode = "debug" if debug else "quick"

    if build_mode not in ["quick", "debug", "release", "profile"]:
        _ts_print(
            f"[MESON] Error: Invalid build_mode '{build_mode}'. Must be 'quick', 'debug', 'release', or 'profile'",
            file=sys.stderr,
        )
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    # Mode-specific build dir lets libfastled.a cache per mode when source is
    # unchanged but compiler flags differ (e.g. .build/meson-quick vs ...-debug).
    build_dir = build_dir.parent / f"{build_dir.name}-{build_mode}"

    if not check_meson_installed():
        _ts_print("[MESON] Error: Meson build system is not installed", file=sys.stderr)
        _ts_print("[MESON] Install with: pip install meson ninja", file=sys.stderr)
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    if clean and build_dir.exists():
        _ts_print(f"[MESON] Cleaning build directory: {build_dir}")
        _safe_rmtree(build_dir)

    use_debug = build_mode == "debug"
    if use_debug:
        _setup_sanitizer_env(source_dir, verbose)

    _ts_print(f"Preparing build ({build_mode} mode)...")
    _start_zccache_session(build_dir)

    phase_tracker = PhaseTracker(build_dir, build_mode)
    phase_tracker.set_phase("CONFIGURE")

    # Always configure with examples enabled — example exclusion is handled at
    # compile-time (target selection) + test-time (suite filter), not configure-time.
    _ts_print("[MESON] Configuring build (compiler probes may take 20-30s)...")
    if not setup_meson_build(
        source_dir,
        build_dir,
        reconfigure=False,
        debug=use_debug,
        check=check,
        build_mode=build_mode,
        verbose=verbose,
        enable_examples=True,
        enable_unit_tests=True,
    ):
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    build_timer.checkpoint("meson_setup_done")
    perform_ninja_maintenance(build_dir)
    build_timer.checkpoint("ninja_maintenance_done")

    # Resolve test name candidates (only when a specific test was requested).
    meson_test_name: Optional[str] = None
    fallback_test_name: Optional[str] = None
    fuzzy_candidates: list[str] = []
    if test_name:
        meson_test_name, fallback_test_name, fuzzy_candidates = (
            _resolve_test_name_candidates(build_dir, test_name)
        )

    use_streaming = not meson_test_name
    # BuildOptimizer suppresses the 328-DLL relink cascade; only worth it for
    # the streaming all-tests path, where the cascade hurts most.
    build_optimizer = make_build_optimizer(build_dir) if use_streaming else None

    try:
        with libfastled_build_lock():
            if use_streaming:
                ctx = StreamingContext(
                    source_dir=source_dir,
                    build_dir=build_dir,
                    use_debug=use_debug,
                    check=check,
                    build_mode=build_mode,
                    verbose=verbose,
                    exclude_suites=exclude_suites,
                    test_file_filter=test_file_filter,
                    log_failures=log_failures,
                    start_time=start_time,
                    build_timer=build_timer,
                    build_optimizer=build_optimizer,
                )
                return run_streaming_path(ctx)

            seq_ctx = SequentialCompileContext(
                source_dir=source_dir,
                build_dir=build_dir,
                build_mode=build_mode,
                verbose=verbose,
                test_name=test_name,
                meson_test_name=meson_test_name,
                fallback_test_name=fallback_test_name,
                fuzzy_candidates=fuzzy_candidates,
                log_failures=log_failures,
                start_time=start_time,
                build_timer=build_timer,
                phase_tracker=phase_tracker,
            )
            compile_outcome = run_sequential_compile(seq_ctx)
            if not compile_outcome.success:
                assert compile_outcome.error_result is not None
                return compile_outcome.error_result
            meson_test_name = compile_outcome.meson_test_name

    except TimeoutError as e:
        _ts_print(f"[MESON] {e}", file=sys.stderr)
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    # IWYU check mode only compiles for static analysis; skip test execution.
    if check:
        duration = time.time() - start_time
        _ts_print(
            "[MESON] ✅ IWYU analysis complete (test execution skipped in check mode)"
        )
        return MesonTestResult(
            success=True,
            duration=duration,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    if meson_test_name:
        return run_direct_test(
            DirectTestContext(
                source_dir=source_dir,
                build_dir=build_dir,
                meson_test_name=meson_test_name,
                build_mode=build_mode,
                verbose=verbose,
                log_failures=log_failures,
                start_time=start_time,
                build_timer=build_timer,
                phase_tracker=phase_tracker,
            )
        )

    return run_meson_test(
        build_dir, test_name=None, verbose=verbose, exclude_suites=exclude_suites
    )


def main() -> None:
    """CLI entry point for Meson build system runner."""
    parser = argparse.ArgumentParser(
        description="Meson build system runner for FastLED"
    )
    parser.add_argument("--test", help="Specific test to build and run")
    parser.add_argument("--clean", action="store_true", help="Clean build directory")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument(
        "--debug", action="store_true", help="Enable debug mode with full symbols"
    )
    parser.add_argument(
        "--build-mode",
        choices=["quick", "debug", "release", "profile"],
        help="Build mode (quick, debug, release, profile)",
    )
    parser.add_argument(
        "--check", action="store_true", help="Enable IWYU static analysis"
    )
    parser.add_argument("--build-dir", default=".build/meson", help="Build directory")

    args = parser.parse_args()

    source_dir = Path.cwd()
    build_dir = Path(args.build_dir)

    result = run_meson_build_and_test(
        source_dir=source_dir,
        build_dir=build_dir,
        test_name=args.test,
        clean=args.clean,
        verbose=args.verbose,
        debug=args.debug,
        build_mode=args.build_mode,
        check=args.check,
    )

    sys.exit(0 if result.success else 1)


if __name__ == "__main__":
    main()
