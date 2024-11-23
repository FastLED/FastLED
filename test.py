#!/usr/bin/env python3
import os
import sys
import tempfile
import subprocess
import time
from pathlib import Path

def run_command(cmd, **kwargs):
    """Run a command and handle errors"""
    try:
        subprocess.run(cmd, check=True, **kwargs)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)

def main():
    # Change to script directory
    os.chdir(Path(__file__).parent)


    if len(sys.argv) > 1 and sys.argv[1] == '--cpp':
        if len(sys.argv) > 2:
            # Compile and run specific C++ test
            start_time = time.time()
            run_command(['uv', 'run', 'ci/cpp_test_run.py', '--test', sys.argv[2]])
            print(f"Time elapsed: {time.time() - start_time:.2f}s")
        else:
            # Compile all C++ tests
            start_time = time.time()
            run_command(['uv', 'run', 'ci/cpp_test_run.py'])
            print(f"Time elapsed: {time.time() - start_time:.2f}s")
    else:
        # Run all tests
        # Start pio check in background and capture output
        with tempfile.NamedTemporaryFile(mode='w+') as temp_file:
            pio_process = subprocess.Popen(
                ['uv', 'run', 'pio', 'check', '--skip-packages', 
                 '--src-filters=+<src/>', '--severity=medium',
                 '--fail-on-defect=high', '--flags',
                 '--inline-suppr --enable=all --std=c++17'],
                stdout=temp_file,
                stderr=temp_file
            )

            # Run other tests while pio check runs
            run_command(['uv', 'run', 'ci/cpp_test_compile.py'])
            run_command(['uv', 'run', 'ci/cpp_test_run.py'])
            run_command(['uv', 'run', 'ci/ci-compile-native.py'])
            run_command(['uv', 'run', 'pytest', 'ci/tests'])

            print("Waiting on pio check to complete...")
            
            # Wait for pio check to complete
            pio_process.wait()
            
            # Display pio check output
            temp_file.seek(0)
            print(temp_file.read())

            # Exit with pio check's status code if it failed
            if pio_process.returncode != 0:
                sys.exit(pio_process.returncode)

if __name__ == '__main__':
    main()
