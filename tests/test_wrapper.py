#!/usr/bin/env python3
"""
Test wrapper for deadlock detection.

This wrapper runs a test (runner.exe + DLL) with timeout monitoring.
If the test hangs (exceeds timeout), it:
1. Detects the hang
2. Attaches lldb/gdb to dump thread stacks
3. Kills the hung process
4. Reports the hang with stack traces
"""

import os
import subprocess
import sys
import time
from pathlib import Path
from typing import Optional

from ci.util.deadlock_detector import handle_hung_test
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


DEFAULT_TIMEOUT = 20.0  # 20 seconds per test


def run_test_with_deadlock_detection(
    runner_exe: str,
    test_dll: str,
    timeout_seconds: float = DEFAULT_TIMEOUT,
) -> int:
    """
    Run a test with deadlock detection.

    Args:
        runner_exe: Path to runner.exe (loads test DLLs)
        test_dll: Path to test DLL
        timeout_seconds: Timeout in seconds (default: 10)

    Returns:
        Exit code (0 = success, non-zero = failure/timeout)
    """
    test_name = Path(test_dll).stem

    print(f"Running test: {test_name} (timeout: {timeout_seconds}s)")

    start_time = time.time()

    # Start the test process (runner.exe loads the test DLL)
    # NOTE: Crash handler uses signal chaining - internal dump first, then external debugger
    # No need to disable crash handler anymore!
    try:
        proc = subprocess.Popen(
            [runner_exe, test_dll],
            stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT,
            text=True,
            bufsize=1,  # Line buffered
        )
    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        raise
    except Exception as e:
        print(f"Failed to start test: {e}", file=sys.stderr)
        return 1

    # Monitor the process
    try:
        while True:
            # Check if process has finished
            returncode = proc.poll()
            if returncode is not None:
                # Process finished
                # Read any remaining output
                if proc.stdout:
                    remaining = proc.stdout.read()
                    if remaining:
                        print(remaining, end="")

                elapsed = time.time() - start_time
                if returncode == 0:
                    print(f"✓ Test passed in {elapsed:.2f}s")
                else:
                    print(f"✗ Test failed with code {returncode} in {elapsed:.2f}s")

                return returncode

            # Check for timeout
            current_time = time.time()
            elapsed = current_time - start_time

            if elapsed > timeout_seconds:
                # Test has hung!
                print(f"\n{'=' * 80}")
                print(f"TEST HUNG: {test_name}")
                print(f"Exceeded timeout of {timeout_seconds}s")
                print(f"{'=' * 80}\n")

                # Get PID
                pid = proc.pid

                # Dump stack trace and kill process
                handle_hung_test(pid, test_name, timeout_seconds)

                # Wait a moment for process to die
                try:
                    proc.wait(timeout=2)
                except subprocess.TimeoutExpired:
                    # Force kill if still alive
                    proc.kill()
                    proc.wait()

                print(f"\nTest was killed due to timeout ({timeout_seconds}s)")
                return 124  # Timeout exit code (similar to timeout command)

            # Try to read output
            if proc.stdout:
                try:
                    # Try to read a line with timeout
                    import select

                    if hasattr(select, "select"):
                        # Unix-like systems with select
                        ready, _, _ = select.select([proc.stdout], [], [], 0.1)
                        if ready:
                            line = proc.stdout.readline()
                            if line:
                                print(line, end="")
                    else:
                        # Windows - just try to read (may block briefly)
                        # Use a thread to avoid blocking
                        import queue
                        import threading

                        q: "queue.Queue[Optional[str]]" = queue.Queue()
                        # Capture stdout for closure (type narrowing)
                        stdout = proc.stdout
                        assert stdout is not None

                        def read_line():
                            try:
                                line = stdout.readline()
                                q.put(line)
                            except KeyboardInterrupt:
                                handle_keyboard_interrupt_properly()
                                q.put(None)
                            except:
                                q.put(None)

                        t = threading.Thread(target=read_line)
                        t.daemon = True
                        t.start()
                        t.join(timeout=0.1)

                        try:
                            line = q.get_nowait()
                            if line:
                                print(line, end="")
                        except queue.Empty:
                            pass

                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                    raise
                except Exception as e:
                    # No output available or error reading
                    time.sleep(0.1)
            else:
                time.sleep(0.1)

    except KeyboardInterrupt:
        handle_keyboard_interrupt_properly()
        print("\nTest interrupted by user")
        proc.kill()
        proc.wait()
        return 130  # SIGINT exit code


def main():
    """Main entry point."""
    if len(sys.argv) < 3:
        print(
            "Usage: test_wrapper.py <runner_exe> <test_dll> [timeout_seconds]",
            file=sys.stderr,
        )
        return 1

    runner_exe = sys.argv[1]
    test_dll = sys.argv[2]
    timeout_seconds = float(sys.argv[3]) if len(sys.argv) > 3 else DEFAULT_TIMEOUT

    if not os.path.exists(runner_exe):
        print(f"Runner executable not found: {runner_exe}", file=sys.stderr)
        return 1

    if not os.path.exists(test_dll):
        print(f"Test DLL not found: {test_dll}", file=sys.stderr)
        return 1

    return run_test_with_deadlock_detection(runner_exe, test_dll, timeout_seconds)


if __name__ == "__main__":
    sys.exit(main())
