#!/usr/bin/env python3
"""
Test script to reproduce the deadlock condition in RunningProcess.wait()
"""
import subprocess
import threading
import time
import sys
import signal
from pathlib import Path

# Import the RunningProcess class
sys.path.insert(0, str(Path(__file__).parent / "ci" / "ci"))
try:
    from running_process import RunningProcess
except ImportError:
    sys.path.insert(0, str(Path(__file__).parent / "ci"))
    from ci.running_process import RunningProcess

def create_hanging_process():
    """Create a process that will hang by not producing output"""
    # This command will hang because it's waiting for input
    return RunningProcess('cat', echo=True, auto_run=False)

def test_deadlock_scenario():
    """Test the deadlock scenario"""
    print("=== DEADLOCK REPRODUCER TEST ===")
    print(f"Starting at: {time.strftime('%H:%M:%S')}")
    
    # Create a process that will hang
    hanging_proc = create_hanging_process()
    
    print("Starting hanging process...")
    hanging_proc.run()
    
    # Start a thread to monitor the process
    def monitor_thread():
        start_time = time.time()
        while time.time() - start_time < 30:  # Monitor for 30 seconds
            print(f"Monitor: Process returncode = {hanging_proc.returncode}")
            print(f"Monitor: Process alive = {hanging_proc.proc.poll() is None if hanging_proc.proc else 'No process'}")
            time.sleep(2)
    
    monitor = threading.Thread(target=monitor_thread, daemon=True)
    monitor.start()
    
    print("Attempting to wait for hanging process (this should timeout)...")
    
    try:
        # This should hang or timeout
        start_time = time.time()
        hanging_proc.wait()
        elapsed = time.time() - start_time
        print(f"Process completed after {elapsed:.2f} seconds")
    except Exception as e:
        print(f"Exception during wait: {e}")
    finally:
        print("Killing hanging process...")
        hanging_proc.kill()
    
    print("Test completed")

if __name__ == "__main__":
    test_deadlock_scenario()
