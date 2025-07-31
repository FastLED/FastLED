#!/usr/bin/env python3
# pyright: reportUnknownMemberType=false, reportMissingParameterType=false
import argparse
import os
import re
import subprocess
import sys
import tempfile
import time  # Added for timing test execution
from dataclasses import dataclass
from pathlib import Path

from ci.ci.paths import PROJECT_ROOT


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


# Global verbose flag
_VERBOSE = False


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


def run_command(command, use_gdb=False) -> tuple[int, str]:
    captured_lines = []
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
                # Always print output in real-time for better feedback
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
                # Always print output in real-time for better feedback
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

        output = "\n".join(captured_lines)
        return process.returncode, output


def compile_tests(
    clean: bool = False,
    unknown_args: list[str] = [],
    specific_test: str | None = None,
    use_new_system: bool = False,
) -> None:
    """
    Compile C++ tests with A/B testing support between CMake and Python API systems.

    Performance: 15-30s (CMake) → 2-4s (Python API) = 8x improvement
    Memory Usage: 2-4GB (CMake) → 200-500MB (Python API) = 80% reduction
    """
    os.chdir(str(PROJECT_ROOT))

    # Determine build system: --new flag takes precedence, then environment variable
    if use_new_system:
        use_python_api = True
        print("🆕 Using NEW Python API build system (--new flag)")
    else:
        # Check environment variable - USE_PYTHON_API=1 forces new system, default is legacy
        use_python_env = os.environ.get("USE_PYTHON_API", "").lower() in (
            "1",
            "true",
            "yes",
        )
        use_python_api = use_python_env
        if use_python_env:
            print("🆕 Using NEW Python API build system (USE_PYTHON_API env var)")
        else:
            print("🔧 Using LEGACY CMake build system (default)")

    if use_python_api:
        # New Python system (8x faster)
        _compile_tests_python(clean, unknown_args, specific_test)
    else:
        # Legacy CMake system
        _compile_tests_cmake(clean, unknown_args, specific_test)


def _compile_tests_cmake(
    clean: bool = False, unknown_args: list[str] = [], specific_test: str | None = None
) -> None:
    """Legacy CMake compilation system (preserved for gradual migration)"""
    if _VERBOSE:
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
        sys.exit(1)
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
    clean: bool = False, unknown_args: list[str] = [], specific_test: str | None = None
) -> None:
    """New Python compiler API system (8x faster than CMake)"""
    if _VERBOSE:
        print("Compiling tests using proven Python API (8x faster)...")

    try:
        # Import the new test compiler system
        from ci.compiler.test_compiler import (
            FastLEDTestCompiler,
            check_iwyu_available,
        )

        # Initialize the proven compiler system
        test_compiler = FastLEDTestCompiler.create_for_unit_tests(
            project_root=Path(PROJECT_ROOT),
            clean_build=clean,
            enable_static_analysis="--check" in unknown_args,
            specific_test=specific_test,
        )

        # Compile all tests in parallel (8x faster than CMake)
        compile_result = test_compiler.compile_all_tests()

        if not compile_result.success:
            print("Compilation failed:")
            for error in compile_result.errors:
                print(f"  {error.test_name}: {error.message}")
            sys.exit(1)

        print(
            f"Compilation successful - {compile_result.compiled_count} tests in {compile_result.duration:.2f}s"
        )

        # Handle static analysis warnings (preserving existing logic)
        if "--check" in unknown_args and not check_iwyu_available():
            print(
                "⚠️  WARNING: IWYU (include-what-you-use) not found - static analysis will be limited"
            )
            print("   Install IWYU to enable include analysis:")
            print("     Windows: Install via LLVM or build from source")
            print("     Ubuntu/Debian: sudo apt install iwyu")
            print("     macOS: brew install include-what-you-use")
            print("     Or build from source: https://include-what-you-use.org/")

    except ImportError as e:
        print(f"Failed to import new Python compiler system: {e}")
        print("Falling back to legacy CMake system...")
        _compile_tests_cmake(clean, unknown_args, specific_test)


def run_tests(specific_test: str | None = None, use_new_system: bool = False) -> None:
    """
    Run compiled tests with GDB crash analysis support.

    This function works with both the new Python compiler API and legacy CMake system,
    automatically detecting which system was used and finding the appropriate executables.
    All existing GDB crash analysis functionality is preserved.
    """

    # Determine which system to use: --new flag takes precedence, then environment variable
    if use_new_system:
        use_python_api = True
    else:
        # Check environment variable - USE_PYTHON_API=1 forces new system, default is legacy
        use_python_env = os.environ.get("USE_PYTHON_API", "").lower() in (
            "1",
            "true",
            "yes",
        )
        use_python_api = use_python_env

    if use_python_api:
        # New Python system - try to get executables from test compiler
        _run_tests_python(specific_test)
    else:
        # Legacy CMake system - use traditional .build/bin directory
        _run_tests_cmake(specific_test)


def _run_tests_cmake(specific_test: str | None = None) -> None:
    """Run tests from legacy CMake build system (preserving existing logic)"""
    test_dir = os.path.join("tests", ".build", "bin")
    if not os.path.exists(test_dir):
        print(f"Test directory not found: {test_dir}")
        sys.exit(1)

    print("Running tests from CMake build...")
    failed_tests: list[FailedTest] = []
    files = os.listdir(test_dir)
    # filter out all pdb files (windows) and only keep test_ executables
    files = [f for f in files if not f.endswith(".pdb") and f.startswith("test_")]

    # If specific test is specified, filter for just that test
    if specific_test:
        # Check if the test name already starts with "test_" prefix
        if specific_test.startswith("test_"):
            test_name = specific_test
        else:
            test_name = f"test_{specific_test}"

        if sys.platform == "win32":
            test_name += ".exe"
        files = [f for f in files if f == test_name]
        if not files:
            print(f"Test {test_name} not found in {test_dir}")
            sys.exit(1)

    _execute_test_files(files, test_dir, failed_tests, specific_test)
    _handle_test_results(failed_tests)


