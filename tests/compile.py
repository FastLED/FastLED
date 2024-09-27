import os
import subprocess
from pathlib import Path

from ci.paths import PROJECT_ROOT

HERE = Path(__file__).resolve().parent

def run_command(command):
    process = subprocess.Popen(command, stdout=subprocess.PIPE, stderr=subprocess.PIPE, shell=True)
    stdout, stderr = process.communicate()
    if process.returncode != 0:
        print(f"Error executing command: {command}")
        print(stderr.decode())
        exit(1)
    return stdout.decode()

def compile_fastled_library():
    makefile_path = HERE / "Makefile"
    run_command(f"make -f {makefile_path}")
    print("FastLED library compiled successfully.")

def main():
    os.chdir(str(PROJECT_ROOT))
    print(f"Current directory: {Path('.').absolute()}")
    compile_fastled_library()

if __name__ == "__main__":
    main()
