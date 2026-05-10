#!/usr/bin/env python3
# Lazy imports: traceback and threading are deferred to avoid paying their
# import cost (~4ms and ~2.5ms respectively) on early-exit paths (CASE 0-4).
# They are imported just-in-time in the code that actually needs them.
from __future__ import annotations

import _thread
import json
import os
import sys
import time
import warnings
from enum import Enum, auto
from pathlib import Path

# Ultra-early-exit cache helpers: imported at module level to avoid heavy
# ci.* imports during startup. See ci/early_exit_cache.py for details.
from ci.early_exit_cache import (
    argv_ultra_early_exit,
    check_single_test_cached,
)


# Record start time as early as possible, before any heavy imports.
# This gives the most accurate timing for ultra-early exit messages.
_START_TIME = time.time()

# Change to script directory for relative path resolution in early exit.
# Ultra-early exit uses relative paths (.cache/, .build/, src/, etc.)
# and requires the working directory to be the project root.
os.chdir(Path(__file__).parent)


# When stdout is piped (e.g., `bash test --cpp 2>&1 | tail -15`), Python
# defaults to block buffering — small writes may not surface until program
# exit, making background test runs appear to "hang silently" even when
# they're producing output. Force line buffering so `\n`-terminated output
# flushes immediately to whatever pipe is reading us.
#
# This must run before any imports that touch stdout (e.g., test runners,
# rich console) so the buffering policy is set on first write.
if not sys.stdout.isatty():
    try:
        sys.stdout.reconfigure(line_buffering=True)  # type: ignore[union-attr]
    except (AttributeError, ValueError):
        pass
if not sys.stderr.isatty():
    try:
        sys.stderr.reconfigure(line_buffering=True)  # type: ignore[union-attr]
    except (AttributeError, ValueError):
        pass


# Ultra-early exit fires BEFORE heavy ci.util.* imports (~318ms savings on cached runs).
# All ultra-early-exit logic is in ci.early_exit_cache (imported above).
# typeguard (177ms), psutil (28ms), asyncio (50ms), unittest.mock (65ms) are skipped
# entirely when the test result is found in cache.
argv_ultra_early_exit(_START_TIME)

# Configure UTF-8 console now that we know we're on the non-early-exit path.
# This was previously in ci/__init__.py but was deferred to avoid ~20ms cost
# on ultra-early-exit paths that only need ci.early_exit_cache (stdlib-only).
import ci  # noqa: PLC0415 - already loaded; just grabbing the module reference


ci._ensure_init()

# --- Heavy imports: only reached if no early exit fired above ---
from ci.util.global_interrupt_handler import (
    handle_keyboard_interrupt,
    signal_interrupt,
    wait_for_cleanup,
)
from ci.util.test_args import parse_args
from ci.util.test_env import (
    dump_thread_stacks,
    setup_environment,
    setup_force_exit,
)
from ci.util.test_types import (
    process_test_flags,
)
from ci.util.timestamp_print import ts_print


class RebuildMode(Enum):
    """Defines the rebuild/cache behavior for test execution."""

    CACHED = auto()  # Use fingerprint cache normally
    NO_CACHE = auto()  # Disable fingerprint cache (--no-fingerprint)
    FULL_REBUILD = auto()  # Force full rebuild (--force, --clean)


import threading  # noqa: PLC0415 - lazy import (not at top level to speed up early-exit paths)


_CANCEL_WATCHDOG = threading.Event()

_TIMEOUT_EVERYTHING = 600

_GDB_CRASH_DIR = Path(".gdb_crash")
_GDB_CRASH_SEEN = _GDB_CRASH_DIR / ".seen"


