#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false, reportMissingParameterType=false
import argparse
import multiprocessing
import os
import re
import subprocess
import sys
import tempfile
import time  # Added for timing test execution
from dataclasses import dataclass
from pathlib import Path
from queue import Empty, PriorityQueue
from threading import Event, Lock, Thread

import psutil

from ci.util.paths import PROJECT_ROOT


def optimize_python_command(cmd: list[str]) -> list[str]:
    """
    Optimize command list by replacing 'python' with sys.executable for direct execution.

    This avoids shell resolution overhead and ensures we use the exact Python interpreter
    that's currently running, which is critical for virtual environments.

    Args:
        cmd: Command list that may contain 'python' as first element

    Returns:
        list[str]: Optimized command with 'python' replaced by sys.executable
    """
    if cmd and cmd[0] == "python":
        # Replace 'python' with the current Python executable path
        optimized_cmd = [sys.executable] + cmd[1:]
        return optimized_cmd
    return cmd


from ci.util.test_exceptions import (
    CompilationFailedException,
    TestExecutionFailedException,
    TestFailureInfo,
    TestTimeoutException,
)


class OutputBuffer:
    """Thread-safe output buffer with ordered output display"""

    def __init__(self) -> None:
        self.output_queue: PriorityQueue[tuple[int, int, str]] = PriorityQueue()
        self.next_sequence: int = 0
        self.sequence_lock: Lock = Lock()
        self.stop_event: Event = Event()
        self.output_thread: Thread = Thread(target=self._output_worker, daemon=True)
        self.output_thread.start()

    def write(self, test_index: int, message: str) -> None:
        """Write a message to the buffer with test index for ordering"""
        with self.sequence_lock:
            sequence = self.next_sequence
            self.next_sequence += 1
        self.output_queue.put((test_index, sequence, message))

    def _output_worker(self) -> None:
        """Worker thread that processes output in order"""
        while not self.stop_event.is_set() or not self.output_queue.empty():
            try:
                item: tuple[int, int, str] = self.output_queue.get(timeout=0.1)
                _, _, message = item
                print(message, flush=True)
                self.output_queue.task_done()
            except Empty:
                continue
            except Exception as e:
                print(f"Error in output worker: {e}")
                continue

    def stop(self) -> None:
        """Stop the output worker thread"""
        self.stop_event.set()
        if self.output_thread.is_alive():
            self.output_thread.join()


# Configure console for UTF-8 output on Windows
if os.name == "nt":  # Windows
    # Try to set console to UTF-8 mode
    try:
        # Set stdout and stderr to UTF-8 encoding
        # Note: reconfigure() was added in Python 3.7
        if hasattr(sys.stdout, "reconfigure") and callable(
            getattr(sys.stdout, "reconfigure", None)
        ):
            sys.stdout.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[attr-defined]
        if hasattr(sys.stderr, "reconfigure") and callable(
            getattr(sys.stderr, "reconfigure", None)
        ):
            sys.stderr.reconfigure(encoding="utf-8", errors="replace")  # type: ignore[attr-defined]
    except (AttributeError, OSError):
        # Fallback for older Python versions or if reconfigure fails
        pass


# Environment flags for backward compatibility
_SHOW_COMPILE = os.environ.get("FASTLED_TEST_SHOW_COMPILE", "").lower() in (
    "1",
    "true",
    "yes",
)
_SHOW_LINK = os.environ.get("FASTLED_TEST_SHOW_LINK", "").lower() in (
    "1",
    "true",
    "yes",
)


@dataclass
class FailedTest:
    name: str
    return_code: int
    stdout: str


def check_iwyu_available() -> bool:
    """Check if include-what-you-use is available in the system"""
    try:
        result = subprocess.run(
            ["include-what-you-use", "--version"],
            capture_output=True,
            text=True,
            timeout=10,
        )
        return result.returncode == 0
    except (
        subprocess.CalledProcessError,
        FileNotFoundError,
        subprocess.TimeoutExpired,
    ):
        return False


