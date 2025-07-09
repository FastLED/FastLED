#!/usr/bin/env python3
"""
Test script to reproduce the hang condition in bash test
"""
import subprocess
import threading
import time
import sys
from pathlib import Path

# Import the RunningProcess class
sys.path.insert(0, str(Path(__file__).parent / "ci" / "ci"))
try:
    from running_process import RunningProcess
except ImportError:
    # Try alternative path
    sys.path.insert(0, str(Path(__file__).parent / "ci"))
    from ci.running_process import RunningProcess

def test_hang_reproducer():
    """Reproduce the hang condition"""
    print("=== HANG REPRODUCER TEST ===")
    print(f"Starting at: {time.strftime('%H:%M:%S')}")
    
    # Simulate the exact pattern from test.py
    tests = []
    
    # Create processes similar to test.py
    pio_process = RunningProcess('echo "pio check is disabled"', echo=False, auto_run=False)
    cpp_test_proc = RunningProcess('uv run ci/cpp_test_run.py', echo=True, auto_run=False)
    compile_native_proc = RunningProcess('uv run ci/ci-compile-native.py', echo=False, auto_run=False)
    pytest_proc = RunningProcess('uv run pytest -s ci/tests -xvs --durations=0', echo=True, auto_run=False)
    impl_files_proc = RunningProcess('uv run ci/ci/check_implementation_files.py --check-inclusion --ascii-only --suppress-summary-on-100-percent', echo=False, auto_run=False)
    
    tests = [cpp_test_proc, compile_native_proc, pytest_proc, impl_files_proc, pio_process]
    
    print(f"Created {len(tests)} test processes")
    
    # Simulate the test execution loop
    is_first = True
    for i, test in enumerate(tests):
        was_first = is_first
        is_first = False
        
        print(f"\n--- Starting test {i+1}/{len(tests)}: {test.command} ---")
        
        if not test.auto_run:
            test.run()
        
        print(f"Waiting for command: {test.command}")
        
        # Create the timeout monitoring thread (like in test.py)
        event_stopped = threading.Event()
        
        def _runner():
            start_time = time.time()
            while not event_stopped.wait(1):
                curr_time = time.time()
                seconds = int(curr_time - start_time)
                if not was_first:
                    print(f"Waiting for command: {test.command} to finish...{seconds} seconds")
        
        runner_thread = threading.Thread(target=_runner, daemon=True)
        runner_thread.start()
        
        # Wait for the test to complete
        try:
            test.wait()
            event_stopped.set()
            runner_thread.join(timeout=1)
            
            if not test.echo:
                for line in test.stdout.splitlines():
                    print(line)
                    
            if test.returncode != 0:
                print(f"Test failed: {test.command} with return code {test.returncode}")
                # Kill all remaining tests
                [t.kill() for t in tests]
                return False
                
        except Exception as e:
            print(f"Exception during test execution: {e}")
            event_stopped.set()
            runner_thread.join(timeout=1)
            [t.kill() for t in tests]
            return False
    
    print("All tests completed successfully")
    return True

if __name__ == "__main__":
    success = test_hang_reproducer()
    sys.exit(0 if success else 1)