def _check_crash_dumps() -> None:
    """Check for crash dump files and warn the user."""
    if not _GDB_CRASH_DIR.exists():
        return
    dumps = sorted(_GDB_CRASH_DIR.glob("crash_*.txt"))
    if not dumps:
        return

    # Build a content fingerprint from file names + sizes + mtimes
    import hashlib

    h = hashlib.md5()
    for d in dumps:
        stat = d.stat()
        h.update(f"{d.name}:{stat.st_size}:{stat.st_mtime_ns}".encode())
    current_hash = h.hexdigest()

    # Check if we already warned about these exact dumps
    seen_hash = ""
    if _GDB_CRASH_SEEN.exists():
        seen_hash = _GDB_CRASH_SEEN.read_text().strip()

    RED = "\033[91m"
    YELLOW = "\033[93m"
    RESET = "\033[0m"
    BOLD = "\033[1m"

    if current_hash == seen_hash:
        # Already seen — muted one-liner
        print(
            f"{YELLOW}(crash dumps unchanged in .gdb_crash/ — {len(dumps)} file(s), run `bash lint` to clean){RESET}"
        )
    else:
        # New or changed dumps — loud red warning
        print(f"\n{RED}{BOLD}{'=' * 60}")
        print(f"  CRASH DUMP(S) DETECTED — {len(dumps)} file(s) in .gdb_crash/")
        print(f"{'=' * 60}{RESET}")
        for d in dumps:
            size = d.stat().st_size
            print(f"  {RED}{d.name}{RESET}  ({size} bytes)")
        print(f"{RED}{BOLD}Inspect with: cat .gdb_crash/<file>{RESET}")
        print(f"{RED}{BOLD}Clean with:   bash lint{RESET}")
        print(f"{RED}{BOLD}{'=' * 60}{RESET}\n")
        # Stamp so subsequent runs show the muted warning
        _GDB_CRASH_SEEN.write_text(current_hash)


# Platform to emulator backend mapping for --run command (lazy-loaded).
# Loading reads a JSON file from disk (~18ms). Deferred to first access since
# the mapping is only needed when --run is used.
_RUN_PLATFORM_BACKENDS: dict[str, list[str]] | None = None


def _get_run_platform_backends() -> dict[str, list[str]]:
    global _RUN_PLATFORM_BACKENDS
    if _RUN_PLATFORM_BACKENDS is None:
        backend_path = Path(__file__).parent / "ci" / "runners" / "backends.json"
        with open(backend_path) as f:
            _RUN_PLATFORM_BACKENDS = json.load(f)
    return _RUN_PLATFORM_BACKENDS  # type: ignore[return-value]


if os.environ.get("GITHUB_ACTIONS"):
    _TIMEOUT_EVERYTHING = 1200  # Extended timeout for GitHub CI builds
    ts_print(
        f"GitHub Actions environment detected - using extended timeout: {_TIMEOUT_EVERYTHING} seconds"
    )


# Deadman timer fires this many seconds after the primary watchdog if the
# process is still alive — bypasses any blocked diagnostics in the primary
# watchdog and unconditionally calls os._exit().
_DEADMAN_GRACE_S = 60


def _release_held_build_locks() -> None:
    """Best-effort: release any libfastled_build locks held by this PID.

    Called from the watchdog before force-exit so subsequent runs don't
    block on a stale lock from this hung process. Intentionally suppresses
    KeyboardInterrupt: the watchdog's caller (deadman_timer / watchdog_timer)
    is about to call os._exit(), so cleanup must run to completion regardless
    of signal state. Re-raising would short-circuit the force-exit.
    """
    try:
        from ci.util.lock_database import get_lock_database  # noqa: PLC0415

        db = get_lock_database()
        my_pid = os.getpid()
        for lock in db.list_all_locks():
            if lock["owner_pid"] == my_pid:
                try:
                    db.release(lock["lock_name"], my_pid)
                except KeyboardInterrupt as ki:
                    handle_keyboard_interrupt(ki)
                except Exception:
                    pass
    except KeyboardInterrupt as ki:
        handle_keyboard_interrupt(ki)
    except Exception:
        pass