def run_command(
    command: str | list[str],
    use_gdb: bool = False,
    *,
    verbose: bool = False,
    show_compile: bool = False,
    show_link: bool = False,
) -> tuple[int, str]:
    captured_lines: list[str] = []

    # Determine command type
    is_test_execution = False
    is_compile = False
    is_link = False
    if isinstance(command, str):
        cmd_lower = command.replace("\\", "/").lower()
        # Check if running test executable
        is_test_execution = (
            "/test_" in cmd_lower
            or ".build/bin/test_" in cmd_lower
            or cmd_lower.endswith(".exe")
        )
        # Check if compiling
        is_compile = "-c" in cmd_lower and (".cpp" in cmd_lower or ".c" in cmd_lower)
        # Check if linking
        is_link = (
            not is_compile
            and not is_test_execution
            and ("-o" in cmd_lower or "lib" in cmd_lower)
        )

    if use_gdb:
        with tempfile.NamedTemporaryFile(mode="w+", delete=False) as gdb_script:
            gdb_script.write("set pagination off\n")
            gdb_script.write("run\n")
            gdb_script.write("bt full\n")
            gdb_script.write("info registers\n")
            gdb_script.write("x/16i $pc\n")
            gdb_script.write("thread apply all bt full\n")
            gdb_script.write("quit\n")

        gdb_command = (
            f"gdb -return-child-result -batch -x {gdb_script.name} --args {command}"
        )
        process = subprocess.Popen(
            gdb_command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # Merge stderr into stdout
            shell=True,
            text=False,
        )
        assert process.stdout is not None
        # Stream and capture output
        while True:
            line_bytes = process.stdout.readline()
            line = line_bytes.decode("utf-8", errors="ignore")
            if not line and process.poll() is not None:
                break
            if line:
                captured_lines.append(line.rstrip())
                # Always print GDB output (it's only used for crashes anyway)
                try:
                    print(line, end="", flush=True)
                except UnicodeEncodeError:
                    # Fallback: replace problematic characters
                    print(
                        line.encode("utf-8", errors="replace").decode(
                            "utf-8", errors="replace"
                        ),
                        end="",
                        flush=True,
                    )

        os.unlink(gdb_script.name)
        output = "\n".join(captured_lines)
        return process.returncode, output
    else:
        # Optimize list commands to avoid shell overhead
        if isinstance(command, list):
            # Optimize python commands and use shell=False for better performance
            python_exe = optimize_python_command(command)
            process = subprocess.Popen(
                python_exe,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,  # Merge stderr into stdout
                shell=False,  # Use shell=False for better performance with list commands
                text=False,
            )
        else:
            # String commands still need shell=True
            process = subprocess.Popen(
                command,
                stdout=subprocess.PIPE,
                stderr=subprocess.STDOUT,  # Merge stderr into stdout
                shell=True,
                text=False,
            )
        assert process.stdout is not None
        # Stream and capture output
        while True:
            line_bytes = process.stdout.readline()
            line = line_bytes.decode("utf-8", errors="ignore")
            if not line and process.poll() is not None:
                break
            if line:
                captured_lines.append(line.rstrip())
                # Determine if we should print this line
                should_print = (
                    verbose  # Always print in verbose mode
                    or (is_compile and show_compile)  # Print compilation if enabled
                    or (is_link and show_link)  # Print linking if enabled
                    or (not is_test_execution)  # Print non-test output
                    or (
                        is_test_execution and process.returncode != 0
                    )  # Print failed test output
                    or (
                        is_test_execution
                        and any(
                            marker in line
                            for marker in [
                                "Running test:",
                                "Test passed",
                                "Test FAILED",
                                "passed with return code",
                                "Test output:",
                            ]
                        )
                    )  # Print test status
                )
                if should_print:
                    try:
                        # Add prefix for compile/link commands
                        if is_compile and show_compile:
                            print("[COMPILE] ", end="", flush=True)
                        elif is_link and show_link:
                            print("[LINK] ", end="", flush=True)
                        print(line, end="", flush=True)
                    except UnicodeEncodeError:
                        # Fallback: replace problematic characters
                        print(
                            line.encode("utf-8", errors="replace").decode(
                                "utf-8", errors="replace"
                            ),
                            end="",
                            flush=True,
                        )

        output = "\n".join(captured_lines)
        return process.returncode, output


