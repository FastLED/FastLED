#!/usr/bin/env python3
import argparse
import hashlib
import json
import os
import subprocess
import sys
import threading
import time
from dataclasses import dataclass
from pathlib import Path
from typing import Any, Dict, List, Optional

from typeguard import typechecked

from ci.running_process import RunningProcess


@typechecked
@dataclass
class TestArgs:
    """Type-safe test arguments"""

    cpp: bool = False
    unit: bool = False
    py: bool = False
    test: Optional[str] = None
    clang: bool = False
    gcc: bool = False
    clean: bool = False
    no_interactive: bool = False
    interactive: bool = False
    verbose: bool = False
    quick: bool = False
    no_stack_trace: bool = False
    check: bool = False
    examples: Optional[list[str]] = None
    no_pch: bool = False
    cache: bool = False
    unity: bool = False
    full: bool = False
    new: bool = False


@typechecked
@dataclass
class TestCategories:
    """Type-safe test category flags"""

    unit: bool
    examples: bool
    py: bool
    integration: bool
    unit_only: bool
    examples_only: bool
    py_only: bool
    integration_only: bool

    def __post_init__(self):
        # Type validation
        for field_name in [
            "unit",
            "examples",
            "py",
            "integration",
            "unit_only",
            "examples_only",
            "py_only",
            "integration_only",
        ]:
            value = getattr(self, field_name)
            if not isinstance(value, bool):
                raise TypeError(f"{field_name} must be bool, got {type(value)}")


@typechecked
@dataclass
class FingerprintResult:
    """Type-safe fingerprint result"""

    hash: str
    elapsed_seconds: Optional[str] = None
    status: Optional[str] = None


_PIO_CHECK_ENABLED = False

# Default to non-interactive mode for safety unless explicitly in interactive environment
# This prevents hanging on prompts when running in automated environments
_IS_GITHUB = True  # Default to non-interactive behavior
if os.environ.get("FASTLED_INTERACTIVE") == "true":
    _IS_GITHUB = False  # Only enable interactive mode if explicitly requested

# Set environment variable to ensure all subprocesses also run in non-interactive mode
os.environ["FASTLED_CI_NO_INTERACTIVE"] = "true"
if not _IS_GITHUB:
    os.environ.pop(
        "FASTLED_CI_NO_INTERACTIVE", None
    )  # Remove if interactive mode is enabled


def run_command(cmd: List[str], **kwargs: Any) -> None:
    """Run a command and handle errors"""
    try:
        subprocess.run(cmd, check=True, **kwargs)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)