def make_watch_dog_thread(
    seconds: int,
) -> threading.Thread:  # 60 seconds default timeout
    def deadman_timer() -> None:
        # Unconditional second-tier kill: if the primary watchdog gets stuck
        # in dump_thread_stacks() / psutil queries / blocked I/O, this fires
        # _DEADMAN_GRACE_S later and exits without running any diagnostics.
        time.sleep(seconds + _DEADMAN_GRACE_S)
        if _CANCEL_WATCHDOG.is_set():
            return
        try:
            _release_held_build_locks()
        except KeyboardInterrupt as ki:
            # Suppress: the unconditional os._exit() below is the entire point
            # of the deadman timer. Re-raising would defeat it.
            handle_keyboard_interrupt(ki)
        except Exception:
            pass
        os._exit(3)  # Exit code 3 = deadman fired (primary watchdog also stuck)

    def watchdog_timer() -> None:
        time.sleep(seconds)
        if _CANCEL_WATCHDOG.is_set():
            return

        warnings.warn(f"Watchdog timer expired after {seconds} seconds.")

        # Release any build locks first, before potentially-blocking diagnostics.
        # If diagnostics hang, the deadman timer will still force-exit, but the
        # lock will already be released so subsequent runs aren't blocked.
        _release_held_build_locks()

        dump_thread_stacks()
        ts_print(f"Watchdog timer expired after {seconds} seconds - forcing exit")

        # Dump outstanding running processes (if any)
        try:
            from ci.util.running_process_manager import (
                RunningProcessManagerSingleton,  # noqa: PLC0415
            )

            RunningProcessManagerSingleton.dump_active()
        except KeyboardInterrupt as ki:
            handle_keyboard_interrupt(ki)
            raise
        except Exception as e:
            ts_print(f"Failed to dump active processes: {e}")

        import traceback  # noqa: PLC0415 - lazy: only imported when watchdog fires

        traceback.print_stack()
        time.sleep(0.5)

        os._exit(2)  # Exit with error code 2 to indicate timeout (SIGTERM)

    thr = threading.Thread(target=watchdog_timer, daemon=True, name="WatchdogTimer")
    thr.start()

    # Second-tier deadman: fires at seconds + _DEADMAN_GRACE_S regardless of
    # whether the primary watchdog blocks.
    deadman = threading.Thread(
        target=deadman_timer, daemon=True, name="WatchdogDeadman"
    )
    deadman.start()
    return thr


