from ci.util.global_interrupt_handler import handle_keyboard_interrupt


#!/usr/bin/env python3
"""Meson build system integration for FastLED example compilation and execution."""

import os
import sys


# Force UTF-8 with replace error handler for child processes (including Meson) on Windows.
# Without this, Meson's mtest.py crashes with UnicodeEncodeError when test output
# contains non-ASCII characters (e.g., ✓) and the console uses cp1252.
if sys.platform == "win32":
    os.environ.setdefault("PYTHONIOENCODING", "utf-8:replace")
import threading
import time
from pathlib import Path

from running_process import RunningProcess

from ci.meson.build_config import perform_ninja_maintenance, setup_meson_build
from ci.meson.compiler import check_meson_installed, get_meson_executable
from ci.meson.test_execution import MesonTestResult
from ci.util.build_lock import libfastled_build_lock
from ci.util.output_formatter import TimestampFormatter, create_filtering_echo_callback
from ci.util.timestamp_print import ts_print as _ts_print


# Detect CI environment for enhanced logging
_IS_CI = os.environ.get("GITHUB_ACTIONS") == "true" or os.environ.get("CI") == "true"


def _get_memory_usage_mb() -> float | None:
    """Get current process memory usage in MB. Returns None if unavailable."""
    try:
        import psutil

        process = psutil.Process()
        return process.memory_info().rss / (1024 * 1024)
    except ImportError:
        return None
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
        raise
    except Exception:
        return None


