#!/usr/bin/env python3
import subprocess

print("Quick debug: bash test --examples --verbose")
print("Looking for why BLINK test failed...")

try:
    result = subprocess.run(
        ["bash", "test", "--examples", "--verbose"],
        capture_output=True,
        text=True,
        timeout=60
    )
    
    print(f"Exit code: {result.returncode}")
    
    # Check if --examples is working
    if "--examples requires argument" in result.stderr or "unrecognized arguments" in result.stderr:
        print("❌ ISSUE FOUND: --examples requires an argument!")
        print("The command should be: bash test --examples Blink --verbose")
        print("Not: bash test --examples --verbose")
    
    # Look for other clues
    blink_count = result.stdout.count('BLINK')
    print(f"BLINK count: {blink_count}")
    
    if result.stderr:
        print(f"Error output: {result.stderr[:300]}...")

except Exception as e:
    print(f"Error: {e}")