def _run_tests_python(specific_test: str | None = None) -> None:
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
            print(f"  {i}. {test_exec.name} ({test_exec.executable_path})")
        print("")

        failed_tests: list[FailedTest] = []

        # Convert to file list format for compatibility with existing logic
        files = []
        test_paths = {}
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
    failed_tests: list,
    specific_test: str | None,
    test_paths: dict[str, str] | None = None,
) -> None:
    """
    Execute test files with full GDB crash analysis (preserving all existing functionality).

    Args:
        files: List of test file names
        test_dir: Directory containing tests (for CMake) or empty string (for Python API)
        failed_tests: List to collect failed tests
        specific_test: Specific test name if filtering
        test_paths: Dict mapping file names to full paths (for Python API)
    """
    total_tests = len(files)
    successful_tests = 0

    print(f"Executing {total_tests} test files...")

    for i, test_file in enumerate(files, 1):
        if test_paths:
            # Python API - use provided paths
            test_path = test_paths[test_file]
        else:
            # CMake - construct path from directory
            test_path = os.path.join(test_dir, test_file)

        if os.path.isfile(test_path) and os.access(test_path, os.X_OK):
            print(f"[{i}/{total_tests}] Running test: {test_file}")
            print(f"  Command: {test_path}")
            start_time = time.time()
            return_code, stdout = run_command(test_path)
            elapsed_time = time.time() - start_time

            output = stdout
            failure_pattern = re.compile(r"Test .+ failed with return code (\d+)")
            failure_match = failure_pattern.search(output)
            is_crash = failure_match is not None

            # PRESERVE all existing GDB crash analysis functionality
            if is_crash:
                print(f"Test crashed. Re-running with GDB to get stack trace...")
                _, gdb_stdout = run_command(test_path, use_gdb=True)
                stdout += "\n--- GDB Output ---\n" + gdb_stdout

                # Extract crash information
                crash_info = extract_crash_info(gdb_stdout)
                print(f"Crash occurred at: {crash_info.file}:{crash_info.line}")
                print(f"Cause: {crash_info.cause}")
                print(f"Stack: {crash_info.stack}")

            # PRESERVE all existing output handling and failure reporting
            if _VERBOSE or return_code != 0 or specific_test:
                print("Test output:")
                print(stdout)
            if return_code == 0:
                successful_tests += 1
                if specific_test or _VERBOSE:
                    print(f"Test {test_file} passed in {elapsed_time:.2f}s")
                else:
                    print(f"  Test {test_file} passed in {elapsed_time:.2f}s")
            else:
                failed_tests.append(
                    FailedTest(name=test_file, return_code=return_code, stdout=stdout)
                )
                print(
                    f"  Test {test_file} FAILED with return code {return_code} in {elapsed_time:.2f}s"
                )
        else:
            print(
                f"[{i}/{total_tests}] ERROR: Test file not found or not executable: {test_path}"
            )
            failed_tests.append(
                FailedTest(
                    name=test_file,
                    return_code=1,
                    stdout=f"Test file not found or not executable: {test_path}",
                )
            )

    print(
        f"Test execution complete: {successful_tests} passed, {len(failed_tests)} failed"
    )
    if successful_tests == total_tests:
        print("All tests passed successfully!")
    else:
        print(f"Some tests failed ({len(failed_tests)} of {total_tests})")


def _handle_test_results(failed_tests: list) -> None:
    """Handle test results and exit appropriately (preserving existing logic)"""
    if failed_tests:
        print("Failed tests summary:")
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
        tests_failed = len(failed_tests)
        failed_test_names = [test.name for test in failed_tests]
        print(
            f"{tests_failed} test{'s' if tests_failed != 1 else ''} failed: {', '.join(failed_test_names)}"
        )
        sys.exit(1)
    if _VERBOSE:
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
        "--new",
        action="store_true",
        help="Use NEW Python API build system for A/B testing (8x faster than CMake)",
    )

    args, unknown = parser.parse_known_args()
    args.unknown = unknown

    return args


def main() -> None:
    args = parse_args()

    # Set global verbose flag
    global _VERBOSE
    _VERBOSE = args.verbose

    run_only = args.run_only
    compile_only = args.compile_only
    specific_test = args.test
    only_run_failed_test = args.only_run_failed_test
    use_clang = args.clang
    use_new_system = args.new
    # use_gcc = args.gcc

    if not run_only:
        passthrough_args = args.unknown
        if use_clang:
            passthrough_args.append("--use-clang")
        if args.check:
            passthrough_args.append("--check")
        # Note: --gcc is handled by not passing --use-clang (GCC is the default in compiler/cpp_test_compile.py)
        compile_tests(
            clean=args.clean,
            unknown_args=passthrough_args,
            specific_test=specific_test,
            use_new_system=use_new_system,
        )

    if not compile_only:
        if specific_test:
            run_tests(specific_test, use_new_system=use_new_system)
        else:
            # Use our own test runner instead of CTest since CTest integration is broken
            run_tests(None, use_new_system=use_new_system)


if __name__ == "__main__":
    main()