class CompilationHeartbeat:
    """Provides periodic heartbeat output during long compilations to prevent CI timeouts."""

    def __init__(self, interval_seconds: int = 30):
        self.interval = interval_seconds
        self._stop_event = threading.Event()
        self._thread: threading.Thread | None = None
        self._start_time = time.time()
        self._last_message = ""

    def start(self, message: str = "Compiling") -> None:
        """Start the heartbeat thread."""
        if not _IS_CI:
            return  # Only run heartbeat in CI environments

        self._last_message = message
        self._start_time = time.time()
        self._stop_event.clear()
        self._thread = threading.Thread(target=self._heartbeat_loop, daemon=True)
        self._thread.start()

    def update_message(self, message: str) -> None:
        """Update the status message shown in heartbeat."""
        self._last_message = message

    def stop(self) -> None:
        """Stop the heartbeat thread."""
        self._stop_event.set()
        if self._thread:
            self._thread.join(timeout=2.0)
            self._thread = None

    def _heartbeat_loop(self) -> None:
        """Periodically print status to keep CI alive and show progress."""
        heartbeat_count = 0
        while not self._stop_event.wait(self.interval):
            heartbeat_count += 1
            elapsed = time.time() - self._start_time
            minutes = int(elapsed // 60)
            seconds = int(elapsed % 60)

            # Build heartbeat message with optional memory info
            mem_mb = _get_memory_usage_mb()
            if mem_mb is not None:
                mem_info = f", mem: {mem_mb:.0f}MB"
            else:
                mem_info = ""

            # Print heartbeat with elapsed time and memory usage
            _ts_print(
                f"[HEARTBEAT {heartbeat_count}] {self._last_message} "
                f"(elapsed: {minutes}m {seconds}s{mem_info})",
                file=sys.stderr,
            )
            # Flush to ensure CI captures the output
            sys.stderr.flush()
            sys.stdout.flush()


def compile_examples(
    build_dir: Path,
    examples: list[str] | None = None,
    verbose: bool = False,
    parallel: bool = True,
    build_mode: str = "quick",
    log_failures: Path | None = None,
) -> bool:
    """
    Compile FastLED examples using Meson.

    Args:
        build_dir: Meson build directory
        examples: List of example names to compile (None = all)
        verbose: Enable verbose compilation output
        parallel: Enable parallel compilation (default: True)
        build_mode: Build mode ("quick", "debug", or "release")

    Returns:
        True if compilation successful, False otherwise
    """
    # Build command
    cmd = [get_meson_executable(), "compile", "-C", str(build_dir)]

    if verbose:
        cmd.append("-v")

    if not parallel:
        # Sequential compilation (-j 1)
        cmd.extend(["-j", "1"])

    # Determine targets to build
    # Note: process_group already shows "Compiling: <examples>" status
    if examples is None:
        # Build all examples via the alias target
        cmd.append("examples-host")
        target_desc = "all examples"
    else:
        # Build specific example targets
        for example_name in examples:
            cmd.append(f"example-{example_name}")
        target_desc = ", ".join(examples)

    # Start heartbeat for CI environments during long compilations
    heartbeat = CompilationHeartbeat(interval_seconds=30)
    heartbeat.start(f"Compiling {target_desc} ({build_mode} mode)")

    try:
        # Use RunningProcess for streaming output
        proc = RunningProcess(
            cmd,
            timeout=600,  # 10 minute timeout
            auto_run=True,
            check=False,  # We'll check returncode manually
            # DLLs are auto-deployed by clang-tool-chain 1.0.31+ to output directory
            output_formatter=TimestampFormatter(),
        )

        # Use filtering callback in verbose mode to suppress noise patterns
        echo_callback = create_filtering_echo_callback() if verbose else False
        returncode = proc.wait(echo=echo_callback)

        # Stop heartbeat before reporting result
        heartbeat.stop()

        if returncode != 0:
            _ts_print(
                f"Compilation failed (return code {returncode})",
                file=sys.stderr,
            )

            # Show output if not already shown
            if not verbose and proc.stdout:
                _ts_print("Compilation output:", file=sys.stderr)
                _ts_print(proc.stdout, file=sys.stderr)

            # Write per-example compile failure logs
            if log_failures is not None:
                import re

                log_failures.mkdir(parents=True, exist_ok=True)
                # Parse ninja FAILED lines to identify per-example failures
                # Note: proc.stdout may have timestamps (e.g., "7.59 FAILED: ...")
                # and [code=N] prefixes, so use "FAILED:" in line, not startswith
                failures: dict[str, list[str]] = {}
                lines = proc.stdout.splitlines()
                current_target: str | None = None
                current_lines: list[str] = []
                for line in lines:
                    if "FAILED:" in line:
                        if current_target and current_lines:
                            failures.setdefault(current_target, []).extend(
                                current_lines
                            )
                        target = line.split("FAILED:", 1)[1].strip()
                        # Strip optional [code=N] prefix
                        target = re.sub(r"^\[code=\d+\]\s*", "", target)
                        m = re.match(r"examples[/\\]example-([^.]+)\.", target)
                        current_target = m.group(1) if m else "unknown"
                        current_lines = [line]
                    elif current_target is not None:
                        if "ninja:" in line and "build stopped" in line:
                            if current_target and current_lines:
                                failures.setdefault(current_target, []).extend(
                                    current_lines
                                )
                            current_target = None
                            current_lines = []
                        else:
                            current_lines.append(line)
                if current_target and current_lines:
                    failures.setdefault(current_target, []).extend(current_lines)

                if failures:
                    for name, err_lines in failures.items():
                        log_path = log_failures / f"{name}_compile.log"
                        with open(
                            log_path, "w", encoding="utf-8", errors="replace"
                        ) as f:
                            f.write("\n".join(err_lines))
                else:
                    # Couldn't parse per-target; write full output
                    log_path = log_failures / "compile_compile.log"
                    with open(log_path, "w", encoding="utf-8", errors="replace") as f:
                        f.write(proc.stdout)

            return False

        # Note: Don't print "Compilation successful" - it's redundant
        # The transition to "Running:" phase implies compilation succeeded
        return True

    except KeyboardInterrupt as ki:
        heartbeat.stop()
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        heartbeat.stop()
        _ts_print(f"Compilation failed: {e}", file=sys.stderr)
        return False


def run_examples(
    build_dir: Path,
    examples: list[str] | None = None,
    verbose: bool = False,
    timeout: int = 30,
    build_mode: str = "quick",
    log_failures: Path | None = None,
) -> MesonTestResult:
    """
    Run compiled FastLED examples using Meson test runner.

    Args:
        build_dir: Meson build directory
        examples: List of example names to run (None = all)
        verbose: Enable verbose test output
        timeout: Timeout per example in seconds (default: 30)
        build_mode: Build mode ("quick", "debug", or "release")

    Returns:
        MesonTestResult with success status, duration, and test counts
    """
    # Build command
    cmd = [get_meson_executable(), "test", "-C", str(build_dir), "--print-errorlogs"]

    if verbose:
        cmd.append("-v")

    # Set timeout
    cmd.extend(["--timeout-multiplier", str(timeout / 30.0)])  # 30s is default

    # Determine which tests to run
    if examples is None:
        # Run all tests in the 'examples' suite
        cmd.extend(["--suite", "examples"])
        target_desc = "all examples"
        if not verbose:
            _ts_print("Running all examples...")
    else:
        # Run specific examples by name pattern
        # Meson test names are "example-<name>"
        for example_name in examples:
            # Add each example as a separate test argument
            cmd.append(f"example-{example_name}")
        target_desc = ", ".join(examples)
        # In verbose mode, meson output shows running tests - skip redundant message
        if not verbose:
            _ts_print(f"Running: {target_desc}")

    start_time = time.time()
    num_passed = 0
    num_failed = 0
    num_run = 0

    # Start heartbeat for CI environments during long test runs
    heartbeat = CompilationHeartbeat(interval_seconds=30)
    heartbeat.start(f"Running {target_desc} ({build_mode} mode)")

    try:
        # Use RunningProcess for streaming output
        proc = RunningProcess(
            cmd,
            timeout=1800,  # 30 minute total timeout
            auto_run=True,
            check=False,  # We'll check returncode manually
            # DLLs are auto-deployed by clang-tool-chain 1.0.31+ to output directory
            output_formatter=TimestampFormatter(),
        )

        # Use filtering callback in verbose mode to suppress noise patterns
        echo_callback = create_filtering_echo_callback() if verbose else False
        returncode = proc.wait(echo=echo_callback)

        # Stop heartbeat before reporting result
        heartbeat.stop()

        if returncode != 0:
            _ts_print(
                f"Examples failed (return code {returncode})",
                file=sys.stderr,
            )
            # Write per-example run failure logs from testlog.txt
            if log_failures is not None:
                from ci.meson.testlog_parser import parse_testlog

                testlog = build_dir / "meson-logs" / "testlog.txt"
                entries = parse_testlog(testlog)
                for entry in entries:
                    if "exit status 0" in entry.result:
                        continue
                    content = f"# Test: {entry.test_name}\n"
                    content += f"# Result: {entry.result}\n"
                    content += f"# Duration: {entry.duration}\n\n"
                    if entry.stdout:
                        content += "--- stdout ---\n" + entry.stdout + "\n\n"
                    if entry.stderr:
                        content += "--- stderr ---\n" + entry.stderr + "\n"
                    safe_name = entry.test_name.replace(":", "_").replace("/", "_")
                    log_failures.mkdir(parents=True, exist_ok=True)
                    log_path = log_failures / f"{safe_name}_run.log"
                    with open(log_path, "w", encoding="utf-8", errors="replace") as fh:
                        fh.write(content)
            return MesonTestResult(
                success=False,
                duration=time.time() - start_time,
                num_tests_run=num_run,
                num_tests_passed=num_passed,
                num_tests_failed=num_failed,
            )

        # Note: Don't print "All examples passed" - the results table shows pass/fail status
        return MesonTestResult(
            success=True,
            duration=time.time() - start_time,
            num_tests_run=num_run,
            num_tests_passed=num_passed,
            num_tests_failed=num_failed,
        )

    except KeyboardInterrupt as ki:
        heartbeat.stop()
        handle_keyboard_interrupt(ki)
        raise
    except Exception as e:
        heartbeat.stop()
        duration = time.time() - start_time
        _ts_print(f"Execution failed: {e}", file=sys.stderr)
        return MesonTestResult(
            success=False,
            duration=duration,
            num_tests_run=num_run,
            num_tests_passed=num_passed,
            num_tests_failed=num_failed,
        )


def run_meson_examples(
    source_dir: Path,
    build_dir: Path,
    examples: list[str] | None = None,
    clean: bool = False,
    verbose: bool = False,
    debug: bool = False,
    build_mode: str | None = None,
    no_pch: bool = False,
    parallel: bool = True,
    full: bool = False,
    log_failures: Path | None = None,
) -> MesonTestResult:
    """
    Complete Meson example build and execution workflow.

    Args:
        source_dir: Project root directory
        build_dir: Build output directory (will be made mode-specific)
        examples: List of example names to compile/run (None = all)
        clean: Clean build directory before setup
        verbose: Enable verbose output
        debug: Enable debug mode (full symbols + sanitizers)
        build_mode: Override build mode ('quick', 'debug', 'release')
        no_pch: Disable precompiled headers (NOT IMPLEMENTED - PCH always enabled)
        parallel: Enable parallel compilation (default: True)
        full: Execute examples after compilation (default: False)

    Returns:
        MesonTestResult with success status, duration, and test counts
    """
    start_time = time.time()

    # Determine build mode (build_mode parameter takes precedence over debug flag)
    if build_mode is None:
        build_mode = "debug" if debug else "quick"

    # Validate build mode
    valid_modes = ["quick", "debug", "release", "profile"]
    if build_mode not in valid_modes:
        _ts_print(
            f"[MESON] Error: Invalid build mode: {build_mode}. "
            f"Valid modes: {', '.join(valid_modes)}",
            file=sys.stderr,
        )
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    # Construct mode-specific build directory
    # This enables caching per mode when source unchanged but flags differ
    # Example: .build/meson -> .build/meson-debug
    original_build_dir = build_dir
    build_dir = build_dir.parent / f"{build_dir.name}-{build_mode}"

    # Build directory removed from output - build mode already shown in Config line
    # and full path rarely useful to users (just noise)

    # Check if Meson is installed
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

    # Clean if requested
    if clean and build_dir.exists():
        _ts_print(f"[MESON] Cleaning build directory: {build_dir}")
        import shutil

        shutil.rmtree(build_dir)

    # When specific examples are requested, disable filters to ensure they can be built
    # even if they have platform restrictions.
    # Use a marker file to track the filter state so we only reconfigure when it changes.
    # Previously this forced reconfigure on EVERY run with specific examples, causing
    # ~15-22s rebuilds when nothing changed (meson setup regenerates build.ninja,
    # which makes ninja think all 300+ targets are stale).
    force_reconfigure = False
    filter_marker = build_dir / ".example_filters_disabled"
    if examples is not None and len(examples) > 0:
        os.environ["FASTLED_IGNORE_EXAMPLE_FILTERS"] = "1"
        # Yellow warning (ANSI code 33 for yellow)
        _ts_print(
            "\033[93m⚠  Example filters disabled for specific example(s): "
            + ", ".join(examples)
            + "\033[0m"
        )
        # Only reconfigure if filters were previously enabled (marker absent)
        if not filter_marker.exists():
            # Invalidate cache to force re-discovery with filters disabled
            cache_file = build_dir / "examples" / "example_metadata.cache"
            if cache_file.exists():
                cache_file.unlink()
            force_reconfigure = True
            # Create marker so subsequent runs skip reconfigure
            try:
                build_dir.mkdir(parents=True, exist_ok=True)
                filter_marker.touch()
            except OSError:
                pass
    else:
        # Running all examples (no specific selection) - filters should be active
        if filter_marker.exists():
            # Filters were previously disabled, need to reconfigure to re-enable them
            try:
                filter_marker.unlink()
            except OSError:
                pass
            cache_file = build_dir / "examples" / "example_metadata.cache"
            if cache_file.exists():
                cache_file.unlink()
            force_reconfigure = True

    # Setup build with explicit build_mode to ensure proper cache invalidation
    if not setup_meson_build(
        source_dir,
        build_dir,
        debug=(build_mode == "debug"),
        reconfigure=force_reconfigure,
        build_mode=build_mode,
        verbose=verbose,
        enable_unit_tests=False,
    ):
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    # Perform periodic maintenance on Ninja dependency database
    perform_ninja_maintenance(build_dir)

    # Compile examples with build lock to prevent conflicts
    try:
        with libfastled_build_lock():  # uses LOCK_TIMEOUT_S default
            if not compile_examples(
                build_dir,
                examples=examples,
                verbose=verbose,
                parallel=parallel,
                build_mode=build_mode,
                log_failures=log_failures,
            ):
                duration = time.time() - start_time
                out: MesonTestResult = MesonTestResult.construct_build_error(
                    time.time() - start_time
                )
                return out
    except TimeoutError as e:
        _ts_print(f"[MESON] {e}", file=sys.stderr)
        duration = time.time() - start_time
        out: MesonTestResult = MesonTestResult.construct_build_error(duration)
        return out

    # If full mode, run the examples
    if full:
        result = run_examples(
            build_dir,
            examples=examples,
            verbose=verbose,
            timeout=60,
            build_mode=build_mode,
            log_failures=log_failures,
        )
        return result
    else:
        # Just compilation, return success
        duration = time.time() - start_time
        _ts_print(f"Compilation complete ({duration:.2f}s)")
        return MesonTestResult(
            success=True,
            duration=duration,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )


if __name__ == "__main__":
    # Simple CLI for testing
    import argparse

    parser = argparse.ArgumentParser(
        description="Meson example compilation and execution runner for FastLED"
    )
    parser.add_argument(
        "examples", nargs="*", help="Specific examples to compile/run (None = all)"
    )
    parser.add_argument("--clean", action="store_true", help="Clean build directory")
    parser.add_argument("--verbose", "-v", action="store_true", help="Verbose output")
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Enable debug mode (full symbols + sanitizers)",
    )
    parser.add_argument(
        "--build-mode",
        type=str,
        choices=["quick", "debug", "release", "profile"],
        default=None,
        help="Override build mode (default: quick, or debug if --debug flag set)",
    )
    parser.add_argument(
        "--no-pch", action="store_true", help="Disable precompiled headers (ignored)"
    )
    parser.add_argument(
        "--no-parallel", action="store_true", help="Disable parallel compilation"
    )
    parser.add_argument(
        "--full", action="store_true", help="Execute examples after compilation"
    )
    parser.add_argument("--build-dir", default=".build/meson", help="Build directory")
    parser.add_argument(
        "--log-failures",
        type=str,
        default=None,
        help="Directory to write per-example failure logs",
    )

    args = parser.parse_args()

    source_dir = Path.cwd()
    build_dir = Path(args.build_dir)

    examples_list = args.examples if args.examples else None

    result = run_meson_examples(
        source_dir=source_dir,
        build_dir=build_dir,
        examples=examples_list,
        clean=args.clean,
        verbose=args.verbose,
        debug=args.debug,
        build_mode=args.build_mode,
        no_pch=args.no_pch,
        parallel=not args.no_parallel,
        full=args.full,
        log_failures=Path(args.log_failures) if args.log_failures else None,
    )

    sys.exit(0 if result.success else 1)