def compile_tests(
    clean: bool = False,
    unknown_args: list[str] = [],
    specific_test: str | None = None,
    use_legacy_system: bool = False,
    quick_build: bool = True,
    *,
    verbose: bool = False,
    show_compile: bool = False,
    show_link: bool = False,
) -> None:
    """
    Compile C++ tests with A/B testing support between CMake and Python API systems.

    Performance: 15-30s (CMake) → 2-4s (Python API) = 8x improvement
    Memory Usage: 2-4GB (CMake) → 200-500MB (Python API) = 80% reduction
    """
    os.chdir(str(PROJECT_ROOT))

    # Determine build system: --legacy flag takes precedence, then environment variable
    if use_legacy_system:
        use_python_api = False
        print("🔧 Using LEGACY CMake build system (--legacy flag)")
    else:
        # Check environment variable - USE_CMAKE=1 forces legacy system, default is new
        use_cmake_env = os.environ.get("USE_CMAKE", "").lower() in (
            "1",
            "true",
            "yes",
        )
        use_python_api = not use_cmake_env
        if use_cmake_env:
            print("🔧 Using LEGACY CMake build system (USE_CMAKE env var)")
        else:
            print("🆕 Using Python API build system (default)")

    if use_python_api:
        # New Python system (8x faster)
        _compile_tests_python(
            clean,
            unknown_args,
            specific_test,
            quick_build=quick_build,
            verbose=verbose,
            show_compile=show_compile,
            show_link=show_link,
        )
    else:
        # Legacy CMake system
        _compile_tests_cmake(clean, unknown_args, specific_test)


def _compile_tests_cmake(
    clean: bool = False,
    unknown_args: list[str] = [],
    specific_test: str | None = None,
    *,
    verbose: bool = False,
    show_compile: bool = False,
    show_link: bool = False,
) -> None:
    """Legacy CMake compilation system (preserved for gradual migration)"""
    if verbose:
        print("Compiling tests using legacy CMake system...")
    command = ["uv", "run", "-m", "ci.compiler.cpp_test_compile"]
    if clean:
        command.append("--clean")
    if specific_test:
        command.extend(["--test", specific_test])
    command.extend(unknown_args)
    return_code, output = run_command(" ".join(command))
    if return_code != 0:
        print("Compilation failed:")
        print(output)  # Always show output on failure
        failure = TestFailureInfo(
            test_name="cmake_compilation",
            command=" ".join(command),
            return_code=return_code,
            output=output,
            error_type="cmake_compilation_error",
        )
        raise CompilationFailedException("CMake compilation failed", [failure])
    print("Compilation successful.")

    # Check if static analysis was requested and warn about IWYU availability
    if "--check" in unknown_args:
        if not check_iwyu_available():
            print(
                "⚠️  WARNING: IWYU (include-what-you-use) not found - static analysis will be limited"
            )
            print("   Install IWYU to enable include analysis:")
            print("     Windows: Install via LLVM or build from source")
            print("     Ubuntu/Debian: sudo apt install iwyu")
            print("     macOS: brew install include-what-you-use")
            print("     Or build from source: https://include-what-you-use.org/")


