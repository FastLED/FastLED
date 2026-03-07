"""Streaming compilation and test execution via Meson build system."""

import os
import re
import sys
import threading
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Callable, Optional

from running_process import RunningProcess

from ci.meson.build_optimizer import BuildOptimizer
from ci.meson.compile import _create_error_context_filter, _is_compilation_error
from ci.meson.compiler import get_meson_executable
from ci.meson.output import print_error, print_success
from ci.meson.phase_tracker import PhaseTracker
from ci.util.global_interrupt_handler import handle_keyboard_interrupt
from ci.util.tee import StreamTee
from ci.util.timestamp_print import ts_print as _ts_print


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
                                    touched = _opt.touch_dlls_if_lib_unchanged(_bdir)
                                    if touched > 0:
                                        _ts_print(
                                            f"[BUILD] ⚡ Suppressed {touched} DLL relinks"
                                            f" (fastled shared lib content unchanged)"
                                        )

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
                            if test_path.suffix.lower() in (".dll", ".so", ".dylib"):
                                continue

                            if verbose:
                                _ts_print(f"[MESON] Test built: {test_path.name}")
                            compiled_tests.append(test_path)

            # Check compilation result
            returncode = proc.wait()

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

            compilation_output = proc.stdout
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

    return CompileOnlyResult(
        success=not compilation_failed,
        compile_output=compilation_output,
        compiled_tests=compiled_tests,
        compile_sub_phases=compile_sub_phases,
    )


def stream_compile_and_run_tests(
    build_dir: Path,
    test_callback: Callable[[Path], bool],
    target: Optional[str] = None,
    verbose: bool = False,
    compile_timeout: int = 600,
    build_optimizer: Optional[BuildOptimizer] = None,
    test_file_filter: Optional[str] = None,
    build_timer=None,
) -> StreamingResult:
    """
    Stream test compilation and then execution sequentially.

    First compiles all tests, then runs them after compilation completes.
    This provides clearer timing breakdown and prevents tests from running
    during ongoing compilation.

    Args:
        build_dir: Meson build directory
        test_callback: Function called with each completed test path.
                      Returns True if test passed, False if failed.
        target: Specific target to build (None = all)
        verbose: Show detailed progress messages (default: False)
        compile_timeout: Timeout in seconds for compilation (default: 600)
        build_optimizer: Optional BuildOptimizer for DLL relink suppression.
        test_file_filter: Optional .hpp filename to filter test execution (e.g., "backbeat.hpp")
        build_timer: Optional BuildTimer for recording test_execution_done checkpoint.
    """
    # Phase 1: Compile only
    cr = stream_compile_only(
        build_dir=build_dir,
        target=target,
        verbose=verbose,
        compile_timeout=compile_timeout,
        build_optimizer=build_optimizer,
    )

    if not cr.success:
        # Compilation failed, return immediately
        return StreamingResult(
            success=False,
            compile_output=cr.compile_output,
            compile_sub_phases=cr.compile_sub_phases,
        )

    # Phase 2: Run tests after compilation completes
    # Only run tests if test_file_filter is set or all tests should be run
    num_passed = 0
    num_failed = 0
    failed_names: list[str] = []

    # Filter tests if test_file_filter is specified
    if test_file_filter and cr.compiled_tests:
        # Convert filter (e.g., "backbeat.hpp") to match test paths
        filtered_tests = [
            t
            for t in cr.compiled_tests
            if test_file_filter.replace(".hpp", "") in t.name.lower()
        ]
    else:
        filtered_tests = cr.compiled_tests

    if not filtered_tests:
        # No directly-runnable tests found (e.g., all targets are DLLs on Windows).
        # Return empty result - caller (runner.py) will fall back to Meson test runner.
        return StreamingResult(
            success=True,
            compile_output=cr.compile_output,
            compile_sub_phases=cr.compile_sub_phases,
        )

    _ts_print(f"[MESON] Running {len(filtered_tests)} tests...")

    tests_run = 0
    for test_path in filtered_tests:
        tests_run += 1
        if verbose:
            _ts_print(f"[TEST {tests_run}] Running: {test_path.name}")

        try:
            # Set test file filter in environment if specified
            if test_file_filter:
                os.environ["FL_TEST_FILE_FILTER"] = test_file_filter

            success = test_callback(test_path)

            # Clean up environment variable
            if test_file_filter and "FL_TEST_FILE_FILTER" in os.environ:
                del os.environ["FL_TEST_FILE_FILTER"]

            if success:
                num_passed += 1
                if verbose:
                    _ts_print(f"[TEST {tests_run}] ✓ PASSED: {test_path.name}")
            else:
                num_failed += 1
                failed_names.append(test_path.stem)
                _ts_print(f"[TEST {tests_run}] ✗ FAILED: {test_path.name}")
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            _ts_print(f"[TEST {tests_run}] ✗ ERROR: {test_path.name}: {e}")
            num_failed += 1
            failed_names.append(test_path.stem)

    # Show test summary
    if tests_run > 0:
        _ts_print(f"[MESON] Test execution complete:")
        _ts_print(f"  Tests run: {tests_run}")
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
