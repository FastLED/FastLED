#!/usr/bin/env python3
import os
import sys
import subprocess
import time
import argparse
import hashlib
import json
from pathlib import Path
from typing import List, Any, Dict
from ci.running_process import RunningProcess

_PIO_CHECK_ENABLED = False

_IS_GITHUB = os.environ.get('GITHUB_ACTIONS') == 'true'


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


def fingerprint_code_base(start_directory: Path, glob: str = "**/*.h,**/*.cpp,**/*.hpp") -> Dict[str, str]:
    """
    Create a fingerprint of the code base by hashing file contents.
    
    Args:
        start_directory: The root directory to start scanning from
        glob: Comma-separated list of glob patterns to match files
    
    Returns:
        A dictionary with hash and status
    """
    result = {
        "hash": "",
        "status": "started"
    }
    
    try:
        hasher = hashlib.sha256()
        patterns = glob.split(',')
        
        # Get all matching files
        all_files = []
        for pattern in patterns:
            pattern = pattern.strip()
            all_files.extend(sorted(start_directory.glob(pattern)))
        
        # Sort files for consistent ordering
        all_files.sort()
        
        # Process each file
        for file_path in all_files:
            if file_path.is_file():
                # Add the relative path to the hash
                rel_path = file_path.relative_to(start_directory)
                hasher.update(str(rel_path).encode('utf-8'))
                
                # Add the file content to the hash
                try:
                    with open(file_path, 'rb') as f:
                        # Read in chunks to handle large files
                        for chunk in iter(lambda: f.read(4096), b''):
                            hasher.update(chunk)
                except Exception as e:
                    # If we can't read the file, include the error in the hash
                    hasher.update(f"ERROR:{str(e)}".encode('utf-8'))
        
        result["hash"] = hasher.hexdigest()
        result["status"] = "all tests pass"
        return result
    except Exception as e:
        result["status"] = f"error: {str(e)}"
        return result


def calculate_fingerprint(root_dir: Path = None) -> Dict[str, str]:
    """
    Calculate the code base fingerprint.
    
    Args:
        root_dir: The root directory to start scanning from. If None, uses src directory.
        
    Returns:
        The fingerprint result dictionary
    """
    if root_dir is None:
        root_dir = Path.cwd() / "src"
    
    msg = "\n#######################################################"
    msg += f"\n# Fingerprint calculation started for {root_dir}"
    msg += "\n#######################################################"
    print(msg)
    start_time = time.time()
    
    # Compute the fingerprint
    result = fingerprint_code_base(root_dir)
    
    elapsed_time = time.time() - start_time
    msg = "\n#######################################################"
    msg += f"\n# Fingerprint calculation completed in {elapsed_time:.2f} seconds"
    msg += "\n#######################################################"
    print(msg)
    
    # Add timing information to the result
    result["elapsed_seconds"] = f"{elapsed_time:.2f}"
    
    return result



def main() -> None:
    try:
        args = parse_args()
        
        # Change to script directory
        os.chdir(Path(__file__).parent)
        
        # Calculate fingerprint
        fingerprint_result = calculate_fingerprint()
        
        # Create .cache directory if it doesn't exist
        cache_dir = Path('.cache')
        cache_dir.mkdir(exist_ok=True)
        
        # Save the fingerprint to a file as JSON
        fingerprint_file = cache_dir / 'fingerprint.json'
        with open(fingerprint_file, 'w') as f:
            json.dump(fingerprint_result, f, indent=2)

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
        sys.exit(0)
    except KeyboardInterrupt:
        sys.exit(130)  # Standard Unix practice: 128 + SIGINT's signal number (2)

if __name__ == '__main__':
    main()