def _compile_tests_python(
    clean: bool = False,
    unknown_args: list[str] = [],
    specific_test: str | None = None,
    quick_build: bool = True,
    *,
    verbose: bool = False,
    show_compile: bool = False,
    show_link: bool = False,
) -> None:
    """Optimized Python API system with PCH (same as examples)"""

    try:
        # Use the optimized cpp_test_compile system directly
        import subprocess

        cmd = ["uv", "run", "python", "-m", "ci.compiler.cpp_test_compile"]

        if specific_test:
            cmd.extend(["--test", specific_test])
        if clean:
            cmd.append("--clean")
        if verbose:
            cmd.append("--verbose")
        if "--check" in unknown_args:
            cmd.append("--check")
        if "--no-pch" in unknown_args:
            cmd.append("--no-pch")

        print("🚀 Using optimized Python API with PCH optimization")
        result = subprocess.run(cmd)
        if result.returncode != 0:
            raise RuntimeError(
                f"Unit test compilation failed with return code {result.returncode}"
            )

    except Exception as e:
        print(f"Failed to compile with optimized Python API: {e}")
        print("Falling back to legacy CMake system...")
        _compile_tests_cmake(clean, unknown_args, specific_test)


def run_tests(
    specific_test: str | None = None,
    use_legacy_system: bool = False,
    *,
    verbose: bool = False,
    show_compile: bool = False,
    show_link: bool = False,
) -> None:
    """
    Run compiled tests with GDB crash analysis support.

    This function works with both the new Python compiler API and legacy CMake system,
    automatically detecting which system was used and finding the appropriate executables.
    All existing GDB crash analysis functionality is preserved.
    """

    # Determine which system to use: --legacy flag takes precedence, then environment variable
    if use_legacy_system:
        use_python_api = False
    else:
        # Check environment variable - USE_CMAKE=1 forces legacy system, default is new
        use_cmake_env = os.environ.get("USE_CMAKE", "").lower() in (
            "1",
            "true",
            "yes",
        )
        use_python_api = not use_cmake_env

    if use_python_api:
        # New Python system - try to get executables from test compiler
        _run_tests_python(specific_test)
    else:
        # Legacy CMake system - use traditional .build/bin directory
        _run_tests_cmake(specific_test)


def _run_tests_cmake(
    specific_test: str | None = None,
    *,
    verbose: bool = False,
    show_compile: bool = False,
    show_link: bool = False,
) -> None:
    """Run tests from legacy CMake build system (preserving existing logic)"""
    test_dir = os.path.join("tests")
    if not os.path.exists(test_dir):
        print(f"Test directory not found: {test_dir}")
        sys.exit(1)

    print("Running tests from CMake build...")
    failed_tests: list[FailedTest] = []
    files = os.listdir(test_dir)
    # filter out all pdb files (windows) and only keep test_ cpp files
    files = [
        f
        for f in files
        if not f.endswith(".pdb") and f.startswith("test_") and f.endswith(".cpp")
    ]

    # If specific test is specified, filter for just that test
    if specific_test:
        # Check if the test name already starts with "test_" prefix
        if specific_test.startswith("test_"):
            test_name = specific_test
        else:
            test_name = f"test_{specific_test}"

        if sys.platform == "win32":
            test_name += ".exe"

        # Modified to support partial matching (case-insensitive)
        matching_files = []
        for f in files:
            # Check for exact match (case-insensitive)
            if f.lower() == test_name.lower():
                matching_files.append(f)
                break

            # Check for partial match in the filename (case-insensitive)
            base_name = f.replace("test_", "").replace(".cpp", "")
            if sys.platform == "win32":
                base_name = base_name.replace(".exe", "")

            if specific_test.lower() in base_name.lower():
                matching_files.append(f)

        if matching_files:
            files = matching_files
        else:
            print(f"Test {test_name} not found in {test_dir}")
            sys.exit(1)

    _execute_test_files(files, test_dir, failed_tests, specific_test)
    _handle_test_results(failed_tests)


