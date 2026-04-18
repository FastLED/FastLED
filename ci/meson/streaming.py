"""Streaming compilation and test execution via Meson build system."""

import concurrent.futures
import os
import re
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Optional, cast

from running_process import RunningProcess

from ci.meson.build_optimizer import BuildOptimizer
from ci.meson.compile import (
    _create_error_context_filter,
    _is_compilation_error,
)
from ci.meson.compiler import get_meson_executable
from ci.meson.link_retry import (
    is_link_permission_denied_error,
    kill_stale_runner_processes,
)
from ci.meson.mtime_stabilizer import stabilize_dll_mtimes
from ci.meson.output import print_error, print_success
from ci.meson.phase_tracker import PhaseTracker
from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.tee import StreamTee
from ci.util.timestamp_print import ts_print as _ts_print


# Maximum number of retries when ld.lld fails with 'permission denied' on
# runner.exe / example_runner.exe (Windows-only transient failure; see #2268).
_LLD_PERM_DENIED_MAX_RETRIES = 3


@dataclass
class StreamingResult:
    """Result from streaming compilation and optional test execution."""

    success: bool
    num_passed: int = 0
    num_failed: int = 0
    compile_output: str = ""
    failed_names: list[str] = field(default_factory=list)
    compile_sub_phases: dict[str, float] = field(default_factory=dict)


@dataclass
class TestResult:
    """Result from a single test execution."""

    success: bool
    output: str = ""


@dataclass
class CompileOnlyResult:
    """Result from stream_compile_only."""

    success: bool
    compile_output: str = ""
    compiled_tests: list[Path] = field(default_factory=list)
    compile_sub_phases: dict[str, float] = field(default_factory=dict)


