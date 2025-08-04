#!/usr/bin/env python3

"""
Test sketch runner execution and output capture.
Validates that the sketch runner can execute Arduino sketches and capture their Serial output.
"""

import subprocess
import sys
import time
import unittest
from pathlib import Path

from ci.util.paths import PROJECT_ROOT


class TestSketchRunnerExecution(unittest.TestCase):
    """Test sketch runner execution functionality."""

    def setUp(self):
        """Set up test environment."""
        self.project_root = PROJECT_ROOT

    def test_sketch_runner_comprehensive(self):
        """Comprehensive test: Blink example execution and output validation.

        This test validates sketch runner functionality by using existing compiled
        examples (if available) or creating minimal test executables to avoid
        competing with the main example compilation test for build resources.
        """
        print("\n=== Testing Sketch Runner - Comprehensive Test ===")

        # First, try to use existing compiled Blink executable if available
        blink_executable = (
            self.project_root / ".build" / "examples" / "Blink" / "Blink.exe"
        )
        used_existing_executable = False

        if blink_executable.exists():
            used_existing_executable = True
            print(f"Found existing Blink executable: {blink_executable}")
            print("Testing sketch runner with existing compiled executable")
            start_time = time.time()

            # Run the existing executable directly
            result = subprocess.run(
                [str(blink_executable)],
                capture_output=True,
                text=True,
                timeout=30,  # 30 second timeout
                cwd=str(blink_executable.parent),
            )
            end_time = time.time()

        else:
            print(
                "No existing Blink executable found - running minimal compilation test"
            )
            print(
                "Running: uv run python ci/compiler/test_example_compilation.py Blink --full --verbose"
            )
            print(
                "Note: This may conflict with parallel example compilation - consider running sequentially"
            )

            try:
                # Fall back to compilation only if no existing executable
                start_time = time.time()
                result = subprocess.run(
                    [
                        "uv",
                        "run",
                        "python",
                        "ci/compiler/test_example_compilation.py",
                        "Blink",
                        "--full",
                        "--verbose",
                    ],
                    cwd=str(self.project_root),
                    capture_output=True,
                    text=True,
                    timeout=600,  # 10 minute timeout
                )
                end_time = time.time()
            except subprocess.TimeoutExpired:
                self.fail("Comprehensive sketch runner test timed out after 10 minutes")

        try:
            print(f"Command completed in {end_time - start_time:.2f} seconds")
            print(f"Return code: {result.returncode}")

            # Check for successful execution
            self.assertEqual(
                result.returncode,
                0,
                f"Sketch runner test failed with return code {result.returncode}",
            )

            # === PART 1: BASIC EXECUTION VALIDATION ===
            print(f"\n=== PART 1: EXECUTION VALIDATION ===")

            # Check for key execution indicators (different for direct execution vs compilation)
            if used_existing_executable:
                # Direct execution - look for sketch runner patterns only
                execution_indicators: list[str] = [
                    "RUNNER: Starting sketch execution",
                    "RUNNER: Calling setup()",
                    "RUNNER: Loop iteration",
                ]
                required_indicators = 2  # Require at least 2 for direct execution
            else:
                # Full compilation - look for compilation + execution patterns
                execution_indicators: list[str] = [
                    "[EXECUTION] Starting sketch runner execution",
                    "[EXECUTION] Running: Blink",
                    "[EXECUTION] SUCCESS: Blink",
                    "RUNNER: Starting sketch execution",
                    "RUNNER: Calling setup()",
                    "RUNNER: Loop iteration",
                ]
                required_indicators = 3  # Require at least 3 for full compilation

            found_indicators: list[str] = []
            for indicator in execution_indicators:
                if indicator in result.stdout:
                    found_indicators.append(indicator)
                    print(f"[OK] Found execution indicator: {indicator}")

            self.assertGreaterEqual(
                len(found_indicators),
                required_indicators,
                f"Missing execution indicators. Found: {found_indicators}",
            )

            # Verify success (different patterns for direct vs full execution)
            if not used_existing_executable:
                # Only check compilation success patterns for full compilation
                success_patterns: list[str] = [
                    "EXAMPLE COMPILATION + LINKING + EXECUTION TEST: SUCCESS",
                    "examples compiled, 1 linked, and 1 executed successfully",
                ]

                found_success: list[str] = []
                for pattern in success_patterns:
                    if pattern in result.stdout:
                        found_success.append(pattern)
                        print(f"[OK] Found success indicator: {pattern}")

                self.assertGreaterEqual(
                    len(found_success),
                    1,
                    f"Missing success indicators. Found: {found_success}",
                )
            else:
                print(
                    "[OK] Direct execution completed - no compilation success check needed"
                )

            # === PART 2: BLINK OUTPUT VALIDATION ===
            print(f"\n=== PART 2: BLINK OUTPUT VALIDATION ===")

            # CHECK FOR "BLINK" in output from Serial.println() (check both stdout and stderr)
            combined_output = result.stdout + result.stderr
            blink_count = combined_output.count("BLINK")
            print(f"Found 'BLINK' {blink_count} times in combined output")

            # Debug: Show where BLINK messages are coming from
            stdout_blinks = result.stdout.count("BLINK")
            stderr_blinks = result.stderr.count("BLINK")
            print(f"BLINK distribution: stdout={stdout_blinks}, stderr={stderr_blinks}")

            # Should find "BLINK" multiple times:
            # - "BLINK setup starting" from setup()
            # - "BLINK setup complete" from setup()
            # - "BLINK" from each loop() iteration (5 times)
            # Total: at least 5 occurrences
            self.assertGreaterEqual(
                blink_count,
                5,
                f"Expected at least 5 'BLINK' occurrences from Serial.println(), found {blink_count}",
            )

            # Verify specific BLINK messages
            expected_blink_messages: list[str] = [
                "BLINK setup starting",
                "BLINK setup complete",
                "BLINK",  # From loop iterations
            ]

            found_messages: list[str] = []
            for message in expected_blink_messages:
                if message in combined_output:
                    found_messages.append(message)
                    print(f"[OK] Found expected message: {message}")
                else:
                    print(f"[MISS] Missing expected message: {message}")

            self.assertGreaterEqual(
                len(found_messages),
                2,
                f"Missing expected BLINK messages. Found: {found_messages}",
            )

            # === PART 3: OUTPUT FILTERING VALIDATION ===
            print(f"\n=== PART 3: OUTPUT FILTERING VALIDATION ===")

            # Filter output for BLINK lines using Python (platform-independent grep replacement)
            all_lines = combined_output.split("\n")
            blink_lines = [
                line for line in all_lines if "BLINK" in line and line.strip()
            ]

            print(f"Filtered {len(blink_lines)} lines containing 'BLINK':")
            for i, line in enumerate(blink_lines[:10]):  # Show first 10 lines only
                print(f"  {i + 1}: {line}")
            if len(blink_lines) > 10:
                print(f"  ... and {len(blink_lines) - 10} more lines")

            self.assertGreater(len(blink_lines), 0, "No lines containing 'BLINK' found")

            # Verify we have the expected BLINK message categories
            blink_setup_lines = [line for line in blink_lines if "BLINK setup" in line]
            blink_loop_lines = [
                line for line in blink_lines if line.strip().endswith("BLINK")
            ]

            print(f"\n=== FILTERING ANALYSIS ===")
            print(f"Total lines with 'BLINK': {len(blink_lines)}")
            print(f"Setup BLINK lines: {len(blink_setup_lines)}")
            print(f"Loop BLINK lines: {len(blink_loop_lines)}")

            self.assertGreater(
                len(blink_setup_lines), 0, "No BLINK setup messages found"
            )
            self.assertGreater(len(blink_loop_lines), 0, "No BLINK loop messages found")

            # === COMPREHENSIVE SUCCESS SUMMARY ===
            print("\n[SUCCESS] COMPREHENSIVE SKETCH RUNNER TEST: SUCCESS")
            print(f"[OK] Sketch executed successfully")
            print(f"[OK] Serial.println('BLINK') output captured ({blink_count} times)")
            print(f"[OK] All expected execution patterns found")
            print(f"[OK] Output filtering validation passed")
            print(f"[OK] Both basic and advanced output validation completed")
            print(f"[OK] Efficient execution cycle completed all validations")

        except subprocess.TimeoutExpired:
            self.fail("Comprehensive sketch runner test timed out after 10 minutes")
        except subprocess.CalledProcessError as e:
            print(f"STDOUT: {e.stdout}")
            print(f"STDERR: {e.stderr}")
            self.fail(f"Sketch runner test failed with error: {e}")
        except Exception as e:
            self.fail(f"Unexpected error during comprehensive sketch runner test: {e}")


if __name__ == "__main__":
    unittest.main()