def _run_tests_python(
    specific_test: str | None = None,
    *,
    verbose: bool = False,
    show_compile: bool = False,
    show_link: bool = False,
) -> None:
    """Run tests from new Python compiler API system"""
    try:
        # Import the new test compiler system
        from ci.compiler.test_compiler import FastLEDTestCompiler

        # Get test executables from Python build system
        test_compiler = FastLEDTestCompiler.get_existing_instance()
        if not test_compiler:
            print("No compiled tests found. Run compilation first.")
            sys.exit(1)

        test_executables = test_compiler.get_test_executables(specific_test)
        if not test_executables:
            test_name = specific_test or "any tests"
            print(f"No test executables found for: {test_name}")
            sys.exit(1)

        print(f"Running {len(test_executables)} tests from Python API build...")

        # Print list of tests that will be executed
        print("Tests to execute:")
        for i, test_exec in enumerate(test_executables, 1):
            # Convert absolute path to relative for display
            rel_path = os.path.relpath(test_exec.executable_path)
            print(f"  {i}. {test_exec.name} ({rel_path})")
        print("")

        failed_tests: list[FailedTest] = []

        # Convert to file list format for compatibility with existing logic
        files: list[str] = []
        test_paths: dict[str, str] = {}
        for test_exec in test_executables:
            file_name = test_exec.name
            if os.name == "nt" and not file_name.endswith(".exe"):
                file_name += ".exe"
            files.append(file_name)
            test_paths[file_name] = str(test_exec.executable_path)

        print(f"Starting test execution for {len(files)} test files...")
        _execute_test_files(files, "", failed_tests, specific_test, test_paths)
        _handle_test_results(failed_tests)

    except ImportError as e:
        print(f"Failed to import Python test system: {e}")
        print("Falling back to CMake test execution...")
        _run_tests_cmake(specific_test)