def parse_args() -> TestArgs:
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(description="Run FastLED tests")
    parser.add_argument(
        "--cpp",
        action="store_true",
        help="Run C++ tests only (equivalent to --unit --examples, suppresses Python tests)",
    )
    parser.add_argument("--unit", action="store_true", help="Run C++ unit tests only")
    parser.add_argument("--py", action="store_true", help="Run Python tests only")
    parser.add_argument(
        "test", type=str, nargs="?", default=None, help="Specific C++ test to run"
    )

    # Create mutually exclusive group for compiler selection
    compiler_group = parser.add_mutually_exclusive_group()
    compiler_group.add_argument(
        "--clang", action="store_true", help="Use Clang compiler"
    )
    compiler_group.add_argument(
        "--gcc", action="store_true", help="Use GCC compiler (default on non-Windows)"
    )

    parser.add_argument(
        "--clean", action="store_true", help="Clean build before compiling"
    )
    parser.add_argument(
        "--no-interactive",
        action="store_true",
        help="Force non-interactive mode (no confirmation prompts)",
    )
    parser.add_argument(
        "--interactive",
        action="store_true",
        help="Enable interactive mode (allows confirmation prompts)",
    )
    parser.add_argument(
        "--verbose",
        "-v",
        action="store_true",
        help="Enable verbose output showing all test details",
    )
    parser.add_argument(
        "--quick", action="store_true", help="Enable quick mode with FASTLED_ALL_SRC=1"
    )
    parser.add_argument(
        "--no-stack-trace",
        action="store_true",
        help="Disable stack trace dumping on timeout",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Enable static analysis (IWYU, clang-tidy) - auto-enables --cpp and --clang",
    )
    parser.add_argument(
        "--examples",
        nargs="*",
        help="Run example compilation tests only (optionally specify example names). Use with --full for complete compilation + linking + execution",
    )
    parser.add_argument(
        "--no-pch",
        action="store_true",
        help="Disable precompiled headers (PCH) when running example compilation tests",
    )
    parser.add_argument(
        "--cache",
        "--sccache",
        action="store_true",
        help="Enable sccache/ccache when running example compilation tests (disabled by default for faster clean builds)",
    )
    parser.add_argument(
        "--unity",
        action="store_true",
        help="Enable UNITY build mode for examples - compile all source files as a single unit for improved performance",
    )
    parser.add_argument(
        "--full",
        action="store_true",
        help="Run full integration tests including compilation + linking + program execution",
    )
    parser.add_argument(
        "--new",
        action="store_true",
        help="Use NEW Python API build system for A/B testing (8x faster than CMake)",
    )

    args = parser.parse_args()

    # Convert argparse.Namespace to TestArgs dataclass
    test_args = TestArgs(
        cpp=args.cpp,
        unit=args.unit,
        py=args.py,
        test=args.test,
        clang=args.clang,
        gcc=args.gcc,
        clean=args.clean,
        no_interactive=args.no_interactive,
        interactive=args.interactive,
        verbose=args.verbose,
        quick=args.quick,
        no_stack_trace=args.no_stack_trace,
        check=args.check,
        examples=args.examples,
        no_pch=args.no_pch,
        cache=args.cache,
        unity=args.unity,
        full=args.full,
        new=args.new,
    )

    # Auto-enable --cpp when a specific test is provided
    if test_args.test and not test_args.cpp:
        test_args.cpp = True
        print(f"Auto-enabled --cpp mode for specific test: {test_args.test}")

    # Auto-enable --cpp and --clang when --check is provided
    if test_args.check:
        if not test_args.cpp:
            test_args.cpp = True
            print("Auto-enabled --cpp mode for static analysis (--check)")
        if not test_args.clang and not test_args.gcc:
            test_args.clang = True
            print("Auto-enabled --clang compiler for static analysis (--check)")

    # Auto-enable --cpp and --quick when --examples is provided
    if test_args.examples is not None:
        if not test_args.cpp:
            test_args.cpp = True
            print("Auto-enabled --cpp mode for example compilation (--examples)")
        if not test_args.quick:
            test_args.quick = True
            print(
                "Auto-enabled --quick mode for faster example compilation (--examples)"
            )

    # Handle --full flag behavior
    if test_args.full:
        if test_args.examples is not None:
            # --examples --full: Run examples with full compilation+linking+execution
            print("Full examples mode: compilation + linking + program execution")
        else:
            # --full alone: Run integration tests
            if not test_args.cpp:
                test_args.cpp = True
                print("Auto-enabled --cpp mode for full integration tests (--full)")
            print("Full integration tests: compilation + linking + program execution")

    # Default to Clang on Windows unless --gcc is explicitly passed
    if sys.platform == "win32" and not test_args.gcc and not test_args.clang:
        test_args.clang = True
        print("Windows detected: defaulting to Clang compiler (use --gcc to override)")
    elif test_args.gcc:
        print("Using GCC compiler")
    elif test_args.clang:
        print("Using Clang compiler")

    return test_args


