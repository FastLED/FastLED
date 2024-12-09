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
    parser.add_argument('--test', type=str,
                       help='Specific C++ test to run')
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

        if args.cpp:
            # Compile and run C++ tests
            start_time = time.time()
            if args.test:
                # Run specific C++ test
                run_command(['uv', 'run', 'ci/cpp_test_run.py', '--test', args.test])
            else:
                # Run all C++ tests
                run_command(['uv', 'run', 'ci/cpp_test_run.py'])
            print(f"Time elapsed: {time.time() - start_time:.2f}s")
            return
        
        python_test_cmds = [
            ['uv', 'run', 'ci/cpp_test_compile.py'],
            ['uv', 'run', 'ci/cpp_test_run.py'],
            ['uv', 'run', 'ci/ci-compile-native.py'],
            ['uv', 'run', 'pytest', 'ci/tests']
        ]
        python_test_procs: List[subprocess.Popen] = []
        for cmd in python_test_cmds:
            proc = subprocess.Popen(cmd, text=True, stdout=subprocess.PIPE, stderr=subprocess.PIPE)
            python_test_procs.append(proc)
        
        # Run all tests
        # Start pio check in background and capture output
        output_buffer = io.StringIO()
        output_queue: queue.Queue[Tuple[str, str]] = queue.Queue()

        cmd_list = _make_pio_check_cmd()
        if not _PIO_CHECK_ENABLED:
            cmd_list = ['echo', 'pio check is disabled']

        cmd_str = subprocess.list2cmdline(cmd_list)
        
        print(f"Running command (in the background): {cmd_str}")

        pio_process = subprocess.Popen(
            cmd_list,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True
        )

        # Create stop event
        stop_event = threading.Event()

        # Start daemon thread to read output
        reader_thread = threading.Thread(
            target=output_reader,
            args=(pio_process, output_queue, stop_event),
            daemon=True
        )
        reader_thread.start()

        if _IS_GITHUB:
            # github doesn't like this to run concurrently so just run it all now.
            # Print output to console
            while pio_process.poll() is None:
                try:
                    stream, line = output_queue.get(timeout=0.1)
                    print(line, end='')
                    output_buffer.write(line)
                except queue.Empty:
                    continue
            # final drain
            while not output_queue.empty():
                stream, line = output_queue.get_nowait()
                print(line, end='')
                output_buffer.write(line)

        try:
            # for cmd, proc in zip(cmds, procs):
            for proc in python_test_procs:
                stdout, _ = proc.communicate()
                # print(f"Command: {cmd}")
                print(f"stdout: {stdout}")

            print("Waiting on pio check to complete...")

            # Process output queue until pio check completes
            while pio_process.poll() is None:
                try:
                    stream, line = output_queue.get(timeout=0.1)
                    print(line, end='')
                    output_buffer.write(line)
                except queue.Empty:
                    continue

            # Process any remaining items in queue
            while not output_queue.empty():
                stream, line = output_queue.get_nowait()
                print(line, end='')
                output_buffer.write(line)

        except KeyboardInterrupt:
            print("\nInterrupted by user. Cleaning up...", file=sys.stderr)
            raise
        except Exception as e:
            print(f"Error: {e}", file=sys.stderr)
            raise

        finally:
            # Signal thread to stop and wait for it
            stop_event.set()
            reader_thread.join(timeout=4.0)  # Wait up to 1 second for thread to finish

            # If process is still running, terminate it
            if pio_process.poll() is None:
                pio_process.terminate()
                try:
                    pio_process.wait(timeout=1.0)
                except subprocess.TimeoutExpired:
                    pio_process.kill()  # Force kill if terminate doesn't work

        # Exit with pio check's status code if it failed
        if pio_process.returncode != 0:
            import warnings
            warnings.warn(f"pio check failed with return code {pio_process.returncode}")
            sys.exit(pio_process.returncode)
        print("All tests passed")
        sys.exit(0)
    except KeyboardInterrupt:
        sys.exit(130)  # Standard Unix practice: 128 + SIGINT's signal number (2)

if __name__ == '__main__':
    main()
