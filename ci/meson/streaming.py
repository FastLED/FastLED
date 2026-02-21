"""Streaming compilation and test execution via Meson build system."""

import os
import queue
import re
import subprocess
import sys
import threading
from pathlib import Path
from typing import Callable, Optional

from running_process import RunningProcess

from ci.meson.build_optimizer import BuildOptimizer
from ci.meson.compile import _create_error_context_filter, _is_compilation_error
from ci.meson.compiler import get_meson_executable
from ci.meson.output import print_banner, print_error, print_success
from ci.meson.phase_tracker import PhaseTracker
from ci.meson.test_execution import MesonTestResult
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.output_formatter import TimestampFormatter
from ci.util.tee import StreamTee
from ci.util.timestamp_print import ts_print as _ts_print


def stream_compile_and_run_tests(
    build_dir: Path,
    test_callback: Callable[[Path], bool],
    target: Optional[str] = None,
    verbose: bool = False,
    compile_timeout: int = 600,
    build_optimizer: Optional[BuildOptimizer] = None,
) -> tuple[bool, int, int, str]:
    """
    Stream test compilation and execution in parallel.

    Monitors Ninja output to detect when test executables finish linking,
    then immediately queues them for execution via the callback.

    Args:
        build_dir: Meson build directory
        test_callback: Function called with each completed test path.
                      Returns True if test passed, False if failed.
        target: Specific target to build (None = all)
        verbose: Show detailed progress messages (default: False)
        compile_timeout: Timeout in seconds for compilation (default: 600)
        build_optimizer: Optional BuildOptimizer for DLL relink suppression.
                        When provided, touches DLL files after libfastled.a is
                        archived (if content unchanged) so ninja skips relinking.

    Returns:
        Tuple of (overall_success, num_passed, num_failed, compile_output)
        compile_output contains the compilation stdout/stderr for error analysis
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
    # WARNING: This build progress reporting is essential for user feedback!
    #   Do NOT suppress these messages - they provide:
    #   1. Build stage visibility (engine → libraries → PCH → tests)
    #   2. Timing information (helps diagnose slow builds)
    #   3. Cache status reporting (shows when artifacts are up-to-date)
    _ts_print(f"[BUILD] Building FastLED engine ({build_mode} mode)...")

    if target:
        cmd.append(target)
        if verbose:
            _ts_print(f"[MESON] Streaming compilation of target: {target}")
    else:
        # Build default targets (unit tests) - no target specified builds defaults
        # Note: examples have build_by_default: false, so we need to build them separately
        if verbose:
            _ts_print(f"[MESON] Streaming compilation of all test targets...")

    # Pattern to detect test executable linking
    # Example: "[142/143] Linking target tests/test_foo.exe"
    link_pattern = re.compile(
        r"^\[\d+/\d+\]\s+Linking (?:CXX executable|target)\s+(.+)$"
    )

    # Pattern to detect static library archiving
    archive_pattern = re.compile(r"^\[\d+/\d+\]\s+Linking static target\s+(.+)$")

    # Pattern to detect PCH compilation
    # Example: "[1/1] Generating tests/test_pch with a custom command"
    pch_pattern = re.compile(r"^\[\d+/\d+\]\s+Generating\s+\S*test_pch\b")

    # Track compilation success and test results
    compilation_failed = False
    compilation_output = ""  # Capture compilation output for error analysis
    num_passed = 0
    num_failed = 0
    tests_run = 0

    # Track build stages for progress reporting
    shown_tests_stage = False  # Track if we've shown "Building tests..." message

    # Track what we've seen during build for cache status reporting
    seen_libfastled = False
    seen_libcrash_handler = False
    seen_pch = False
    seen_any_test = False

    # Queue for completed test executables
    test_queue: queue.Queue[Optional[Path]] = queue.Queue()

    # Sentinel value to signal end of compilation
    COMPILATION_DONE = None

    def producer_thread() -> None:
        """Parse Ninja output and queue completed test executables"""
        nonlocal compilation_failed, compilation_output, shown_tests_stage
        nonlocal seen_libfastled, seen_libcrash_handler, seen_pch, seen_any_test

        # Create tee for error log capture (stdout + stderr merged)
        stderr_tee = StreamTee(error_log_path, echo=False)
        last_error_lines: list[str] = []

        try:
            # Use RunningProcess for streaming output
            # Note: No formatter here - we need raw Ninja output for regex pattern matching
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
                    # Write to error log (captures both stdout and stderr)
                    stderr_tee.write_line(line)

                    # Track errors for smart detection
                    if _is_compilation_error(line):
                        last_error_lines.append(line)
                        if len(last_error_lines) > 50:
                            last_error_lines.pop(0)

                    # Filter out noisy Meson/Ninja INFO lines that clutter output
                    # These provide no useful information for normal operation
                    stripped = line.strip()
                    if stripped.startswith("INFO:"):
                        continue  # Skip Meson INFO lines
                    if "Entering directory" in stripped:
                        continue  # Skip Ninja directory change messages

                    # Check for key build milestones (show even in non-verbose mode)
                    is_key_milestone = False

                    # Check for library archiving (static libraries like libcrash_handler.a)
                    archive_match = archive_pattern.match(stripped)
                    if archive_match:
                        if "libcrash_handler" in stripped:
                            is_key_milestone = True
                            rel_path = archive_match.group(1)
                            full_path = build_dir / rel_path
                            try:
                                display_path = full_path.relative_to(Path.cwd())
                            except ValueError:
                                display_path = full_path
                            _ts_print(f"[BUILD] ✓ Core library: {display_path}")
                            seen_libcrash_handler = True

                    # Check for PCH compilation
                    elif pch_pattern.match(stripped):
                        is_key_milestone = True
                        seen_pch = True  # Track that we've seen PCH compilation
                        # Extract PCH path from the line
                        # Format: "[1/1] Generating tests/test_pch with a custom command"
                        pch_match_result = re.search(
                            r"Generating\s+(\S+test_pch)\b", stripped
                        )
                        if pch_match_result:
                            rel_path = (
                                pch_match_result.group(1) + ".h.pch"
                            )  # Add extension for display
                            full_path = build_dir / rel_path
                            try:
                                display_path = full_path.relative_to(Path.cwd())
                            except ValueError:
                                display_path = full_path
                            _ts_print(f"[BUILD] ✓ Precompiled header: {display_path}")

                    # Rewrite Ninja paths to show full build-relative paths for clarity
                    # Ninja outputs paths relative to build directory (e.g., "tests/fx_frame.exe")
                    # Users expect to see full paths (e.g., ".build/meson-quick/tests/fx_frame.exe")
                    display_line = line
                    link_match = link_pattern.match(stripped)
                    if link_match:
                        rel_path = link_match.group(1)
                        # Convert build-relative path to project-relative path
                        full_path = build_dir / rel_path
                        try:
                            # Make path relative to project root for cleaner display
                            display_path = full_path.relative_to(Path.cwd())
                            # Rewrite the line with full path
                            display_line = line.replace(rel_path, str(display_path))
                        except ValueError:
                            # If path is outside project, show absolute path
                            display_line = line.replace(rel_path, str(full_path))

                        # Detect fastled shared library linking (fastled.dll/fastled.so)
                        link_stem = Path(rel_path).stem
                        if link_stem == "fastled" and Path(rel_path).suffix.lower() in (
                            ".dll",
                            ".so",
                            ".dylib",
                        ):
                            is_key_milestone = True
                            seen_libfastled = True
                            _lib_display = full_path
                            try:
                                _lib_display = full_path.relative_to(Path.cwd())
                            except ValueError:
                                pass
                            _ts_print(f"[BUILD] ✓ Core library: {_lib_display}")
                            # Binary fingerprint optimization: fastled shared lib just finished
                            # linking. If its content matches saved fingerprint, touch
                            # all DLLs now so ninja sees them as newer → skips relinking.
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

                        # Test/example linking is also a key milestone
                        if (
                            "tests/" in rel_path
                            or "tests\\" in rel_path
                            or "examples/" in rel_path
                            or "examples\\" in rel_path
                        ):
                            # Exclude test infrastructure (runner.exe is not a test)
                            test_name = Path(rel_path).stem
                            if test_name not in (
                                "runner",
                                "test_runner",
                                "example_runner",
                            ):
                                seen_any_test = (
                                    True  # Track that we've seen at least one test
                                )
                                # Show "Building tests..." stage message on first test
                                if not shown_tests_stage:
                                    _ts_print("[BUILD] Building tests...")
                                    shown_tests_stage = True
                                is_key_milestone = True

                    # Echo output for visibility (skip empty lines)
                    # Show in verbose mode OR if it's a key milestone (but skip if we already printed a custom message)
                    if (
                        stripped
                        and (verbose or is_key_milestone)
                        and not archive_match
                        and not pch_pattern.match(stripped)
                    ):
                        _ts_print(f"[BUILD] {display_line}")

                    # Check for link completion
                    match = (
                        link_match if link_match else link_pattern.match(line.strip())
                    )
                    if match:
                        # Extract test executable path
                        rel_path = match.group(1)
                        test_path = build_dir / rel_path

                        # Only queue if it's a test executable (in tests/ or examples/ directory)
                        if (
                            "tests/" in rel_path
                            or "tests\\" in rel_path
                            or "examples/" in rel_path
                            or "examples\\" in rel_path
                        ):
                            # Exclude infrastructure executables that aren't actual tests
                            test_name = test_path.stem  # Get filename without extension
                            if test_name in ("runner", "test_runner", "example_runner"):
                                continue  # Skip these infrastructure executables

                            # Skip profile tests (standalone benchmarking binaries)
                            if (
                                "tests/profile/" in rel_path
                                or "tests\\profile\\" in rel_path
                            ):
                                continue  # Profile tests are run via 'bash profile <function>', not as unit tests

                            # Skip DLL/shared library files
                            if test_path.suffix.lower() in (".dll", ".so", ".dylib"):
                                continue  # Skip DLL/shared library files

                            if verbose:
                                _ts_print(f"[MESON] Test ready: {test_path.name}")
                            test_queue.put(test_path)

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

            compilation_output = proc.stdout  # Capture for error analysis
            if returncode != 0:
                # Track link phase failure
                phase_tracker.set_phase("LINK", target=target, path="streaming")
                _ts_print(
                    f"[MESON] Compilation failed with return code {returncode}",
                    file=sys.stderr,
                )

                # Show error context from compilation output
                error_filter: Callable[[str], None] = _create_error_context_filter(
                    context_lines=20
                )
                output_lines = compilation_output.splitlines()
                for line in output_lines:
                    error_filter(line)

                compilation_failed = True
            else:
                # Track successful link phase
                phase_tracker.set_phase("LINK", target=target, path="streaming")

                # Report cached artifacts (things we didn't see being built)
                # Only report if we saw at least one artifact being built (otherwise it's a no-op build)
                if (
                    seen_libfastled
                    or seen_libcrash_handler
                    or seen_pch
                    or seen_any_test
                ):
                    # Report cached core libraries
                    cached_libs: list[str] = []
                    if not seen_libfastled:
                        cached_libs.append("fastled (shared)")
                    if not seen_libcrash_handler:
                        cached_libs.append("libcrash_handler.a")

                    if cached_libs:
                        _ts_print(
                            f"[BUILD] ✓ Core libraries: up-to-date (cached: {', '.join(cached_libs)})"
                        )

                    # Report cached PCH
                    if not seen_pch:
                        _ts_print(f"[BUILD] ✓ Precompiled header: up-to-date (cached)")

                if verbose:
                    _ts_print(f"[MESON] Compilation completed successfully")

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            _ts_print(f"[MESON] Producer thread error: {e}", file=sys.stderr)
            compilation_failed = True
        finally:
            # Signal end of compilation
            test_queue.put(COMPILATION_DONE)

    # Start producer thread
    producer = threading.Thread(
        target=producer_thread, name="NinjaProducer", daemon=True
    )
    producer.start()

    # Consumer: Run tests as they become available
    try:
        while True:
            try:
                # Get next test from queue (blocking with timeout for responsiveness)
                test_path = test_queue.get(timeout=1.0)

                if test_path is COMPILATION_DONE:
                    # Compilation finished
                    break

                # Type narrowing: test_path is now guaranteed to be Path (not None)
                assert test_path is not None

                # Track execution phase
                phase_tracker.set_phase(
                    "EXECUTE", test_name=test_path.stem, path="streaming"
                )

                # Run the test
                tests_run += 1
                if verbose:
                    _ts_print(f"[TEST {tests_run}] Running: {test_path.name}")

                try:
                    success = test_callback(test_path)
                    if success:
                        num_passed += 1
                        if verbose:
                            _ts_print(f"[TEST {tests_run}] ✓ PASSED: {test_path.name}")
                    else:
                        num_failed += 1
                        # Always show failures even in non-verbose mode
                        _ts_print(f"[TEST {tests_run}] ✗ FAILED: {test_path.name}")
                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise
                except Exception as e:
                    # Always show errors even in non-verbose mode
                    _ts_print(f"[TEST {tests_run}] ✗ ERROR: {test_path.name}: {e}")
                    num_failed += 1

            except queue.Empty:
                # No test available yet, check if producer is still alive
                if not producer.is_alive() and test_queue.empty():
                    # Producer died unexpectedly
                    _ts_print(
                        "[MESON] Producer thread died unexpectedly", file=sys.stderr
                    )
                    break
                continue

    except KeyboardInterrupt:
        _ts_print("[MESON] Interrupted by user", file=sys.stderr)
        handle_keyboard_interrupt_properly()
        raise
    finally:
        # Wait for producer to finish
        producer.join(timeout=10.0)

    # Determine overall success
    overall_success = not compilation_failed and num_failed == 0

    # Only show streaming test summary if there were actual tests run
    # Skip verbose "0/0" output when build was up-to-date
    if tests_run > 0 or compilation_failed:
        _ts_print(f"[MESON] Streaming test execution complete:")
        _ts_print(f"  Tests run: {tests_run}")
        if num_passed > 0:
            print_success(f"  Passed: {num_passed}")
        else:
            _ts_print(f"  Passed: {num_passed}")
        if num_failed > 0:
            print_error(f"  Failed: {num_failed}")
        else:
            _ts_print(f"  Failed: {num_failed}")
        if compilation_failed:
            print_error(f"  Compilation: ✗ FAILED")
        else:
            print_success(f"  Compilation: ✓ OK")

    return overall_success, num_passed, num_failed, compilation_output