def process_test_flags(args: TestArgs) -> TestArgs:
    """Process and validate test execution flags"""

    # Check which specific test flags are provided
    specific_flags = [args.unit, args.examples is not None, args.py, args.full]
    specific_count = sum(bool(flag) for flag in specific_flags)

    # If --cpp is provided, default to --unit --examples (no Python)
    if args.cpp and specific_count == 0:
        args.unit = True
        args.examples = []  # Empty list means run all examples
        print("--cpp mode: Running unit tests and examples (Python tests suppressed)")
        return args

    # If any specific flags are provided, ONLY run those (exclusive behavior)
    if specific_count > 0:
        # When specific flags are provided, disable everything else unless explicitly set
        if not args.unit:
            # Unit was not explicitly set, so disable it
            pass  # args.unit is already False
        if args.examples is None:
            # Examples was not explicitly set, so disable it
            pass  # args.examples is already None
        if not args.py:
            # Python was not explicitly set, so disable it
            pass  # args.py is already False
        if not args.full:
            # Full was not explicitly set, so disable it
            pass  # args.full is already False

        # Log what's running
        enabled_tests: list[str] = []
        if args.unit:
            enabled_tests.append("unit tests")
        if args.examples is not None:
            enabled_tests.append("examples")
        if args.py:
            enabled_tests.append("Python tests")
        if args.full:
            if args.examples is not None:
                enabled_tests.append("full example integration tests")
            else:
                enabled_tests.append("full integration tests")

        print(f"Specific test flags provided: Running only {', '.join(enabled_tests)}")
        return args

    # If no specific flags, run everything (backward compatibility)
    if specific_count == 0:
        args.unit = True
        args.examples = []  # Empty list means run all examples
        args.py = True
        print("No test flags specified: Running all tests (unit, examples, Python)")
        return args

    return args


def determine_test_categories(args: TestArgs) -> TestCategories:
    """Determine which test categories should run based on flags"""
    unit_enabled = args.unit
    examples_enabled = args.examples is not None
    py_enabled = args.py
    # Integration tests only run when --full is used alone (not with --examples)
    integration_enabled = args.full and args.examples is None

    return TestCategories(
        unit=unit_enabled,
        examples=examples_enabled,
        py=py_enabled,
        integration=integration_enabled,
        unit_only=unit_enabled
        and not examples_enabled
        and not py_enabled
        and not integration_enabled,
        examples_only=examples_enabled
        and not unit_enabled
        and not py_enabled
        and not integration_enabled,
        py_only=py_enabled
        and not unit_enabled
        and not examples_enabled
        and not integration_enabled,
        integration_only=integration_enabled
        and not unit_enabled
        and not examples_enabled
        and not py_enabled,
    )


def _make_pio_check_cmd() -> List[str]:
    return [
        "pio",
        "check",
        "--skip-packages",
        "--src-filters=+<src/>",
        "--severity=medium",
        "--fail-on-defect=high",
        "--flags",
        "--inline-suppr --enable=all --std=c++17",
    ]


def make_compile_uno_test_process(enable_stack_trace: bool = True) -> RunningProcess:
    """Create a process to compile the uno tests"""
    cmd = [
        "uv",
        "run",
        "ci/ci-compile.py",
        "uno",
        "--examples",
        "Blink",
        "--no-interactive",
    ]
    # shell=True wasn't working for some reason
    # cmd = cmd + ['||'] + cmd
    # return RunningProcess(cmd, echo=False, auto_run=not _IS_GITHUB, shell=True)
    return RunningProcess(
        cmd, echo=False, auto_run=not _IS_GITHUB, enable_stack_trace=enable_stack_trace
    )


def fingerprint_code_base(
    start_directory: Path, glob: str = "**/*.h,**/*.cpp,**/*.hpp"
) -> FingerprintResult:
    """
    Create a fingerprint of the code base by hashing file contents.

    Args:
        start_directory: The root directory to start scanning from
        glob: Comma-separated list of glob patterns to match files

    Returns:
        A FingerprintResult with hash and optional status
    """
    try:
        hasher = hashlib.sha256()
        patterns = glob.split(",")

        # Get all matching files
        all_files: list[Path] = []
        for pattern in patterns:
            pattern = pattern.strip()
            all_files.extend(sorted(start_directory.glob(pattern)))

        # Sort files for consistent ordering
        all_files.sort()

        # Process each file
        for file_path in all_files:
            if file_path.is_file():
                # Add the relative path to the hash
                rel_path = file_path.relative_to(start_directory)
                hasher.update(str(rel_path).encode("utf-8"))

                # Add the file content to the hash
                try:
                    with open(file_path, "rb") as f:
                        # Read in chunks to handle large files
                        for chunk in iter(lambda: f.read(4096), b""):
                            hasher.update(chunk)
                except Exception as e:
                    # If we can't read the file, include the error in the hash
                    hasher.update(f"ERROR:{str(e)}".encode("utf-8"))

        return FingerprintResult(hash=hasher.hexdigest())
    except Exception as e:
        return FingerprintResult(hash="", status=f"error: {str(e)}")