def stream_compile_only(
    build_dir: Path,
    target: Optional[str] = None,
    verbose: bool = False,
    compile_timeout: int = 600,
    build_optimizer: Optional[BuildOptimizer] = None,
    build_timer=None,
    on_test_compiled: Optional[Callable[[Path], None]] = None,
) -> CompileOnlyResult:
    """
    Stream compilation only - collect all compiled test paths without running them.

    Monitors Ninja output to detect when test executables finish linking,
    collecting them for later execution.

    Args:
        build_dir: Meson build directory
        target: Specific target to build (None = all)
        verbose: Show detailed progress messages (default: False)
        compile_timeout: Timeout in seconds for compilation (default: 600)
        build_optimizer: Optional BuildOptimizer for DLL relink suppression.
        build_timer: Optional BuildTimer for recording compile_done checkpoint.
    """
    cmd = [get_meson_executable(), "compile", "-C", str(build_dir)]

    # Determine build mode from build directory name
    build_mode = "unknown"
    if "meson-quick" in str(build_dir):
        build_mode = "quick"
    elif "meson-debug" in str(build_dir):
        build_mode = "debug"
    elif "meson-release" in str(build_dir):
        build_mode = "release"

    # Initialize phase tracker for error diagnostics
    phase_tracker = PhaseTracker(build_dir, build_mode)
    phase_tracker.set_phase("COMPILE", target=target, path="streaming")

    # Create compile-errors directory for error logging
    compile_errors_dir = build_dir.parent / "compile-errors"
    compile_errors_dir.mkdir(exist_ok=True)

    # Generate log filename
    test_name_slug = target.replace("/", "_") if target else "all"
    error_log_path = compile_errors_dir / f"{test_name_slug}.log"

    # Show build stage banner
    _ts_print(f"[BUILD] Building FastLED engine ({build_mode} mode)...")

    if target:
        cmd.append(target)
        if verbose:
            _ts_print(f"[MESON] Streaming compilation of target: {target}")
    else:
        if verbose:
            _ts_print(f"[MESON] Streaming compilation of all test targets...")

    # Pre-link safeguard (#2268): on Windows, kill any stale runner.exe /
    # example_runner.exe processes under this build_dir. Windows refuses to
    # overwrite a running .exe, so a leftover process from a prior invocation
    # will cause ld.lld to fail with "permission denied". No-op off-Windows.
    _killed = kill_stale_runner_processes(build_dir)
    if _killed:
        _ts_print(f"[MESON] 🧹 Killed {_killed} stale runner process(es) before link")

    # Pattern to detect test executable linking
    link_pattern = re.compile(
        r"^\[\d+/\d+\]\s+Linking (?:CXX executable|target)\s+(.+)$"
    )

    # Pattern to detect static library archiving
    archive_pattern = re.compile(r"^\[\d+/\d+\]\s+Linking static target\s+(.+)$")

    # Pattern to detect PCH compilation
    pch_pattern = re.compile(r"^\[\d+/\d+\]\s+Generating\s+\S*test_pch\b")

    # Pattern to extract Ninja step numbers from any line (e.g., [35/1084])
    ninja_step_pattern = re.compile(r"^\[(\d+)/(\d+)\]")

    # Track compilation success
    compilation_failed = False
    compilation_output = ""
    compiled_tests: list[Path] = []

    # Track what we've seen during build for cache status reporting
    shown_tests_stage = False
    shown_examples_stage = False
    seen_libfastled = False
    seen_libcrash_handler = False
    seen_pch = False
    seen_any_test = False

    # Track compile sub-phase timestamps
    compile_sub_phases: dict[str, float] = {}

    # Gate test execution until the DLL touch thread completes.
    # On Windows, os.utime() opens files with FILE_WRITE_ATTRIBUTES which
    # can conflict with LoadLibrary's SEC_IMAGE mapping of the same DLL,
    # causing intermittent error 126 (module not found) during parallel runs.
    dll_touch_done = threading.Event()
    dll_touch_done.set()  # Initially set (no touch pending)

    def producer_thread() -> None:
        """Parse Ninja output and collect compiled test executables"""
        nonlocal compilation_failed, compilation_output, shown_tests_stage
        nonlocal shown_examples_stage
        nonlocal seen_libfastled, seen_libcrash_handler, seen_pch, seen_any_test

        # Create tee for error log capture
        stderr_tee = StreamTee(error_log_path, echo=False)
        last_error_lines: list[str] = []

        # Track progress for periodic updates during long silent compilations
        last_progress_time = time.time()
        last_step = 0
        total_steps = 0
        progress_interval = 15  # seconds between progress updates

        try:
            # Use RunningProcess for streaming output
            proc = RunningProcess(
                cmd,
                timeout=compile_timeout,
                auto_run=True,
                check=False,
                env=os.environ.copy(),
            )

            # Stream output line by line
            with proc.line_iter(timeout=None) as it:
                for line in it:
                    # Write to error log
                    stderr_tee.write_line(line)

                    # Track errors for smart detection
                    if _is_compilation_error(line):
                        last_error_lines.append(line)
                        if len(last_error_lines) > 50:
                            last_error_lines.pop(0)

                    # Filter out noisy Meson/Ninja INFO lines
                    stripped = line.strip()
                    if stripped.startswith("INFO:"):
                        continue
                    if "Entering directory" in stripped:
                        continue

                    # Extract Ninja step numbers for progress tracking (Fix 4)
                    step_match = ninja_step_pattern.match(stripped)
                    if step_match:
                        last_step = int(step_match.group(1))
                        total_steps = int(step_match.group(2))

                    is_key_milestone = False
                    milestone_already_printed = False

                    # Check for library archiving
                    archive_match = archive_pattern.match(stripped)
                    if archive_match:
                        if "libcrash_handler" in stripped:
                            is_key_milestone = True
                            milestone_already_printed = True
                            rel_path = archive_match.group(1)
                            full_path = build_dir / rel_path
                            try:
                                display_path = full_path.relative_to(Path.cwd())
                            except ValueError:
                                display_path = full_path
                            _ts_print(f"[BUILD] ✓ Core library: {display_path}")
                            seen_libcrash_handler = True
                            if "compile_start" not in compile_sub_phases:
                                compile_sub_phases["compile_start"] = time.time()

                    # Check for PCH compilation
                    elif pch_pattern.match(stripped):
                        is_key_milestone = True
                        milestone_already_printed = True
                        seen_pch = True
                        pch_match_result = re.search(
                            r"Generating\s+(\S+test_pch)\b", stripped
                        )
                        if pch_match_result:
                            rel_path = pch_match_result.group(1) + ".h.pch"
                            full_path = build_dir / rel_path
                            try:
                                display_path = full_path.relative_to(Path.cwd())
                            except ValueError:
                                display_path = full_path
                            _ts_print(f"[BUILD] ✓ Precompiled header: {display_path}")

                    # Rewrite Ninja paths for clarity
                    display_line = line
                    link_match = link_pattern.match(stripped)
                    if link_match:
                        rel_path = link_match.group(1)
                        full_path = build_dir / rel_path
                        try:
                            display_path = full_path.relative_to(Path.cwd())
                            display_line = line.replace(rel_path, str(display_path))
                        except ValueError:
                            display_line = line.replace(rel_path, str(full_path))

                        # Detect fastled shared library linking (Fix 2)
                        link_stem = Path(rel_path).stem
                        if link_stem == "fastled" and Path(rel_path).suffix.lower() in (
                            ".dll",
                            ".so",
                            ".dylib",
                        ):
                            is_key_milestone = True
                            milestone_already_printed = True
                            seen_libfastled = True
                            compile_sub_phases["core_done"] = time.time()
                            _lib_display = full_path
                            try:
                                _lib_display = full_path.relative_to(Path.cwd())
                            except ValueError:
                                pass
                            _ts_print(f"[BUILD] ✓ Core library: {_lib_display}")
                            if build_optimizer is not None:
                                _opt = build_optimizer
                                _bdir = build_dir

                                def _touch_dlls_bg() -> None:
                                    try:
                                        touched = _opt.touch_dlls_if_lib_unchanged(
                                            _bdir
                                        )
                                        if touched > 0:
                                            _ts_print(
                                                f"[BUILD] ⚡ Suppressed {touched} DLL relinks"
                                                f" (fastled shared lib content unchanged)"
                                            )
                                    finally:
                                        dll_touch_done.set()

                                dll_touch_done.clear()
                                threading.Thread(
                                    target=_touch_dlls_bg,
                                    daemon=True,
                                    name="DllTouch",
                                ).start()

                        # Test/example linking (Fix 1: separate tracking)
                        is_example = "examples/" in rel_path or "examples\\" in rel_path
                        is_test = "tests/" in rel_path or "tests\\" in rel_path
                        if is_test or is_example:
                            test_name = Path(rel_path).stem
                            if test_name not in (
                                "runner",
                                "test_runner",
                                "example_runner",
                            ):
                                seen_any_test = True
                                is_key_milestone = True
                                if is_example and not shown_examples_stage:
                                    _ts_print("[BUILD] Linking examples...")
                                    shown_examples_stage = True
                                    compile_sub_phases["example_link_start"] = (
                                        time.time()
                                    )
                                elif is_test and not shown_tests_stage:
                                    _ts_print("[BUILD] Linking tests...")
                                    shown_tests_stage = True
                                    compile_sub_phases["test_link_start"] = time.time()

                    # Echo output for visibility (Fix 2: skip if already printed)
                    if (
                        stripped
                        and (verbose or is_key_milestone)
                        and not milestone_already_printed
                    ):
                        _ts_print(f"[BUILD] {display_line}")

                    # Periodic progress indicator during long compilations (Fix 4)
                    if step_match and is_key_milestone:
                        last_progress_time = time.time()
                    elif (
                        step_match
                        and time.time() - last_progress_time > progress_interval
                    ):
                        _ts_print(f"[BUILD] Compiling... [{last_step}/{total_steps}]")
                        last_progress_time = time.time()

                    # Collect compiled test paths
                    match = (
                        link_match if link_match else link_pattern.match(line.strip())
                    )
                    if match:
                        rel_path = match.group(1)
                        test_path = build_dir / rel_path

                        # Only collect if it's a test executable
                        if (
                            "tests/" in rel_path
                            or "tests\\" in rel_path
                            or "examples/" in rel_path
                            or "examples\\" in rel_path
                        ):
                            test_name = test_path.stem
                            if test_name in ("runner", "test_runner", "example_runner"):
                                continue
                            if (
                                "tests/profile/" in rel_path
                                or "tests\\profile\\" in rel_path
                            ):
                                continue
                            # DLLs/.so/.dylib are handled by runner in test_callback

                            if verbose:
                                _ts_print(f"[MESON] Test built: {test_path.name}")
                            compiled_tests.append(test_path)
                            if on_test_compiled is not None:
                                # Wait for DLL touch thread to finish before
                                # submitting tests — os.utime() on Windows
                                # conflicts with LoadLibrary (error 126).
                                dll_touch_done.wait()
                                on_test_compiled(test_path)

            # Check compilation result
            returncode = cast(int, proc.wait())

            # Write standardized footer to error log
            stderr_tee.write_footer(returncode)
            stderr_tee.close()

            # Save last error snippet if compilation failed
            if returncode != 0 and last_error_lines:
                last_error_path = (
                    build_dir.parent / "meson-state" / "last-compilation-error.txt"
                )
                last_error_path.parent.mkdir(exist_ok=True)

                with open(last_error_path, "w", encoding="utf-8") as f:
                    f.write(f"# Target: {target or 'all'}\n")
                    f.write(f"# Full log: {error_log_path}\n\n")
                    f.write("--- Error Context ---\n\n")
                    f.write("\n".join(last_error_lines))

            compilation_output = str(proc.stdout)

            # Windows ld.lld 'permission denied' retry (#2268).
            # If the link failed because runner.exe / example_runner.exe was
            # locked (stale runner process or antivirus handle), retry the
            # ninja invocation a few times with a short backoff. No-op on
            # non-Windows platforms because ``is_link_permission_denied_error``
            # returns False off-Windows.
            if returncode != 0 and is_link_permission_denied_error(compilation_output):
                for _attempt in range(_LLD_PERM_DENIED_MAX_RETRIES):
                    _ts_print(
                        f"[MESON] ⚠️  ld.lld permission denied on runner binary "
                        f"(attempt {_attempt + 1}/{_LLD_PERM_DENIED_MAX_RETRIES}) - "
                        f"killing stale runner processes and retrying",
                        file=sys.stderr,
                    )
                    _killed_retry = kill_stale_runner_processes(build_dir)
                    if _killed_retry:
                        _ts_print(
                            f"[MESON] 🧹 Killed {_killed_retry} stale runner process(es)",
                            file=sys.stderr,
                        )
                    # Backoff 0.5s, 1.0s, 1.5s — gives Windows / antivirus
                    # time to release any remaining handles on the .exe.
                    time.sleep(0.5 * (_attempt + 1))

                    try:
                        retry_proc = RunningProcess(
                            cmd,
                            timeout=compile_timeout,
                            auto_run=True,
                            check=False,
                            env=os.environ.copy(),
                        )
                        retry_proc.wait(echo=False)
                        returncode = cast(int, retry_proc.returncode)
                        retry_output = str(retry_proc.stdout)
                        compilation_output = compilation_output + "\n" + retry_output
                    except KeyboardInterrupt as ki:
                        handle_keyboard_interrupt(ki)
                        raise
                    except Exception as retry_error:
                        _ts_print(
                            f"[MESON] ⚠️  Retry invocation failed: {retry_error}",
                            file=sys.stderr,
                        )
                        break

                    if returncode == 0:
                        _ts_print(
                            f"[MESON] ✅ Link retry succeeded on attempt {_attempt + 1}",
                            file=sys.stderr,
                        )
                        break

                    # Only keep retrying while the failure is still the same
                    # permission-denied pattern. Non-retryable failures fall
                    # through to the normal error path below.
                    if not is_link_permission_denied_error(retry_output):
                        break

            if returncode != 0:
                phase_tracker.set_phase("LINK", target=target, path="streaming")
                _ts_print(
                    f"[MESON] Compilation failed with return code {returncode}",
                    file=sys.stderr,
                )

                error_filter: Callable[[str], None] = _create_error_context_filter(
                    context_lines=20
                )
                output_lines = compilation_output.splitlines()
                for line in output_lines:
                    error_filter(line)

                compilation_failed = True
            else:
                phase_tracker.set_phase("LINK", target=target, path="streaming")

                # Report cached artifacts
                if (
                    seen_libfastled
                    or seen_libcrash_handler
                    or seen_pch
                    or seen_any_test
                ):
                    cached_libs: list[str] = []
                    if not seen_libfastled:
                        cached_libs.append("fastled (shared)")
                    if not seen_libcrash_handler:
                        cached_libs.append("libcrash_handler.a")

                    if cached_libs:
                        _ts_print(
                            f"[BUILD] ✓ Core libraries: up-to-date (cached: {', '.join(cached_libs)})"
                        )

                    if not seen_pch:
                        _ts_print(f"[BUILD] ✓ Precompiled header: up-to-date (cached)")

                if verbose:
                    _ts_print(f"[MESON] Compilation completed successfully")

        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            _ts_print(f"[MESON] Producer thread error: {e}", file=sys.stderr)
            compilation_failed = True

    # Run producer thread to completion
    producer = threading.Thread(
        target=producer_thread, name="NinjaProducer", daemon=False
    )
    producer.start()
    # Use timed join loop so the main thread can receive KeyboardInterrupt (Ctrl+C).
    # A bare thread.join() on Windows swallows SIGINT until the thread finishes.
    while producer.is_alive():
        producer.join(timeout=0.2)

    # Record compilation completion checkpoint and end time for sub-phases
    compile_sub_phases["compile_done"] = time.time()
    if build_timer is not None:
        build_timer.checkpoint("compile_done")

    # Post-compile mtime stabilization: fix zccache relink loop.
    # zccache restores cached link results with old mtimes, making DLLs
    # perpetually older than the symbols file → infinite relink cascade.
    # Touch stale outputs to break the loop (~5ms, saves ~1-2s per build).
    if not compilation_failed:
        stabilize_dll_mtimes(build_dir, verbose=verbose)

    return CompileOnlyResult(
        success=not compilation_failed,
        compile_output=compilation_output,
        compiled_tests=compiled_tests,
        compile_sub_phases=compile_sub_phases,
    )


