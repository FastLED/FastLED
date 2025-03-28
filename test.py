#!/usr/bin/env python3
import os
import sys
import io
import subprocess
import time
import threading
import queue
import argparse
import _thread
from pathlib import Path
from typing import List, Tuple, Any, Optional
from ci.running_process import RunningProcess

_PIO_CHECK_ENABLED = False

_IS_GITHUB = os.environ.get('GITHUB_ACTIONS') == 'true'

def run_command(cmd: List[str], **kwargs: Any) -> None:
    """Run a command and handle errors"""
    try:
        subprocess.run(cmd, check=True, **kwargs)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)

def output_reader(process: subprocess.Popen[str], 
                 output_queue: queue.Queue[Tuple[str, str]], 
                 stop_event: threading.Event) -> None:
    """Read output from process and put it in the queue"""
    try:
        assert process.stdout is not None  # for mypy
        assert process.stderr is not None  # for mypy
        
        while not stop_event.is_set():
            # Use a small timeout so we can check the stop_event regularly
            if process.stdout.readable():
                stdout_line = process.stdout.readline()
                if stdout_line:
                    output_queue.put(('stdout', stdout_line))
            if process.stderr.readable():
                stderr_line = process.stderr.readline()
                if stderr_line:
                    output_queue.put(('stderr', stderr_line))
            
            # Check if process has ended and all output has been read
            if process.poll() is not None:
                # Get any remaining output
                remaining_out, remaining_err = process.communicate()
                if remaining_out:
                    output_queue.put(('stdout', remaining_out))
                if remaining_err:
                    output_queue.put(('stderr', remaining_err))
                break
    except KeyboardInterrupt:
        # Interrupt main thread and exit
        _thread.interrupt_main()
        return

def parse_args() -> argparse.Namespace:
    """Parse command line arguments"""
    parser = argparse.ArgumentParser(description='Run FastLED tests')
    parser.add_argument('--cpp', action='store_true',
                       help='Run C++ tests only')
    parser.add_argument('test', type=str, nargs='?', default=None,
                       help='Specific C++ test to run')
    parser.add_argument("--clang", action="store_true", help="Use Clang compiler")
    parser.add_argument("--clean", action="store_true", help="Clean build before compiling")
    return parser.parse_args()


def _make_pio_check_cmd() -> List[str]:
    return ['pio', 'check', '--skip-packages', 
                            '--src-filters=+<src/>', '--severity=medium',
                            '--fail-on-defect=high', '--flags',
                            '--inline-suppr --enable=all --std=c++17']


def main() -> None:
    try:
        args = parse_args()
        
        # Change to script directory
        os.chdir(Path(__file__).parent)

        cmd_list = [
            "uv",
            "run",
            "ci/cpp_test_run.py"
        ]

        if args.clang:
            cmd_list.append("--clang")

        if args.test:
            cmd_list.append("--test")
            cmd_list.append(args.test)
        if args.clean:
            cmd_list.append("--clean")

        cmd_str_cpp = subprocess.list2cmdline(cmd_list)

        if args.cpp:
            # Compile and run C++ tests
            start_time = time.time()
            if args.test:
                # Run specific C++ test
                proc = RunningProcess(cmd_str_cpp)
                proc.wait()
                if proc.returncode != 0:
                    print(f"Command failed: {proc.command}")
                    sys.exit(proc.returncode)
            else:
                # Run all C++ tests
                proc = RunningProcess(cmd_str_cpp)
                proc.wait()
                if proc.returncode != 0:
                    print(f"Command failed: {proc.command}")
                    sys.exit(proc.returncode)
            print(f"Time elapsed: {time.time() - start_time:.2f}s")
            return
        


        cmd_list = _make_pio_check_cmd()
        if not _PIO_CHECK_ENABLED:
            cmd_list = ['echo', 'pio check is disabled']

        cmd_str = subprocess.list2cmdline(cmd_list)
    
        print(f"Running command (in the background): {cmd_str}")
        pio_process = RunningProcess(cmd_str, echo=False, auto_run=not _IS_GITHUB)
        cpp_test_proc = RunningProcess(cmd_str_cpp)
        compile_native_proc = RunningProcess('uv run ci/ci-compile-native.py', echo=False)
        pytest_proc = RunningProcess('uv run pytest ci/tests', echo=False)
        tests = [cpp_test_proc, compile_native_proc, pytest_proc, pio_process]

        for test in tests:
            sys.stdout.flush()
            if not test.auto_run:
                test.run()
            test.wait()
            if not test.echo:
                for line in test.stdout.splitlines():
                    print(line)
            if test.returncode != 0:
                [t.kill() for t in tests]
                print(f"\nCommand failed: {test.command} with return code {test.returncode}")
                sys.exit(test.returncode)

        print("All tests passed")
        sys.exit(0)
    except KeyboardInterrupt:
        sys.exit(130)  # Standard Unix practice: 128 + SIGINT's signal number (2)

if __name__ == '__main__':
    main()