def calculate_fingerprint(root_dir: Path | None = None) -> FingerprintResult:
    """
    Calculate the code base fingerprint.

    Args:
        root_dir: The root directory to start scanning from. If None, uses src directory.

    Returns:
        The fingerprint result
    """
    if root_dir is None:
        root_dir = Path.cwd() / "src"

    start_time = time.time()
    # Compute the fingerprint
    result = fingerprint_code_base(root_dir)
    elapsed_time = time.time() - start_time
    # Add timing information to the result
    result.elapsed_seconds = f"{elapsed_time:.2f}"

    return result


def run_namespace_check(enable_stack_trace: bool) -> None:
    """Run the namespace check"""
    print("Running namespace check...")
    namespace_check_proc = RunningProcess(
        "uv run python ci/tests/no_using_namespace_fl_in_headers.py",
        echo=True,
        auto_run=True,
        enable_stack_trace=enable_stack_trace,
    )
    namespace_check_proc.wait()
    if namespace_check_proc.returncode != 0:
        print(
            f"Namespace check failed with return code {namespace_check_proc.returncode}"
        )
        sys.exit(namespace_check_proc.returncode)
    print("Namespace check passed.")


def build_cpp_test_command(args: TestArgs) -> str:
    """Build the C++ test command based on arguments"""
    cmd_list = ["uv", "run", "ci/cpp_test_run.py"]

    if args.clang:
        cmd_list.append("--clang")

    if args.test:
        cmd_list.append("--test")
        cmd_list.append(args.test)
    if args.clean:
        cmd_list.append("--clean")
    if args.verbose:
        cmd_list.append("--verbose")
    if args.check:
        cmd_list.append("--check")
    if args.new:
        cmd_list.append("--new")

    return subprocess.list2cmdline(cmd_list)


def run_unit_tests(args: TestArgs, enable_stack_trace: bool) -> None:
    """Run C++ unit tests only"""
    print("Running C++ unit tests...")

    # Namespace check (always run with C++ tests)
    run_namespace_check(enable_stack_trace)

    # Build and run unit tests
    cmd_str_cpp = build_cpp_test_command(args)

    # Set compiler-specific timeout for C++ tests
    # GCC builds are 5x slower due to poor unified compilation performance
    cpp_test_timeout = (
        900 if args.gcc else 300
    )  # 15 minutes for GCC, 5 minutes for Clang/default
    if args.gcc:
        print(
            f"Using extended timeout for GCC builds: {cpp_test_timeout} seconds (15 minutes)"
        )

    start_time = time.time()
    proc = RunningProcess(
        cmd_str_cpp, enable_stack_trace=enable_stack_trace, timeout=cpp_test_timeout
    )
    proc.wait()
    if proc.returncode != 0:
        print(f"Command failed: {proc.command}")
        sys.exit(proc.returncode)

    print(f"C++ unit tests completed in {time.time() - start_time:.2f}s")