def stream_compile_and_run_tests(
    build_dir: Path,
    test_callback: Callable[[Path], TestResult],
    target: Optional[str] = None,
    verbose: bool = False,
    compile_timeout: int = 600,
    build_optimizer: Optional[BuildOptimizer] = None,
    test_file_filter: Optional[str] = None,
    build_timer=None,
    max_failures: int = 10,
) -> StreamingResult:
    """
    Stream test compilation and execution with overlap.

    Tests start executing as soon as they finish linking, while remaining
    tests continue compiling. This reduces total wall-clock time compared
    to a sequential compile-then-run approach.

    Args:
        build_dir: Meson build directory
        test_callback: Function called with each completed test path.
                      Returns a TestResult indicating pass/fail and captured output.
        target: Specific target to build (None = all)
        verbose: Show detailed progress messages (default: False)
        compile_timeout: Timeout in seconds for compilation (default: 600)
        build_optimizer: Optional BuildOptimizer for DLL relink suppression.
        test_file_filter: Optional .hpp filename to filter test execution (e.g., "backbeat.hpp")
        build_timer: Optional BuildTimer for recording test_execution_done checkpoint.
        max_failures: Maximum number of test failures before halting (default: 10, 0 = unlimited).
    """
    # Pass test file filter to the callback via an attribute so it can
    # inject it into the subprocess environment (os.environ is stale after
    # _streaming_env was copied).
    if test_file_filter:
        test_callback._test_file_filter = test_file_filter  # type: ignore[attr-defined]

    # Prepare concurrent test execution infrastructure.
    # Tests are submitted to the executor as they finish linking (during
    # compilation), overlapping test execution with ongoing compilation.
    max_workers = min(os.cpu_count() or 4, 16)
    halt_event = threading.Event()
    _kill_all = getattr(test_callback, "kill_all", None)

    @dataclass
    class _WorkerResult:
        """Internal result from a worker thread."""

        test_path: Path
        test_result: TestResult
        error_msg: str = ""

    def _run_one_test(test_path: Path) -> _WorkerResult:
        """Run a single test in a worker thread."""
        if halt_event.is_set():
            return _WorkerResult(test_path, TestResult(success=False))
        try:
            return _WorkerResult(test_path, test_callback(test_path))
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            return _WorkerResult(test_path, TestResult(success=False), "Interrupted")
        except Exception as e:
            return _WorkerResult(test_path, TestResult(success=False), str(e))

    # Do NOT use `with` for the executor — its __exit__ calls
    # shutdown(wait=True) which blocks until every running worker
    # finishes, making Ctrl+C unresponsive for up to 600s per test.
    executor = concurrent.futures.ThreadPoolExecutor(max_workers=max_workers)
    futures: list[concurrent.futures.Future[_WorkerResult]] = []
    futures_lock = threading.Lock()

    # Prepare filter for test_file_filter
    filter_needle = (
        test_file_filter.replace(".hpp", "").lower() if test_file_filter else None
    )

    def _on_test_compiled(test_path: Path) -> None:
        """Submit a newly compiled test for execution immediately.

        Called from the Ninja producer thread as each test finishes linking,
        allowing test execution to overlap with ongoing compilation.
        """
        if halt_event.is_set():
            return
        if filter_needle and filter_needle not in test_path.name.lower():
            return
        try:
            future = executor.submit(_run_one_test, test_path)
        except RuntimeError:
            # Executor was shut down (e.g. compilation failed concurrently)
            return
        with futures_lock:
            futures.append(future)

    # Compile and run tests with overlap — tests start executing as soon
    # as they finish linking while remaining tests continue compiling.
    cr = stream_compile_only(
        build_dir=build_dir,
        target=target,
        verbose=verbose,
        compile_timeout=compile_timeout,
        build_optimizer=build_optimizer,
        build_timer=build_timer,
        on_test_compiled=_on_test_compiled,
    )

    if not cr.success:
        # Compilation failed — cancel pending tests and shutdown
        halt_event.set()
        if _kill_all:
            _kill_all()
        with futures_lock:
            for f in futures:
                f.cancel()
        executor.shutdown(wait=False, cancel_futures=True)
        return StreamingResult(
            success=False,
            compile_output=cr.compile_output,
            compile_sub_phases=cr.compile_sub_phases,
        )

    # Take snapshot of futures (no more will be added after compile returns)
    with futures_lock:
        all_futures = list(futures)

    if not all_futures:
        # No tests found during compilation (build was fully cached).
        # Return empty result - caller will fall back to Meson test runner.
        executor.shutdown(wait=False)
        return StreamingResult(
            success=True,
            compile_output=cr.compile_output,
            compile_sub_phases=cr.compile_sub_phases,
        )

    total = len(all_futures)
    _ts_print(
        f"[MESON] Collecting {total} test results ({max_workers} parallel workers)..."
    )

    num_passed = 0
    num_failed = 0
    failed_names: list[str] = []
    tests_completed = 0

    try:
        # Wait sequentially so output order matches submission order
        # (tests still execute in parallel across worker threads).
        # Many tests will have already completed during compilation.
        for future in all_futures:
            if halt_event.is_set():
                break
            wr = future.result()
            tests_completed += 1
            idx = tests_completed
            if wr.test_result.success:
                num_passed += 1
                if not verbose:
                    print_success(f"  [{idx}/{total}] ✓ {wr.test_path.stem}")
                else:
                    _ts_print(f"[TEST {idx}/{total}] ✓ PASSED: {wr.test_path.name}")
                    if wr.test_result.output:
                        for line in wr.test_result.output.splitlines():
                            _ts_print(f"  {line}")
            else:
                num_failed += 1
                failed_names.append(wr.test_path.stem)
                if wr.error_msg:
                    _ts_print(
                        f"  [{idx}/{total}] ✗ {wr.test_path.stem} ERROR: {wr.error_msg}"
                    )
                else:
                    print_error(f"  [{idx}/{total}] ✗ {wr.test_path.stem} FAILED")
                # Print captured output for failed tests so failures
                # are visible inline (output was captured silently in
                # the worker thread to avoid interleaving).
                if wr.test_result.output:
                    for line in wr.test_result.output.splitlines()[-30:]:
                        print_error(f"  {line}")
                if max_failures > 0 and num_failed >= max_failures:
                    print_error(
                        f"\n[MESON] ⚠️  {num_failed} test failures detected — halting early"
                    )
                    halt_event.set()
                    if _kill_all:
                        _kill_all()
                    for f in all_futures:
                        f.cancel()
                    break
    except KeyboardInterrupt as ki:
        halt_event.set()
        if _kill_all:
            _kill_all()
        for f in all_futures:
            f.cancel()
        handle_keyboard_interrupt(ki)
        raise
    finally:
        # wait=False so Ctrl+C and max_failures exit immediately.
        # NOTE: ThreadPoolExecutor threads are non-daemon, so Python's
        # shutdown will wait for running workers to finish.  This is
        # acceptable because each test has a 600s RunningProcess timeout.
        executor.shutdown(wait=False, cancel_futures=True)

    # Show test summary
    if tests_completed > 0:
        _ts_print(f"[MESON] Test execution complete:")
        _ts_print(f"  Tests run: {tests_completed}")
        if num_passed > 0:
            print_success(f"  Passed: {num_passed}")
        else:
            _ts_print(f"  Passed: {num_passed}")
        if num_failed > 0:
            print_error(f"  Failed: {num_failed}")
        else:
            _ts_print(f"  Failed: {num_failed}")

    # Record test execution completion checkpoint
    if build_timer is not None:
        build_timer.checkpoint("test_execution_done")

    return StreamingResult(
        success=cr.success and num_failed == 0,
        num_passed=num_passed,
        num_failed=num_failed,
        compile_output=cr.compile_output,
        failed_names=failed_names,
        compile_sub_phases=cr.compile_sub_phases,
    )
