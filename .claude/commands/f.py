#!/usr/bin/env python3
"""
Claude command /f - Finalize the codebase by running lint and test, fixing issues
"""

import subprocess
import sys
import os
from pathlib import Path

def run_command(cmd, description):
    """Run a command and return success status"""
    print(f"\n[RUN] {description}...")
    try:
        result = subprocess.run(cmd, shell=True, capture_output=True, text=True, cwd=Path.cwd(), encoding='utf-8', errors='replace')
        if result.returncode == 0:
            print(f"[PASS] {description} passed")
            if result.stdout.strip():
                print(result.stdout)
            return True
        else:
            print(f"[FAIL] {description} failed")
            if result.stdout and result.stdout.strip():
                print("STDOUT:", result.stdout)
            if result.stderr and result.stderr.strip():
                print("STDERR:", result.stderr)
            return False
    except Exception as e:
        print(f"[FAIL] {description} failed with exception: {e}")
        return False

def main():
    """Main finalization workflow"""
    print("Starting finalization workflow...")

    # Step 1: Run linting
    lint_success = run_command("bash lint", "Linting")

    # Step 2: Run tests
    test_success = run_command("bash test", "Testing")

    # Summary
    print("\nFinalization Summary:")
    print(f"  Lint: {'PASS' if lint_success else 'FAIL'}")
    print(f"  Test: {'PASS' if test_success else 'FAIL'}")

    if lint_success and test_success:
        print("\nAll checks passed! Codebase is ready.")
        return 0
    else:
        print("\nSome checks failed. Please review the output above and fix issues.")
        return 1

if __name__ == "__main__":
    sys.exit(main())