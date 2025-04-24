#!/usr/bin/env python3
import os
import sys
import subprocess
import time
import argparse
import hashlib
import threading
import signal
from pathlib import Path
from typing import List, Any, Optional
from ci.running_process import RunningProcess

_PIO_CHECK_ENABLED = False

_IS_GITHUB = os.environ.get('GITHUB_ACTIONS') == 'true'

# Global variable to store the fingerprint thread
_fingerprint_thread: Optional[threading.Thread] = None
_stop_fingerprint = threading.Event()

def run_command(cmd: List[str], **kwargs: Any) -> None:
    """Run a command and handle errors"""
    try:
        subprocess.run(cmd, check=True, **kwargs)
    except subprocess.CalledProcessError as e:
        sys.exit(e.returncode)


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


def make_compile_uno_test_process() -> RunningProcess:
    """Create a process to compile the uno tests"""
    cmd = ['uv', 'run', 'ci/ci-compile.py', 'uno', '--examples', 'Blink', '--no-interactive']
    return RunningProcess(cmd)


def _fingerprint_code_base(start_directory: Path, glob: str = "**/*.h,**/*.cpp,**/*.hpp", stop_event: Optional[threading.Event] = None) -> str:
    """
    Create a fingerprint of the code base by hashing file contents.
    
    Args:
        start_directory: The root directory to start scanning from
        glob: Comma-separated list of glob patterns to match files
        stop_event: Event to check for cancellation
    
    Returns:
        A hex digest string representing the fingerprint of the code base
    """
    hasher = hashlib.sha256()
    patterns = glob.split(',')
    
    # Get all matching files
    all_files = []
    for pattern in patterns:
        pattern = pattern.strip()
        if stop_event and stop_event.is_set():
            return ""
        all_files.extend(sorted(start_directory.glob(pattern)))
    
    # Sort files for consistent ordering
    all_files.sort()
    
    # Process each file
    for file_path in all_files:
        if stop_event and stop_event.is_set():
            return ""
            
        if file_path.is_file():
            # Add the relative path to the hash
            rel_path = file_path.relative_to(start_directory)
            hasher.update(str(rel_path).encode('utf-8'))
            
            # Add the file content to the hash
            try:
                with open(file_path, 'rb') as f:
                    # Read in chunks to handle large files
                    for chunk in iter(lambda: f.read(4096), b''):
                        if stop_event and stop_event.is_set():
                            return ""
                        hasher.update(chunk)
            except Exception as e:
                # If we can't read the file, include the error in the hash
                hasher.update(f"ERROR:{str(e)}".encode('utf-8'))
    
    return hasher.hexdigest()


def start_fingerprint_thread(root_dir: Path = None) -> None:
    """
    Start a daemon thread to compute the code base fingerprint and save it to a file.
    
    Args:
        root_dir: The root directory to start scanning from. If None, uses the current directory.
    """
    global _fingerprint_thread, _stop_fingerprint
    
    # Reset the stop event
    _stop_fingerprint.clear()
    
    if root_dir is None:
        root_dir = Path.cwd() / "src"
    
    def fingerprint_worker():
        try:
            # Create .cache directory if it doesn't exist
            cache_dir = Path('.cache')
            cache_dir.mkdir(exist_ok=True)
            
            # Compute the fingerprint
            fingerprint = _fingerprint_code_base(root_dir, stop_event=_stop_fingerprint)
            
            # If we were cancelled, don't write the file
            if _stop_fingerprint.is_set():
                return
                
            # Save the fingerprint to a file
            fingerprint_file = cache_dir / 'fingerprint'
            with open(fingerprint_file, 'w') as f:
                f.write(fingerprint)
                
        except Exception as e:
            print(f"Error in fingerprint thread: {e}", file=sys.stderr)
    
    # Create and start the daemon thread
    _fingerprint_thread = threading.Thread(target=fingerprint_worker, daemon=True)
    _fingerprint_thread.start()


def stop_fingerprint_thread() -> None:
    """
    Signal the fingerprint thread to stop and wait for it to finish.
    """
    global _fingerprint_thread, _stop_fingerprint
    
    if _fingerprint_thread and _fingerprint_thread.is_alive():
        _stop_fingerprint.set()
        # We don't join since it's a daemon thread and will be terminated when the program exits



def main() -> None:
    try:
        args = parse_args()
        
        # Change to script directory
        os.chdir(Path(__file__).parent)
        
        # Start the fingerprint thread in the background
        start_fingerprint_thread()
        
        # Set up signal handler to stop the fingerprint thread on Ctrl+C
        def signal_handler(sig, frame):
            stop_fingerprint_thread()
            sys.exit(130)  # Standard Unix practice: 128 + SIGINT's signal number (2)
            
        signal.signal(signal.SIGINT, signal_handler)

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
        
        # Start the compile uno process
        compile_uno_proc = make_compile_uno_test_process()
        
        cmd_list = _make_pio_check_cmd()
        if not _PIO_CHECK_ENABLED:
            cmd_list = ['echo', 'pio check is disabled']

        cmd_str = subprocess.list2cmdline(cmd_list)
    
        print(f"Running command (in the background): {cmd_str}")
        pio_process = RunningProcess(cmd_str, echo=False, auto_run=not _IS_GITHUB)
        cpp_test_proc = RunningProcess(cmd_str_cpp)
        compile_native_proc = RunningProcess('uv run ci/ci-compile-native.py', echo=False, auto_run=not _IS_GITHUB)
        pytest_proc = RunningProcess('uv run pytest ci/tests', echo=False)
        tests = [cpp_test_proc, compile_native_proc, pytest_proc, pio_process, compile_uno_proc]

        for test in tests:
            sys.stdout.flush()
            if not test.auto_run:
                test.run()
            print(f"Waiting for command: {test.command}")
            test.wait()
            if not test.echo:
                for line in test.stdout.splitlines():
                    print(line)
            if test.returncode != 0:
                [t.kill() for t in tests]
                print(f"\nCommand failed: {test.command} with return code {test.returncode}")
                sys.exit(test.returncode)

        print("All tests passed")
        # Wait a moment for the fingerprint thread to finish if it's still running
        if _fingerprint_thread and _fingerprint_thread.is_alive():
            print("Waiting for fingerprint calculation to complete...")
            _fingerprint_thread.join(timeout=2.0)
        sys.exit(0)
    except KeyboardInterrupt:
        stop_fingerprint_thread()
        sys.exit(130)  # Standard Unix practice: 128 + SIGINT's signal number (2)

if __name__ == '__main__':
    main()
