import os
import subprocess
from pathlib import Path

from ci.paths import PROJECT_ROOT

HERE = Path(__file__).resolve().parent


def run_command(command):
    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True, text=True
    )
    stdout, stderr = process.communicate()
    if process.returncode != 0:
        print(f"Error executing command: {command}")
        print("STDOUT:")
        print(stdout)
        print("STDERR:")
        print(stderr)
        print(f"Return code: {process.returncode}")
        exit(1)
    return stdout, stderr


def compile_fastled_library():
    build_dir = HERE / ".build"
    build_dir.mkdir(parents=True, exist_ok=True)

    # Configure CMake
    cmake_configure_command = (
        f'cmake -S {PROJECT_ROOT / "tests"} -B {build_dir} -G "Ninja"'
    )
    stdout, stderr = run_command(cmake_configure_command)
    print(stdout)
    print(stderr)

    # Build the project
    cmake_build_command = f"cmake --build {build_dir} --verbose"
    stdout = run_command(cmake_build_command)
    print(stdout)
    print(stderr)

    print("FastLED library compiled successfully.")


def main():
    os.chdir(str(HERE))
    print(f"Current directory: {Path('.').absolute()}")
    compile_fastled_library()


if __name__ == "__main__":
    main()
