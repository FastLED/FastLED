import os
import subprocess
import sys
from dataclasses import dataclass

from ci.paths import PROJECT_ROOT


@dataclass
class FailedTest:
    name: str
    return_code: int
    stdout: str
    stderr: str


def run_command(command):
    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True
    )
    stdout, stderr = process.communicate()
    return process.returncode, stdout.decode(), stderr.decode()


def compile_tests():
    os.chdir(str(PROJECT_ROOT))
    print("Compiling tests...")
    return_code, stdout, stderr = run_command("uv run ci/cpp_test_compile.py")
    if return_code != 0:
        print("Compilation failed:")
        print(stdout)
        print(stderr)
        sys.exit(1)
    print("Compilation successful.")


def run_tests():
    test_dir = os.path.join("tests", ".build", "bin")
    if not os.path.exists(test_dir):
        print(f"Test directory not found: {test_dir}")
        sys.exit(1)

    print("Running tests...")
    failed_tests: list[FailedTest] = []
    for test_file in os.listdir(test_dir):
        test_path = os.path.join(test_dir, test_file)
        if os.path.isfile(test_path) and os.access(test_path, os.X_OK):
            print(f"Running test: {test_file}")
            return_code, stdout, stderr = run_command(test_path)
            print("Test output:")
            print(stdout)
            if stderr:
                print("Test errors:")
                print(stderr)
            print(
                f"Test {'passed' if return_code == 0 else 'failed'} with return code {return_code}"
            )
            print("-" * 40)
            if return_code != 0:
                failed_tests.append(FailedTest(test_file, return_code, stdout, stderr))
    if failed_tests:
        for failed_test in failed_tests:
            print(
                f"Test {failed_test.name} failed with return code {failed_test.return_code}\n{failed_test.stdout}"
            )
        tests_failed = len(failed_tests)
        failed_test_names = [test.name for test in failed_tests]
        # print(f"{tests_failed} test{'s' if tests_failed != 1 else ''} failed.")
        print(
            f"{tests_failed} test{'s' if tests_failed != 1 else ''} failed: {', '.join(failed_test_names)}"
        )
        sys.exit(1)
    print("All tests passed.")


def main():
    compile_tests()
    run_tests()


if __name__ == "__main__":
    main()