def _execute_test_files(
    files: list[str],
    test_dir: str,
    failed_tests: list[FailedTest],
    specific_test: str | None,
    test_paths: dict[str, str] | None = None,
    *,
    verbose: bool = False,
    show_compile: bool = False,
    show_link: bool = False,
) -> None:
    """
    Execute test files in parallel with full GDB crash analysis.

    Args:
        files: List of test file names
        test_dir: Directory containing tests (for CMake) or empty string (for Python API)
        failed_tests: List to collect failed tests
        specific_test: Specific test name if filtering
        test_paths: Dict mapping file names to full paths (for Python API)
    """
    total_tests = len(files)
    successful_tests = 0
    completed_tests = 0

    # Initialize output buffer for ordered output
    output_buffer = OutputBuffer()
    output_buffer.write(0, f"Executing {total_tests} test files in parallel...")

    # Determine number of workers based on configuration

    # Get configuration from args
    args = parse_args()

    # Force sequential execution if NO_PARALLEL is set
    if os.environ.get("NO_PARALLEL"):
        max_workers = 1
        output_buffer.write(
            0, "NO_PARALLEL environment variable set - forcing sequential execution"
        )
    elif args.sequential:
        max_workers = 1
    elif args.parallel:
        max_workers = args.parallel
    else:
        max_workers = max(1, multiprocessing.cpu_count() - 1)  # Leave one core free

    # Check memory limit
    if args.max_memory:
        memory_limit = args.max_memory * 1024 * 1024  # Convert MB to bytes
        available_memory = psutil.virtual_memory().available
        if memory_limit > available_memory:
            output_buffer.write(
                0,
                f"Warning: Requested memory limit {args.max_memory}MB exceeds available memory {available_memory / (1024 * 1024):.0f}MB",
            )
            output_buffer.write(
                0, "Reducing number of parallel workers to stay within memory limits"
            )
            # Estimate memory per test based on previous runs or default to 100MB
            memory_per_test = 100 * 1024 * 1024  # 100MB per test
            max_parallel_by_memory = max(1, memory_limit // memory_per_test)
            max_workers = min(max_workers, max_parallel_by_memory)

    output_buffer.write(0, f"Using {max_workers} parallel workers")

    # Thread-safe counter
    counter_lock = Lock()

    def run_single_test(
        test_file: str, test_index: int
    ) -> tuple[bool, float, str, int]:
        """Run a single test and return its results"""
        nonlocal completed_tests

        if test_paths:
            test_path = test_paths[test_file]
        else:
            test_path = os.path.join(test_dir, test_file)

        # For .cpp files, compile them first
        if test_path.endswith(".cpp"):
            # Create a temporary directory for compilation
            with tempfile.TemporaryDirectory() as temp_dir:
                # Compile the test file
                output_buffer.write(
                    test_index,
                    f"[{test_index}/{total_tests}] Compiling test: {test_file}",
                )
                compile_cmd = [
                    "python",
                    "-m",
                    "ziglang",
                    "c++",
                    "-o",
                    os.path.join(temp_dir, "test.exe"),
                    test_path,
                    "-I",
                    os.path.join(PROJECT_ROOT, "src"),
                    "-I",
                    os.path.join(PROJECT_ROOT, "tests"),
                    # NOTE: Compiler flags now come from build_flags.toml
                ]
                return_code, stdout = run_command(compile_cmd)
                if return_code != 0:
                    output_buffer.write(
                        test_index,
                        f"[{test_index}/{total_tests}] ERROR: Failed to compile test: {test_file}",
                    )
                    return False, 0.0, f"Failed to compile test: {stdout}", return_code

                # Update test_path to point to the compiled executable
                test_path = os.path.join(temp_dir, "test.exe")

        if not (os.path.isfile(test_path) and os.access(test_path, os.X_OK)):
            output_buffer.write(
                test_index,
                f"[{test_index}/{total_tests}] ERROR: Test file not found or not executable: {test_path}",
            )
            return False, 0.0, f"Test file not found or not executable: {test_path}", 1

        output_buffer.write(
            test_index, f"[{test_index}/{total_tests}] Running test: {test_file}"
        )
        if verbose:
            output_buffer.write(test_index, f"  Command: {test_path}")

        start_time = time.time()
        # Pass --minimal flag to doctest when not in verbose mode to suppress output unless tests fail
        cmd = [test_path]
        if not verbose:
            cmd.append("--minimal")
        return_code, stdout = run_command(cmd)
        elapsed_time = time.time() - start_time

        output = stdout
        failure_pattern = re.compile(r"Test .+ failed with return code (\d+)")
        failure_match = failure_pattern.search(output)
        is_crash = failure_match is not None

        # Handle crashes with GDB (must be done synchronously)
        if is_crash:
            output_buffer.write(
                test_index, f"Test crashed. Re-running with GDB to get stack trace..."
            )
            _, gdb_stdout = run_command(test_path, use_gdb=True)
            stdout += "\n--- GDB Output ---\n" + gdb_stdout

            # Extract crash information
            crash_info = extract_crash_info(gdb_stdout)
            output_buffer.write(
                test_index, f"Crash occurred at: {crash_info.file}:{crash_info.line}"
            )
            output_buffer.write(test_index, f"Cause: {crash_info.cause}")
            output_buffer.write(test_index, f"Stack: {crash_info.stack}")

        # Print output based on verbosity and status
        if verbose or return_code != 0:
            output_buffer.write(test_index, "Test output:")
            output_buffer.write(test_index, stdout)

        if return_code == 0:
            output_buffer.write(
                test_index, f"  Test {test_file} passed in {elapsed_time:.2f}s"
            )
        else:
            output_buffer.write(
                test_index,
                f"  Test {test_file} FAILED with return code {return_code} in {elapsed_time:.2f}s",
            )

        with counter_lock:
            completed_tests += 1

        return return_code == 0, elapsed_time, stdout, return_code

    try:
        # Run tests in parallel using ThreadPoolExecutor
        from concurrent.futures import ThreadPoolExecutor, as_completed

        with ThreadPoolExecutor(max_workers=max_workers) as executor:
            # Submit all tests
            future_to_test = {
                executor.submit(run_single_test, test_file, i + 1): test_file
                for i, test_file in enumerate(files)
            }

            # Process results as they complete
            for future in as_completed(future_to_test):
                test_file = future_to_test[future]
                try:
                    success, _, stdout, return_code = future.result()
                    if success:
                        with counter_lock:
                            successful_tests += 1
                    else:
                        failed_tests.append(
                            FailedTest(
                                name=test_file, return_code=return_code, stdout=stdout
                            )
                        )
                except Exception as e:
                    output_buffer.write(
                        0, f"ERROR: Test {test_file} failed with exception: {e}"
                    )
                    failed_tests.append(
                        FailedTest(name=test_file, return_code=1, stdout=str(e))
                    )

        # Print final summary
        output_buffer.write(
            0,
            f"Test execution complete: {successful_tests} passed, {len(failed_tests)} failed",
        )
        if successful_tests == total_tests:
            output_buffer.write(0, "All tests passed successfully!")
        else:
            output_buffer.write(
                0, f"Some tests failed ({len(failed_tests)} of {total_tests})"
            )

    finally:
        # Ensure output buffer is stopped
        output_buffer.stop()


def _handle_test_results(
    failed_tests: list[FailedTest], *, verbose: bool = False
) -> None:
    """Handle test results and exit appropriately (preserving existing logic)"""
    if failed_tests:
        print("Failed tests summary:")
        failures = []
        for failed_test in failed_tests:
            print(
                f"Test {failed_test.name} failed with return code {failed_test.return_code}"
            )
            # Always show output on failure
            print("Output:")
            # Show indented output for better readability
            for line in failed_test.stdout.splitlines():
                print(f"  {line}")
            print()  # Add spacing between failed tests

            failures.append(
                TestFailureInfo(
                    test_name=failed_test.name,
                    command=f"test_{failed_test.name}",
                    return_code=failed_test.return_code,
                    output=failed_test.stdout,
                    error_type="test_execution_failure",
                )
            )

        tests_failed = len(failed_tests)
        failed_test_names = [test.name for test in failed_tests]
        print(
            f"{tests_failed} test{'s' if tests_failed != 1 else ''} failed: {', '.join(failed_test_names)}"
        )
        raise TestExecutionFailedException(f"{tests_failed} test(s) failed", failures)
    if verbose:
        print("All tests passed.")


@dataclass
class CrashInfo:
    cause: str = "Unknown"
    stack: str = "Unknown"
    file: str = "Unknown"
    line: str = "Unknown"


def extract_crash_info(gdb_output: str) -> CrashInfo:
    lines = gdb_output.split("\n")
    crash_info = CrashInfo()

    try:
        for i, line in enumerate(lines):
            if line.startswith("Program received signal"):
                try:
                    crash_info.cause = line.split(":", 1)[1].strip()
                except IndexError:
                    crash_info.cause = line.strip()
            elif line.startswith("#0"):
                crash_info.stack = line
                for j in range(i, len(lines)):
                    if "at" in lines[j]:
                        try:
                            _, location = lines[j].split("at", 1)
                            location = location.strip()
                            if ":" in location:
                                crash_info.file, crash_info.line = location.rsplit(
                                    ":", 1
                                )
                            else:
                                crash_info.file = location
                        except ValueError:
                            pass  # If split fails, we keep the default values
                        break
                break
    except Exception as e:
        print(f"Error parsing GDB output: {e}")

    return crash_info


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description="Compile and run C++ tests")
    parser.add_argument(
        "--compile-only",
        action="store_true",
        help="Only compile the tests without running them",
    )
    parser.add_argument(
        "--run-only",
        action="store_true",
        help="Only run the tests without compiling them",
    )
    parser.add_argument(
        "--only-run-failed-test",
        action="store_true",
        help="Only run the tests that failed in the previous run",
    )
    parser.add_argument(
        "--clean", action="store_true", help="Clean build before compiling"
    )
    parser.add_argument(
        "--test",
        help="Specific test to run (without extension)",
    )
    parser.add_argument(
        "--verbose",
        action="store_true",
        help="Enable verbose output",
    )
    parser.add_argument(
        "--show-compile",
        action="store_true",
        help="Show compilation commands and output",
    )
    parser.add_argument(
        "--show-link",
        action="store_true",
        help="Show linking commands and output",
    )
    parser.add_argument(
        "--parallel",
        type=int,
        help="Number of parallel test processes to run (default: CPU count - 1)",
    )
    parser.add_argument(
        "--sequential",
        action="store_true",
        help="Run tests sequentially (disables parallel execution)",
    )
    parser.add_argument(
        "--max-memory",
        type=int,
        help="Maximum memory usage in MB for parallel test execution",
    )

    # Create mutually exclusive group for compiler selection
    compiler_group = parser.add_mutually_exclusive_group()
    compiler_group.add_argument(
        "--clang",
        help="Use Clang compiler",
        action="store_true",
    )
    compiler_group.add_argument(
        "--gcc",
        help="Use GCC compiler (default on non-Windows)",
        action="store_true",
    )
    parser.add_argument(
        "--check",
        action="store_true",
        help="Enable static analysis (IWYU, clang-tidy)",
    )
    parser.add_argument(
        "--legacy",
        action="store_true",
        help="Use legacy CMake system instead of new Python API (8x slower)",
    )
    parser.add_argument(
        "--no-unity",
        action="store_true",
        help="Disable unity builds for cpp tests",
    )
    parser.add_argument(
        "--no-pch",
        action="store_true",
        help="Disable precompiled headers (PCH) for unit tests",
    )
    parser.add_argument(
        "--debug",
        action="store_true",
        help="Use debug build mode with full debug symbols (default is quick mode with -g0)",
    )

    args, unknown = parser.parse_known_args()
    args.unknown = unknown

    return args


