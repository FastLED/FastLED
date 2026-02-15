#!/usr/bin/env python3
"""Meson build system runner for FastLED tests."""

import argparse
import os
import shutil
import sys
import time
from pathlib import Path
from typing import Optional

from clang_tool_chain import prepare_sanitizer_environment
from running_process import RunningProcess

from ci.meson.build_config import (
    cleanup_build_artifacts,
    perform_ninja_maintenance,
    setup_meson_build,
)
from ci.meson.compile import (
    CompileResult,
    _is_compilation_error,
    compile_meson,
    is_stale_build_error,
)
from ci.meson.compiler import check_meson_installed, get_meson_executable
from ci.meson.output import _print_banner, _print_error, _print_info, _print_success
from ci.meson.phase_tracker import PhaseTracker
from ci.meson.streaming import stream_compile_and_run_tests
from ci.meson.test_discovery import get_fuzzy_test_candidates
from ci.meson.test_execution import MesonTestResult, run_meson_test
from ci.util.build_lock import libfastled_build_lock
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly
from ci.util.output_formatter import TimestampFormatter, create_filtering_echo_callback
from ci.util.timestamp_print import ts_print as _ts_print


def _recover_stale_build(
    source_dir: Path,
    build_dir: Path,
    debug: bool,
    check: bool,
    build_mode: str,
    verbose: bool,
) -> bool:
    """
    Attempt to recover from a stale build state.

    This is called when compilation fails due to missing/renamed/deleted files.
    It cleans stale Ninja deps, clears metadata caches, and forces Meson
    reconfiguration so the next compilation attempt has a fresh build graph.

    Args:
        source_dir: Project root directory
        build_dir: Meson build directory
        debug: Debug mode flag
        check: IWYU check flag
        build_mode: Build mode string
        verbose: Verbose output flag

    Returns:
        True if recovery setup succeeded (caller should retry compilation),
        False if recovery failed
    """
    _ts_print("=" * 80)
    _ts_print("[MESON] 🔧 SELF-HEALING: Stale build state detected - auto-recovering")
    _ts_print("=" * 80)
    _ts_print("[MESON] This typically happens when files are renamed or deleted.")
    _ts_print("[MESON] Cleaning stale dependencies and reconfiguring...")

    try:
        # Step 1: Clean stale ninja deps using ninja -t cleandead
        ninja_exe = shutil.which("ninja") or str(
            Path(sys.prefix) / "Scripts" / "ninja.EXE"
        )
        try:
            import subprocess

            result = subprocess.run(
                [ninja_exe, "-C", str(build_dir), "-t", "cleandead"],
                capture_output=True,
                text=True,
                timeout=60,
            )
            if result.returncode == 0:
                _ts_print(
                    f"[MESON] 🗑️  Cleaned stale Ninja outputs: {result.stdout.strip()}"
                )
        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            _ts_print(f"[MESON] Warning: ninja cleandead failed: {e}")

        # Step 2: Delete .ninja_deps to force full dependency re-scan
        ninja_deps = build_dir / ".ninja_deps"
        if ninja_deps.exists():
            try:
                ninja_deps.unlink()
                _ts_print("[MESON] 🗑️  Deleted stale .ninja_deps")
            except OSError as e:
                _ts_print(f"[MESON] Warning: Could not delete .ninja_deps: {e}")

        # Step 3: Clear metadata caches
        cache_files = [
            build_dir / "src_metadata.cache",
            build_dir / "test_list_cache.txt",
            build_dir / ".source_files_hash",
            build_dir / "tests" / "test_metadata.cache",
        ]
        for cache_file in cache_files:
            if cache_file.exists():
                try:
                    cache_file.unlink()
                    _ts_print(f"[MESON] 🗑️  Deleted cache: {cache_file.name}")
                except OSError as e:
                    _ts_print(
                        f"[MESON] Warning: Could not delete {cache_file.name}: {e}"
                    )

        # Step 4: Clean build artifacts (stale .obj files referencing old headers)
        cleanup_build_artifacts(build_dir, "Stale build recovery")

        # Step 5: Force Meson reconfiguration
        _ts_print("[MESON] 🔄 Forcing Meson reconfiguration...")
        success = setup_meson_build(
            source_dir,
            build_dir,
            reconfigure=True,
            debug=debug,
            check=check,
            build_mode=build_mode,
            verbose=verbose,
        )

        if success:
            _ts_print("[MESON] ✅ Self-healing reconfiguration complete")
            _ts_print("[MESON] 🔄 Retrying compilation...")
        else:
            _ts_print("[MESON] ❌ Self-healing reconfiguration failed")

        return success

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        _ts_print(f"[MESON] ❌ Self-healing failed with exception: {e}")
        return False


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

    Returns:
        MesonTestResult with success status, duration, and test counts
    """
    start_time = time.time()

    # Determine build mode: explicit build_mode parameter takes precedence over debug flag
    if build_mode is None:
        build_mode = "debug" if debug else "quick"

    # Validate build_mode
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

    # Construct mode-specific build directory
    # This enables caching libfastled.a per mode when source unchanged but flags differ
    # Example: .build/meson-quick, .build/meson-debug, .build/meson-release
    original_build_dir = build_dir
    build_dir = build_dir.parent / f"{build_dir.name}-{build_mode}"

    # Build dir and mode info is consolidated in setup_meson_build output
    # No need for separate verbose messages here

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
        shutil.rmtree(build_dir)

    # Setup build
    # Pass debug=True when build_mode is "debug" to enable sanitizers and full symbols
    # Also pass explicit build_mode to ensure proper cache invalidation on mode changes
    use_debug = build_mode == "debug"

    # Configure ASan/LSan options for debug mode using clang-tool-chain's API
    # This automatically:
    # - Sets ASAN_OPTIONS with optimal settings (fast_unwind_on_malloc=0:symbolize=1)
    # - Sets LSAN_OPTIONS with optimal settings
    # - Sets ASAN_SYMBOLIZER_PATH to llvm-symbolizer from clang-tool-chain
    # The API only injects settings when sanitizers are detected in compiler flags
    if use_debug:
        # Get sanitizer-configured environment from clang-tool-chain
        # Pass compiler flags so the API knows sanitizers are active
        sanitizer_flags = ["-fsanitize=address"]
        sanitizer_env = prepare_sanitizer_environment(
            base_env=os.environ.copy(),
            compiler_flags=sanitizer_flags,
        )
        # Update current environment with sanitizer settings
        os.environ.update(sanitizer_env)

        # Add LSAN suppression file for known false positives (pthread library leaks)
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

    _ts_print(f"Preparing build ({build_mode} mode)...")

    # Initialize phase tracker for error diagnostics
    phase_tracker = PhaseTracker(build_dir, build_mode)
    phase_tracker.set_phase("CONFIGURE")

    if not setup_meson_build(
        source_dir,
        build_dir,
        reconfigure=False,
        debug=use_debug,
        check=check,
        build_mode=build_mode,
        verbose=verbose,
    ):
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    # Perform periodic maintenance on Ninja dependency database (once per day)
    # This helps prevent .ninja_deps corruption and keeps builds fast
    perform_ninja_maintenance(build_dir)

    # Note: PCH dependency tracking is handled automatically by Ninja via depfiles.
    # The compile_pch.py wrapper ensures depfiles reference the .pch output correctly,
    # and Ninja loads these dependencies into .ninja_deps database (897 headers tracked).
    # No manual staleness checking needed - Ninja rebuilds PCH when any dependency changes.

    # Convert test name to executable name (convert to lowercase to match Meson target naming)
    # Support both test_*.exe and *.exe naming patterns with fallback
    # Try test_<name> first (for tests like test_async.cpp), then fallback to <name> (for async.cpp)
    meson_test_name: Optional[str] = None
    fallback_test_name: Optional[str] = None
    fuzzy_candidates: list[str] = []
    if test_name:
        # Convert to lowercase to match Meson target naming convention
        test_name_lower = test_name.lower()

        # Check if test name already starts with "test_"
        if test_name_lower.startswith("test_"):
            # Already has test_ prefix, try as-is first, then without prefix as fallback
            meson_test_name = test_name_lower
            # Fallback: strip test_ prefix to try bare name
            fallback_test_name = test_name_lower[5:]  # Remove "test_" prefix
        else:
            # Try with test_ prefix first, then fallback to without prefix
            meson_test_name = f"test_{test_name_lower}"
            fallback_test_name = test_name_lower

        # Build fuzzy candidate list using meson introspect after setup
        # This handles patterns like fl_async.exe when user specifies "async"
        # Note: We'll populate this after meson setup, since introspect requires
        # a configured build directory
        # For now, try legacy file-based approach as fallback
        tests_dir = build_dir / "tests"
        if tests_dir.exists():
            # Look for executables matching *<name>*
            import glob

            exe_pattern = f"*{test_name_lower}*.exe"
            matches = glob.glob(str(tests_dir / exe_pattern))
            # Extract just the executable name without path and extension
            for match_path in matches:
                exe_name = Path(match_path).stem  # Remove .exe extension
                # Add to candidates if not already in primary/fallback
                if exe_name not in [meson_test_name, fallback_test_name]:
                    fuzzy_candidates.append(exe_name)

    # Compile and test with build lock to prevent conflicts with example builds
    # STREAMING MODE: When no specific test requested, use stream_compile_and_run_tests()
    # for parallel compilation + execution
    use_streaming = not meson_test_name

    try:
        with libfastled_build_lock(timeout=600):  # 10 minute timeout
            if use_streaming:
                # STREAMING EXECUTION PATH
                # Compile and run tests in parallel - as soon as a test finishes linking,
                # it's immediately executed while other tests continue compiling
                if verbose:
                    _ts_print(
                        "[MESON] Using streaming execution (compile + test in parallel)"
                    )

                # Create test callback for streaming execution
                def test_callback(test_path: Path) -> bool:
                    """Run a single test executable and return success status"""
                    try:
                        # Use current environment which includes ASAN_OPTIONS if in debug mode
                        # (set at top of run_meson_build_and_test)
                        proc = RunningProcess(
                            [str(test_path)],
                            cwd=source_dir,  # Run from project root
                            timeout=600,  # 10 minute timeout per test
                            auto_run=True,
                            check=False,
                            env=os.environ.copy(),
                            output_formatter=TimestampFormatter(),
                        )

                        # Use filtering callback in verbose mode to suppress noise patterns
                        echo_callback = (
                            create_filtering_echo_callback() if verbose else False
                        )
                        returncode = proc.wait(echo=echo_callback)
                        return returncode == 0

                    except KeyboardInterrupt:
                        handle_keyboard_interrupt_properly()
                        raise
                    except Exception as e:
                        _ts_print(f"[MESON] Test execution error: {e}", file=sys.stderr)
                        return False

                # Determine compile timeout based on build mode
                # Debug builds with ASAN are significantly slower, especially on Windows CI
                # Default: 10 minutes (600s), Debug: 45 minutes (2700s)
                compile_timeout = 2700 if use_debug else 600

                # Run streaming compilation and testing for UNIT TESTS
                (
                    overall_success_tests,
                    num_passed_tests,
                    num_failed_tests,
                    compile_output_tests,
                ) = stream_compile_and_run_tests(
                    build_dir=build_dir,
                    test_callback=test_callback,
                    target=None,  # Build all default test targets (unit tests)
                    verbose=verbose,
                    compile_timeout=compile_timeout,
                )

                # SELF-HEALING: If compilation failed due to stale build state,
                # recover and retry once
                if not overall_success_tests and is_stale_build_error(
                    compile_output_tests
                ):
                    if _recover_stale_build(
                        source_dir, build_dir, use_debug, check, build_mode, verbose
                    ):
                        # Retry compilation after recovery
                        (
                            overall_success_tests,
                            num_passed_tests,
                            num_failed_tests,
                            compile_output_tests,
                        ) = stream_compile_and_run_tests(
                            build_dir=build_dir,
                            test_callback=test_callback,
                            target=None,
                            verbose=verbose,
                            compile_timeout=compile_timeout,
                        )

                # Run streaming compilation and testing for EXAMPLES
                if verbose:
                    _ts_print("[MESON] Starting examples compilation and execution...")
                (
                    overall_success_examples,
                    num_passed_examples,
                    num_failed_examples,
                    compile_output_examples,
                ) = stream_compile_and_run_tests(
                    build_dir=build_dir,
                    test_callback=test_callback,
                    target="examples-host",  # Build examples explicitly
                    verbose=verbose,
                    compile_timeout=compile_timeout,
                )

                # SELF-HEALING: If examples compilation failed due to stale build state,
                # recover and retry once
                if not overall_success_examples and is_stale_build_error(
                    compile_output_examples
                ):
                    if _recover_stale_build(
                        source_dir, build_dir, use_debug, check, build_mode, verbose
                    ):
                        # Retry compilation after recovery
                        (
                            overall_success_examples,
                            num_passed_examples,
                            num_failed_examples,
                            compile_output_examples,
                        ) = stream_compile_and_run_tests(
                            build_dir=build_dir,
                            test_callback=test_callback,
                            target="examples-host",
                            verbose=verbose,
                            compile_timeout=compile_timeout,
                        )

                # Combine results
                overall_success = overall_success_tests and overall_success_examples
                num_passed = num_passed_tests + num_passed_examples
                num_failed = num_failed_tests + num_failed_examples

                duration = time.time() - start_time
                num_tests_run = num_passed + num_failed

                # FALLBACK: If no tests were run during streaming (e.g., everything already compiled),
                # fall back to running all tests via Meson test runner
                # Show simplified message instead of detailed "0/0" breakdown
                if num_tests_run == 0 and overall_success:
                    if verbose:
                        _print_info(
                            "[MESON] Build up-to-date, running existing tests..."
                        )
                    # Note: run_meson_test already prints the RUNNING TESTS banner
                    result = run_meson_test(
                        build_dir,
                        test_name=None,
                        verbose=verbose,
                        exclude_suites=exclude_suites,
                    )
                    return result

                if not overall_success:
                    _print_error(
                        f"[MESON] ❌ Some tests failed ({num_passed}/{num_tests_run} tests in {duration:.2f}s)"
                    )
                    return MesonTestResult(
                        success=False,
                        duration=duration,
                        num_tests_run=num_tests_run,
                        num_tests_passed=num_passed,
                        num_tests_failed=num_failed,
                    )

                _print_success(
                    f"✅ All tests passed ({num_passed}/{num_tests_run} in {duration:.2f}s)"
                )
                return MesonTestResult(
                    success=True,
                    duration=duration,
                    num_tests_run=num_tests_run,
                    num_tests_passed=num_passed,
                    num_tests_failed=num_failed,
                )

            else:
                # TRADITIONAL SEQUENTIAL PATH
                # Used for specific test requests
                compile_target: Optional[str] = None
                if meson_test_name:
                    compile_target = meson_test_name

                # Try target resolution with fallback - suppress all output during discovery
                # to avoid confusing users with internal target name guessing
                targets_to_try = [compile_target]
                if fallback_test_name and fallback_test_name != compile_target:
                    targets_to_try.append(fallback_test_name)

                # Build the full list of candidates up front so we can try quietly
                if test_name:
                    # Get fuzzy candidates early
                    fuzzy_candidates = (
                        get_fuzzy_test_candidates(build_dir, test_name)
                        if not fuzzy_candidates
                        else fuzzy_candidates
                    )
                    # Add fuzzy candidates not already in the list
                    for c in fuzzy_candidates:
                        if c not in targets_to_try:
                            targets_to_try.append(c)

                # Add path-qualified variants for disambiguation.
                # Meson supports "subdir/target" format to resolve ambiguous names
                # (e.g., "tests/fastled" disambiguates from the library "fastled").
                path_qualified: list[str] = []
                for candidate in targets_to_try:
                    qualified = f"tests/{candidate}"
                    if (
                        candidate
                        and "/" not in candidate
                        and qualified not in targets_to_try
                    ):
                        path_qualified.append(qualified)
                targets_to_try.extend(path_qualified)

                # Determine build mode from build directory name
                build_mode = "unknown"
                if "meson-quick" in str(build_dir):
                    build_mode = "quick"
                elif "meson-debug" in str(build_dir):
                    build_mode = "debug"
                elif "meson-release" in str(build_dir):
                    build_mode = "release"

                # Show build stage banner before compilation starts
                # WARNING: This build progress reporting is essential for user feedback!
                #   Do NOT suppress this message - it provides:
                #   1. Build mode visibility (quick/debug/release)
                #   2. Stage progression (users know compilation has started)
                #   3. Context for subsequent build messages (libraries, PCH, tests)
                _ts_print(f"[BUILD] Building FastLED engine ({build_mode} mode)...")

                # When there are multiple candidates, run all in quiet mode
                # Only show banner once with final resolved target
                has_fallbacks = len(targets_to_try) > 1
                compilation_success = False
                successful_target: Optional[str] = None
                last_error_output: str = ""
                last_result: Optional[CompileResult] = (
                    None  # Track last result for error reporting
                )
                all_suppressed_errors: list[
                    str
                ] = []  # Collect validation errors from all attempts

                for candidate in targets_to_try:
                    # Track compilation phase for error diagnostics
                    phase_tracker.set_phase(
                        "COMPILE",
                        test_name=test_name,
                        target=candidate,
                        path="sequential",
                    )

                    # Use quiet mode when there are fallbacks (suppress failure noise)
                    result = compile_meson(
                        build_dir,
                        target=candidate,
                        quiet=has_fallbacks,
                        verbose=verbose,
                    )
                    last_result = result  # Save for error reporting
                    compilation_success = result.success
                    if compilation_success:
                        # Track link phase for successful compilation
                        phase_tracker.set_phase(
                            "LINK",
                            test_name=test_name,
                            target=candidate,
                            path="sequential",
                        )

                        successful_target = candidate
                        compile_target = candidate
                        # Strip path prefix for executable lookup: meson_test_name is used
                        # to find binaries in build_dir/tests/, so "tests/fastled" → "fastled"
                        meson_test_name = (
                            candidate.removeprefix("tests/") if candidate else candidate
                        )
                        break
                    last_error_output = result.error_output
                    # Collect suppressed validation errors (capped at 5 per attempt)
                    all_suppressed_errors.extend(result.suppressed_errors)

                # Show the final result after target resolution
                if compilation_success and has_fallbacks:
                    _print_banner("Compile", "📦", verbose=verbose)
                    print(f"Compiling: {successful_target}")
                    # Note: Don't print "Compilation successful" - redundant before Running phase

                if not compilation_success:
                    # SMART ERROR DETECTION: Check for saved compilation error
                    last_error_file = (
                        build_dir.parent / "meson-state" / "last-compilation-error.txt"
                    )
                    phase_data = phase_tracker.get_phase()
                    phase_name = (
                        phase_data.get("phase", "COMPILE") if phase_data else "COMPILE"
                    )

                    # Display header with phase context
                    _print_error(f"\n[MESON] ❌ Build FAILED during {phase_name} phase")
                    _print_error("=" * 80)

                    # Always show full log path prominently if available
                    if last_result and last_result.error_log_file:
                        _print_error(f"[MESON] 📄 Full compilation log:")
                        _print_error(f"[MESON]    {last_result.error_log_file}")
                        _print_error("")

                    # Try to show error snippet from saved file or directly from log
                    error_snippet_shown = False

                    if last_error_file.exists():
                        # Show saved error snippet with metadata
                        with open(last_error_file, "r", encoding="utf-8") as f:
                            error_context = f.read()
                            # Print with error line highlighting
                            for line in error_context.splitlines():
                                if _is_compilation_error(line):
                                    _print_error(f"\033[91m{line}\033[0m")  # Red
                                else:
                                    _print_error(line)
                        error_snippet_shown = True

                    elif (
                        last_result
                        and last_result.error_log_file
                        and last_result.error_log_file.exists()
                    ):
                        # Fallback: read last 50 error lines directly from log file
                        _print_error("[MESON] 🔍 Error excerpt from compilation log:")
                        _print_error("")
                        try:
                            with open(
                                last_result.error_log_file, "r", encoding="utf-8"
                            ) as f:
                                log_lines = f.readlines()
                                # Find error lines and show context
                                error_lines = [
                                    (i, line.rstrip())
                                    for i, line in enumerate(log_lines)
                                    if _is_compilation_error(line)
                                ]
                                if error_lines:
                                    # Show last error with context (up to 10 lines)
                                    last_error_idx = error_lines[-1][0]
                                    start_idx = max(0, last_error_idx - 5)
                                    end_idx = min(len(log_lines), last_error_idx + 5)
                                    for line in log_lines[start_idx:end_idx]:
                                        line = line.rstrip()
                                        if _is_compilation_error(line):
                                            _print_error(
                                                f"\033[91m{line}\033[0m"
                                            )  # Red
                                        else:
                                            _print_error(line)
                                    error_snippet_shown = True
                        except KeyboardInterrupt:
                            handle_keyboard_interrupt_properly()
                            raise
                        except Exception as e:
                            _print_error(f"[MESON] ⚠️  Could not read error log: {e}")

                    if error_snippet_shown:
                        _print_error("=" * 80)
                    else:
                        # No error snippet available - show legacy diagnostics
                        # SELF-HEALING DIAGNOSTICS: Show validation errors that were suppressed during fallback
                        if all_suppressed_errors:
                            _print_error(
                                "[MESON] Self-healing triggered - showing suppressed validation errors:"
                            )
                            for err in all_suppressed_errors[
                                :5
                            ]:  # Cap at 5 total (already capped per-attempt)
                                _print_error(f"  {err}")
                            _print_error("")

                        # Print diagnostic: show what was tried and why it failed
                        _print_error(
                            f"[MESON] Compilation failed for test '{test_name}'"
                        )
                        if has_fallbacks:
                            tried = ", ".join(str(t) for t in targets_to_try)
                            _print_error(f"[MESON] Tried targets: {tried}")
                        # Show the last meson error if it's informative (e.g., ambiguous target)
                        if last_error_output:
                            for line in last_error_output.splitlines():
                                if "ERROR:" in line or "Can't invoke target" in line:
                                    _print_error(f"[MESON] {line.strip()}")
                                    break
                        _print_error("=" * 80)

                    return MesonTestResult(
                        success=False,
                        duration=time.time() - start_time,
                        num_tests_run=0,
                        num_tests_passed=0,
                        num_tests_failed=0,
                    )
    except TimeoutError as e:
        _ts_print(f"[MESON] {e}", file=sys.stderr)
        return MesonTestResult(
            success=False,
            duration=time.time() - start_time,
            num_tests_run=0,
            num_tests_passed=0,
            num_tests_failed=0,
        )

    # In IWYU check mode, we only compile (to run static analysis)
    # Skip test execution and return success after successful compilation
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

    # Run tests (traditional path only - streaming already ran tests above)
    # When a specific test is requested, run the executable directly
    # to avoid meson test discovery issues
    if meson_test_name:
        # Find the test executable or DLL
        # Tests can be either:
        # 1. A copied runner .exe (e.g., fl_async.exe) - legacy naming
        # 2. A DLL loaded by the shared runner.exe (e.g., fl_fixed_point_s16x16.dll)
        # 3. A profile test in tests/profile/ subdirectory
        test_cmd: list[str] = []

        # Check for profile tests first (in tests/profile/ subdirectory)
        if meson_test_name.startswith("profile_"):
            profile_exe_path = (
                build_dir / "tests" / "profile" / f"{meson_test_name}.exe"
            )
            if profile_exe_path.exists():
                test_cmd = [str(profile_exe_path)]
            else:
                # Try Unix variant (no .exe extension)
                profile_exe_unix = build_dir / "tests" / "profile" / meson_test_name
                if profile_exe_unix.exists():
                    test_cmd = [str(profile_exe_unix)]

        # If not a profile test, check standard locations
        if not test_cmd:
            test_exe_path = build_dir / "tests" / f"{meson_test_name}.exe"
            if test_exe_path.exists():
                # Found copied runner .exe
                test_cmd = [str(test_exe_path)]

        if not test_cmd:
            # Try Unix variant (no .exe extension)
            test_exe_unix = build_dir / "tests" / meson_test_name
            if test_exe_unix.exists():
                test_cmd = [str(test_exe_unix)]
            else:
                # Try DLL-based test architecture: runner.exe + test.dll
                runner_exe = build_dir / "tests" / "runner.exe"
                test_dll = build_dir / "tests" / f"{meson_test_name}.dll"

                if not runner_exe.exists():
                    # Try compiling the runner target
                    _runner_result = compile_meson(
                        build_dir, target="runner", quiet=True, verbose=verbose
                    )

                if runner_exe.exists() and test_dll.exists():
                    test_cmd = [str(runner_exe), str(test_dll)]
                elif runner_exe.exists() and not test_dll.exists():
                    # Try .so for Linux
                    test_so = build_dir / "tests" / f"{meson_test_name}.so"
                    if test_so.exists():
                        test_cmd = [str(runner_exe), str(test_so)]

        if not test_cmd:
            _ts_print(
                f"[MESON] Error: test executable not found for: {meson_test_name}",
                file=sys.stderr,
            )
            return MesonTestResult(
                success=False,
                duration=time.time() - start_time,
                num_tests_run=0,
                num_tests_passed=0,
                num_tests_failed=0,
            )

        _print_banner("Test", "▶️", verbose=verbose)
        print(f"Running: {meson_test_name}")

        # Track execution phase for error diagnostics
        phase_tracker.set_phase("EXECUTE", test_name=meson_test_name, path="sequential")

        # Run the test executable directly
        try:
            proc = RunningProcess(
                test_cmd,
                cwd=source_dir,  # Run from project root
                timeout=600,  # 10 minute timeout
                auto_run=True,
                check=False,
                env=os.environ.copy(),
                output_formatter=TimestampFormatter(),
            )

            # Use filtering callback in verbose mode to suppress noise patterns
            echo_callback = create_filtering_echo_callback() if verbose else False
            returncode = proc.wait(echo=echo_callback)
            duration = time.time() - start_time

            if returncode != 0:
                # Phase-aware error message
                phase_data = phase_tracker.get_phase()

                if phase_data and phase_data["phase"] == "EXECUTE":
                    _print_error(
                        f"[MESON] ❌ Test FAILED during execution (exit code {returncode})"
                    )
                    _print_error(f"[MESON] Test: {meson_test_name}")
                    # Check if crash vs test failure
                    if returncode == 1 and not proc.stdout.strip():
                        _print_error(
                            f"[MESON] ⚠️  No output - possible crash at startup"
                        )
                else:
                    phase_name = (
                        phase_data.get("phase", "UNKNOWN") if phase_data else "UNKNOWN"
                    )
                    _print_error(
                        f"[MESON] ❌ Build FAILED during {phase_name} phase (exit code {returncode})"
                    )

                # Show test output for failures (even if not verbose)
                if not verbose and proc.stdout:
                    _print_error("[MESON] Test output:")
                    for line in proc.stdout.splitlines()[-50:]:  # Last 50 lines
                        _ts_print(f"  {line}")
                return MesonTestResult(
                    success=False,
                    duration=duration,
                    num_tests_run=1,
                    num_tests_passed=0,
                    num_tests_failed=1,
                )

            # Clear phase tracking on success
            phase_tracker.clear()

            _print_success(f"✅ All tests passed (1/1 in {duration:.2f}s)")
            return MesonTestResult(
                success=True,
                duration=duration,
                num_tests_run=1,
                num_tests_passed=1,
                num_tests_failed=0,
            )

        except KeyboardInterrupt:
            handle_keyboard_interrupt_properly()
            raise
        except Exception as e:
            duration = time.time() - start_time
            _ts_print(
                f"[MESON] Test execution failed with exception: {e}",
                file=sys.stderr,
            )
            return MesonTestResult(
                success=False,
                duration=duration,
                num_tests_run=1,
                num_tests_passed=0,
                num_tests_failed=1,
            )
    else:
        # No specific test - use Meson's test runner for all tests
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
