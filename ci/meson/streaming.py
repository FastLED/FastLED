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

from ci.meson.compile import _create_error_context_filter
from ci.meson.compiler import get_meson_executable
from ci.meson.output import _print_banner, _print_error, _print_success
from ci.meson.test_execution import MesonTestResult
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.output_formatter import TimestampFormatter
from ci.util.timestamp_print import ts_print as _ts_print


def stream_compile_and_run_tests(
    build_dir: Path,
    test_callback: Callable[[Path], bool],
    target: Optional[str] = None,
    verbose: bool = False,
) -> tuple[bool, int, int]:
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

    Returns:
        Tuple of (overall_success, num_passed, num_failed)
    """
    cmd = [get_meson_executable(), "compile", "-C", str(build_dir)]

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

    # Track compilation success and test results
    compilation_failed = False
    num_passed = 0
    num_failed = 0
    tests_run = 0

    # Queue for completed test executables
    test_queue: queue.Queue[Optional[Path]] = queue.Queue()

    # Sentinel value to signal end of compilation
    COMPILATION_DONE = None

    def producer_thread() -> None:
        """Parse Ninja output and queue completed test executables"""
        nonlocal compilation_failed

        try:
            # Use RunningProcess for streaming output
            # Note: No formatter here - we need raw Ninja output for regex pattern matching
            proc = RunningProcess(
                cmd,
                timeout=600,  # 10 minute timeout for compilation
                auto_run=True,
                check=False,
                env=os.environ.copy(),
            )

            # Stream output line by line
            with proc.line_iter(timeout=None) as it:
                for line in it:
                    # Filter out noisy Meson/Ninja INFO lines that clutter output
                    # These provide no useful information for normal operation
                    stripped = line.strip()
                    if stripped.startswith("INFO:"):
                        continue  # Skip Meson INFO lines
                    if "Entering directory" in stripped:
                        continue  # Skip Ninja directory change messages

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

                    # Echo output for visibility (skip empty lines) - only in verbose mode
                    if stripped and verbose:
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

                            # Skip DLL/shared library files
                            if test_path.suffix.lower() in (".dll", ".so", ".dylib"):
                                continue  # Skip DLL/shared library files

                            if verbose:
                                _ts_print(f"[MESON] Test ready: {test_path.name}")
                            test_queue.put(test_path)

            # Check compilation result
            returncode = proc.wait()
            if returncode != 0:
                _ts_print(
                    f"[MESON] Compilation failed with return code {returncode}",
                    file=sys.stderr,
                )

                # Show error context from compilation output
                error_filter: Callable[[str], None] = _create_error_context_filter(
                    context_lines=20
                )
                output_lines = proc.stdout.splitlines()
                for line in output_lines:
                    error_filter(line)

                compilation_failed = True
            else:
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
            _print_success(f"  Passed: {num_passed}")
        else:
            _ts_print(f"  Passed: {num_passed}")
        if num_failed > 0:
            _print_error(f"  Failed: {num_failed}")
        else:
            _ts_print(f"  Failed: {num_failed}")
        if compilation_failed:
            _print_error(f"  Compilation: ✗ FAILED")
        else:
            _print_success(f"  Compilation: ✓ OK")

    return overall_success, num_passed, num_failed
