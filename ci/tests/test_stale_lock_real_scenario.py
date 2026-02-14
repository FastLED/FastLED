#!/usr/bin/env python3
"""
Real-World Stale Lock Scenario Test

This test creates REAL stale locks by force-killing actual processes,
demonstrating the bug and validating the auto-recovery mechanism.

Unlike test_stale_lock_recovery.py (which uses simulated fake PIDs),
this test uses actual process spawning and killing to reproduce the
exact scenario that occurs when users Ctrl+C or kill processes.
"""

import os
import subprocess
import sys
import tempfile
import time
import unittest
from pathlib import Path

import pytest

from ci.util.build_lock import BuildLock
from ci.util.file_lock_rw import FileLock, is_lock_stale, is_process_alive
from ci.util.global_interrupt_handler import handle_keyboard_interrupt_properly


class TestRealStaleLockScenario(unittest.TestCase):
    """Test suite for real stale lock scenarios with actual process killing."""

    def setUp(self):
        """Create temporary directory for test locks."""
        self.temp_dir = Path(tempfile.mkdtemp())
        self.lock_path = self.temp_dir / "test.lock"

    def tearDown(self):
        """Clean up test files."""
        # Clean up lock files
        if self.lock_path.exists():
            try:
                self.lock_path.unlink()
            except OSError:
                pass

        metadata_file = self.lock_path.with_suffix(".lock.pid")
        if metadata_file.exists():
            try:
                metadata_file.unlink()
            except OSError:
                pass

        # Clean up temp directory
        try:
            self.temp_dir.rmdir()
        except OSError:
            pass

    def _create_lock_holder_script(self) -> Path:
        """
        Create a standalone Python script that holds a lock.

        This script:
        1. Acquires a lock
        2. Prints its own PID to stdout (for parent to kill)
        3. Prints "READY" to signal lock acquisition
        4. Sleeps forever (until killed)

        Returns:
            Path to the created script
        """
        script_path = self.temp_dir / "lock_holder.py"

        script_content = f"""
import sys
import os
import time
from pathlib import Path

# Add project root to path
sys.path.insert(0, r'{Path(__file__).parent.parent.parent.resolve()}')

from ci.util.file_lock_rw import FileLock

# Get lock path from command line
lock_path = Path(sys.argv[1])

# Print our PID first (so parent can kill us)
print(f"PID:{{os.getpid()}}", flush=True)

# Acquire lock
with FileLock(lock_path, operation="real_test_scenario"):
    print("READY", flush=True)  # Signal that lock is acquired
    time.sleep(9999)  # Hold lock forever (until killed)
"""
        script_path.write_text(script_content)
        return script_path

    @pytest.mark.serial  # Can't run in parallel (process killing)
    @pytest.mark.slow  # This test is slower due to process spawning
    def test_real_force_kill_and_auto_recovery(self):
        """
        REAL SCENARIO: Force-kill a process holding a lock, verify auto-recovery.

        This test demonstrates the actual bug:
        1. Process acquires lock
        2. Process is force-killed (SIGKILL/Ctrl+C/task manager)
        3. Lock file remains on disk (stale lock)
        4. Next process should auto-recover (break stale lock and continue)
        """
        # Create lock holder script
        script_path = self._create_lock_holder_script()

        # Spawn child process that holds lock
        proc = subprocess.Popen(
            [sys.executable, str(script_path), str(self.lock_path)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,  # Line buffered
        )

        try:
            # Read the PID from child process
            pid_line = proc.stdout.readline()
            self.assertTrue(
                pid_line.startswith("PID:"), f"Expected PID line, got: {pid_line}"
            )
            child_pid = int(pid_line.split(":")[1].strip())
            print(f"\n[TEST] Child process PID: {child_pid}")

            # Wait for "READY" signal (lock acquired)
            ready_line = proc.stdout.readline()
            self.assertEqual(ready_line.strip(), "READY", "Lock not acquired by child")
            print(f"[TEST] Child acquired lock successfully")

            # Give it a moment to fully write metadata
            time.sleep(0.2)

            # Verify lock file and metadata exist
            metadata_file = self.lock_path.with_suffix(".lock.pid")
            self.assertTrue(self.lock_path.exists(), "Lock file should exist")
            self.assertTrue(
                metadata_file.exists(), "Metadata file should exist after lock acquired"
            )
            print(f"[TEST] Lock files verified: {self.lock_path}")

            # Verify child process is alive BEFORE killing
            self.assertTrue(
                is_process_alive(child_pid),
                f"Child process {child_pid} should be alive before kill",
            )
            print(f"[TEST] Child process {child_pid} is alive (verified)")

            # ====================================================================
            # SIMULATE USER PRESSING CTRL+C OR TASK MANAGER KILL
            # ====================================================================
            print(f"[TEST] Force-killing child process {child_pid} (SIGKILL)...")

            try:
                import psutil

                # Kill the specific PID (the one holding the lock)
                process = psutil.Process(child_pid)
                process.kill()  # SIGKILL (immediate termination, no cleanup)
                print(f"[TEST] Sent SIGKILL to PID {child_pid}")
            except ImportError:
                # Fallback: kill wrapper process
                proc.kill()
                print(f"[TEST] Sent kill signal to wrapper process")

            # Wait for process to actually die
            try:
                proc.wait(timeout=5.0)
                print(f"[TEST] Process terminated (exit code: {proc.returncode})")
            except subprocess.TimeoutExpired:
                print(f"[TEST] WARNING: Process did not terminate within 5 seconds")
                proc.kill()  # Force kill wrapper
                proc.wait()

            # Give OS time to clean up process (especially on Windows)
            time.sleep(1.0)

            # ====================================================================
            # VERIFY STALE LOCK EXISTS (THE BUG)
            # ====================================================================
            print(f"[TEST] Verifying stale lock condition...")

            # Lock file should still exist (not cleaned up due to force-kill)
            self.assertTrue(
                self.lock_path.exists(),
                "Lock file should remain after force-kill (this is the bug!)",
            )
            print(f"[TEST] ✓ Lock file remains after kill: {self.lock_path}")

            # Metadata should still exist
            self.assertTrue(
                metadata_file.exists(),
                "Metadata should remain after force-kill (this is the bug!)",
            )
            print(f"[TEST] ✓ Metadata remains after kill: {metadata_file}")

            # Process should be dead (verify kill worked)
            max_retries = 10
            for retry in range(max_retries):
                if not is_process_alive(child_pid):
                    print(
                        f"[TEST] ✓ Child process {child_pid} confirmed dead (retry {retry + 1})"
                    )
                    break
                print(
                    f"[TEST] Waiting for PID {child_pid} to die... (retry {retry + 1}/{max_retries})"
                )
                time.sleep(0.5)
            else:
                # Last attempt
                if is_process_alive(child_pid):
                    # On Windows, PID might be reused very quickly
                    # This is acceptable - the stale lock detection will still work
                    print(
                        f"[TEST] WARNING: PID {child_pid} still appears alive (may be reused)"
                    )
                    print(
                        f"[TEST] Proceeding with test - stale detection should handle this"
                    )

            # Lock should be detected as stale
            is_stale = is_lock_stale(self.lock_path)
            if is_stale:
                print(f"[TEST] ✓ Lock correctly detected as STALE")
            else:
                print(f"[TEST] ⚠️  Lock NOT detected as stale (may indicate PID reuse)")
                # This is acceptable on Windows - auto-recovery will still trigger on timeout

            # ====================================================================
            # VERIFY AUTO-RECOVERY (THE FIX)
            # ====================================================================
            print(
                f"\n[TEST] Attempting to acquire lock (should auto-recover from stale lock)..."
            )

            # Try to acquire the lock - should succeed via auto-recovery
            start_time = time.time()
            acquisition_succeeded = False

            try:
                with FileLock(self.lock_path, timeout=15.0, operation="recovery_test"):
                    acquisition_succeeded = True
                    elapsed = time.time() - start_time
                    print(
                        f"[TEST] ✅ SUCCESS: Lock acquired after {elapsed:.2f}s (auto-recovery worked!)"
                    )

                    # Verify we actually hold the lock
                    self.assertTrue(
                        self.lock_path.exists(), "Lock should exist while we hold it"
                    )
                    print(f"[TEST] ✓ Verified we now hold the lock")

            except TimeoutError as e:
                elapsed = time.time() - start_time
                self.fail(
                    f"❌ FAILED: Auto-recovery did not work! Lock acquisition timed out after {elapsed:.2f}s. "
                    f"Error: {e}"
                )

            # Verify acquisition succeeded
            self.assertTrue(
                acquisition_succeeded,
                "Lock acquisition should succeed via auto-recovery",
            )

            # After releasing lock, files should be cleaned up
            print(f"[TEST] Verifying cleanup after lock release...")
            self.assertFalse(
                metadata_file.exists(),
                "Metadata should be removed after normal lock release",
            )
            print(f"[TEST] ✓ Metadata cleaned up after release")

            print(f"\n[TEST] ✅ ALL CHECKS PASSED - Auto-recovery works correctly!")

        finally:
            # Cleanup: ensure child process is dead
            if proc.poll() is None:
                try:
                    proc.kill()
                    proc.wait(timeout=2.0)
                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                except Exception:
                    pass

            # Cleanup: remove script
            try:
                script_path.unlink()
            except OSError:
                pass

    @pytest.mark.serial
    @pytest.mark.slow
    def test_buildlock_auto_recovery(self):
        """
        Test BuildLock auto-recovery with real process killing.

        This verifies that BuildLock (used for libfastled builds) also
        properly recovers from stale locks.
        """
        print("\n[TEST] Testing BuildLock auto-recovery...")

        # Create a BuildLock instance
        lock_name = "test_buildlock_recovery"
        build_lock = BuildLock(lock_name=lock_name, use_global=False)

        # Create script that holds BuildLock
        script_path = self.temp_dir / "buildlock_holder.py"
        script_content = f"""
import sys
import os
import time
from pathlib import Path

# Add project root to path
sys.path.insert(0, r'{Path(__file__).parent.parent.parent.resolve()}')

from ci.util.build_lock import BuildLock

# Create BuildLock
lock = BuildLock(lock_name="{lock_name}", use_global=False)

# Print our PID
print(f"PID:{{os.getpid()}}", flush=True)

# Acquire lock
if lock.acquire(timeout=5.0):
    print("READY", flush=True)
    time.sleep(9999)  # Hold forever
else:
    print("FAILED", flush=True)
    sys.exit(1)
"""
        script_path.write_text(script_content)

        # Spawn child process
        proc = subprocess.Popen(
            [sys.executable, str(script_path)],
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            bufsize=1,
        )

        try:
            # Get PID and wait for ready
            pid_line = proc.stdout.readline()
            child_pid = int(pid_line.split(":")[1].strip())
            ready_line = proc.stdout.readline()
            self.assertEqual(ready_line.strip(), "READY")
            print(f"[TEST] Child PID {child_pid} acquired BuildLock")

            time.sleep(0.2)

            # Force-kill
            try:
                import psutil

                psutil.Process(child_pid).kill()
            except ImportError:
                proc.kill()

            proc.wait(timeout=5.0)
            time.sleep(1.0)
            print(f"[TEST] Killed child process {child_pid}")

            # Verify lock file exists (stale)
            self.assertTrue(
                build_lock.lock_file.exists(),
                "BuildLock file should remain after force-kill",
            )
            print(f"[TEST] ✓ BuildLock file remains: {build_lock.lock_file}")

            # Try to acquire - should succeed via auto-recovery
            print(f"[TEST] Attempting to acquire BuildLock (auto-recovery)...")
            start_time = time.time()

            # Create new BuildLock instance (simulates new build process)
            new_lock = BuildLock(lock_name=lock_name, use_global=False)

            success = new_lock.acquire(timeout=15.0)
            elapsed = time.time() - start_time

            if success:
                print(
                    f"[TEST] ✅ BuildLock acquired after {elapsed:.2f}s (auto-recovery worked!)"
                )
                new_lock.release()
            else:
                self.fail(
                    f"❌ BuildLock auto-recovery failed - could not acquire after {elapsed:.2f}s"
                )

            print(f"[TEST] ✅ BuildLock auto-recovery works correctly!")

        finally:
            # Cleanup
            if proc.poll() is None:
                try:
                    proc.kill()
                    proc.wait(timeout=2.0)
                except KeyboardInterrupt:
                    handle_keyboard_interrupt_properly()
                except Exception:
                    pass

            try:
                script_path.unlink()
            except OSError:
                pass

    @pytest.mark.serial
    @pytest.mark.slow
    def test_multiple_sequential_recoveries(self):
        """
        Test that auto-recovery works multiple times in sequence.

        This ensures the fix is robust and doesn't leave the lock system
        in a bad state after recovery.
        """
        print("\n[TEST] Testing multiple sequential recoveries...")

        for iteration in range(3):
            print(f"\n[TEST] === Iteration {iteration + 1}/3 ===")

            # Create and kill process holding lock
            script_path = self._create_lock_holder_script()
            proc = subprocess.Popen(
                [sys.executable, str(script_path), str(self.lock_path)],
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                bufsize=1,
            )

            try:
                # Wait for lock acquisition
                pid_line = proc.stdout.readline()
                child_pid = int(pid_line.split(":")[1].strip())
                ready_line = proc.stdout.readline()
                self.assertEqual(ready_line.strip(), "READY")
                print(f"[TEST] Child PID {child_pid} acquired lock")

                time.sleep(0.2)

                # Force-kill
                try:
                    import psutil

                    psutil.Process(child_pid).kill()
                except ImportError:
                    proc.kill()

                proc.wait(timeout=5.0)
                time.sleep(0.5)
                print(f"[TEST] Killed child PID {child_pid}")

                # Verify stale lock
                self.assertTrue(
                    self.lock_path.exists(), "Lock should remain after kill"
                )

                # Acquire and release (triggers recovery)
                print(f"[TEST] Acquiring lock (auto-recovery)...")
                with FileLock(
                    self.lock_path, timeout=15.0, operation=f"iter_{iteration}"
                ):
                    print(f"[TEST] ✓ Lock acquired successfully")

                print(f"[TEST] ✓ Lock released successfully")

            finally:
                if proc.poll() is None:
                    try:
                        proc.kill()
                        proc.wait(timeout=2.0)
                    except KeyboardInterrupt:
                        handle_keyboard_interrupt_properly()
                    except Exception:
                        pass

                try:
                    script_path.unlink()
                except OSError:
                    pass

        print(f"\n[TEST] ✅ All {3} sequential recoveries succeeded!")


if __name__ == "__main__":
    unittest.main()