def run_examples_tests(args: TestArgs, enable_stack_trace: bool) -> None:
    """Run example compilation tests (with optional enhanced compilation for --full)"""
    # Determine test mode
    if args.full and args.examples is not None:
        print("Running example tests with enhanced compilation (--full mode)...")
        test_mode = "enhanced"
    elif args.examples:
        # Specific examples provided
        examples_str = " ".join(args.examples)
        print(f"Running example compilation tests for: {examples_str}")
        test_mode = "compilation"
    else:
        # No specific examples, run all
        print("Running example compilation tests")
        test_mode = "compilation"

    start_time = time.time()

    # Build command with optional example names
    cmd = ["uv", "run", "ci/test_example_compilation.py"]
    if args.examples:
        cmd.extend(args.examples)
    if args.clean:
        cmd.append("--clean")
    if args.no_pch:
        cmd.append("--no-pch")
    if args.cache:
        cmd.append("--cache")
    if args.unity:
        cmd.append("--unity")
    if args.full and args.examples is not None:
        cmd.append("--full")
    # Note: --full is now passed to the script when examples and full are both specified

    # Run the example compilation test script
    proc = RunningProcess(
        cmd,
        echo=True,
        auto_run=True,
        enable_stack_trace=enable_stack_trace,
    )
    proc.wait()
    if proc.returncode != 0:
        test_type = (
            "enhanced example tests"
            if test_mode == "enhanced"
            else "example compilation tests"
        )
        print(f"{test_type} failed with return code {proc.returncode}")
        sys.exit(proc.returncode)

    test_type = (
        "enhanced example tests"
        if test_mode == "enhanced"
        else "example compilation tests"
    )
    print(f"{test_type} completed successfully in {time.time() - start_time:.2f}s")


def run_python_tests(args: TestArgs, enable_stack_trace: bool) -> None:
    """Run Python tests only"""
    print("Running Python tests...")

    pytest_proc = RunningProcess(
        "uv run pytest -s ci/tests -xvs --durations=0",
        echo=True,
        auto_run=True,
        enable_stack_trace=enable_stack_trace,
    )
    pytest_proc.wait()
    if pytest_proc.returncode != 0:
        print(f"Python tests failed with return code {pytest_proc.returncode}")
        sys.exit(pytest_proc.returncode)

    print("Python tests completed successfully")


def run_integration_tests(args: TestArgs, enable_stack_trace: bool) -> None:
    """Run full integration tests including compilation + linking + program execution"""

    if args.examples is not None:
        # When --examples --full is specified, only run example-related integration tests
        print(
            "Running example integration tests (compilation + linking + program execution)..."
        )
        test_filter = (
            "TestFullProgramLinking"  # Only run the full program linking tests
        )
    else:
        # When --full alone is specified, run all integration tests
        print(
            "Running full integration tests (compilation + linking + program execution)..."
        )
        test_filter = None

    start_time = time.time()

    # Run integration tests in the test_integration directory
    cmd = ["uv", "run", "pytest", "-s", "ci/test_integration", "-xvs", "--durations=0"]

    if test_filter:
        cmd.extend(["-k", test_filter])

    if args.verbose:
        cmd.append("-v")

    # Run the integration test script
    proc = RunningProcess(
        cmd,
        echo=True,
        auto_run=True,
        enable_stack_trace=enable_stack_trace,
    )
    proc.wait()
    if proc.returncode != 0:
        test_type = "example integration tests" if test_filter else "integration tests"
        print(f"{test_type} failed with return code {proc.returncode}")
        sys.exit(proc.returncode)

    test_type = "example integration tests" if test_filter else "integration tests"
    print(f"{test_type} completed successfully in {time.time() - start_time:.2f}s")


def run_cpp_tests(
    args: TestArgs, test_categories: TestCategories, enable_stack_trace: bool
) -> None:
    """Run C++ tests (unit and/or examples)"""
    if test_categories.unit_only:
        run_unit_tests(args, enable_stack_trace)
    elif test_categories.examples_only:
        run_examples_tests(args, enable_stack_trace)
    else:
        # Both unit and examples
        run_unit_tests(args, enable_stack_trace)
        run_examples_tests(args, enable_stack_trace)