def main() -> None:
    try:
        args = parse_args()

        # Get verbosity flags from args

        run_only = args.run_only
        compile_only = args.compile_only
        specific_test = args.test
        # only_run_failed_test feature to be implemented in future
        _ = args.only_run_failed_test
        use_clang = args.clang
        use_legacy_system = args.legacy
        no_unity = args.no_unity
        quick_build = (
            not args.debug
        )  # Default to quick mode unless --debug is specified
        # use_gcc = args.gcc

        if not run_only:
            passthrough_args = args.unknown
            if use_clang:
                passthrough_args.append("--use-clang")
            if args.check:
                passthrough_args.append("--check")
            if no_unity:
                passthrough_args.append("--no-unity")
            if args.no_pch:
                passthrough_args.append("--no-pch")
            # Note: --gcc is handled by not passing --use-clang (GCC is the default in compiler/cpp_test_compile.py)
            compile_tests(
                clean=args.clean,
                unknown_args=passthrough_args,
                specific_test=specific_test,
                use_legacy_system=use_legacy_system,
                quick_build=quick_build,
                verbose=args.verbose,
                show_compile=args.show_compile,
                show_link=args.show_link,
            )

        if not compile_only:
            if specific_test:
                run_tests(
                    specific_test,
                    use_legacy_system=use_legacy_system,
                    verbose=args.verbose,
                    show_compile=args.show_compile,
                    show_link=args.show_link,
                )
            else:
                # Use our own test runner instead of CTest since CTest integration is broken
                run_tests(
                    None,
                    use_legacy_system=use_legacy_system,
                    verbose=args.verbose,
                    show_compile=args.show_compile,
                    show_link=args.show_link,
                )
    except (
        CompilationFailedException,
        TestExecutionFailedException,
        TestTimeoutException,
    ) as e:
        # Print detailed failure information
        print("\n" + "=" * 60)
        print("FASTLED TEST FAILURE DETAILS")
        print("=" * 60)
        print(e.get_detailed_failure_info())
        print("=" * 60)

        # Exit with appropriate code
        if e.failures:
            # Use the return code from the first failure, or 1 if none available
            exit_code = (
                e.failures[0].return_code if e.failures[0].return_code != 0 else 1
            )
        else:
            exit_code = 1
        sys.exit(exit_code)


if __name__ == "__main__":
    main()
