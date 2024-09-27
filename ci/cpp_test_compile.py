import os
import subprocess
from pathlib import Path

from ci.paths import PROJECT_ROOT

HERE = Path(__file__).resolve().parent


def run_command(command):
    process = subprocess.Popen(
        command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True
    )
    stdout, stderr = process.communicate()
    if process.returncode != 0:
        print(f"Error executing command: {command}")
        print("STDOUT:")
        print(stdout.decode())
        print("STDERR:")
        print(stderr.decode())
        print(f"Return code: {process.returncode}")
        exit(1)
    return stdout.decode()


def compile_fastled_library():
    build_dir = HERE / ".build"
    build_dir.mkdir(parents=True, exist_ok=True)

    # Configure CMake
    cmake_configure_command = (
        f'cmake -S {PROJECT_ROOT / "tests"} -B {build_dir} -G "Ninja"'
    )
    run_command(cmake_configure_command)

    # Build the project
    cmake_build_command = f"cmake --build {build_dir}"
    run_command(cmake_build_command)

    print("FastLED library compiled successfully.")


def main():
    os.chdir(str(HERE))
    print(f"Current directory: {Path('.').absolute()}")
    compile_fastled_library()


if __name__ == "__main__":
    main()