def run_all_tests(
    args: TestArgs,
    test_categories: TestCategories,
    enable_stack_trace: bool,
    src_code_change: bool,
) -> None:
    """Run all tests in parallel (original behavior)"""
    print("Running all tests...")

    # Namespace check first
    run_namespace_check(enable_stack_trace)

    # Build C++ test command
    cmd_str_cpp = build_cpp_test_command(args)

    # Build PIO check command
    cmd_list = _make_pio_check_cmd()
    if not _PIO_CHECK_ENABLED:
        cmd_list = ["echo", "pio check is disabled"]
    cmd_str = subprocess.list2cmdline(cmd_list)

    print(f"Running command (in the background): {cmd_str}")
    pio_process = RunningProcess(
        cmd_str,
        echo=False,
        auto_run=not _IS_GITHUB,
        enable_stack_trace=enable_stack_trace,
    )

    # Set compiler-specific timeout for C++ tests
    # GCC builds are 5x slower due to poor unified compilation performance
    cpp_test_timeout = (
        900 if args.gcc else 300
    )  # 15 minutes for GCC, 5 minutes for Clang/default
    if args.gcc:
        print(
            f"Using extended timeout for GCC builds: {cpp_test_timeout} seconds (15 minutes)"
        )

    cpp_test_proc = RunningProcess(
        cmd_str_cpp, enable_stack_trace=enable_stack_trace, timeout=cpp_test_timeout
    )
    compile_native_proc = RunningProcess(
        "uv run ci/ci-compile-native.py",
        echo=False,
        auto_run=not _IS_GITHUB,
        enable_stack_trace=enable_stack_trace,
    )
    pytest_proc = RunningProcess(
        "uv run pytest -s ci/tests -xvs --durations=0",
        echo=True,
        auto_run=not _IS_GITHUB,
        enable_stack_trace=enable_stack_trace,
    )

    tests = [
        cpp_test_proc,
        compile_native_proc,
        pytest_proc,
        pio_process,
    ]
    if src_code_change:
        print("Source code changed, running uno tests")
        tests += [make_compile_uno_test_process(enable_stack_trace)]

    is_first = True
    for test in tests:
        was_first = is_first
        is_first = False
        sys.stdout.flush()
        if not test.auto_run:
            test.run()
        print(f"Waiting for command: {test.command}")
        # make a thread that will say waiting for test {test} to finish...<seconds>
        # and then kill the test if it takes too long (> 120 seconds)
        event_stopped = threading.Event()

        def _runner() -> None:
            start_time = time.time()
            while not event_stopped.wait(1):
                curr_time = time.time()
                seconds = int(curr_time - start_time)
                if (
                    not was_first
                ):  # skip printing for the first test since it echo's out.
                    print(
                        f"Waiting for command: {test.command} to finish...{seconds} seconds"
                    )

        runner_thread = threading.Thread(target=_runner, daemon=True)
        runner_thread.start()
        test.wait()
        event_stopped.set()
        runner_thread.join(timeout=1)
        if not test.echo:
            for line in test.stdout.splitlines():
                print(line)
        if test.returncode != 0:
            [t.kill() for t in tests]
            print(
                f"\nCommand failed: {test.command} with return code {test.returncode}"
            )
            sys.exit(test.returncode)

    print("All tests passed")


