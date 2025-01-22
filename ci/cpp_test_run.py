import argparse
import os
import re
import subprocess
import sys
import tempfile
from dataclasses import dataclass

from ci.paths import PROJECT_ROOT


@dataclass
class FailedTest:
    name: str
    return_code: int
    stdout: str


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
            text=True,
            bufsize=1,  # Line buffered
        )
        assert process.stdout is not None
        # Stream and capture output
        while True:
            line = process.stdout.readline()
            if not line and process.poll() is not None:
                break
            if line:
                captured_lines.append(line.rstrip())
                print(line, end="")  # Print in real-time

        os.unlink(gdb_script.name)
        output = "\n".join(captured_lines)
        return process.returncode, output
    else:
        process = subprocess.Popen(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,  # Merge stderr into stdout
            shell=True,
            text=True,
            bufsize=1,  # Line buffered
        )
        assert process.stdout is not None
        # Stream and capture output
        while True:
            line = process.stdout.readline()
            if not line and process.poll() is not None:
                break
            if line:
                captured_lines.append(line.rstrip())
                print(line, end="")  # Print in real-time

        output = "\n".join(captured_lines)
        return process.returncode, output


def compile_tests(clean: bool = False, unknown_args: list[str] = []) -> None:
    os.chdir(str(PROJECT_ROOT))
    print("Compiling tests...")
    command = ["uv", "run", "ci/cpp_test_compile.py"]
    if clean:
        command.append("--clean")
    command.extend(unknown_args)
    return_code, _ = run_command(" ".join(command))
    if return_code != 0:
        print("Compilation failed:")
        sys.exit(1)
    print("Compilation successful.")


def run_tests(specific_test: str | None = None) -> None:
    test_dir = os.path.join("tests", ".build", "bin")
    if not os.path.exists(test_dir):
        print(f"Test directory not found: {test_dir}")
        sys.exit(1)

    print("Running tests...")
    failed_tests: list[FailedTest] = []
    files = os.listdir(test_dir)
    # filter out all pdb files (windows) and only keep test_ executables
    files = [f for f in files if not f.endswith(".pdb") and f.startswith("test_")]

    # If specific test is specified, filter for just that test
    if specific_test:
        test_name = f"test_{specific_test}"
        if sys.platform == "win32":
            test_name += ".exe"
        files = [f for f in files if f == test_name]
        if not files:
            print(f"Test {test_name} not found in {test_dir}")
            sys.exit(1)
    for test_file in files:
        test_path = os.path.join(test_dir, test_file)
        if os.path.isfile(test_path) and os.access(test_path, os.X_OK):
            print(f"Running test: {test_file}")
            return_code, stdout = run_command(test_path)

            output = stdout
            failure_pattern = re.compile(r"Test .+ failed with return code (\d+)")
            failure_match = failure_pattern.search(output)
            is_crash = failure_match is not None

            if is_crash:
                print("Test crashed. Re-running with GDB to get stack trace...")
                _, gdb_stdout = run_command(test_path, use_gdb=True)
                stdout += "\n--- GDB Output ---\n" + gdb_stdout

                # Extract crash information
                crash_info = extract_crash_info(gdb_stdout)
                print(f"Crash occurred at: {crash_info.file}:{crash_info.line}")
                print(f"Cause: {crash_info.cause}")
                print(f"Stack: {crash_info.stack}")

            print("Test output:")
            print(stdout)
            if return_code == 0:
                print("Test passed")
            elif is_crash:
                if failure_match:
                    print(f"Test crashed with return code {failure_match.group(1)}")
                else:
                    print(f"Test crashed with return code {return_code}")
            else:
                print(f"Test failed with return code {return_code}")

            print("-" * 40)
            if return_code != 0:
                failed_tests.append(FailedTest(test_file, return_code, stdout))
    if failed_tests:
        for failed_test in failed_tests:
            print(
                f"Test {failed_test.name} failed with return code {failed_test.return_code}\n{failed_test.stdout}"
            )
        tests_failed = len(failed_tests)
        failed_test_names = [test.name for test in failed_tests]
        print(
            f"{tests_failed} test{'s' if tests_failed != 1 else ''} failed: {', '.join(failed_test_names)}"
        )
        sys.exit(1)
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
        help="Specific test to run (without test_ prefix)",
    )
    parser.add_argument(
        "--clang",
        help="Use Clang compiler",
        action="store_true",
    )
    args, unknown = parser.parse_known_args()
    args.unknown = unknown
    return args


def main() -> None:
    args = parse_args()
    run_only = args.run_only
    compile_only = args.compile_only
    specific_test = args.test
    only_run_failed_test = args.only_run_failed_test
    use_clang = args.clang

    if not run_only:
        passthrough_args = args.unknown
        if use_clang:
            passthrough_args.append("--use-clang")
        compile_tests(clean=args.clean, unknown_args=passthrough_args)

    if not compile_only:
        if specific_test:
            run_tests(specific_test)
        else:
            cmd = "ctest --test-dir tests/.build --output-on-failure"
            if only_run_failed_test:
                cmd += " --rerun-failed"
            rtn, stdout = run_command(cmd)
            if rtn != 0:
                print("Failed tests:")
                print(stdout)
                sys.exit(1)


if __name__ == "__main__":
    main()