def main() -> None:
    try:
        # Use module-level start time (recorded before heavy imports for accuracy).
        # _argv_ultra_early_exit() already fired at module level; if we reached main(),
        # the early exit did not apply and we proceed normally.
        start_time = _START_TIME

        # Parse and process arguments
        args = parse_args()

        # Warn about any leftover crash dumps from previous runs
        _check_crash_dumps()

        if args.debug_test:
            from ci.util.global_interrupt_handler import set_debug_test

            set_debug_test(True)

        # Handle --list-tests flag: list available tests and exit
        if args.list_tests:
            from ci.meson.test_discovery import list_all_tests

            list_all_tests(filter_pattern=args.test, filter_type=None)
            sys.exit(0)

        # Handle --setup-only flag: run Meson setup to generate compile_commands.json, then exit.
        # This is used by the install script to avoid a full build when only IntelliSense
        # configuration is needed.
        if args.setup_only:
            from ci.meson.build_config import setup_meson_build

            build_mode = (
                args.build_mode
                if args.build_mode
                else ("debug" if args.debug else "quick")
            )
            source_dir = Path(".")
            build_dir = Path(".build") / f"meson-{build_mode}"
            ts_print(f"Running Meson setup only ({build_mode} mode)...")
            ok = setup_meson_build(
                source_dir,
                build_dir,
                reconfigure=False,
                debug=(build_mode == "debug"),
                build_mode=build_mode,
                verbose=args.verbose,
                enable_examples=True,
                enable_unit_tests=True,
            )
            if ok:
                ts_print("Meson setup complete.")
            else:
                ts_print("Meson setup failed.", file=sys.stderr)
                sys.exit(1)
            sys.exit(0)

        # ULTRA-EARLY EXIT: Check if the requested test result is already cached.
        # Uses check_single_test_cached() from ci.early_exit_cache for the full
        # pipeline: name normalization → candidate matching → ninja skip → result cache.
        if (
            args.test
            and not getattr(args, "clean", False)
            and not getattr(args, "check", False)
            and not getattr(args, "no_fingerprint", False)
        ):
            try:
                _build_mode_early = (
                    args.build_mode
                    if args.build_mode
                    else ("debug" if args.debug else "quick")
                )
                _build_dir_early = Path(".build") / f"meson-{_build_mode_early}"
                if check_single_test_cached(args.test, _build_dir_early):
                    print("✅ All tests passed (1/1 cached)")
                    _CANCEL_WATCHDOG.set()
                    print(f"Total: {time.time() - start_time:.2f}s")
                    sys.exit(0)
            except KeyboardInterrupt:
                _thread.interrupt_main()
            except Exception:
                pass  # If ultra-early check fails, continue normally

        # Handle --no-unity flag
        if args.no_unity:
            ts_print("(--no-unity is assumed by default now)")

        # Default to parallel execution for better performance
        # Users can disable parallel compilation by setting NO_PARALLEL=1 or using --no-parallel
        if os.environ.get("NO_PARALLEL", "0") == "1":
            args.no_parallel = True

        args = process_test_flags(args)

        timeout = _TIMEOUT_EVERYTHING
        # Adjust watchdog timeout based on test configuration
        # Sequential builds with debug mode (sanitizers) are very slow, especially on Windows
        if args.debug and args.no_parallel:
            # 45 minutes for sequential debug builds (sanitizers are slow)
            timeout = 2700
            ts_print(
                f"Adjusted watchdog timeout for sequential debug builds: {timeout} seconds"
            )
        # Sequential examples compilation can take up to 30 minutes
        elif args.examples is not None and args.no_parallel:
            # 35 minutes for sequential examples compilation
            timeout = 2100
            ts_print(
                f"Adjusted watchdog timeout for sequential examples compilation: {timeout} seconds"
            )

        # Set up watchdog timer
        _ = make_watch_dog_thread(seconds=timeout)

        # Handle --no-interactive flag
        if args.no_interactive:
            os.environ["FASTLED_CI_NO_INTERACTIVE"] = "true"
            os.environ["GITHUB_ACTIONS"] = (
                "true"  # This ensures all subprocess also run in non-interactive mode
            )

        # Handle --interactive flag
        if args.interactive:
            os.environ.pop("FASTLED_CI_NO_INTERACTIVE", None)
            os.environ.pop("GITHUB_ACTIONS", None)

        # Set up remaining environment based on arguments
        setup_environment(args)

        # Handle stack trace control
        enable_stack_trace = not args.no_stack_trace
        # Stack trace messages are only useful when debugging timeouts
        # Don't clutter normal output with this implementation detail

        # Validate conflicting arguments
        if args.no_interactive and args.interactive:
            ts_print(
                "Error: --interactive and --no-interactive cannot be used together",
                file=sys.stderr,
            )
            sys.exit(1)

        # Set up fingerprint caching
        # Lazy import: FingerprintManager pulls in ci.util.test_types -> typeguard (~245ms).
        # We defer this until after all ultra-early exits so cached runs skip it entirely.
        from ci.util.fingerprint import FingerprintManager

        cache_dir = Path(".cache")
        # Determine build mode (default to "quick")
        # IMPORTANT: If --debug is set, use "debug" mode even if --build-mode is not specified
        # This ensures fingerprint caching correctly separates debug builds from quick builds
        build_mode = (
            args.build_mode if args.build_mode else ("debug" if args.debug else "quick")
        )
        fingerprint_manager = FingerprintManager(cache_dir, build_mode=build_mode)

        # Calculate fingerprints
        # OPTIMIZATION: When --no-fingerprint is set, skip computing fingerprints entirely.
        # Fingerprint computation reads all source files and hashes them (~2-3s on typical
        # machines). When --no-fingerprint is used, these values are ignored anyway because
        # rebuild_mode forces all change flags to True. Skipping saves ~2-3s per invocation.
        if args.no_fingerprint:
            src_code_change = True
            cpp_test_change = True
            examples_change = True
            python_test_change = True
            wasm_change = True
        else:
            # OPTIMIZATION: Run fingerprint checks in parallel threads.
            # Each check independently scans directories and hashes files (~20-30ms each).
            # Running all 5 concurrently reduces total from ~110ms to ~30ms.
            _fp_results: dict[str, bool] = {}

            def _fp_check(name: str, fn) -> None:
                _fp_results[name] = fn()

            _fp_threads = [
                threading.Thread(
                    target=_fp_check,
                    args=("all", fingerprint_manager.check_all),
                    daemon=True,
                ),
                threading.Thread(
                    target=_fp_check,
                    args=("cpp", lambda: fingerprint_manager.check_cpp(args)),
                    daemon=True,
                ),
                threading.Thread(
                    target=_fp_check,
                    args=("examples", lambda: fingerprint_manager.check_examples(args)),
                    daemon=True,
                ),
                threading.Thread(
                    target=_fp_check,
                    args=("python", fingerprint_manager.check_python),
                    daemon=True,
                ),
                threading.Thread(
                    target=_fp_check,
                    args=("wasm", fingerprint_manager.check_wasm),
                    daemon=True,
                ),
            ]
            for _t in _fp_threads:
                _t.start()
            for _t in _fp_threads:
                _t.join()
            src_code_change = _fp_results["all"]
            cpp_test_change = _fp_results["cpp"]
            examples_change = _fp_results["examples"]
            python_test_change = _fp_results["python"]
            wasm_change = _fp_results["wasm"]

        # Handle --docker flag: run tests in Docker container
        if args.docker:
            from ci.runners.docker_runner import run_docker_tests

            ts_print("=== Docker Testing ===")
            exit_code = run_docker_tests(args)
            sys.exit(exit_code)

        # Handle --run flag (unified emulation interface)
        if args.run is not None:
            if len(args.run) < 1:
                ts_print("Error: --run requires a platform/board (e.g., esp32s3, uno)")
                sys.exit(1)

            platform = args.run[0].lower()

            # Look up backend from mapping table
            backend = None
            for b, platforms in _get_run_platform_backends().items():
                if platform in platforms:
                    backend = b
                    break

            if backend is None:
                # Platform not found - show error with supported platforms
                ts_print(f"Error: Unknown platform '{platform}'")
                ts_print()
                ts_print("Supported platforms:")

                # Group platforms by backend
                for b, platforms in _get_run_platform_backends().items():
                    ts_print(f"  {b.upper()}: {', '.join(sorted(platforms))}")

                sys.exit(1)

            # Route to appropriate backend
            if backend == "qemu":
                from ci.runners.qemu_runner import run_qemu_tests

                ts_print(f"=== QEMU Testing ({platform}) ===")
                # Convert --run to --qemu format for backward compatibility
                args.qemu = args.run
                run_qemu_tests(args)
                return
            elif backend == "avr8js":
                from ci.runners.avr8js_runner import run_avr8js_tests

                ts_print(f"=== avr8js Testing ({platform}) ===")
                # Run avr8js tests
                run_avr8js_tests(args)
                return
            else:
                ts_print(
                    f"Error: Unknown backend '{backend}' for platform '{platform}'"
                )
                sys.exit(1)

        # Handle QEMU testing (deprecated - use --run)
        if args.qemu is not None:
            from ci.runners.qemu_runner import run_qemu_tests

            ts_print("=== QEMU Testing ===")
            ts_print("Note: --qemu is deprecated, use --run instead")
            run_qemu_tests(args)
            return

        # Track test success/failure for fingerprint status
        tests_passed = False

        try:
            # Run tests using the test runner with sequential example compilation
            # Check if we need to use sequential execution to avoid resource conflicts
            if not args.unit and not args.examples and not args.py and args.full:
                # Full test mode - use RunningProcessGroup for dependency-based execution

                from ci.util.running_process_group import (
                    ExecutionMode,
                    ProcessExecutionConfig,
                    RunningProcessGroup,
                )

                # Discover test counts in background to avoid blocking process
                # creation (~1-10s for pytest --collect-only).
                from ci.util.test_runner import (
                    TestCounts,
                    create_examples_test_process,
                    create_python_test_process,
                    get_test_counts,
                )

                _counts_result: list[TestCounts] = []

                def _discover_counts() -> None:
                    try:
                        _counts_result.append(get_test_counts())
                    except KeyboardInterrupt as ki:
                        handle_keyboard_interrupt(ki)
                    except Exception:
                        pass

                _discovery_thread = threading.Thread(
                    target=_discover_counts, daemon=True, name="TestDiscovery"
                )
                _discovery_thread.start()

                # Create processes while discovery runs in background
                python_process = create_python_test_process(
                    enable_stack_trace=False, run_slow=True
                )

                examples_process = create_examples_test_process(
                    args, not args.no_stack_trace
                )

                # Now wait for discovery and display counts
                _discovery_thread.join(timeout=15)
                if _counts_result:
                    counts = _counts_result[0]
                    total_tests = (
                        counts.unit_test_count
                        + counts.example_count
                        + counts.python_test_count
                    )
                    ts_print(
                        f"Found {total_tests} tests: {counts.unit_test_count} unit tests, {counts.example_count} examples, {counts.python_test_count} Python tests"
                    )

                # Configure parallel execution — Python tests and examples are
                # independent and can run concurrently for faster wall-clock time.
                config = ProcessExecutionConfig(
                    execution_mode=ExecutionMode.PARALLEL,
                    verbose=args.verbose,
                    timeout_seconds=2100,  # 35 minutes for examples compilation
                    live_updates=True,  # Enable real-time display
                    display_type="auto",  # Auto-detect best display format
                )

                # Create process group — no dependency between Python tests and examples
                group = RunningProcessGroup(config=config, name="FullTestParallel")
                group.add_process(python_process)
                group.add_process(examples_process)

                try:
                    # Start real-time display for full test mode
                    display_thread = None
                    if not args.verbose and config.live_updates:
                        try:
                            from ci.util.process_status_display import (
                                display_process_status,
                            )

                            display_thread = display_process_status(
                                group,
                                display_type=config.display_type,
                                update_interval=config.update_interval,
                            )
                        except ImportError:
                            pass  # Fall back to normal execution

                    timings = group.run()

                    # Stop display thread if it was started
                    if display_thread:
                        time.sleep(0.5)

                    ts_print("Parallel test execution completed successfully")

                    # Print timing summary
                    if timings:
                        ts_print("\nExecution Summary:")
                        for timing in timings:
                            ts_print(f"  {timing.name}: {timing.duration:.2f}s")
                except KeyboardInterrupt:
                    _thread.interrupt_main()
                    raise
                except Exception as e:
                    ts_print(f"Parallel test execution failed: {e}")
                    sys.exit(1)
            else:
                # Use normal test runner for other cases
                # Determine rebuild mode based on flags (single source of truth)
                if args.no_fingerprint:
                    rebuild_mode = RebuildMode.NO_CACHE
                elif args.force or args.clean:
                    rebuild_mode = RebuildMode.FULL_REBUILD
                else:
                    rebuild_mode = RebuildMode.CACHED

                # Force change flags=True when running a specific test or when cache is disabled
                force_cpp_test_change = (
                    cpp_test_change
                    or (args.test is not None)
                    or rebuild_mode != RebuildMode.CACHED
                )
                # Note: args.examples == [] means "all examples" (e.g., default mode or --examples flag)
                # args.examples with specific items (e.g., ['Blink']) means specific examples requested
                # Only force when specific examples are requested, not for "run all examples"
                force_examples_change = (
                    examples_change
                    or (args.examples is not None and len(args.examples) > 0)
                    or rebuild_mode != RebuildMode.CACHED
                )
                force_python_test_change = (
                    python_test_change
                    or (args.test is not None)
                    or rebuild_mode != RebuildMode.CACHED
                )
                force_wasm_change = wasm_change or rebuild_mode != RebuildMode.CACHED
                force_src_code_change = (
                    src_code_change or rebuild_mode != RebuildMode.CACHED
                )

                # Only show cache status when it's enabled (the notable case)
                # When disabled (--no-fingerprint), this is the default so no message needed

                # Lazy import: test_runner loads rich.console (~106ms) and other heavy
                # modules. By importing here (not at top level), we skip those imports
                # entirely on the ultra-early-exit path (cached test result).
                from ci.util.test_runner import runner as test_runner

                test_runner(
                    args,
                    force_src_code_change,
                    force_cpp_test_change,
                    force_examples_change,
                    force_python_test_change,
                    force_wasm_change,
                    fingerprint_manager=fingerprint_manager,
                )

            # If we got here, tests passed
            tests_passed = True

        except SystemExit as e:
            # Test runner calls sys.exit() on failure
            if e.code != 0:
                tests_passed = False
            raise
        finally:
            # Only save fingerprints when running ALL tests, not specific tests
            # This prevents running a specific test from marking the full fingerprint as valid
            # Note: args.examples == [] means "all examples" (e.g., --cpp mode), which is OK
            # args.examples with specific items (e.g., ['Blink']) means specific examples only
            running_specific_examples = (
                args.examples is not None and len(args.examples) > 0
            )
            running_all_tests = (
                args.test is None
                and not running_specific_examples
                and not args.no_fingerprint
            )
            if running_all_tests:
                status = "success" if tests_passed else "failure"
                fingerprint_manager.save_all(status)

        # Set up force exit daemon and exit
        _ = setup_force_exit()
        _CANCEL_WATCHDOG.set()

        # Print total execution time
        elapsed_time = time.time() - start_time
        print(f"Total: {elapsed_time:.2f}s")

        sys.exit(0)

    except KeyboardInterrupt as ki:
        # Only notify main thread if we're in a worker thread
        if threading.current_thread() != threading.main_thread():
            from ci.util.global_interrupt_handler import (
                handle_keyboard_interrupt,
            )

            handle_keyboard_interrupt(ki)
        signal_interrupt()
        wait_for_cleanup()
        sys.exit(130)


if __name__ == "__main__":
    try:
        main()
    except KeyboardInterrupt as ki:  # noqa: KBI002
        # Top-level safety net: main() already handles cleanup.
        # Print the full exception chain then exit without propagating
        # KeyboardInterrupt to parent processes (e.g. os.system() callers).
        import traceback  # noqa: PLC0415

        traceback.print_exception(ki)
        sys.exit(1)