def main() -> None:
    try:
        # Start a watchdog timer to kill the process if it takes too long (10 minutes)
        def watchdog_timer():
            time.sleep(600)  # 10 minutes
            print("Watchdog timer expired after 10 minutes - forcing exit")
            os._exit(2)  # Exit with error code 2 to indicate timeout

        watchdog = threading.Thread(
            target=watchdog_timer, daemon=True, name="WatchdogTimer"
        )
        watchdog.start()

        args = parse_args()
        args = process_test_flags(args)

        # Handle --no-interactive flag
        if args.no_interactive:
            global _IS_GITHUB
            _IS_GITHUB = True
            os.environ["FASTLED_CI_NO_INTERACTIVE"] = "true"
            os.environ["GITHUB_ACTIONS"] = (
                "true"  # This ensures all subprocess also run in non-interactive mode
            )

        # Handle --interactive flag
        if args.interactive:
            _IS_GITHUB = False
            os.environ.pop("FASTLED_CI_NO_INTERACTIVE", None)
            os.environ.pop("GITHUB_ACTIONS", None)

        # Handle --quick flag
        if args.quick:
            os.environ["FASTLED_ALL_SRC"] = "1"
            print("Quick mode enabled. FASTLED_ALL_SRC=1")

        # Handle stack trace control
        enable_stack_trace = not args.no_stack_trace
        if enable_stack_trace:
            print("Stack trace dumping enabled for test timeouts")
        else:
            print("Stack trace dumping disabled for test timeouts")

        # Validate conflicting arguments
        if args.no_interactive and args.interactive:
            print(
                "Error: --interactive and --no-interactive cannot be used together",
                file=sys.stderr,
            )
            sys.exit(1)

        # Change to script directory
        os.chdir(Path(__file__).parent)

        cache_dir = Path(".cache")
        cache_dir.mkdir(exist_ok=True)
        fingerprint_file = cache_dir / "fingerprint.json"

        def write_fingerprint(fingerprint: FingerprintResult) -> None:
            # Convert dataclass to dict for JSON serialization
            fingerprint_dict = {
                "hash": fingerprint.hash,
                "elapsed_seconds": fingerprint.elapsed_seconds,
                "status": fingerprint.status,
            }
            with open(fingerprint_file, "w") as f:
                json.dump(fingerprint_dict, f, indent=2)

        def read_fingerprint() -> FingerprintResult | None:
            if fingerprint_file.exists():
                with open(fingerprint_file, "r") as f:
                    try:
                        data = json.load(f)
                        return FingerprintResult(
                            hash=data.get("hash", ""),
                            elapsed_seconds=data.get("elapsed_seconds"),
                            status=data.get("status"),
                        )
                    except json.JSONDecodeError:
                        print("Invalid fingerprint file. Recalculating...")
            return None

        prev_fingerprint = read_fingerprint()
        # Calculate fingerprint
        fingerprint_data = calculate_fingerprint()
        src_code_change: bool
        if prev_fingerprint is None:
            src_code_change = True
        else:
            try:
                src_code_change = fingerprint_data.hash != prev_fingerprint.hash
            except Exception:
                print("Invalid fingerprint file. Recalculating...")
                src_code_change = True
        # print(f"Fingerprint: {fingerprint_result['hash']}")

        # Create .cache directory if it doesn't exist

        # Save the fingerprint to a file as JSON

        write_fingerprint(fingerprint_data)

        # Determine which test categories to run
        test_categories = determine_test_categories(args)

        # Execute test categories based on flags
        if test_categories.integration_only:
            run_integration_tests(args, enable_stack_trace)
        elif test_categories.unit_only:
            run_unit_tests(args, enable_stack_trace)
        elif test_categories.examples_only:
            run_examples_tests(args, enable_stack_trace)
        elif test_categories.py_only:
            run_python_tests(args, enable_stack_trace)
        elif (
            test_categories.unit
            and test_categories.examples
            and not test_categories.py
            and not test_categories.integration
        ):
            # C++ mode: unit + examples, no Python (sequential execution)
            run_unit_tests(args, enable_stack_trace)
            run_examples_tests(args, enable_stack_trace)
        elif (
            test_categories.unit
            and test_categories.py
            and not test_categories.examples
            and not test_categories.integration
        ):
            # Unit + Python only
            run_unit_tests(args, enable_stack_trace)
            run_python_tests(args, enable_stack_trace)
        elif (
            test_categories.examples
            and test_categories.py
            and not test_categories.unit
            and not test_categories.integration
        ):
            # Examples + Python only
            run_examples_tests(args, enable_stack_trace)
            run_python_tests(args, enable_stack_trace)
        else:
            # All tests enabled or complex combinations: run sequentially
            if test_categories.unit:
                run_unit_tests(args, enable_stack_trace)
            if test_categories.examples:
                run_examples_tests(args, enable_stack_trace)
            if test_categories.py:
                run_python_tests(args, enable_stack_trace)
            if test_categories.integration:
                run_integration_tests(args, enable_stack_trace)

        # Force exit daemon thread remains at the end
        def force_exit():
            time.sleep(1)
            print("Force exit daemon thread invoked")
            os._exit(1)

        daemon_thread = threading.Thread(
            target=force_exit, daemon=True, name="ForceExitDaemon"
        )
        daemon_thread.start()
        sys.exit(0)
    except KeyboardInterrupt:
        sys.exit(130)  # Standard Unix practice: 128 + SIGINT's signal number (2)


if __name__ == "__main__":
    main()
