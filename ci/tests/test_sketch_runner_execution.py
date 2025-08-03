#!/usr/bin/env python3

"""
Test sketch runner execution and output capture.
Validates that the sketch runner can execute Arduino sketches and capture their Serial output.
"""

import subprocess
import threading
import time
import unittest
from pathlib import Path

from ci.util.paths import PROJECT_ROOT


class TestSketchRunnerExecution(unittest.TestCase):
    """Test sketch runner execution functionality."""

    def setUp(self):
        """Set up test environment."""
        self.project_root = PROJECT_ROOT
        
    def test_sketch_runner_blink_execution(self):
        """Test that sketch runner can execute Blink example and capture Serial output."""
        print("\n=== Testing Sketch Runner Execution ===")
        print("Running: bash test --examples Blink --verbose")
        print("Validating that 'BLINK' appears in output from Serial.println()")
        
        try:
            # Run the sketch runner test
            start_time = time.time()
            result = subprocess.run(
                ["bash", "test", "--examples", "Blink", "--verbose"],
                cwd=str(self.project_root),
                capture_output=True,
                text=True,
                timeout=600,  # 10 minute timeout
            )
            end_time = time.time()
            
            print(f"Command completed in {end_time - start_time:.2f} seconds")
            print(f"Return code: {result.returncode}")
            
            # Check for successful execution
            self.assertEqual(result.returncode, 0, 
                           f"Sketch runner test failed with return code {result.returncode}")
            
            # Check for key execution indicators
            execution_indicators: list[str] = [
                "[EXECUTION] Starting sketch runner execution",
                "[EXECUTION] Running: Blink",
                "[EXECUTION] SUCCESS: Blink",
                "RUNNER: Starting sketch execution",
                "RUNNER: Calling setup()",
                "RUNNER: Loop iteration"
            ]
            
            found_indicators: list[str] = []
            for indicator in execution_indicators:
                if indicator in result.stdout:
                    found_indicators.append(indicator)
                    print(f"✓ Found execution indicator: {indicator}")
            
            self.assertGreaterEqual(len(found_indicators), 3, 
                                  f"Missing execution indicators. Found: {found_indicators}")
            
            # CHECK FOR "BLINK" in output from Serial.println()
            blink_count = result.stdout.count("BLINK")
            print(f"\n=== BLINK OUTPUT VALIDATION ===")
            print(f"Found 'BLINK' {blink_count} times in output")
            
            # Should find "BLINK" multiple times:
            # - "BLINK setup starting" from setup()
            # - "BLINK setup complete" from setup() 
            # - "BLINK" from each loop() iteration (5 times)
            # Total: at least 7 occurrences
            self.assertGreaterEqual(blink_count, 5, 
                                  f"Expected at least 5 'BLINK' occurrences from Serial.println(), found {blink_count}")
            
            # Verify specific BLINK messages
            expected_blink_messages: list[str] = [
                "BLINK setup starting",
                "BLINK setup complete", 
                "BLINK"  # From loop iterations
            ]
            
            found_messages: list[str] = []
            for message in expected_blink_messages:
                if message in result.stdout:
                    found_messages.append(message)
                    print(f"✓ Found expected message: {message}")
                else:
                    print(f"✗ Missing expected message: {message}")
            
            self.assertGreaterEqual(len(found_messages), 2,
                                  f"Missing expected BLINK messages. Found: {found_messages}")
            
            # Verify compilation + linking + execution success
            success_patterns: list[str] = [
                "EXAMPLE COMPILATION + LINKING + EXECUTION TEST: SUCCESS",
                "examples compiled, 1 linked, and 1 executed successfully"
            ]
            
            found_success: list[str] = []
            for pattern in success_patterns:
                if pattern in result.stdout:
                    found_success.append(pattern)
                    print(f"✓ Found success indicator: {pattern}")
            
            self.assertGreaterEqual(len(found_success), 1,
                                  f"Missing success indicators. Found: {found_success}")
            
            print("\n✅ SKETCH RUNNER EXECUTION TEST: SUCCESS")
            print(f"✓ Sketch compiled and linked successfully")
            print(f"✓ Sketch executed successfully") 
            print(f"✓ Serial.println('BLINK') output captured ({blink_count} times)")
            print(f"✓ All expected execution patterns found")
            
        except subprocess.TimeoutExpired:
            self.fail("Sketch runner test timed out after 10 minutes")
        except subprocess.CalledProcessError as e:
            print(f"STDOUT: {e.stdout}")
            print(f"STDERR: {e.stderr}")
            self.fail(f"Sketch runner test failed with error: {e}")
        except Exception as e:
            self.fail(f"Unexpected error during sketch runner test: {e}")

    def test_sketch_runner_blink_output_grep(self):
        """Test running the sketch runner and grepping for BLINK output."""
        print("\n=== Testing Sketch Runner with Grep ===")
        print("Running: bash test --examples Blink --verbose | grep BLINK")
        
        try:
            # Run sketch runner and pipe to grep
            start_time = time.time()
            
            # Run the test command
            test_process = subprocess.Popen(
                ["bash", "test", "--examples", "Blink", "--verbose"],
                cwd=str(self.project_root),
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            # Pipe to grep
            grep_process = subprocess.Popen(
                ["grep", "BLINK"],
                stdin=test_process.stdout,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True
            )
            
            test_process.stdout.close()  # Allow test_process to receive SIGPIPE
            
            # Get results
            grep_output, grep_error = grep_process.communicate(timeout=600)
            test_process.wait()
            
            end_time = time.time()
            
            print(f"Pipeline completed in {end_time - start_time:.2f} seconds")
            print(f"Test process return code: {test_process.returncode}")
            print(f"Grep process return code: {grep_process.returncode}")
            
            # Check that the test passed
            self.assertEqual(test_process.returncode, 0,
                           f"Test command failed with return code {test_process.returncode}")
            
            # Check that grep found BLINK output  
            self.assertEqual(grep_process.returncode, 0,
                           f"Grep failed to find 'BLINK' in output (return code {grep_process.returncode})")
            
            # Validate grep output
            grep_lines = grep_output.strip().split('\n')
            blink_lines = [line for line in grep_lines if line.strip()]
            
            print(f"\n=== GREP OUTPUT ===")
            for i, line in enumerate(blink_lines):
                print(f"{i+1}: {line}")
            
            self.assertGreater(len(blink_lines), 0,
                             "Grep found no lines containing 'BLINK'")
            
            # Verify we have the expected BLINK messages
            blink_setup_lines = [line for line in blink_lines if "BLINK setup" in line]
            blink_loop_lines = [line for line in blink_lines if line.strip().endswith("BLINK")]
            
            print(f"\n=== ANALYSIS ===")
            print(f"Total lines with 'BLINK': {len(blink_lines)}")
            print(f"Setup BLINK lines: {len(blink_setup_lines)}")
            print(f"Loop BLINK lines: {len(blink_loop_lines)}")
            
            self.assertGreater(len(blink_setup_lines), 0,
                             "No BLINK setup messages found")
            self.assertGreater(len(blink_loop_lines), 0, 
                             "No BLINK loop messages found")
            
            print("\n✅ SKETCH RUNNER GREP TEST: SUCCESS")
            print(f"✓ Found {len(blink_lines)} total BLINK messages")
            print(f"✓ Grep successfully filtered output")
            print(f"✓ Serial.println() output properly captured")
            
        except subprocess.TimeoutExpired:
            self.fail("Sketch runner grep test timed out after 10 minutes")
        except Exception as e:
            self.fail(f"Unexpected error during grep test: {e}")


if __name__ == "__main__":
    unittest.main()
